#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "encode_video.h"
#include "process_image.h"
#include "slimemold_simulation.h"
#include "util.h"

#define FFMPEG_LOG_LEVEL "info"
#define CODEC "libx265"
#define CODEC_PARAM "-x265-params"
#define CODEC_LOG_LEVEL "log-level=error"
// #define CRF "18"
#define CRF "28"
// #define ENCODER_PRESET "fast"
#define ENCODER_PRESET "superfast"

#define RED "\x1B[31m"
#define RESET "\x1B[0m"

#define TRAIL_MAX 1000
// the max food value is the factor times the max trail value
#define FOOD_FACTOR 2
// provides how spread out the food chemicals are
#define FOOD_SIGMA 5
#define N_FOOD 0

// how many cycles between changing the food
#define FOOD_CHANGE_PERIOD 1800

struct Coord {
    int x, y;
};

int parse_int(char *str, char *val, int min, int max) {
    int n = atoi(str);
    if(n < min || n > max) {
        fprintf(stderr, "Error: %s must be in range [%d, %d]\n", val, min, max);
        exit(1);
    }
    printf("%s=%d\n", val, n);
    return n;
}

double parse_double(char *str, char *val, double min, double max) {
    double n = strtod(str, NULL);
    if(n < min || n > max) {
        fprintf(stderr, "Error: %s must be in range [%lf, %lf]\n", val, min, max);
        exit(1);
    }
    printf("%s=%lf\n", val, n);
    return n;
}

void write_image (struct Color *image, int width, int height, int fd) {
    ssize_t written;
    // write the header
    dprintf(fd, "P6\n%d %d 255\n", width, height);
    //write the image
    written = write(fd, image, width * height * sizeof(*image));
    if (written != width * height * (long int) sizeof(*image)) {
        fprintf(stderr, "Error writing to pipe\n");
        exit(1);
    }
    // to debug, write out each frame as an image, need to pass in index i
    /*char name[256];
    sprintf(name, "frame.ppm");
    FILE *fp = fopen(name, "wb");
    if(!fp) {
        fprintf(stderr, "Could not open file %s\n", name);
        exit(1);
    }
    fprintf(fp, "P6\n%d %d 255\n", width, height);
    fwrite(image, sizeof(*image), width * height, fp);
    fclose(fp);*/
}

void prepare_and_write_image (double* trail_map, double* food_map, int width, int height, struct ColorMap colormap, int fd) {
    struct Color *prepared_image = color_image(trail_map, food_map, width, height, colormap, TRAIL_MAX, FOOD_FACTOR * TRAIL_MAX);
    write_image(prepared_image, width, height, fd);
    free(prepared_image);
}

void intialize_agents(struct Agent *agents, int nagents, int width, int height, unsigned int* seedp) {
    // give each agent a random position and direction
    for (int i = 0; i < nagents; i++) {
        agents[i].x = randd(0, width, seedp);
        agents[i].y = randd(0, height, seedp);
        agents[i].direction = randd(0, 2 * M_PI, seedp);

        //agents[i].x = 0.5 * width;
        //agents[i].y = 0.5 * height;
        //agents[i].direction = 0;

        //double rad = 0.4 * (width < height ? width : height);
        //agents[i].direction = randd(-M_PI, M_PI, seedp);
        //agents[i].x = -rad * randd(0.99, 1.01, seedp) * sin(agents[i].direction) + width/2;
        //agents[i].y = rad * randd(0.99, 1.01, seedp)* cos(agents[i].direction) + height/2;
    }
}

void initialize_foods(struct Coord *foods, int nfood, int width, int height, unsigned int* seedp) {
    for (int i = 0; i < nfood; i++) {
        foods[i].x = randint( (int) (0.1 * width), (int) (0.9 * width), seedp);
        foods[i].y = randint((int) (0.1 * height), (int) (0.9 * height), seedp);
    }
}

// randomly changes two food to another location
void change_food(struct Coord *foods, int nfood, int width, int height, unsigned int* seedp) {
    int i1 = randint(0, nfood - 1, seedp);
    int i2 = randint(0, nfood - 2, seedp);
    if (i2 >= i1) {
        i2++;
    }
    foods[i1].x = randint( (int) (0.1 * width), (int) (0.9 * width), seedp);
    foods[i1].y = randint((int) (0.1 * height), (int) (0.9 * height), seedp);

    foods[i2].x = randint( (int) (0.1 * width), (int) (0.9 * width), seedp);
    foods[i2].y = randint((int) (0.1 * height), (int) (0.9 * height), seedp);
}

