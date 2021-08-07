#include <stdio.h>
#include <stdlib.h>
#include "util.h"

void* malloc_or_die(size_t size) {
    void *ptr = malloc(size);
    if(ptr == NULL) {
        perror("malloc failed");
        exit(1);
    }
    return ptr;
}

// gets a random double between min and max, not thread safe
double randd(double min, double max) {
    return min + (((double)rand())/RAND_MAX) * (max - min);
}
