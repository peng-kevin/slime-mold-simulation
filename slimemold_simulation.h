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

void simulate_step(struct Map map, struct Agent *agents,
                    int nagents, double movement_speed, double trail_deposit_rate,
                    double movement_noise, double turn_rate, double dispersion_rate,
                    double evaporation_rate);
#endif
