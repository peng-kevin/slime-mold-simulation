#ifndef UTIL_H
#define UTIL_H

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
 * gets a random double between min and max, thread safe with different seeds
 */
double randd(double min, double max, unsigned int *seedp);

#endif
