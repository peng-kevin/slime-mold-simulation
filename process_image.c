#include <math.h>
#include <string.h>
#include <stdio.h>
#include "process_image.h"
#include "util.h"

double* downscale_image(double *image, int width, int height, int factor) {
    // size of the resulting image
    int image_width = width / factor;
    int image_height = height / factor;
    double *new_image = malloc_or_die(image_width * image_height * sizeof(*new_image));
    memset(new_image, 0, image_width * image_height * sizeof(*new_image));

    double mult_factor = 1.0/(factor * factor);
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int image_row = row / factor;
            int image_col = col / factor;
            new_image[image_row * image_width + image_col] += mult_factor * image[row * width + col];
        }
    }
    return new_image;
}

struct Color color_pixel(double val, double minval, double maxval) {
    struct Color color;
    int index = (int) (val - minval) * (255 / (maxval - minval));
    // highest value will be out of bounds
    if (index == 255) {
        index--;
    }
    color.r = index;
    color.g = index;
    color.b = index;
    return color;
}

struct Color* color_image(double *image, int width, int height, double minval, double maxval) {
    struct Color *new_image = malloc_or_die(width * height *sizeof(*new_image));
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            double val = fmax(fmin(image[row * width + col], maxval), minval);
            new_image[row * width + col] = color_pixel(val, minval, maxval);
        }
    }
    return new_image;
}
