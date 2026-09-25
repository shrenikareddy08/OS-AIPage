#include <stdio.h>
#include <stdlib.h>

#define MAX_TRACE 100000
#define NUM_FRAME_TESTS 7

int load_trace(const char *filename, int trace[])
{
    FILE *file = fopen(filename, "r");

    if (file == NULL)
    {
        perror("Could not open trace file");
        return -1;
    }

    int length = 0;

    while (length < MAX_TRACE &&
           fscanf(file, "%d", &trace[length]) == 1)
    {
        length++;
    }

    fclose(file);

    return length;
}


/*
 * FIFO Page Replacement
 */
int fifo(const int trace[], int length, int frames)
{
    int memory[frames];

    int count = 0;
    int next = 0;
    int faults = 0;

    for (int i = 0; i < frames; i++)
        memory[i] = -1;

    for (int i = 0; i < length; i++)
    {
        int page = trace[i];
        int hit = 0;

        for (int j = 0; j < frames; j++)
        {
            if (memory[j] == page)
            {
                hit = 1;
                break;
            }
        }

        if (!hit)
        {
            faults++;

            if (count < frames)
            {
                memory[count] = page;
                count++;
            }
            else
            {
                memory[next] = page;
                next = (next + 1) % frames;
            }
        }
    }

    return faults;
}


/*
 * LRU Page Replacement
 */
int lru(const int trace[], int length, int frames)
{
    int memory[frames];
    int last_used[frames];

    int count = 0;
    int faults = 0;

    for (int i = 0; i < frames; i++)
    {
        memory[i] = -1;
        last_used[i] = -1;
    }

    for (int i = 0; i < length; i++)
    {
        int page = trace[i];
        int hit = 0;

        for (int j = 0; j < frames; j++)
        {
            if (memory[j] == page)
            {
                hit = 1;
                last_used[j] = i;
                break;
            }
        }

        if (!hit)
        {
            faults++;

            if (count < frames)
            {
                memory[count] = page;
                last_used[count] = i;
                count++;
            }
            else
            {
                int victim = 0;

                for (int j = 1; j < frames; j++)
                {
                    if (last_used[j] < last_used[victim])
                        victim = j;
                }

                memory[victim] = page;
                last_used[victim] = i;
            }
        }
    }

    return faults;
}


/*
 * Optimal Page Replacement
 *
 * Used only as an offline theoretical lower-bound/reference.
 */
int optimal(const int trace[], int length, int frames)
{
    int memory[frames];

    int count = 0;
    int faults = 0;

    for (int i = 0; i < frames; i++)
        memory[i] = -1;

    for (int i = 0; i < length; i++)
    {
        int page = trace[i];
        int hit = 0;

        for (int j = 0; j < frames; j++)
        {
            if (memory[j] == page)
            {
                hit = 1;
                break;
            }
        }

        if (hit)
            continue;

        faults++;

        if (count < frames)
        {
            memory[count] = page;
            count++;
            continue;
        }

        int victim = -1;
        int farthest = -1;

        for (int j = 0; j < frames; j++)
        {
            int next_use = length;

            for (int k = i + 1; k < length; k++)
            {
                if (trace[k] == memory[j])
                {
                    next_use = k;
                    break;
                }
            }

            if (next_use > farthest)
            {
                farthest = next_use;
                victim = j;
            }
        }

        memory[victim] = page;
    }

    return faults;
}


/*
 * Print one experiment row.
 */
void run_experiment(
    const int trace[],
    int length,
    int frames)
{
    int fifo_faults = fifo(trace, length, frames);
    int lru_faults = lru(trace, length, frames);
    int optimal_faults = optimal(trace, length, frames);

    double fifo_hit_rate =
        100.0 * (length - fifo_faults) / length;

    double lru_hit_rate =
        100.0 * (length - lru_faults) / length;

    double optimal_hit_rate =
        100.0 * (length - optimal_faults) / length;

    printf(
        "%-7d | %-8d | %-8d | %-8d | "
        "%7.2f%% | %7.2f%% | %7.2f%%\n",
        frames,
        fifo_faults,
        lru_faults,
        optimal_faults,
        fifo_hit_rate,
        lru_hit_rate,
        optimal_hit_rate
    );
}


int main(int argc, char *argv[])
{
    int trace[MAX_TRACE];

    if (argc != 2)
    {
        printf("Usage: %s <trace_file>\n", argv[0]);

        printf("\nExample:\n");
        printf("  %s traces/locality.trace\n", argv[0]);

        return 1;
    }

    int length = load_trace(argv[1], trace);

    if (length <= 0)
    {
        printf("No trace data available.\n");
        return 1;
    }

    /*
     * Multiple memory sizes.
     *
     * This allows us to study how algorithms
     * behave as available memory changes.
     */
    int frame_sizes[NUM_FRAME_TESTS] =
    {
        2, 3, 4, 5, 6, 8, 10
    };

    printf("\n");
    printf("============================================================\n");
    printf("              AIPage Baseline Evaluation\n");
    printf("============================================================\n");

    printf("Trace file   : %s\n", argv[1]);
    printf("Trace length : %d accesses\n", length);

    printf("\n");
    printf("Frames  | FIFO     | LRU      | OPTIMAL  | "
           "FIFO Hit | LRU Hit  | OPT Hit\n");

    printf("------------------------------------------------------------\n");

    for (int i = 0; i < NUM_FRAME_TESTS; i++)
    {
        run_experiment(
            trace,
            length,
            frame_sizes[i]
        );
    }

    printf("============================================================\n");

    printf("\nInterpretation:\n");
    printf("FIFO   = First-In First-Out\n");
    printf("LRU    = Least Recently Used\n");
    printf("OPT    = Optimal offline reference\n");

    printf("\nNote:\n");
    printf("Optimal uses future knowledge and is therefore\n");
    printf("not implementable as a real-time replacement policy.\n");

    printf("\n");

    return 0;
}
