#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 256
FILE *trace_file = NULL;

void record_page_access(int page)
{
    if (trace_file != NULL)
    {
        fprintf(trace_file, "%d\n", page);
        fflush(trace_file);
    }
}

void sleep_5ms(void)
{
    struct timespec delay;

    delay.tv_sec = 0;
    delay.tv_nsec = 5000000L;

    nanosleep(&delay, NULL);
}

void sequential_access(char *memory)
{
    for (int i = 0; i < NUM_PAGES; i++)
    {
        memory[i * PAGE_SIZE]++;
       record_page_access(i);
       sleep_5ms();
    }
}

void strided_access(char *memory)
{
    for (int i = 0; i < NUM_PAGES; i += 4)
    {
        memory[i * PAGE_SIZE]++;
record_page_access(i);
        sleep_5ms();
    }
}

void random_access(char *memory)
{
    int page = rand() % NUM_PAGES;

    memory[page * PAGE_SIZE]++;
record_page_access(page);
    sleep_5ms();
}

void locality_access(char *memory)
{
    /*
     * Frequently access a small "hot" region.
     * This simulates temporal locality.
     */
    int hot_pages = 16;

    for (int i = 0; i < 20; i++)
    {
        int page = rand() % hot_pages;

        memory[page * PAGE_SIZE]++;
       record_page_access(page);
 sleep_5ms();
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <pattern>\n", argv[0]);
        printf("\nPatterns:\n");
        printf("  sequential  - sequential page access\n");
        printf("  strided     - every 4th page\n");
        printf("  random      - random page access\n");
        printf("  locality    - repeated hot-page access\n");
        return 1;
    }

    char *pattern = argv[1];

    printf("========================================\n");
    printf("        AIPage Memory Workload\n");
    printf("========================================\n");

    printf("PID: %d\n", getpid());
trace_file = fopen("page_trace.txt", "w");

if (trace_file == NULL)
{
    perror("Could not open trace file");
    return 1;
}
    char *memory = malloc(NUM_PAGES * PAGE_SIZE);

    if (memory == NULL)
    {
        perror("malloc failed");
        return 1;
    }

    /*
     * Initialize memory.
     *
     * This ensures the allocated pages are actually
     * backed by physical memory before our experiment.
     */
    memset(memory, 0, NUM_PAGES * PAGE_SIZE);

    printf("Allocated: %d pages (%d KB)\n",
           NUM_PAGES,
           (NUM_PAGES * PAGE_SIZE) / 1024);

    printf("Access Pattern: %s\n", pattern);
    printf("Starting memory access pattern...\n");
    printf("Press Ctrl+C to stop.\n\n");

    srand((unsigned int)time(NULL));

    while (1)
    {
        if (strcmp(pattern, "sequential") == 0)
        {
            sequential_access(memory);
        }
        else if (strcmp(pattern, "strided") == 0)
        {
            strided_access(memory);
        }
        else if (strcmp(pattern, "random") == 0)
        {
            random_access(memory);
        }
        else if (strcmp(pattern, "locality") == 0)
        {
            locality_access(memory);
        }
        else
        {
            printf("Unknown pattern: %s\n", pattern);
            free(memory);
            return 1;
        }
    }

    free(memory);
fclose(trace_file);
    return 0;
}
