#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "process_image.h"
#include "util.h"

#define ARRAY_RESIZE_INCREMENT 100;

// value from 0 to 1, with 0 being invisible and 1 being maximally visible
#define FOOD_VISIBILITY 0.5

struct ColorMap load_colormap(const char *filename) {
    struct ColorMap cmap;
    int arraysize = ARRAY_RESIZE_INCREMENT;
    int r, g, b;

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
        int n = fscanf(file, "%d,%d,%d\n", &r, &g, &b);
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
        // check validity
        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
            fprintf(stderr, "Error: record %d: %d %d %d out of range\n", i + 1, r, g, b);
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
        cmap.colors[i].r = (uint8_t) r;
        cmap.colors[i].g = (uint8_t) g;
        cmap.colors[i].b = (uint8_t) b;
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

struct Color color_pixel(double trail_val, double food_val, struct ColorMap colormap, double trail_maxval, double food_maxval) {
    struct Color color;
    int trail_index = (int) trail_val * (colormap.length / trail_maxval);
    // highest value will be out of bounds
    if (trail_index == colormap.length) {
        trail_index--;
    }
    struct Color trail_color = colormap.colors[trail_index];
    // treat the food color as if it has a certain transparency alpha
    double food_alpha = FOOD_VISIBILITY * food_val / food_maxval;
    color.r = trail_color.r * (1 - food_alpha);
    color.g = trail_color.g * (1 - food_alpha) + 255 * food_alpha;
    color.b = trail_color.b * (1 - food_alpha);
    return color;
}

struct Color* color_image(double *trail_grid, double *food_grid, int width, int height, struct ColorMap colormap, double trail_maxval, double food_maxval) {
    struct Color *new_image = malloc_or_die(width * height *sizeof(*new_image));
    #pragma omp parallel for
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            double trail_val = fmax(fmin(trail_grid[row * width + col], trail_maxval), 0);
            double food_val = fmax(fmin(food_grid[row * width + col], food_maxval), 0);
            new_image[row * width + col] = color_pixel(trail_val, food_val, colormap, trail_maxval, food_maxval);
        }
    }
    return new_image;
}
