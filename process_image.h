#ifndef PROCESS_IMAGE_H
#define PROCESS_IMAGE_H

#include <stdint.h>
/*
 * Stores an rgb value as a triplet of bytes
 */
struct Color {
    uint8_t r, g, b;
};

/*
 * Stores a color map for converting from a scalar to a color.
 * It is stored as an array where each color is equally spaced. THe length is
 * the number of color values.
 */
struct ColorMap {
    struct Color *colors;
    int length;
};

/**
 * Reads in a csv file containing the ColorMap. The file must contain three
 * columns labeled "RGB_r,RGB_g,RGB_b" in that order. Each row defines a color
 * as a triplet of numbers from 0-255 progressing from the lowest to highest
 * color. The colors are assumed to be equally spaced. There can be any number
 * of colors included.
 *
 * In the event of failure, may print an error and returns a colormap where
 * colormap.colors is not allocated and length is set to -1.
 */
struct ColorMap load_colormap(const char *filename);

/**
 * Frees dynamically allocated memory
 */
void destroy_colormap(struct ColorMap colormap);

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
struct Color* color_image(double *image, int width, int height, struct ColorMap colormap, double minval, double maxval);

#endif
