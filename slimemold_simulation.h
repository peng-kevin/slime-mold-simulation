#ifndef SLIMEMOLD_SIMULATION_H
#define SLIMEMOLD_SIMULATION_H

struct Agent {
    double direction;
    double x;
    double y;
};

struct Map {
    double *grid;
    int width;
    int height;
};

// Parameters that control the simulation
struct Behavior {
    double movement_speed;
    double trail_deposit_rate;
    double movement_noise;
    double turn_rate;
    double dispersion_rate;
    double evaporation_rate;
    double trail_max;
};

void simulate_step(struct Map *p_map, struct Agent *agents, int nagents, struct Behavior behavior, unsigned int *seeds);
#endif
