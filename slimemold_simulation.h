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
    double step_size;
    double trail_deposit_rate;
    double jitter_angle;
    double rotation_angle;
    double sensor_length;
    double sensor_angle;
    double dispersion_rate;
    double evaporation_rate_exp;
    double evaporation_rate_lin;
    double trail_max;
};

void simulate_step(struct Map *p_trail_map, struct Agent *agents, int nagents, struct Behavior behavior, int *agent_pos_freq, unsigned int *seeds);
#endif
