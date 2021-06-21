#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simulation.h"
#include "util.h"

#define LOOK_AHEAD 1.5
#define EPSILON 0.001
#define MAX_TRAIL 255
// 10 degrees
#define SCATTER_BUFFER M_PI/18
// get the next x after moving distance units in direction
float next_x(float x, float distance, float direction) {
    return x + distance * cos(direction);
}
// get the next y after moving distance units in direction
float next_y(float y, float distance, float direction) {
    return y + distance * sin(direction);
}

// bounds the float between min and max
float bound(float n, float min, float max) {
    return fmaxf(min, fminf(max, n));
}

void deposit_trail(struct Map map, struct Agent *agent, float trail_deposit_rate) {
    int index = (int) agent->y * map.width + (int) agent->x;
    float updated_trail = fmin(MAX_TRAIL, map.grid[index] + trail_deposit_rate);
    map.grid[index] = updated_trail;
}

void check_wall_collision (struct Agent *agent, float *new_x, float *new_y, struct Map map) {
    if (*new_x < 0) {
        *new_x = 0;
        // scatter off of left wall
        agent->direction = randf(-M_PI_2 + SCATTER_BUFFER, M_PI_2 - SCATTER_BUFFER);
    } else if (*new_x >= map.width) {
        // a little is subtracted from width because x is rounded down and
        // map[y][width] would be out of bound
        *new_x = map.width - EPSILON;
        // scatter off of right wall
        agent->direction = randf(M_PI_2 + SCATTER_BUFFER, 3 * M_PI_2 - SCATTER_BUFFER);
    }
    // note that the directions are a little weird since y = 0 is the top wall
    if (*new_y < 0) {
        *new_y = 0;
        // scatter off of top wall
        agent->direction = randf(SCATTER_BUFFER, M_PI - SCATTER_BUFFER);
    } else if (*new_y >= map.height) {
        *new_y = map.height - EPSILON;
        // scatter off of bottom wall
        agent->direction = randf(M_PI + SCATTER_BUFFER, 2 * M_PI - SCATTER_BUFFER);
    }
}

void move_and_check_wall_collision (struct Agent *agent, float movement_speed, struct Map map) {
    // move in direction
    float new_x = next_x(agent->x, movement_speed, agent->direction);
    float new_y = next_y(agent->y, movement_speed, agent->direction);
    // check for collision
    check_wall_collision(agent, &new_x, &new_y, map);
    // update position
    agent->x = new_x;
    agent->y = new_y;
}

void evaporate_trail (struct Map map, float evaporation_rate) {
    for (int i = 0; i < map.width * map.height; i++) {
        map.grid[i] = fmax(0, map.grid[i] - evaporation_rate);
    }
}

void simulate_step(struct Map map, struct Agent *agents, int nagents, float movement_speed,
                    float trail_deposit_rate, float movement_noise,
                    float turn_rate, float dispersion_rate, float evaporation_rate) {
    //suppress unused warning
    // TODO remove later
    (void) movement_noise;
    (void) turn_rate;
    (void) dispersion_rate;
    (void) evaporation_rate;
    // iterates through each agent
    for (int i = 0; i < nagents; i++) {
        struct Agent *agent = &agents[i];
        deposit_trail(map, agent, trail_deposit_rate);
        move_and_check_wall_collision(agent, movement_speed, map);
    }

    evaporate_trail(map, evaporation_rate);
}
