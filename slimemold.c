#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/wait.h>
#include <limits.h>

#include "slimemold_simulation.h"
#include "encode_video.h"
#include "util.h"

#define FFMPEG_LOG_LEVEL "info"
#define CODEC "libx265"
#define CODEC_PARAM "-x265-params"
#define CODEC_LOG_LEVEL "log-level=error"
#define CRF "20"
#define ENCODER_PRESET "fast"

#define RED "\x1B[31m"
#define RESET "\x1B[0m"

#define TRAIL_MAX 255

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
    // dispersion_rate * dt / (dx)^2
    behavior_N.dispersion_rate = behavior.dispersion_rate * factor / fps;
    // Each cell evaporates exponentially
    behavior_N.evaporation_rate = behavior.evaporation_rate / (factor * fps);
    // The trail might be concentrated in a subcell
    behavior_N.trail_max = behavior.trail_max * factor * factor;
    return behavior_N;
}

// Prepare an image grid to be written.
// Downscales the image and rounds to byte
uint8_t* prepare_image(double* map, int width, int height, int resolution_factor) {
    // size of resulting image
    int image_width = width / resolution_factor;
    int image_height = height / resolution_factor;
    // convert double array to byte array
    uint8_t *rounded_map =  malloc_or_die(image_width * image_height * sizeof(*rounded_map));
    for (int row = 0; row < image_height; row++) {
        for (int col = 0; col < image_width; col ++) {
            // average a resolution_factor x resolution_factor block
            double sum = 0;
            // corresponding position in the grid
            int grid_row = row * resolution_factor;
            int grid_col = col * resolution_factor;
            // sum the pixels in the block
            for (int row_block = 0; row_block < resolution_factor; row_block++) {
                for (int col_block = 0; col_block < resolution_factor; col_block++) {
                    sum += map[(grid_row + row_block) * width + (grid_col + col_block)];
                }
            }
            double avg = sum/(resolution_factor * resolution_factor);
            avg = fmin(avg, TRAIL_MAX);
            rounded_map[row * image_width + col] = (uint8_t) round(avg);
        }
    }

    return rounded_map;
}

// resolution_factor is the factor by which the image must be downsized
void write_image (uint8_t *image, int width, int height, int fd) {
    ssize_t written;
    // write the header
    dprintf(fd, "P5\n%d %d 255\n", width, height);
    //write the image
    written = write(fd, image, width * height * sizeof(uint8_t));
    if (written != width * height) {
        fprintf(stderr, "Error writing to pipe\n");
        exit(1);
    }
    // to debug, write out each frame as an image, need to pass in index i
    /*char name[256];
    sprintf(name, "frame%d.pgm", i);
    FILE *fp = fopen(name, "wb");
    if(!fp) {
        fprintf(stderr, "Could not open file %s\n", name);
        exit(1);
    }
    fprintf(fp, "P5\n%d %d\n255\n", width, height);
    fwrite(rounded_map, sizeof(uint8_t), width * height, fp);
    fclose(fp);*/
}

void prepare_and_write_image (double* map, int width, int height, int resolution_factor, int fd) {
    uint8_t* prepared_image = prepare_image (map, width, height, resolution_factor);
    write_image (prepared_image, width/resolution_factor, height/resolution_factor, fd);
    free(prepared_image);
}

void intialize_agents(struct Agent *agents, int nagents, int width, int height) {
    // give each agent a random position and direction
    for (int i = 0; i < nagents; i++) {
        agents[i].x = randd(0, width);
        agents[i].y = randd(0, height);
        agents[i].direction = randd(0, 2 * M_PI);
        //agents[i].x = 0.5 * width;
        //agents[i].y = 0.5 * height;
        //agents[i].direction = 0;
    }
}

int main(int argc, char *argv[]) {
    // Parse the command line arguments
    if (argc != 14) {
        fprintf(stderr, "usage: %s width height fps resolution_factor seconds nagents movement_speed "
                "trail_deposit_rate movement_noise turn_rate dispersion_rate "
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
    behavior.turn_rate = parse_double(argv[10], "turn_rate", 0, M_PI);
    behavior.dispersion_rate = parse_double(argv[11], "dispersion_rate", 0, INFINITY);
    behavior.evaporation_rate = parse_double(argv[12], "evaporation_rate", 0, 1);

    behavior.trail_max = TRAIL_MAX;
    char *filename = argv[13];
    printf("\n");

    //initiate FFmpeg
    int outfd;
    pid_t pid;
    if (open_pipe(fps, filename, FAST, &outfd, &pid) == -1) {
        perror("Error");
        exit(1);
    }
    //srand(0);
    srand(time(0));

    // normalize behavior
    struct Behavior behavior_normalized = normalize_behavior(behavior, resolution_factor, fps);

    //check for instability
    if (behavior_normalized.dispersion_rate > 0.25) {
        printf(RED "Warning:" RESET " dispersion unstable because (dispersion_rate)(dt)/(dx^2) = %lf > 0.25\n", behavior_normalized.dispersion_rate);
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
    // main simulation loop
    for (int i = 0; i < seconds * fps; i++) {
        //printf("----Cycle %d----\n", i);
        // Perform resolution_factor cycles per frame
        for (int j = 0; j < resolution_factor; j++) {
            simulate_step(&map, agents, nagents, behavior_normalized);
        }
        prepare_and_write_image(map.grid, map.width, map.height, resolution_factor, outfd);
    }

    free(map.grid);
    free(agents);
    close_pipe(outfd, pid);
}
