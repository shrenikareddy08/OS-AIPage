#include <stdio.h>
#include <stdlib.h>

#define MAX_PAGES 256
#define MAX_TRACE 100000

int main(void)
{
    FILE *file = fopen("page_trace.txt", "r");

    if (file == NULL)
    {
        perror("Could not open page_trace.txt");
        return 1;
    }

    int trace[MAX_TRACE];
    int frequency[MAX_PAGES] = {0};
    int last_access[MAX_PAGES];

    long reuse_distance_sum[MAX_PAGES] = {0};
    int reuse_count[MAX_PAGES] = {0};

    for (int i = 0; i < MAX_PAGES; i++)
    {
        last_access[i] = -1;
    }

    int trace_length = 0;

    /*
     * Read the page-reference trace.
     */
    while (trace_length < MAX_TRACE &&
           fscanf(file, "%d", &trace[trace_length]) == 1)
    {
        int page = trace[trace_length];

        if (page >= 0 && page < MAX_PAGES)
        {
            frequency[page]++;

            /*
             * If this page was accessed before,
             * calculate the number of accesses
             * between the previous access and now.
             */
            if (last_access[page] != -1)
            {
                int distance = trace_length - last_access[page] - 1;

                reuse_distance_sum[page] += distance;
                reuse_count[page]++;
            }

            last_access[page] = trace_length;
        }

        trace_length++;
    }

    fclose(file);

    if (trace_length == 0)
    {
        printf("Trace is empty.\n");
        return 1;
    }

    /*
     * Current position in the trace.
     */
    int current_position = trace_length - 1;

    printf("========================================\n");
    printf("       AIPage Feature Extractor\n");
    printf("========================================\n");

    printf("Trace length: %d accesses\n\n", trace_length);

    printf("Page Features\n");
    printf("----------------------------------------\n");

    printf("%-6s %-12s %-12s %-16s\n",
           "Page",
           "Frequency",
           "Recency",
           "Avg Reuse Dist.");

    printf("----------------------------------------\n");

    for (int page = 0; page < MAX_PAGES; page++)
    {
        if (frequency[page] > 0)
        {
            int recency = current_position - last_access[page];

            double average_reuse_distance = 0.0;

            if (reuse_count[page] > 0)
            {
                average_reuse_distance =
                    (double)reuse_distance_sum[page] /
                    reuse_count[page];
            }

            printf("%-6d %-12d %-12d %-16.2f\n",
                   page,
                   frequency[page],
                   recency,
                   average_reuse_distance);
        }
    }

    printf("----------------------------------------\n");

    return 0;
}
