#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "slimemold_simulation.h"
#include "util.h"

#define LOOK_AHEAD 1.5
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

// bounds the double between min and max
double bound(double n, double min, double max) {
    return fmax(min, fmin(max, n));
}

// uses a heat equation with boundary conditions deriviative = 0
// warning: changes the map.grid pointer
void disperse_trail(struct Map map, double dispersion_rate) {
    // allocates a new map
    double *next_map = malloc_or_die(map.width * map.height * sizeof(*next_map));
    // handles the center cells using a FTCS scheme.
    // finds the sum of the difference between the current and adjacent cells
    // and moves the current value by that difference scaled by the dispersion_rate
    for (int row = 1; row < map.height - 1; row++) {
        for (int col = 1; col < map.width - 1; col++) {
            int index = row * map.width + col;
            // sum the four adjacent cell
            next_map[index] = map.grid[row * map.width + (col - 1)];
            next_map[index] += map.grid[row * map.width + (col + 1)];
            next_map[index] += map.grid[(row - 1) * map.width + col];
            next_map[index] += map.grid[(row + 1) * map.width + col];
            // multiply the current sum by the dispersion_rate
            next_map[index] *= dispersion_rate;
            // add (1 - 4 * dispersion_rate) * center cell
            next_map[index] += (1 - 4 * dispersion_rate) * map.grid[row * map.width + col];
        }
    }
    // handles boundary condition
    // top row
    for (int col = 1; col < map.width - 1; col++) {
        int index = 0 * map.width + col;
        // sum adjacent columns
        next_map[index] = map.grid[0 * map.width + (col - 1)];
        next_map[index] += map.grid[0 * map.width + (col + 1)];
        // mirror lower column
        next_map[index] += 2*map.grid[1 * map.width + col];
        // multiply the current sum by the dispersion_rate
        next_map[index] *= dispersion_rate;
        // add (1 - 4 * dispersion_rate) * center cell
        next_map[index] += (1 - 4 * dispersion_rate) * map.grid[0 * map.width + col];
    }
    // bottom row
    for (int col = 1; col < map.width - 1; col++) {
        int index = (map.height - 1) * map.width + col;
        // sum adjacent columns
        next_map[index] = map.grid[(map.height - 1) * map.width + (col - 1)];
        next_map[index] += map.grid[(map.height - 1) * map.width + (col + 1)];
        // mirror above column
        next_map[index] += 2*map.grid[(map.height - 2) * map.width + col];
        // multiply the current sum by the dispersion_rate
        next_map[index] *= dispersion_rate;
        // add (1 - 4 * dispersion_rate) * center cell
        next_map[index] += (1 - 4 * dispersion_rate) * map.grid[(map.height - 1) * map.width + col];
    }
    // left column
    for (int row = 1; row < map.height - 1; row++) {
        int index = row * map.width + 0;
        // sum adjacent rows
        next_map[index] = map.grid[(row - 1) * map.width + 0];
        next_map[index] += map.grid[(row + 1) * map.width + 0];
        // mirror right column
        next_map[index] += 2*map.grid[row * map.width + 1];
        // multiply the current sum by the dispersion_rate
        next_map[index] *= dispersion_rate;
        // add (1 - 4 * dispersion_rate) * center cell
        next_map[index] += (1 - 4 * dispersion_rate) * map.grid[row * map.width + 0];
    }
    // right column
    for (int row = 1; row < map.height - 1; row++) {
        int index = row * map.width + (map.width - 1);
        // sum adjacent rows
        next_map[index] = map.grid[(row - 1) * map.width + (map.width - 1)];
        next_map[index] += map.grid[(row + 1) * map.width + (map.width - 1)];
        // mirror left column
        next_map[index] += 2*map.grid[row * map.width + (map.width - 2)];
        // multiply the current sum by the dispersion_rate
        next_map[index] *= dispersion_rate;
        // add (1 - 4 * dispersion_rate) * center cell
        next_map[index] += (1 - 4 * dispersion_rate) * map.grid[row * map.width + (map.width - 1)];
    }
    // top left corner
    next_map[0 * map.width + 0] = 2*map.grid[0 * map.width + 1];
    next_map[0 * map.width + 0] += 2*map.grid[1 * map.width + 0];
    next_map[0 * map.width + 0] *= dispersion_rate;
    next_map[0 * map.width + 0] += (1 - 4 * dispersion_rate) * map.grid[0 * map.width + 0];
    // top right corner
    next_map[0 * map.width + (map.width - 1)] = 2*map.grid[0 * map.width + (map.width - 2)];
    next_map[0 * map.width + (map.width - 1)] += 2*map.grid[1 * map.width + (map.width - 1)];
    next_map[0 * map.width + (map.width - 1)] *= dispersion_rate;
    next_map[0 * map.width + (map.width - 1)] += (1 - 4 * dispersion_rate) * map.grid[0 * map.width + (map.width - 1)];
    // bottom left corner
    next_map[(map.height - 1) * map.width + 0] = 2*map.grid[(map.height - 1)  * map.width + 1];
    next_map[(map.height - 1)  * map.width + 0] += 2*map.grid[(map.height - 2)  * map.width + 0];
    next_map[(map.height - 1)  * map.width + 0] *= dispersion_rate;
    next_map[(map.height - 1)  * map.width + 0] += (1 - 4 * dispersion_rate) * map.grid[(map.height - 1)  * map.width + 0];
    // bottom right corner
    next_map[(map.height - 1)  * map.width + (map.width - 1)] = 2*map.grid[(map.height - 1)  * map.width + (map.width - 2)];
    next_map[(map.height - 1)  * map.width + (map.width - 1)] += 2*map.grid[(map.height - 2)  * map.width + (map.width - 1)];
    next_map[(map.height - 1)  * map.width + (map.width - 1)] *= dispersion_rate;
    next_map[(map.height - 1)  * map.width + (map.width - 1)] += (1 - 4 * dispersion_rate) * map.grid[(map.height - 1)  * map.width + (map.width - 1)];

    // problem with switching pointers is that it wouldn't propagate back without turning the map into a pointer all the way back
    memcpy(map.grid, next_map, map.width * map.height * sizeof(*next_map));
    free(next_map);
}

