#ifndef SIMULATION_H
#define SIMULATION_H

struct Agent {
    float direction;
    float x;
    float y;
};

struct Map {
    float *grid;
    int width;
    int height;
};

void simulate_step(struct Map map, struct Agent *agents,
                    int nagents, float movement_speed, float trail_deposit_rate,
                    float movement_noise, float turn_rate, float dispersion_rate,
                    float evaporation_rate);
#endif
