#include <math.h>
#include <omp.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slimemold_simulation.h"
#include "util.h"

#define EPSILON 0.001
// 45 degrees
#define SCATTER_BUFFER M_PI/4
// max number of agents that be in once cell before randomization
#define AGENTS_PER_CELL_THRESHOLD 1

// get the next x after moving distance units in direction
double next_x(double x, double distance, double direction) {
    return x + distance * cos(direction);
}
// get the next y after moving distance units in direction
double next_y(double y, double distance, double direction) {
    return y + distance * sin(direction);
}

//gets index in map from a position
int get_index(int width, double x, double y) {
    return (int) y * width + (int) x;
}

// bounds the double between min and max
double bound(double n, double min, double max) {
    return fmax(min, fmin(max, n));
}

// Given some space to store every cell in widthxheight, stores the number of agents there
void record_position(int *agent_pos_freq, int width, int height, struct Agent *agents, int nagents) {
    memset(agent_pos_freq, 0, width * height * sizeof(*agent_pos_freq));
    #pragma omp parallel for
    for (int i = 0; i < nagents; i++) {
        int index = get_index(width, agents[i].x, agents[i].y);
        int oldval = agent_pos_freq[index];
        while (!atomic_compare_exchange_weak(&agent_pos_freq[index], &oldval, oldval + 1));
    }
}

void disperse_grid(double *grid, double *next_grid, int width, int height, double dispersion_rate) {
    // handles the center cells using a FTCS scheme.
    // finds the sum of the difference between the current and adjacent cells
    // and moves the current value by that difference scaled by the dispersion_rate
    #pragma omp parallel
    {
        #pragma omp for nowait
        for (int row = 1; row < height - 1; row++) {
            for (int col = 1; col < width - 1; col++) {
                int index = row * width + col;
                // sum the four adjacent cell
                next_grid[index] = grid[row * width + (col - 1)];
                next_grid[index] += grid[row * width + (col + 1)];
                next_grid[index] += grid[(row - 1) * width + col];
                next_grid[index] += grid[(row + 1) * width + col];
                // multiply the current sum by the dispersion_rate
                next_grid[index] *= dispersion_rate;
                // add (1 - 4 * dispersion_rate) * center cell
                next_grid[index] += (1 - 4 * dispersion_rate) * grid[row * width + col];
            }
        }
        // handles boundary condition
        // it is organized like this to make it easier to change the boundary condition
        // top row
        #pragma omp for nowait
        for (int col = 1; col < width - 1; col++) {
            next_grid[0 * width + col] = 0;
        }
        // bottom row
        #pragma omp for nowait
        for (int col = 1; col < width - 1; col++) {
            next_grid[(height - 1) * width + col] = 0;
        }
        // left column
        #pragma omp for nowait
        for (int row = 1; row < height - 1; row++) {
            next_grid[row * width + 0] = 0;
        }
        // right column
        #pragma omp for nowait
        for (int row = 1; row < height - 1; row++) {
            next_grid[row * width + (width - 1)] = 0;
        }
    }
    // top left corner
    next_grid[0 * width + 0] = 0;
    // top right corner
    next_grid[0 * width + (width - 1)] = 0;
    // bottom left corner
    next_grid[(height - 1) * width + 0] = 0;
    // bottom right corner
    next_grid[(height - 1)  * width + (width - 1)] = 0;
}

// uses a heat equation with prescribed boundary conditions value = 0
// warning: changes the map.grid pointer
void disperse_trail(struct Map *p_trail_map, double dispersion_rate) {
    // allocates a new map
    double *next_grid = malloc_or_die(p_trail_map->width * p_trail_map->height * sizeof(*next_grid));
    disperse_grid(p_trail_map->grid, next_grid, p_trail_map->width, p_trail_map->height, dispersion_rate);
    // switch next_map with the current grid
    free(p_trail_map->grid);
    p_trail_map->grid = next_grid;
}

