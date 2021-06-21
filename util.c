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

// gets a random float between min and max, not thread safe
float randf(float min, float max) {
    return min + (((float)rand())/RAND_MAX) * (max - min);
}
