#ifndef SIMULATION_H
#define SIMULATION_H

struct Agent {
    float direction;
    float x;
    float y;
};

void simulate_step(float *map, float *next_map, struct Agent *agents, int width,
                    int height, int nagents, float movement_speed,
                    float trail_deposit_rate, float movement_noise,
                    float turn_rate, float dispersion_rate, float evaporation_rate);
#endif
