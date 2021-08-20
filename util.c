#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void* malloc_or_die(size_t size) {
    void *ptr = malloc(size);
    if(ptr == NULL) {
        perror("malloc failed");
        exit(1);
    }
    return ptr;
}

int randint(int min, int max, unsigned int *seedp) {
    return min + (rand_r(seedp) % (max - min + 1));
}

double randd(double min, double max, unsigned int *seedp) {
    return min + (((double)rand_r(seedp))/RAND_MAX) * (max - min);
}

void shuffle(void *base, size_t nitems, size_t size, unsigned int *seedp) {
    char *base_c = (char*) base;
    void *tmp = malloc_or_die(size);
    for (size_t i = 0; i < nitems - 1; i++) {
        size_t j = randint(i, nitems - 1, seedp);
        memcpy(tmp, base_c + (i * size), size);
        memcpy(base_c + (i * size), base_c + (j * size), size);
        memcpy(base_c + (j * size), tmp, size);
    }
    free(tmp);
}
