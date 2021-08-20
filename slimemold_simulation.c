#include <math.h>
#include <omp.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slimemold_simulation.h"
#include "util.h"

// Used to prevent rounding overflows right at the edge
#define EPSILON 0.001
// 20 degrees
#define SCATTER_BUFFER M_PI/9
// how many agents are allowed in one cell at once
#define MAX_PER_CELL 2

//
// get the next x after moving distance units in direction
double next_x(double x, double distance, double direction) {
    return x + distance * cos(direction);
}
// get the next y after moving distance units in direction
double next_y(double y, double distance, double direction) {
    return y + distance * sin(direction);
}

//gets index in map from a position
double get_index(int width, double x, double y) {
    return (int) y * width + (int) x;
}

// bounds the double between min and max
double bound(double n, double min, double max) {
    return fmax(min, fmin(max, n));
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
void disperse_trail(struct Map *p_map, double dispersion_rate) {
    // allocates a new map
    double *next_grid = malloc_or_die(p_map->width * p_map->height * sizeof(*next_grid));
    disperse_grid(p_map->grid, next_grid, p_map->width, p_map->height, dispersion_rate);
    // switch next_map with the current grid
    free(p_map->grid);
    p_map->grid = next_grid;
}
// turns in the direction with the highest trail value
void turn_uptrail(struct Agent *agent, double turn_rate, double sensor_length, double sensor_angle_factor, struct Map map, unsigned int *seedp) {
    // randomize order in which directions are checked to avoid bias in certain direction
    const int length = 3;
    int order[3];
    switch (randint(0, 5, seedp)) {
        case 0: order[0] = -1; order[1] = 0; order[2] = 1; break;
        case 1: order[0] = -1; order[1] = 1; order[2] = 0; break;
        case 2: order[0] = 0; order[1] = -1; order[2] = 1; break;
        case 3: order[0] = 0; order[1] = 1; order[2] = -1; break;
        case 4: order[0] = 1; order[1] = -1; order[2] = 0; break;
        case 5: order[0] = 1; order[1] = 0; order[2] = -1; break;
        default: fprintf(stderr, "Error selecting sensor order\n"); exit(1);
    }
    double max_direction = agent->direction;
    double max_trail = -INFINITY;
    for (int i = 0; i < length; i++) {
        double dir = agent->direction + (order[i] * turn_rate * sensor_angle_factor);
        double ahead_x = next_x(agent->x, sensor_length, dir);
        double ahead_y = next_y(agent->y, sensor_length, dir);
        // check if it is in bounds
        if (ahead_x < EPSILON || ahead_x > map.width - EPSILON || ahead_y < EPSILON || ahead_y > map.height - EPSILON) {
            continue;
        }
        int index = get_index(map.width, ahead_x, ahead_y);
        if(map.grid[index] > max_trail) {
            max_trail = map.grid[index];
            max_direction = agent->direction + (order[i] * turn_rate);
        }
    }
    agent->direction = max_direction;
}

void add_noise_to_movement(struct Agent *agent, double movement_noise, unsigned int *seedp) {
    // returns uniform distribution with correct standard deviation
    agent->direction += randd(-movement_noise * sqrt(3), movement_noise * sqrt(3), seedp);
}


bool will_collide(double new_x, double new_y, int *agent_pos_frequency, struct Map map) {
    int index = get_index(map.width, new_x, new_y);
    return new_x < EPSILON || new_x > map.width - EPSILON ||
            new_y < EPSILON || new_y > map.height - EPSILON ||
            agent_pos_frequency[index] >= MAX_PER_CELL;

}

// The agents cannot move into a wall or into a cell with too many agents.
// If it attempts to do so, it will not move an it's direction will be randomized.
void move_and_deposit_trail (struct Agent *agent, double movement_speed, double trail_deposit_rate, double trail_max, struct Map map, int *agent_pos_frequency, unsigned int *seedp) {
    // move in direction
    double new_x = next_x(agent->x, movement_speed, agent->direction);
    double new_y = next_y(agent->y, movement_speed, agent->direction);
    // check for collision
    if (will_collide(new_x, new_y, agent_pos_frequency, map)) {
        agent->direction = randd(0, 2 * M_PI, seedp);
    } else {
        // check if we need to update agent_pos_frequency
        int old_index = get_index(map.width, agent->x, agent->y);
        int new_index = get_index(map.width, new_x, new_y);
        if (old_index != new_index) {
            // subtract from old location
            int oldval = agent_pos_frequency[old_index];
            while (!atomic_compare_exchange_weak(&agent_pos_frequency[old_index], &oldval, oldval - 1));
            // add to new location
            oldval = agent_pos_frequency[new_index];
            while (!atomic_compare_exchange_weak(&agent_pos_frequency[new_index], &oldval, oldval + 1));
        }
        // update position
        agent->x = new_x;
        agent->y = new_y;
        // deposit trail
        double oldval = map.grid[new_index];
        while (!atomic_compare_exchange_weak(&map.grid[new_index], &oldval, fmin(trail_max, oldval + trail_deposit_rate)));
    }
}

void evaporate_trail (struct Map map, double evaporation_rate_exp, double evaporation_rate_lin) {
    #pragma omp parallel for
    for (int i = 0; i < map.width * map.height; i++) {
        map.grid[i] = fmax(map.grid[i] * (1 - evaporation_rate_exp) - evaporation_rate_lin, 0);
    }
}

void move_agents(struct Map map, struct Agent *agents, int nagents, struct Behavior behavior, int* agent_pos_frequency, unsigned int *seeds) {
    // to add randomness to which agent successfully moves
    shuffle(agents, nagents, sizeof(*agents), &seeds[0]);
    #pragma omp parallel
    {
        // copies the value to avoid false sharing
        unsigned int seed = seeds[omp_get_thread_num()];
        #pragma omp for
        for (int i = 0; i < nagents; i++) {
            struct Agent *agent = &agents[i];
            turn_uptrail(agent, behavior.turn_rate, behavior.sensor_length, behavior.sensor_angle_factor, map, &seed);
            add_noise_to_movement(agent, behavior.movement_noise, &seed);
        }
        #pragma omp for
        for (int i = 0; i < nagents; i++) {
            struct Agent *agent = &agents[i];
            move_and_deposit_trail(agent, behavior.movement_speed, behavior.trail_deposit_rate, behavior.trail_max, map, agent_pos_frequency, &seed);
        }
        seeds[omp_get_thread_num()] = seed;
    }
}

void simulate_step(struct Map *p_map, struct Agent *agents, int nagents, int *agent_pos_frequency, struct Behavior behavior, unsigned int *seeds) {
    disperse_trail(p_map, behavior.dispersion_rate);
    evaporate_trail(*p_map, behavior.evaporation_rate_exp, behavior.evaporation_rate_lin);
    move_agents(*p_map, agents, nagents, behavior, agent_pos_frequency, seeds);
}
