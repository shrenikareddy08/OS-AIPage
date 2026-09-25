#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 256
#define SIZE ((size_t)NUM_PAGES * PAGE_SIZE)

static volatile sig_atomic_t running = 1;

static void stop_workload(int sig)
{
    (void)sig;
    running = 0;
}

static unsigned long long now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((unsigned long long)ts.tv_sec * 1000ULL) +
           ((unsigned long long)ts.tv_nsec / 1000000ULL);
}

int main(void)
{
    unsigned char *mem = malloc(SIZE);

    if (!mem)
    {
        perror("malloc");
        return 1;
    }

    signal(SIGINT, stop_workload);
    signal(SIGTERM, stop_workload);

    printf("REAL WORKLOAD PID: %d\n", getpid());
    printf("Memory Size       : %zu KB\n", SIZE / 1024);
    printf("Pages             : %d\n", NUM_PAGES);
    printf("Page Size         : %d bytes\n", PAGE_SIZE);
    printf("Mode              : LIVE MEMORY ACCESS\n");
    printf("Running... press Ctrl+C to stop.\n");
    fflush(stdout);

    /*
     * Deterministic locality pattern:
     *
     * 0 1 2 3 4 5 6 7
     * 0 1 2 3 4 5 6 7
     * ...
     *
     * followed by a second region.
     *
     * This gives AIPage a meaningful access pattern
     * to learn/predict.
     */

    unsigned long access_id = 0;

    while (running)
    {
        for (int page = 0; page < 16 && running; page++)
        {
            size_t offset = (size_t)page * PAGE_SIZE;

            mem[offset]++;

            printf(
                "ACCESS | id=%lu | page=%d | offset=%zu | time_ms=%llu\n",
                access_id++,
                page,
                offset,
                now_ms()
            );

            fflush(stdout);

            usleep(20000);
        }

        for (int page = 0; page < 16 && running; page += 2)
        {
            size_t offset = (size_t)page * PAGE_SIZE;

            mem[offset]++;

            printf(
                "ACCESS | id=%lu | page=%d | offset=%zu | time_ms=%llu\n",
                access_id++,
                page,
                offset,
                now_ms()
            );

            fflush(stdout);

            usleep(20000);
        }
    }

    printf("REAL WORKLOAD STOPPED.\n");

    free(mem);

    return 0;
}
