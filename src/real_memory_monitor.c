#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

static volatile sig_atomic_t running = 1;

static void stop_monitor(int sig)
{
    (void)sig;
    running = 0;
}

static int process_exists(pid_t pid)
{
    char path[128];

    snprintf(path, sizeof(path), "/proc/%d", pid);

    return access(path, F_OK) == 0;
}

static int read_rss(pid_t pid)
{
    char path[128];
    char line[512];

    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *fp = fopen(path, "r");

    if (!fp)
        return -1;

    int rss = -1;

    while (fgets(line, sizeof(line), fp))
    {
        if (sscanf(line, "VmRSS: %d", &rss) == 1)
            break;
    }

    fclose(fp);

    return rss;
}

static unsigned long read_referenced(pid_t pid)
{
    char path[128];
    char line[512];

    snprintf(path, sizeof(path), "/proc/%d/smaps", pid);

    FILE *fp = fopen(path, "r");

    if (!fp)
        return 0;

    unsigned long total = 0;

    while (fgets(line, sizeof(line), fp))
    {
        unsigned long kb;

        if (sscanf(line, "Referenced: %lu kB", &kb) == 1)
            total += kb;
    }

    fclose(fp);

    return total;
}

static int clear_references(pid_t pid)
{
    char path[128];

    snprintf(path, sizeof(path), "/proc/%d/clear_refs", pid);

    FILE *fp = fopen(path, "w");

    if (!fp)
        return -1;

    fprintf(fp, "1\n");

    fclose(fp);

    return 0;
}

static int count_memory_regions(pid_t pid)
{
    char path[128];
    char line[1024];

    snprintf(path, sizeof(path), "/proc/%d/maps", pid);

    FILE *fp = fopen(path, "r");

    if (!fp)
        return 0;

    int count = 0;

    while (fgets(line, sizeof(line), fp))
        count++;

    fclose(fp);

    return count;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <PID>\n", argv[0]);
        return 1;
    }

    pid_t pid = (pid_t)atoi(argv[1]);

    signal(SIGINT, stop_monitor);
    signal(SIGTERM, stop_monitor);

    if (!process_exists(pid))
    {
        fprintf(stderr, "Process %d does not exist.\n", pid);
        return 1;
    }

    printf("\n");
    printf("============================================================\n");
    printf("AIPage REAL PROCESS MEMORY MONITOR\n");
    printf("============================================================\n");
    printf("PID              : %d\n", pid);
    printf("Source           : Linux /proc/%d\n", pid);
    printf("Memory Source    : REAL PROCESS MEMORY\n");
    printf("Page Activity    : Linux Referenced information\n");
    printf("Trace File       : NONE\n");
    printf("============================================================\n\n");

    unsigned long sample = 0;

    while (running && process_exists(pid))
    {
        int rss = read_rss(pid);
        unsigned long referenced = read_referenced(pid);
        int regions = count_memory_regions(pid);

        printf(
            "MEMORY | PID=%d | RSS=%d KB | REFERENCED=%lu KB | REGIONS=%d | SAMPLE=%lu\n",
            pid,
            rss,
            referenced,
            regions,
            sample++
        );

        fflush(stdout);

        /*
         * Reset Linux reference information so the next
         * interval measures new memory references.
         */
        clear_references(pid);

        usleep(500000);
    }

    printf("\nProcess %d ended or monitor stopped.\n", pid);

    return 0;
}
