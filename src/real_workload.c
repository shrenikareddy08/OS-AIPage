#define _GNU_SOURCE

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <stdint.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 4096
#define REGION_SIZE ((size_t)NUM_PAGES * PAGE_SIZE)

static volatile sig_atomic_t running = 1;

static void stop_workload(int sig)
{
    (void)sig;
    running = 0;
}

/*
 * Runtime pseudo-random generator.
 * Seeded from current time + PID, so the access sequence
 * is different on every execution.
 */
static uint64_t rng_state;

static uint64_t next_random(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static int random_page(void)
{
    return (int)(next_random() % NUM_PAGES);
}

static void emit_access(unsigned char *memory, int page)
{
    static unsigned long access_id = 0;

    uintptr_t address =
        (uintptr_t)(memory + ((size_t)page * PAGE_SIZE));

    unsigned long offset =
        (unsigned long)(address - (uintptr_t)memory);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    unsigned long long time_ms =
        (unsigned long long)ts.tv_sec * 1000ULL +
        (unsigned long long)ts.tv_nsec / 1000000ULL;

    /*
     * page = actual page offset inside the allocated
     * runtime memory region.
     */
uint64_t virtual_page =
    (uint64_t)(address / PAGE_SIZE);

printf(
    "ACCESS | id=%lu | page=%" PRIu64
    " | offset=%lu | time_ms=%llu\n",
    access_id++,
    virtual_page,
    offset,
    time_ms
);
    fflush(stdout);
}

static void touch_page(unsigned char *memory, int page)
{
    if (page < 0 || page >= NUM_PAGES)
        return;

    size_t offset = (size_t)page * PAGE_SIZE;

    /*
     * ACTUAL MEMORY ACCESS.
     */
    memory[offset]++;

    emit_access(memory, page);
}

int main(void)
{
    /*
     * Allocate REAL process memory.
     */
    unsigned char *memory =
        malloc(REGION_SIZE);

    if (!memory)
    {
        perror("malloc");
        return 1;
    }

    /*
     * Force Linux to back the allocated region with
     * physical memory pages.
     */
    memset(memory, 0, REGION_SIZE);

    signal(SIGINT, stop_workload);
    signal(SIGTERM, stop_workload);

    /*
     * Runtime seed.
     */
    struct timespec seed_time;
    clock_gettime(CLOCK_REALTIME, &seed_time);

    rng_state =
        ((uint64_t)seed_time.tv_sec << 32) ^
        (uint64_t)seed_time.tv_nsec ^
        (uint64_t)getpid();

    if (rng_state == 0)
        rng_state = 0x123456789abcdefULL;

    fprintf(stderr,
        "\n============================================================\n"
        "AIPage REAL DYNAMIC LINUX MEMORY WORKLOAD\n"
        "============================================================\n"
        "PID              : %d\n"
        "Memory Region    : %zu KB\n"
        "Virtual Pages    : %d\n"
        "Page Size        : %d bytes\n"
        "Mode             : REAL PROCESS MEMORY\n"
        "Access Pattern   : RUNTIME GENERATED\n"
        "Page Selection   : DYNAMIC\n"
        "IPC              : UNIX PIPE / STDOUT\n"
        "============================================================\n\n",
        getpid(),
        REGION_SIZE / 1024,
        NUM_PAGES,
        PAGE_SIZE
    );

    fflush(stderr);

    while (running)
    {
        /*
         * Runtime-generated working-set size.
         * Changes during execution.
         */
        int working_set =
            8 + (int)(next_random() % 57);

        /*
         * Runtime-generated starting point.
         */
        int start =
            random_page();

        /*
         * Randomly choose whether this burst is:
         *
         * 1. Localized
         * 2. Random
         * 3. Strided
         *
         * Nothing is hardcoded as a page sequence.
         */
        int mode =
            (int)(next_random() % 3);

        for (int i = 0;
             i < working_set && running;
             i++)
        {
            int page;

            if (mode == 0)
            {
                /*
                 * Local access around a runtime-selected page.
                 */
                int delta =
                    (int)(next_random() % 16) - 8;

                page =
                    (start + delta + NUM_PAGES)
                    % NUM_PAGES;
            }
            else if (mode == 1)
            {
                /*
                 * Completely runtime-random page.
                 */
                page = random_page();
            }
            else
            {
                /*
                 * Runtime-generated stride.
                 */
                int stride =
                    1 + (int)(next_random() % 32);

                page =
                    (start + i * stride)
                    % NUM_PAGES;
            }

            touch_page(memory, page);

            /*
             * Small runtime-generated delay.
             */
            usleep(
                1000 +
                (useconds_t)(next_random() % 9000)
            );
        }
    }

    fprintf(stderr,
        "\n============================================================\n"
        "REAL DYNAMIC WORKLOAD STOPPED\n"
        "============================================================\n");

    free(memory);

    return 0;
}