void deposit_trail(struct Map map, struct Agent *agent, double trail_deposit_rate, double trail_max) {
    int index = (int) agent->y * map.width + (int) agent->x;
    double updated_trail = fmin(trail_max, map.grid[index] + trail_deposit_rate);
    map.grid[index] = updated_trail;
}

void add_noise_to_movement(struct Agent *agent, double movement_noise) {
    agent->direction += normal_dist(movement_noise);
}

void check_wall_collision (struct Agent *agent, double *new_x, double *new_y, struct Map map) {
    if (*new_x < 0) {
        *new_x = 0;
        // scatter off of left wall
        agent->direction = randd(-M_PI_2 + SCATTER_BUFFER, M_PI_2 - SCATTER_BUFFER);
    } else if (*new_x >= map.width) {
        // a little is subtracted from width because x is rounded down and
        // map[y][width] would be out of bound
        *new_x = map.width - EPSILON;
        // scatter off of right wall
        agent->direction = randd(M_PI_2 + SCATTER_BUFFER, 3 * M_PI_2 - SCATTER_BUFFER);
    }
    // note that the directions are a little weird since y = 0 is the top wall
    if (*new_y < 0) {
        *new_y = 0;
        // scatter off of top wall
        agent->direction = randd(SCATTER_BUFFER, M_PI - SCATTER_BUFFER);
    } else if (*new_y >= map.height) {
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

void simulate_step(struct Map map, struct Agent *agents, int nagents, struct Behavior behavior) {
    // TODO to be used
    (void) behavior.movement_noise;
    (void) behavior.turn_rate;
    (void) behavior.dispersion_rate;
    // Sets direction of each agent before depositing a trail
    for (int i = 0; i < nagents; i++) {
        struct Agent *agent = &agents[i];
        add_noise_to_movement(agent, behavior.movement_noise);
    }

    for (int i = 0; i < nagents; i++) {
        struct Agent *agent = &agents[i];
        deposit_trail(map, agent, behavior.trail_deposit_rate, behavior.trail_max);
        move_and_check_wall_collision(agent, behavior.movement_speed, map);
    }

    disperse_trail(map, behavior.dispersion_rate);
    evaporate_trail(map, behavior.evaporation_rate);
}
