#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <sys/wait.h>
#include "simulation.h"

#define FFMPEG_LOG_LEVEL "info"
#define CODEC "libx265"
#define CODEC_PARAM "-x265-params"
#define CODEC_LOG_LEVEL "log-level=error"
#define CRF "20"
#define ENCODER_PRESET "fast"

int logfd;

int parse_int(char *str, char *val, int min) {
    int n = atoi(str);
    if(n < min) {
        fprintf(stderr, "Error: %s < %d\n", val, min);
        exit(1);
    }
    printf("%s=%d\n", val, n);
    return n;
}

float parse_float(char *str, char *val, float min) {
    float n = strtof(str, NULL);
    if(n < min) {
        fprintf(stderr, "Error: %s < %f\n", val, min);
        exit(1);
    }
    printf("%s=%f\n", val, n);
    return n;
}

void write_image(float *map, int width, int height, int fd) {
    // convert float array to byte array
    uint8_t *rounded_map =  malloc (width * height * sizeof(*rounded_map));
    for (int i = 0; i < width * height; i++) {
        rounded_map[i] = (uint8_t) roundf(map[i]);
    }
    // write the header
    dprintf(fd, "P5\n%d %d 255\n", width, height);
    // log the header to debug invalud max val;
    dprintf(logfd, "P5\n%d %d 255\n", width, height);
    //write the image
    ssize_t written = write(fd, rounded_map, width * height * sizeof(uint8_t));
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

    free(rounded_map);
}

void intialize_agents(struct Agent *agents, int nagents, int width, int height) {
    // give each agent a random position and direction
    for (int i = 0; i < nagents; i++) {
        agents[i].x = (((float)rand())/RAND_MAX) * width;
        agents[i].y = (((float)rand())/RAND_MAX) * height;
        agents[i].direction = (((float)rand())/RAND_MAX) * (2 * M_PI);
    }
}

int main(int argc, char *argv[]) {
    // for debugging
    logfd = open("slimemold.log", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    // Parse the command line arguments
    if (argc != 13) {
        fprintf(stderr, "usage: %s width height fps seconds nagents movement_speed "
                "trail_deposit_rate movement_noise turn_rate dispersion_rate "
                "evaporation_rate output_file\n", argv[0]);
        exit(1);
    }
    int width = parse_int(argv[1], "width", 1);
    int height = parse_int(argv[2], "height", 1);
    int fps = parse_int(argv[3], "fps", 1);
    int seconds = parse_int(argv[4], "seconds", 1);
    int nagents = parse_int(argv[5], "nagents", 1);
    float movement_speed = parse_float(argv[6], "movement_speed", 0);
    float trail_deposit_rate = parse_float(argv[7], "trail_deposit_rate", 0);
    float movement_noise = parse_float(argv[8], "movement_noise", 0);
    float turn_rate = parse_float(argv[9], "turn_rate", 0);
    float dispersion_rate = parse_float(argv[10], "dispersion_rate", 0);
    float evaporation_rate = parse_float(argv[11], "evaporation_rate", 0);
    char *filename = argv[12];
    printf("\n");

    // create the pipe
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Failed to create pipe");
        exit(1);
    }
    //initiate ffmpeg
    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork failed");
        exit(1);
    } else if(pid == 0) {
        // convert arguments to strings
        char fpsbuf[256];
        sprintf(fpsbuf, "%d", fps);

        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        execlp("ffmpeg", "ffmpeg", "-hide_banner", "-loglevel", FFMPEG_LOG_LEVEL, "-f", "image2pipe", "-r", fpsbuf, "-i", "pipe:", "-c:v", CODEC, CODEC_PARAM, CODEC_LOG_LEVEL, "-crf", CRF, "-preset", ENCODER_PRESET, filename, (char *) NULL);
        perror("Failed to execute ffmpeg");
        exit(1);
    }
    close(pipefd[0]);
    int outfd = pipefd[1];
    srand(time(0));

    // allocate space for the grid, one for current, one for next
    float *map[2];
    map[0] = calloc(width * height, sizeof(*(map[0])));
    map[1] = calloc(width * height, sizeof(*(map[1])));

    // intialize agents
    struct Agent *agents = malloc(nagents * sizeof(*agents));
    intialize_agents(agents, nagents, width, height);
    for (int i = 0; i < seconds * fps; i++) {
        //printf("----Cycle %d----\n", i);
        //switch which map is the current and which is next each step
        int cur = i % 2;
        // normalize the parameters by fps
        simulate_step(map[cur], map[1 - cur], agents, width, height, nagents,
                        movement_speed/fps, trail_deposit_rate/fps,
                        movement_noise/fps, turn_rate/fps, dispersion_rate/fps,
                        evaporation_rate/fps);
        write_image(map[1 - cur], width, height, outfd);
    }

    free(map[0]);
    free(map[1]);
    free(agents);
    close(outfd);
    close(logfd);

    // wait for ffmpeg to finish
    waitpid(pid, NULL, 0);
}
