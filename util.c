#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "util.h"

void* malloc_or_die(size_t size) {
    void *ptr = malloc(size);
    if(ptr == NULL) {
        perror("malloc failed");
        exit(1);
    }
    return ptr;
}

double randd(double min, double max) {
    return min + (((double)rand())/RAND_MAX) * (max - min);
}

// Uses Box-Muller method
double normal_dist(double sigma) {
    double u = randd(0, 1);
    double v = randd(0, 1);
    return sigma * sqrt(-2 * log(u)) * cos(2 * M_PI *  v);
}
