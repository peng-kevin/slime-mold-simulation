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

void deposit_trail(struct Map map, struct Agent *agent, double trail_deposit_rate, double trail_max) {
    int index = (int) agent->y * map.width + (int) agent->x;
    double updated_trail = fmin(trail_max, map.grid[index] + trail_deposit_rate);
    map.grid[index] = updated_trail;
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
    // iterates through each agent
    for (int i = 0; i < nagents; i++) {
        struct Agent *agent = &agents[i];
        deposit_trail(map, agent, behavior.trail_deposit_rate, behavior.trail_max);
        move_and_check_wall_collision(agent, behavior.movement_speed, map);
    }

    evaporate_trail(map, behavior.evaporation_rate);
}
