#ifndef PROCESS_IMAGE_H
#define PROCESS_IMAGE_H

#include <stdint.h>

struct Color {
    uint8_t r, g, b;
};

/**
 * Scales down an image by a factor. Width and height must be a multiple of the factor.
 *
 * The image is a row-major array of doubles. The returned image is dynamically
 * allocated
 */
double* downscale_image(double *image, int width, int height, int factor);

/**
 * Returns an image where every pixel is colored according to the corresponding value and given color_map
 *
 * Image must be in row-major order. Pixels below minval or treated as minval
 * and pixels above maxval are treated as maxval.
 */
struct Color* color_image(double *image, int width, int height, double minval, double maxval);

#endif
