#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "process_image.h"
#include "util.h"

#define ARRAY_RESIZE_INCREMENT 100;

struct ColorMap load_colormap(const char *filename) {
    struct ColorMap cmap;
    int arraysize = ARRAY_RESIZE_INCREMENT;
    uint8_t r, g, b;

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error");
        cmap.length = -1;
        return cmap;
    }
    // check header
    char buf[64];
    if (fgets(buf, 64, file) == NULL) {
        fprintf(stderr, "Error: Failed to read header in colormap\n");
        cmap.length = -1;
        return cmap;
    }
    if (strcmp(buf, "RGB_r,RGB_g,RGB_b\n") != 0) {
        fprintf(stderr, "Error: Wrong header in colormap\n");
        printf("%s\n", buf);
        cmap.length = -1;
        return cmap;
    }
    // begin reading
    cmap.colors = malloc_or_die(arraysize * sizeof(*cmap.colors));
    int i = 0;
    while(1) {
        int n = fscanf(file, "%hhu,%hhu,%hhu\n", &r, &g, &b);
        // check for end of file or invalid format
        if (n == EOF && !ferror(file)) {
            break;
        } else if(n != 3) {
            // The format must be invalid
            fprintf(stderr, "Error: colormap file format invalid\n");
            free(cmap.colors);
            cmap.length = -1;
            return cmap;
        }
        // resize array if it is full
        if (i == arraysize) {
            arraysize += ARRAY_RESIZE_INCREMENT;
            cmap.colors = realloc(cmap.colors, arraysize * sizeof(*cmap.colors));
            if (cmap.colors == NULL) {
                perror("Error");
                free(cmap.colors);
                cmap.length = -1;
                return cmap;
            }
        }
        cmap.colors[i].r = r;
        cmap.colors[i].g = g;
        cmap.colors[i].b = b;
        i++;
    }
    // shrink the array if necessary
    if (i != arraysize) {
        // in case realloc fails
        struct Color *newptr = realloc(cmap.colors, i * sizeof(*newptr));
        if(newptr != NULL) {
            cmap.colors = newptr;
        }
    }
    cmap.length = i;
    return cmap;
}

void destroy_colormap(struct ColorMap colormap) {
    free(colormap.colors);
}

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

struct Color color_pixel(double val, struct ColorMap colormap, double minval, double maxval) {
    int index = (int) (val - minval) * (colormap.length / (maxval - minval));
    // highest value will be out of bounds
    if (index == colormap.length) {
        index--;
    }
    return colormap.colors[index];
}

struct Color* color_image(double *image, int width, int height, struct ColorMap colormap, double minval, double maxval) {
    struct Color *new_image = malloc_or_die(width * height *sizeof(*new_image));
    #pragma omp parallel for
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            double val = fmax(fmin(image[row * width + col], maxval), minval);
            new_image[row * width + col] = color_pixel(val, colormap, minval, maxval);
        }
    }
    return new_image;
}
