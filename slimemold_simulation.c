#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "slimemold_simulation.h"
#include "util.h"

#define LOOK_AHEAD 3
#define EPSILON 0.001
// 10 degrees
#define SCATTER_BUFFER M_PI/18
// get the next x after moving distance units in direction
double next_x(double x, double distance, double direction) {
    return x + distance * cos(direction);
}
// get the next y after moving distance units in direction
double next_y(double y, double distance, double direction) {
    return y + distance * sin(direction);
}

//gets index in map from a position
double get_index(struct Map map, double x, double y) {
    return (int) y * map.width + (int) x;
}

// bounds the double between min and max
double bound(double n, double min, double max) {
    return fmax(min, fmin(max, n));
}

void disperse_grid(double *grid, double *next_grid, int width, int height, double dispersion_rate) {
    // handles the center cells using a FTCS scheme.
    // finds the sum of the difference between the current and adjacent cells
    // and moves the current value by that difference scaled by the dispersion_rate
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
    for (int col = 1; col < width - 1; col++) {
        next_grid[0 * width + col] = 0;
    }
    // bottom row
    for (int col = 1; col < width - 1; col++) {
        next_grid[(height - 1) * width + col] = 0;
    }
    // left column
    for (int row = 1; row < height - 1; row++) {
        next_grid[row * width + 0] = 0;
    }
    // right column
    for (int row = 1; row < height - 1; row++) {
        next_grid[row * width + (width - 1)] = 0;
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

void deposit_trail(struct Map map, struct Agent *agent, double trail_deposit_rate, double trail_max) {
    int index = get_index(map, agent->x, agent->y);
    double updated_trail = fmin(trail_max, map.grid[index] + trail_deposit_rate);
    map.grid[index] = updated_trail;
}

// turns in the direction with the highest trail value
void turn_uptrail(struct Agent *agent, double turn_rate, double movement_speed, struct Map map) {
    double max_direction = agent->direction;
    double max_trail = -INFINITY;
    for (double i = -1; i <= 1; i += 0.5) {
        double dir = agent->direction + (i * turn_rate);
        double ahead_x = next_x(agent->x, movement_speed*LOOK_AHEAD, dir);
        double ahead_y = next_y(agent->y, movement_speed*LOOK_AHEAD, dir);
        // check if it is in bounds
        if (ahead_x < EPSILON || ahead_x > map.width - EPSILON || ahead_y < EPSILON || ahead_y > map.height - EPSILON) {
            continue;
        }
        int index = get_index(map, ahead_x, ahead_y);
        if(map.grid[index] > max_trail) {
            max_trail = map.grid[index];
            max_direction = dir;
        }
    }
    agent->direction = max_direction;
}

void add_noise_to_movement(struct Agent *agent, double movement_noise) {
    agent->direction += normal_dist(movement_noise);
}

void check_wall_collision (struct Agent *agent, double *new_x, double *new_y, struct Map map) {
    if (*new_x < EPSILON) {
        *new_x = EPSILON;
        // scatter off of left wall
        agent->direction = randd(-M_PI_2 + SCATTER_BUFFER, M_PI_2 - SCATTER_BUFFER);
    } else if (*new_x > map.width - EPSILON) {
        // a little is subtracted from width because x is rounded down and
        // map[y][width] would be out of bound
        *new_x = map.width - EPSILON;
        // scatter off of right wall
        agent->direction = randd(M_PI_2 + SCATTER_BUFFER, 3 * M_PI_2 - SCATTER_BUFFER);
    }
    // note that the directions are a little weird since y = 0 is the top wall
    if (*new_y < EPSILON) {
        *new_y = EPSILON;
        // scatter off of top wall
        agent->direction = randd(SCATTER_BUFFER, M_PI - SCATTER_BUFFER);
    } else if (*new_y > map.height - EPSILON) {
        *new_y = map.height - EPSILON;
        // scatter off of bottom wall
        agent->direction = randd(M_PI + SCATTER_BUFFER, 2 * M_PI - SCATTER_BUFFER);
    }
}

void move_and_check_wall_collision (struct Agent *agent, double movement_speed, struct Map map) {
    // move in direction
    double new_x = next_x(agent->x, movement_speed, agent->direction);
    double new_y = next_y(agent->y, movement_speed, agent->direction);
    // check for collision
    check_wall_collision(agent, &new_x, &new_y, map);
    // update position
    agent->x = new_x;
    agent->y = new_y;
}

void evaporate_trail (struct Map map, double evaporation_rate) {
    for (int i = 0; i < map.width * map.height; i++) {
        map.grid[i] *= (1 - evaporation_rate);
    }
}

void simulate_step(struct Map *p_map, struct Agent *agents, int nagents, struct Behavior behavior) {
    // Sets direction of each agent before depositing a trail
    for (int i = 0; i < nagents; i++) {
        struct Agent *agent = &agents[i];
        turn_uptrail(agent, behavior.turn_rate, behavior.movement_speed, *p_map);
        add_noise_to_movement(agent, behavior.movement_noise);
    }

    for (int i = 0; i < nagents; i++) {
        struct Agent *agent = &agents[i];
        deposit_trail(*p_map, agent, behavior.trail_deposit_rate, behavior.trail_max);
        move_and_check_wall_collision(agent, behavior.movement_speed, *p_map);
    }

    disperse_trail(p_map, behavior.dispersion_rate);
    evaporate_trail(*p_map, behavior.evaporation_rate);
}
