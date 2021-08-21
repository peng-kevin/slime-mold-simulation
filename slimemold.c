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
#define CRF "20"
#define ENCODER_PRESET "fast"

#define RED "\x1B[31m"
#define RESET "\x1B[0m"

#define TRAIL_MAX 1000

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

// Changes the parameters so they work with the assumption that dx = 1 dt = 1
struct Behavior normalize_behavior(struct Behavior behavior, int factor, int fps) {
    struct Behavior behavior_N;
    // Cell distance and steps per second scale in opposite directions
    behavior_N.movement_speed = behavior.movement_speed / fps;
    // because each square is smaller, this is factor^2 / (factor * fps)
    behavior_N.trail_deposit_rate = behavior.trail_deposit_rate * factor / fps;
    /*
     * using the Central Limit Theorem, we find that the distribution after
     * summing n random numbers has a standard deviation of sqrt(n)*sigma, so
     * we need to divide sigma by sqrt(n) to keep the distribution the same
     */
    behavior_N.movement_noise = behavior.movement_noise / sqrt(factor * fps);
    // Same time to turn same amount
    behavior_N.turn_rate = behavior.turn_rate / (factor * fps);
    // no change, just need to ensure that sensor stays ahead of own trail
    behavior_N.sensor_length = behavior.sensor_length;
    // no change, this is a factor for the turn rate
    behavior_N.sensor_angle_factor = behavior.sensor_angle_factor;
    // dispersion_rate * dt / (dx)^2
    behavior_N.dispersion_rate = behavior.dispersion_rate * factor / fps;
    // Each cell evaporates exponentially
    behavior_N.evaporation_rate_exp = behavior.evaporation_rate_exp / (factor * fps);
    // Same rate per cell at all factor levels
    behavior_N.evaporation_rate_lin = behavior.evaporation_rate_lin / (factor * fps);
    // The trail might be concentrated in a subcell
    behavior_N.trail_max = behavior.trail_max * factor * factor;
    return behavior_N;
}

// resolution_factor is the factor by which the image must be downsized
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

void prepare_and_write_image (double* map, int width, int height, int resolution_factor, struct ColorMap colormap, int fd) {
    if (resolution_factor == 1) {
        struct Color *prepared_image = color_image(map, width, height, colormap, 0, TRAIL_MAX);
        write_image(prepared_image, width, height, fd);
        free(prepared_image);
    } else {
        double *downscaled_image = downscale_image(map, width, height, resolution_factor);
        struct Color *prepared_image = color_image(downscaled_image, width/resolution_factor, height/resolution_factor, colormap, 0, TRAIL_MAX);
        write_image(prepared_image, width/resolution_factor, height/resolution_factor, fd);
        free(downscaled_image);
        free(prepared_image);
    }
}

void intialize_agents(struct Agent *agents, int nagents, int width, int height) {
    // give each agent a random position and direction
    unsigned int seed = time(0);
    for (int i = 0; i < nagents; i++) {
        //agents[i].x = randd(0, width, &seed);
        //agents[i].y = randd(0, height, &seed);
        //agents[i].direction = randd(0, 2 * M_PI, &seed);

        //agents[i].x = 0.5 * width;
        //agents[i].y = 0.5 * height;
        //agents[i].direction = 0;

        double rad = 0.4 * (width < height ? width : height);
        agents[i].direction = randd(-M_PI, M_PI, &seed);
        agents[i].x = -rad * randd(0.95, 1.05, &seed) * cos(agents[i].direction) + width/2;
        agents[i].y = -rad * randd(0.95, 1.05, &seed)* sin(agents[i].direction) + height/2;
    }
}

