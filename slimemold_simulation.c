#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simulation.h"

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

// gets a random float between min and max, not thread safe
float randf(float min, float max) {
    return min + (((float)rand())/RAND_MAX) * (max - min);
}

void deposit_trail(float *next_map, struct Agent *agent, float trail_deposit_rate, int width) {
    float updated_trail = fmin(MAX_TRAIL, next_map[(int) agent->y * width + (int) agent->x] + trail_deposit_rate);
    next_map[(int) agent->y * width + (int) agent->x] = updated_trail;
}

void check_wall_collision (struct Agent *agent, float *new_x, float *new_y, int width, int height) {
    if (*new_x < 0) {
        *new_x = 0;
        // scatter off of left wall
        agent->direction = randf(-M_PI_2 + SCATTER_BUFFER, M_PI_2 - SCATTER_BUFFER);
    } else if (*new_x >= width) {
        // a little is subtracted from width because x is rounded down and
        // map[y][width] would be out of bound
        *new_x = width - EPSILON;
        // scatter off of right wall
        agent->direction = randf(M_PI_2 + SCATTER_BUFFER, 3 * M_PI_2 - SCATTER_BUFFER);
    }
    // note that the directions are a little weird since y = 0 is the top wall
    if (*new_y < 0) {
        *new_y = 0;
        // scatter off of top wall
        agent->direction = randf(SCATTER_BUFFER, M_PI - SCATTER_BUFFER);
    } else if (*new_y >= height) {
        *new_y = height - EPSILON;
        // scatter off of bottom wall
        agent->direction = randf(M_PI + SCATTER_BUFFER, 2 * M_PI - SCATTER_BUFFER);
    }
}

void move_and_check_wall_collision (struct Agent *agent, float movement_speed, int width, int height) {
    // move in direction
    float new_x = next_x(agent->x, movement_speed, agent->direction);
    float new_y = next_y(agent->y, movement_speed, agent->direction);
    // check for collision
    check_wall_collision(agent, &new_x, &new_y, width, height);
    // update position
    agent->x = new_x;
    agent->y = new_y;
}

void evaporate_trail (float * next_map, float evaporation_rate, int width, int height) {
    for (int i = 0; i < width * height; i++) {
        next_map[i] = fmax(0, next_map[i] - evaporation_rate);
    }
}

void simulate_step(float *map, float *next_map, struct Agent *agents, int width,
                    int height, int nagents, float movement_speed,
                    float trail_deposit_rate, float movement_noise,
                    float turn_rate, float dispersion_rate, float evaporation_rate) {
    //suppress unused warning
    // TODO remove later
    (void) movement_noise;
    (void) turn_rate;
    (void) dispersion_rate;
    (void) evaporation_rate;
    // copy current map into next map
    memcpy(next_map, map, width * height * sizeof(*map));
    // iterates through each agent
    for (int i = 0; i < nagents; i++) {
        struct Agent *agent = &agents[i];
        deposit_trail(next_map, agent, trail_deposit_rate, width);

        // turn to follow trail
        // follows the highest intesity in the three directions
        /*float next_direction;
        float max_intensity = -1;
        for (int i = -1; i <= 1; i++) {
            float dir = agents->direction + turn_rate * i;
            int x = (int) bound(next_x(agent->x, LOOK_AHEAD, dir), 0, width);
            int y = (int) bound(next_y(agent->y, LOOK_AHEAD, dir), 0, height);
            float intensity = map[y * width + x];
            if (intensity > max_intensity) {
                max_intensity = intensity;
                next_direction = dir;
            }
        }*/

        move_and_check_wall_collision(agent, movement_speed, width, height);
    }

    evaporate_trail(next_map, evaporation_rate, width, height);
}