// given the trail and food value, return the level of attraction
double attraction(double trail, double food) {
    // TODO un hardcode trail max
    double trail_max = 1000;
    return trail + 0*(1 - exp(10*trail/trail_max)) + food;
}

// turns in the direction with the highest trail value
void turn_uptrail(struct Agent *agent, double rotation_angle, double sensor_length, double sensor_angle, struct Map trail_map, struct Map food_map, unsigned int *seedp) {
    // randomize left and right order
    const int length = 3;
    int order[3];
    switch (randint(0, 1, seedp)) {
        case 0: order[0] = 0; order[1] = -1; order[2] = 1; break;
        case 1: order[0] = 0; order[1] = 1; order[2] = -1; break;
        default: fprintf(stderr, "Error selecting sensor order\n"); exit(1);
    }
    double max_direction = agent->direction;
    double max_trail = -INFINITY;
    for (int i = 0; i < length; i++) {
        double dir = agent->direction + (order[i] * sensor_angle);
        double ahead_x = next_x(agent->x, sensor_length, dir);
        double ahead_y = next_y(agent->y, sensor_length, dir);
        // check if it is in bounds
        if (ahead_x < EPSILON || ahead_x > trail_map.width - EPSILON || ahead_y < EPSILON || ahead_y > trail_map.height - EPSILON) {
            continue;
        }
        int index = get_index(trail_map.width, ahead_x, ahead_y);
        double attr = attraction(trail_map.grid[index], food_map.grid[index]);
        if(attr > max_trail) {
            max_trail = attr;
            max_direction = agent->direction + (order[i] * rotation_angle);
        }
    }
    agent->direction = max_direction;
}

void add_noise_to_movement(struct Agent *agent, double jitter_angle, unsigned int *seedp) {
    // returns uniform distribution with correct standard deviation
    agent->direction += randd(-jitter_angle, jitter_angle, seedp);
}

void check_wall_collision (struct Agent *agent, double *new_x, double *new_y, struct Map trail_map, unsigned int *seedp) {
    if (*new_x < EPSILON) {
        *new_x = EPSILON;
        // scatter off of left wall
        agent->direction = randd(-M_PI_2 + SCATTER_BUFFER, M_PI_2 - SCATTER_BUFFER, seedp);
    } else if (*new_x > trail_map.width - EPSILON) {
        // a little is subtracted from width because x is rounded down and
        // trail_map[y][width] would be out of bound
        *new_x = trail_map.width - EPSILON;
        // scatter off of right wall
        agent->direction = randd(M_PI_2 + SCATTER_BUFFER, 3 * M_PI_2 - SCATTER_BUFFER, seedp);
    }
    // note that the directions are a little weird since y = 0 is the top wall
    if (*new_y < EPSILON) {
        *new_y = EPSILON;
        // scatter off of top wall
        agent->direction = randd(SCATTER_BUFFER, M_PI - SCATTER_BUFFER, seedp);
    } else if (*new_y > trail_map.height - EPSILON) {
        *new_y = trail_map.height - EPSILON;
        // scatter off of bottom wall
        agent->direction = randd(M_PI + SCATTER_BUFFER, 2 * M_PI - SCATTER_BUFFER, seedp);
    }
}

void move_and_check_wall_collision (struct Agent *agent, double step_size, double sensor_length, double trail_max, struct Map trail_map, unsigned int *seedp) {
    // check trail strength from forward sensor
    double sensor_x = next_x(agent->x, sensor_length, agent->direction);
    sensor_x = bound(sensor_x, EPSILON, trail_map.width - EPSILON);
    double sensor_y = next_y(agent->y, sensor_length, agent->direction);
    sensor_y = bound(sensor_y, EPSILON, trail_map.height - EPSILON);
    double trail_strength = trail_map.grid[get_index(trail_map.width, sensor_x, sensor_y)];
    // set movement speed base on trail strength
    double cur_speed = step_size * (0.2 + 0.8 * (trail_strength / trail_max));

    // move in direction
    double new_x = next_x(agent->x, cur_speed, agent->direction);
    double new_y = next_y(agent->y, cur_speed, agent->direction);
    // check for collision
    // use seedp to prevent unused parameter error
    (void) seedp;
    // check_wall_collision(agent, &new_x, &new_y, trail_map, seedp);
    // wrap instead
    // TODO: wrap trail dispersion too
    new_x = fmod(new_x + trail_map.width, trail_map.width);
    new_y = fmod(new_y + trail_map.height, trail_map.height);
    // a little is subtracted from width because x is rounded down and
    // trail_map[y][width] would be out of bound
    if (new_x > trail_map.width - EPSILON) {
        new_x = trail_map.width - EPSILON;
    }
    if (new_y > trail_map.width - EPSILON) {
        new_y = trail_map.width - EPSILON;
    }
    //printf("new_x: %f new_y: %f\n", new_x, new_y);
    // update position
    agent->x = new_x;
    agent->y = new_y;
}

