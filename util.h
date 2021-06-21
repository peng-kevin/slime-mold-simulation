#ifndef UTIL_H
#define UTIL_H

/*
 * Attempts to malloc size bytes, exits on failure
 */
void* malloc_or_die(size_t size);

/*
 * gets a random float between min and max, not thread safe
 */
float randf(float min, float max);

#endif
