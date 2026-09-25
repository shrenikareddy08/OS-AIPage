#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#define PAGE_SIZE 4096
#define PAGES 512

volatile uint64_t sink = 0;
volatile sig_atomic_t running = 1;

static unsigned long long accesses = 0;

void handle_signal(int sig)
{
    (void)sig;
    running = 0;
}

static void access_page(uint8_t *memory, int page, FILE *trace)
{
    if (page < 0 || page >= PAGES)
        return;

    uintptr_t address =
        (uintptr_t)(memory + ((size_t)page * PAGE_SIZE));

    /* REAL memory accesses */
    sink += memory[page * PAGE_SIZE];
    sink += memory[page * PAGE_SIZE + 128];
    sink += memory[page * PAGE_SIZE + 256];

    accesses++;

    if (trace)
    {
        fprintf(trace,
                "%llu,%p,%d\n",
                accesses,
                (void *)address,
                page);

        fflush(trace);
    }
}

int main(void)
{
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("============================================================\n");
    printf("AIPage REAL MEMORY PREDICTION WORKLOAD\n");
    printf("============================================================\n");
    printf("PID       : %d\n", getpid());
    printf("Pages     : %d\n", PAGES);
    printf("Page size : %d bytes\n", PAGE_SIZE);
    printf("============================================================\n");

    fflush(stdout);

    size_t memory_size = (size_t)PAGES * PAGE_SIZE;

    uint8_t *memory =
        aligned_alloc(PAGE_SIZE, memory_size);

    if (!memory)
    {
        perror("aligned_alloc");
        return 1;
    }

    /* Initialize every page */
    for (int i = 0; i < PAGES; i++)
    {
        memory[i * PAGE_SIZE] = (uint8_t)i;
    }

    char trace_path[256];

    snprintf(trace_path,
             sizeof(trace_path),
             "results/live_access_%d.csv",
             getpid());

    FILE *trace = fopen(trace_path, "w");

    if (!trace)
    {
        perror("trace");
        free(memory);
        return 1;
    }

    fprintf(trace,
            "access_id,virtual_address,page\n");

    fflush(trace);

    printf("Trace      : %s\n", trace_path);
    printf("Pattern    : 3 -> 7 -> 7 -> 12 -> 7 -> 3 -> 7 -> 12\n");
    printf("Mode       : Repeating locality\n");
    printf("Running...\n\n");

    fflush(stdout);

    /*
     * Strong locality pattern.
     *
     * The important transition is:
     *
     * 7 -> 12
     *
     * which occurs repeatedly.
     */
    int pattern[] = {
        3, 7, 7, 12,
        7, 3, 7, 12
    };

    int pattern_size =
        sizeof(pattern) / sizeof(pattern[0]);

    while (running)
    {
        for (int i = 0;
             i < pattern_size && running;
             i++)
        {
            access_page(
                memory,
                pattern[i],
                trace);

            usleep(100000);
        }
    }

    printf("\nStopping workload...\n");
    printf("Total accesses : %llu\n", accesses);
    printf("Final sink     : %llu\n",
           (unsigned long long)sink);

    fclose(trace);
    free(memory);

    return 0;
}