void evaporate_trail (struct Map trail_map, double evaporation_rate_exp, double evaporation_rate_lin) {
    #pragma omp parallel for
    for (int i = 0; i < trail_map.width * trail_map.height; i++) {
        trail_map.grid[i] = fmax(trail_map.grid[i] * (1 - evaporation_rate_exp) - evaporation_rate_lin, 0);
    }
}

void set_direction(struct Agent *agent, double rotation_angle, double sensor_length, double sensor_angle, double jitter_angle, struct Map trail_map, struct Map food_map, int *agent_pos_freq, unsigned int *seedp) {
    int index = get_index(trail_map.width, agent->x, agent->y);
    int freq = agent_pos_freq[index];
    if (freq > AGENTS_PER_CELL_THRESHOLD && randint(1, freq, seedp) > AGENTS_PER_CELL_THRESHOLD) {
        // randomized direction
        agent->direction = randd(-M_PI, M_PI, seedp);
    } else {
        turn_uptrail(agent, rotation_angle, sensor_length, sensor_angle, trail_map, food_map, seedp);
        add_noise_to_movement(agent, jitter_angle, seedp);
    }
}

void move_agents(struct Map trail_map, struct Map food_map, struct Agent *agents, int nagents, struct Behavior behavior, int *agent_pos_freq, unsigned int *seeds) {
    record_position(agent_pos_freq, trail_map.width, trail_map.height, agents, nagents);
    #pragma omp parallel
    {
        // copies the value to avoid false sharing
        unsigned int seed = seeds[omp_get_thread_num()];
        #pragma omp for
        for (int i = 0; i < nagents; i++) {
            struct Agent *agent = &agents[i];
            set_direction(agent, behavior.rotation_angle, behavior.sensor_length, behavior.sensor_angle, behavior.jitter_angle, trail_map, food_map, agent_pos_freq, &seed);
            move_and_check_wall_collision(agent, behavior.step_size, behavior.sensor_length, behavior.trail_max, trail_map, &seed);
        }
        seeds[omp_get_thread_num()] = seed;
    }
}

void deposit_trail(struct Map trail_map, struct Agent *agents, int nagents, double trail_deposit_rate, double trail_max) {
    #pragma omp parallel for
    for (int i = 0; i < nagents; i++) {
        int index = get_index(trail_map.width, agents[i].x, agents[i].y);
        double oldval = trail_map.grid[index];
        while (!atomic_compare_exchange_weak(&trail_map.grid[index], &oldval, fmin(trail_max, oldval + trail_deposit_rate)));
    }
}

void simulate_step(struct Map *p_trail_map, struct Map food_map, struct Agent *agents, int nagents, struct Behavior behavior, int *agent_pos_freq, unsigned int *seeds) {
    disperse_trail(p_trail_map, behavior.dispersion_rate);
    evaporate_trail(*p_trail_map, behavior.evaporation_rate_exp, behavior.evaporation_rate_lin);

    move_agents(*p_trail_map, food_map, agents, nagents, behavior, agent_pos_freq, seeds);
    deposit_trail(*p_trail_map, agents, nagents, behavior.trail_deposit_rate, behavior.trail_max);
}
