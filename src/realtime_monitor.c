#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define MAX_PAGES 64
#define PAGE_SIZE 4096

typedef struct
{
    unsigned long page;
    unsigned long accesses;
    unsigned long last_seen;
} PageInfo;

static volatile sig_atomic_t running = 1;

void stop_monitor(int sig)
{
    (void)sig;
    running = 0;
}

int read_rss_pages(pid_t pid)
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

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <PID>\n", argv[0]);
        return 1;
    }

    pid_t pid = atoi(argv[1]);

    signal(SIGINT, stop_monitor);
    signal(SIGTERM, stop_monitor);

    printf("\n============================================================\n");
    printf("AIPage REAL-TIME LINUX MEMORY MONITOR\n");
    printf("============================================================\n");
    printf("PID              : %d\n", pid);
    printf("Page Size        : %d bytes\n", PAGE_SIZE);
    printf("Mode             : LIVE\n");
    printf("Trace File       : NONE\n");
    printf("============================================================\n\n");

    unsigned long tick = 0;

    while (running)
    {
        int rss = read_rss_pages(pid);

        if (rss < 0)
        {
            printf("Process %d no longer exists.\n", pid);
            break;
        }

        printf(
            "LIVE | PID=%d | VmRSS=%d KB | Sample=%lu\n",
            pid,
            rss,
            tick++
        );

        fflush(stdout);

        usleep(500000);
    }

    printf("\nAIPage monitor stopped.\n");

    return 0;
}
