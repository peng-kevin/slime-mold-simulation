#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

/**
 * Attempts to malloc size bytes, exits on failure
 */
void* malloc_or_die(size_t size);

/**
 * Generates a random int between min and max inclusive.
 *
 * Thread safe with different seeds
 */
int randint(int min, int max, unsigned int *seedp);

/**
 * Gets a random double between min and max, thread safe with different seeds
 */
double randd(double min, double max, unsigned int *seedp);

/**
 * Generic function to shuffle an array with nitems each size bytes wide
 */
void shuffle(void *base, size_t nitems, size_t size, unsigned int *seedp);

#endif