int main(int argc, char *argv[]) {
    // Parse the command line arguments
    if (argc != 17) {
        fprintf(stderr, "usage: %s width height fps resolution_factor seconds nagents movement_speed "
                "trail_deposit_rate movement_noise turn_rate sensor_length sensor_angle_factor dispersion_rate "
                "evaporation_rate output_file\n", argv[0]);
        exit(1);
    }
    struct Behavior behavior;

    int width = parse_int(argv[1], "width", 3, INT_MAX);
    int height = parse_int(argv[2], "height", 3, INT_MAX);
    int fps = parse_int(argv[3], "fps", 1, INT_MAX);
    int resolution_factor = parse_int(argv[4], "resolution_factor", 1, INT_MAX);
    int seconds = parse_int(argv[5], "seconds", 1, INT_MAX);
    int nagents = parse_int(argv[6], "nagents", 1, INT_MAX);
    behavior.movement_speed = parse_double(argv[7], "movement_speed", 0, INFINITY);
    behavior.trail_deposit_rate = parse_double(argv[8], "trail_deposit_rate", 0, INFINITY);
    behavior.movement_noise = parse_double(argv[9], "movement_noise", 0, INFINITY);
    behavior.turn_rate = parse_double(argv[10], "turn_rate", 0, INFINITY);
    behavior.sensor_length = parse_double(argv[11], "sensor_length", 0, INFINITY);
    behavior.sensor_angle_factor = parse_double(argv[12], "sensor_angle_factor", 0, INFINITY);
    behavior.dispersion_rate = parse_double(argv[13], "dispersion_rate", 0, INFINITY);
    behavior.evaporation_rate_exp = parse_double(argv[14], "evaporation_rate_exp", 0, 1);
    behavior.evaporation_rate_lin = parse_double(argv[15], "evaporation_rate_lin", 0, INFINITY);

    behavior.trail_max = TRAIL_MAX;
    char *filename = argv[16];
    printf("\n");

    // create an array of seeds for the threads
    unsigned int *seeds = malloc_or_die(omp_get_max_threads() * sizeof(*seeds));
    time_t curtime = time(0);
    for (int i = 0; i < omp_get_max_threads(); i++) {
        seeds[i] = curtime ^ (i + 1);
    }

    // normalize behavior
    struct Behavior behavior_normalized = normalize_behavior(behavior, resolution_factor, fps);

    //check for instability
    if (behavior_normalized.dispersion_rate > 0.25) {
        printf(RED "Warning:" RESET " dispersion unstable because (dispersion_rate)(dt)/(dx^2) = %lf > 0.25\n", behavior_normalized.dispersion_rate);
    }

    // reads in color map
    struct ColorMap colormap = load_colormap("black-body-table-byte-1024.csv");
    if (colormap.length == -1) {
        fprintf(stderr, RED "Error:" RESET " Failed to load colormap from %s\n", "black-body-table-byte-1024.csv");
        exit(1);
    }

    // allocate space for the grid
    // width and height scaled by resolution_factor
    struct Map map;
    map.width = width * resolution_factor;
    map.height = height * resolution_factor;
    map.grid = malloc_or_die(map.width * map.height * sizeof(*(map.grid)));
    // zero out memory
    memset(map.grid, 0, map.width * map.height * sizeof(*(map.grid)));

    // intialize agents
    struct Agent *agents = malloc_or_die(nagents * sizeof(*agents));
    intialize_agents(agents, nagents, map.width, map.height);
    // record the number of agents at each point
    int *agent_pos_freq = malloc_or_die(map.width * map.height* sizeof(*agent_pos_freq));

    //initiate FFmpeg
    int outfd;
    pid_t pid;
    if (open_pipe(fps, filename, FAST, &outfd, &pid) == -1) {
        perror("Error");
        exit(1);
    }

    // main simulation loop
    for (int i = 0; i < seconds * fps; i++) {
        // Perform resolution_factor cycles per frame
        for (int j = 0; j < resolution_factor; j++) {
            //printf("----Cycle %d----\n", i*resolution_factor + j);
            simulate_step(&map, agents, nagents, behavior_normalized, agent_pos_freq, seeds);
        }
        prepare_and_write_image(map.grid, map.width, map.height, resolution_factor, colormap, outfd);
    }

    free(map.grid);
    free(agents);
    destroy_colormap(colormap);
    close_pipe(outfd, pid);
}
