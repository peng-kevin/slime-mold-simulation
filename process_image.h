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
 * Returns an image where every pixel is colored according to the corresponding value and given color_map
 *
 * Image must be in row-major order. The minval is assumed to be 0. Pixels
 * below 0 are treated as 0 and pixels above maxval are treated as maxval.
 */
struct Color* color_image(double *trail_grid, double *food_grid, int width, int height, struct ColorMap colormap, double trail_maxval, double food_maxval);

#endif