void fill_food_map(struct Map food_map, struct Coord *foods, int nfood) {
    memset(food_map.grid, 0, food_map.width * food_map.height * sizeof(*food_map.grid));
    for (int i = 0; i < nfood; i++) {
        int cx = foods[i].x;
        int cy = foods[i].y;
        int start_y = fmax(cy - 4 * FOOD_SIGMA, 0);
        int end_y = fmin(cy + 4 * FOOD_SIGMA, food_map.height - 1);
        int start_x = fmax(cx - 4 * FOOD_SIGMA, 0);
        int end_x = fmin(cx + 4 * FOOD_SIGMA, food_map.width - 1);
        // y in outer loop for better cache performance
        for (int y = start_y; y <= end_y; y++) {
            for (int x = start_x; x <= end_x; x++) {
                // 2d gaussian function
                double food_value = TRAIL_MAX * FOOD_FACTOR *exp(-(pow(cx - x, 2) + pow(cy - y, 2))/(2.0 * FOOD_SIGMA * FOOD_SIGMA));
                food_map.grid[y * food_map.width + x] += food_value;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // Parse the command line arguments
    if (argc != 16) {
        fprintf(stderr, "usage: %s width height fps seconds nagents step_size "
                "trail_deposit_rate jitter_angle rotation_angle sensor_length sensor_angle dispersion_rate "
                "evaporation_rate_exp evaporation_rate_lin output_file\n", argv[0]);
        exit(1);
    }
    struct Behavior behavior;

    int width = parse_int(argv[1], "width", 3, INT_MAX);
    int height = parse_int(argv[2], "height", 3, INT_MAX);
    int fps = parse_int(argv[3], "fps", 1, INT_MAX);
    int seconds = parse_int(argv[4], "seconds", 1, INT_MAX);
    int nagents = parse_int(argv[5], "nagents", 1, INT_MAX);
    behavior.step_size = parse_double(argv[6], "step_size", 0, INFINITY);
    behavior.trail_deposit_rate = parse_double(argv[7], "trail_deposit_rate", 0, INFINITY);
    behavior.jitter_angle = parse_double(argv[8], "jitter_angle", 0, INFINITY);
    behavior.rotation_angle = parse_double(argv[9], "rotation_angle", 0, INFINITY);
    behavior.sensor_length = parse_double(argv[10], "sensor_length", 0, INFINITY);
    behavior.sensor_angle = parse_double(argv[11], "sensor_angle", 0, INFINITY);
    behavior.dispersion_rate = parse_double(argv[12], "dispersion_rate", 0, INFINITY);
    behavior.evaporation_rate_exp = parse_double(argv[13], "evaporation_rate_exp", 0, 1);
    behavior.evaporation_rate_lin = parse_double(argv[14], "evaporation_rate_lin", 0, INFINITY);

    behavior.trail_max = TRAIL_MAX;
    char *filename = argv[15];
    printf("\n");

    // create an array of seeds for the threads
    unsigned int *seeds = malloc_or_die(omp_get_max_threads() * sizeof(*seeds));
    time_t curtime = time(0);
    for (int i = 0; i < omp_get_max_threads(); i++) {
        seeds[i] = curtime ^ (i + 1);
    }

    //check for instability
    if (behavior.dispersion_rate > 0.25) {
        printf(RED "Warning:" RESET " dispersion unstable because dispersion_rate = %lf > 0.25\n", behavior.dispersion_rate);
    }

    // reads in color map
    struct ColorMap colormap = load_colormap("black-body-table-byte-1024.csv");
    if (colormap.length == -1) {
        fprintf(stderr, RED "Error:" RESET " Failed to load colormap from %s\n", "black-body-table-byte-1024.csv");
        exit(1);
    }

    // allocate space for the grid
    struct Map trail_map;
    trail_map.width = width;
    trail_map.height = height;
    trail_map.grid = malloc_or_die(trail_map.width * trail_map.height * sizeof(*(trail_map.grid)));
    // zero out memory
    memset(trail_map.grid, 0, trail_map.width * trail_map.height * sizeof(*(trail_map.grid)));

    // intialize agents
    struct Agent *agents = malloc_or_die(nagents * sizeof(*agents));
    intialize_agents(agents, nagents, trail_map.width, trail_map.height, &seeds[0]);
    // record the number of agents at each point
    int *agent_pos_freq = malloc_or_die(trail_map.width * trail_map.height* sizeof(*agent_pos_freq));

    // initialize food
    struct Coord *foods = malloc_or_die(N_FOOD * sizeof(*foods));
    struct Map food_map;
    food_map.width = trail_map.width;
    food_map.height = trail_map.height;
    food_map.grid = malloc_or_die(food_map.width * food_map.height * sizeof(*food_map.grid));
    initialize_foods(foods, N_FOOD, food_map.width, food_map.height, &seeds[0]);
    fill_food_map(food_map, foods, N_FOOD);

    //initiate FFmpeg
    int outfd;
    pid_t pid;
    if (open_pipe(fps, filename, FAST, &outfd, &pid) == -1) {
        perror("Error");
        exit(1);
    }

    // main simulation loop
    for (int i = 0; i < seconds * fps; i++) {
        //printf("----Cycle %d----\n", i);
        if (N_FOOD != 0 && i % FOOD_CHANGE_PERIOD == 0) {
            change_food(foods, N_FOOD, food_map.width, food_map.height, &seeds[0]);
            fill_food_map(food_map, foods, N_FOOD);
        }
        simulate_step(&trail_map, food_map, agents, nagents, behavior, agent_pos_freq, seeds);
        prepare_and_write_image(trail_map.grid, food_map.grid, trail_map.width, trail_map.height, colormap, outfd);
    }

    free(trail_map.grid);
    free(agents);
    destroy_colormap(colormap);
    close_pipe(outfd, pid);
}
