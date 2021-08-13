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

double randd(double min, double max, unsigned int *seedp) {
    return min + (((double)rand_r(seedp))/RAND_MAX) * (max - min);
}
