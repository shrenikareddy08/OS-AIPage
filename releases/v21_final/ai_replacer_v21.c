#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>


/*
 * ============================================================
 * AIPage v2.1
 * Intelligent Page Replacement using Logistic Regression
 * ============================================================
 *
 * The model predicts:
 *
 *     P(future reuse)
 *
 * HIGH probability:
 *     -> page is likely to be reused
 *     -> KEEP the page
 *
 * LOW probability:
 *     -> page is unlikely to be reused
 *     -> EVICT the page
 *
 * Therefore:
 *
 *     AI victim = resident page with LOWEST score
 *
 * Runtime features:
 *
 *     1. frequency_10
 *     2. frequency_50
 *     3. frequency_100
 *     4. recency
 *     5. last_reuse_distance
 *     6. avg_reuse_distance
 *     7. reuse_count
 *
 * ============================================================
 */


#define MAX_TRACE 100000
#define MAX_PAGES 256
#define MAX_FRAMES 10

#define WINDOW_SHORT 10
#define WINDOW_MEDIUM 50
#define WINDOW_LONG 100


/*
 * ============================================================
 * FRAME
 * ============================================================
 */
typedef struct
{
    int page;
    int valid;

} Frame;


/*
 * ============================================================
 * LOGISTIC REGRESSION MODEL
 * ============================================================
 */
typedef struct
{
    double bias;

    double weight[7];

    double mean[7];

    double std[7];

} Model;


/*
 * ============================================================
 * SIGMOID
 * ============================================================
 */
double sigmoid(double z)
{
    if (z >= 0.0)
    {
        double e = exp(-z);

        return 1.0 /
               (1.0 + e);
    }

    double e = exp(z);

    return e /
           (1.0 + e);
}


/*
 * ============================================================
 * LOAD MODEL
 * ============================================================
 *
 * Reads:
 *
 *     model_v21.txt
 *
 * The file contains:
 *
 *     BIAS
 *     7 weights
 *     7 means
 *     7 standard deviations
 *
 * ============================================================
 */
int load_model(
    const char *filename,
    Model *model)
{
    FILE *file =
        fopen(filename, "r");

    if (file == NULL)
    {
        perror("Could not open model");
        return 0;
    }


    char name[128];

    double value;


    while (
        fscanf(
            file,
            "%127s %lf",
            name,
            &value
        ) == 2
    )
    {
        /*
         * ----------------------------------------------------
         * BIAS
         * ----------------------------------------------------
         */
        if (strcmp(name, "BIAS") == 0)
        {
            model->bias = value;
        }


        /*
         * ----------------------------------------------------
         * WEIGHTS
         * ----------------------------------------------------
         */
        else if (
            strcmp(
                name,
                "FREQUENCY_10_WEIGHT"
            ) == 0
        )
        {
            model->weight[0] = value;
        }

        else if (
            strcmp(
                name,
                "FREQUENCY_50_WEIGHT"
            ) == 0
        )
        {
            model->weight[1] = value;
        }

        else if (
            strcmp(
                name,
                "FREQUENCY_100_WEIGHT"
            ) == 0
        )
        {
            model->weight[2] = value;
        }

        else if (
            strcmp(
                name,
                "RECENCY_WEIGHT"
            ) == 0
        )
        {
            model->weight[3] = value;
        }

        else if (
            strcmp(
                name,
                "LAST_REUSE_DISTANCE_WEIGHT"
            ) == 0
        )
        {
            model->weight[4] = value;
        }

        else if (
            strcmp(
                name,
                "AVG_REUSE_DISTANCE_WEIGHT"
            ) == 0
        )
        {
            model->weight[5] = value;
        }

        else if (
            strcmp(
                name,
                "REUSE_COUNT_WEIGHT"
            ) == 0
        )
        {
            model->weight[6] = value;
        }


        /*
         * ----------------------------------------------------
         * MEANS
         * ----------------------------------------------------
         */
        else if (
            strcmp(
                name,
                "FREQUENCY_10_MEAN"
            ) == 0
        )
        {
            model->mean[0] = value;
        }

        else if (
            strcmp(
                name,
                "FREQUENCY_50_MEAN"
            ) == 0
        )
        {
            model->mean[1] = value;
        }

        else if (
            strcmp(
                name,
                "FREQUENCY_100_MEAN"
            ) == 0
        )
        {
            model->mean[2] = value;
        }

        else if (
            strcmp(
                name,
                "RECENCY_MEAN"
            ) == 0
        )
        {
            model->mean[3] = value;
        }

        else if (
            strcmp(
                name,
                "LAST_REUSE_DISTANCE_MEAN"
            ) == 0
        )
        {
            model->mean[4] = value;
        }

        else if (
            strcmp(
                name,
                "AVG_REUSE_DISTANCE_MEAN"
            ) == 0
        )
        {
            model->mean[5] = value;
        }

        else if (
            strcmp(
                name,
                "REUSE_COUNT_MEAN"
            ) == 0
        )
        {
            model->mean[6] = value;
        }


        /*
         * ----------------------------------------------------
         * STANDARD DEVIATIONS
         * ----------------------------------------------------
         */
        else if (
            strcmp(
                name,
                "FREQUENCY_10_STD"
            ) == 0
        )
        {
            model->std[0] = value;
        }

        else if (
            strcmp(
                name,
                "FREQUENCY_50_STD"
            ) == 0
        )
        {
            model->std[1] = value;
        }

        else if (
            strcmp(
                name,
                "FREQUENCY_100_STD"
            ) == 0
        )
        {
            model->std[2] = value;
        }

        else if (
            strcmp(
                name,
                "RECENCY_STD"
            ) == 0
        )
        {
            model->std[3] = value;
        }

        else if (
            strcmp(
                name,
                "LAST_REUSE_DISTANCE_STD"
            ) == 0
        )
        {
            model->std[4] = value;
        }

        else if (
            strcmp(
                name,
                "AVG_REUSE_DISTANCE_STD"
            ) == 0
        )
        {
            model->std[5] = value;
        }

        else if (
            strcmp(
                name,
                "REUSE_COUNT_STD"
            ) == 0
        )
        {
            model->std[6] = value;
        }
    }


    fclose(file);


    /*
     * Prevent division by zero during
     * feature normalization.
     */
    for (int i = 0;
         i < 7;
         i++)
    {
        if (model->std[i] == 0.0)
        {
            model->std[i] = 1.0;
        }
    }


    return 1;
}


/*
 * ============================================================
 * PAGE SEARCH
 * ============================================================
 *
 * Returns the frame index containing page.
 *
 * Returns:
 *
 *     >= 0  -> page found
 *     -1    -> page not found
 *
 * ============================================================
 */
int find_page(
    Frame frames[],
    int frame_count,
    int page)
{
    for (int i = 0;
         i < frame_count;
         i++)
    {
        if (
            frames[i].valid &&
            frames[i].page == page
        )
        {
            return i;
        }
    }


    return -1;
}


/*
 * ============================================================
 * FEATURE CALCULATION
 * ============================================================
 *
 * Calculates the seven features used by the model.
 *
 * IMPORTANT:
 *
 * Only the history BEFORE the current access is examined.
 *
 * The current access at:
 *
 *     trace[position]
 *
 * is NOT included.
 *
 * ============================================================
 */
void calculate_features(
    const int trace[],
    int position,
    int page,

    int *frequency10,
    int *frequency50,
    int *frequency100,

    int *recency,
    int *last_reuse_distance,

    double *avg_reuse_distance,

    int *reuse_count)
{
    /*
     * Examine at most the previous
     * WINDOW_LONG accesses.
     */
    int start =
        position - WINDOW_LONG;


    if (start < 0)
    {
        start = 0;
    }


    int f10 = 0;

    int f50 = 0;

    int f100 = 0;


    /*
     * Most recent previous access.
     */
    int last_access =
        -1;


    /*
     * Default value when the page
     * has not been reused.
     */
    int last_distance =
        WINDOW_LONG;


    long reuse_sum =
        0;


    int reuse_total =
        0;


    /*
     * --------------------------------------------------------
     * Scan the historical window.
     * --------------------------------------------------------
     */
    for (int i = start;
         i < position;
         i++)
    {
        /*
         * Ignore other pages.
         */
        if (trace[i] != page)
        {
            continue;
        }


        /*
         * Distance from current position.
         *
         * Example:
         *
         * position = 20
         * previous access = 17
         *
         * distance = 20 - 1 - 17
         *          = 2
         */
        int distance_from_now =
            position - 1 - i;


        /*
         * Frequency in the last 10 accesses.
         */
        if (
            distance_from_now
            < WINDOW_SHORT
        )
        {
            f10++;
        }


        /*
         * Frequency in the last 50 accesses.
         */
        if (
            distance_from_now
            < WINDOW_MEDIUM
        )
        {
            f50++;
        }


        /*
         * Frequency in the last 100 accesses.
         */
        f100++;


        /*
         * ----------------------------------------------------
         * Reuse distance
         * ----------------------------------------------------
         *
         * If this page was already seen earlier
         * in this window, calculate the number of
         * accesses between the two occurrences.
         */
        if (last_access != -1)
        {
            int distance =
                i - last_access - 1;


            reuse_sum +=
                distance;


            reuse_total++;


            last_distance =
                distance;
        }


        /*
         * Current occurrence becomes the
         * most recent occurrence.
         */
        last_access =
            i;
    }


    /*
     * --------------------------------------------------------
     * Output features
     * --------------------------------------------------------
     */
    *frequency10 =
        f10;


    *frequency50 =
        f50;


    *frequency100 =
        f100;


    /*
     * Recency:
     *
     * Smaller value = accessed recently.
     *
     * Larger value = accessed longer ago.
     */
    if (last_access == -1)
    {
        *recency =
            WINDOW_LONG;
    }
    else
    {
        *recency =
            position - 1 - last_access;
    }


    /*
     * Last reuse distance.
     */
    *last_reuse_distance =
        last_distance;


    /*
     * Average reuse distance.
     */
    if (reuse_total > 0)
    {
        *avg_reuse_distance =
            (double)reuse_sum
            / reuse_total;
    }
    else
    {
        *avg_reuse_distance =
            0.0;
    }


    /*
     * Number of observed reuses.
     */
    *reuse_count =
        reuse_total;
}


/*
 * ============================================================
 * AI SCORE
 * ============================================================
 *
 * Returns:
 *
 *     P(optimal victim)
 *
 * calculated using logistic regression.
 *
 * The model was trained with:
 *
 *     target = 1
 *         -> OPT selected this page as victim
 *
 *     target = 0
 *         -> OPT kept this page
 *
 * Therefore:
 *
 *     HIGH score = strong victim candidate
 *     LOW score  = likely to be retained
 *
 * ============================================================
 */
double ai_score(
    const Model *model,
    const int trace[],
    int position,
    int page)
{
    int f10;

    int f50;

    int f100;


    int recency;

    int last_distance;


    double avg_distance;


    int reuse_count;


    /*
     * Calculate runtime features.
     */
    calculate_features(
        trace,
        position,
        page,

        &f10,
        &f50,
        &f100,

        &recency,
        &last_distance,

        &avg_distance,

        &reuse_count
    );


    /*
     * Put the features into the same
     * order used during training.
     */
    double features[7];


    features[0] =
        f10;


    features[1] =
        f50;


    features[2] =
        f100;


    features[3] =
        recency;


    features[4] =
        last_distance;


    features[5] =
        avg_distance;


    features[6] =
        reuse_count;


    /*
     * Start with model bias.
     */
    double z =
        model->bias;


    /*
     * Standardized logistic regression.
     */
    for (int i = 0;
         i < 7;
         i++)
    {
        double normalized =
            (
                features[i]
                - model->mean[i]
            )
            / model->std[i];


        z +=
            model->weight[i]
            * normalized;
    }


    /*
     * Convert logit to probability.
     */
    return sigmoid(z);
}


/*
 * ============================================================
 * AI VICTIM
 * ============================================================
 *
 * The model predicts:
 *
 *     P(OPT selects candidate as victim)
 *
 * HIGH score:
 *     -> likely victim
 *     -> EVICT
 *
 * LOW score:
 *     -> likely to be retained
 *     -> KEEP
 *
 * Therefore:
 *
 *     choose the HIGHEST score.
 *
 * ============================================================
 */
/*
 * ============================================================
 * OPT VICTIM - DIAGNOSTIC ONLY
 * ============================================================
 *
 * Used only to compare the AI decision against the
 * theoretically optimal replacement decision.
 *
 * OPT is allowed to see future accesses.
 *
 * It selects the resident page whose next access is
 * farthest in the future.
 *
 * This function does NOT influence AIPage replacement.
 * ============================================================
 */
int find_optimal_victim(
    const int trace[],
    int trace_length,
    int position,
    Frame frames[],
    int frame_count)
{
    int victim = 0;
    int farthest = -1;

    for (int i = 0;
         i < frame_count;
         i++)
    {
        int page =
            frames[i].page;

        int next_use =
            INT_MAX;

        for (int j = position + 1;
             j < trace_length;
             j++)
        {
            if (trace[j] == page)
            {
                next_use =
                    j - position;

                break;
            }
        }

        /*
         * Page is never used again.
         * Therefore it is the ideal victim.
         */
        if (next_use == INT_MAX)
        {
            return i;
        }

        if (next_use > farthest)
        {
            farthest =
                next_use;

            victim =
                i;
        }
    }

    return victim;
}
/*
 * ============================================================
 * OPT VICTIM - EVALUATION ONLY
 * ============================================================
 *
 * Uses future references to identify the theoretically
 * optimal victim.
 *
 * IMPORTANT:
 * This function is NEVER used to make the AI replacement
 * decision. It is used only to measure how closely AIPage
 * follows OPT.
 *
 * OPT selects the resident page whose next access is
 * farthest in the future.
 *
 * ============================================================
 */


/*
 * ============================================================
 * OPT VICTIM - EVALUATION ONLY
 * ============================================================
 *
 * Uses future references to identify the theoretically
 * optimal victim.
 *
 * IMPORTANT:
 * This function is NEVER used to make the AI replacement
 * decision. It is used only to measure how closely AIPage
 * follows OPT.
 *
 * OPT selects the resident page whose next access is
 * farthest in the future.
 *
 * ============================================================
 */


/*
 * ============================================================
 * OPT VICTIM - EVALUATION ONLY
 * ============================================================
 *
 * Uses future references to identify the theoretically
 * optimal victim.
 *
 * IMPORTANT:
 * This function is NEVER used to make the AI replacement
 * decision. It is used only to measure how closely AIPage
 * follows OPT.
 *
 * OPT selects the resident page whose next access is
 * farthest in the future.
 *
 * ============================================================
 */



/*
 * ============================================================
 * AI VICTIM
 * ============================================================
 */
/*
 * ============================================================
 * OPT VICTIM - EVALUATION ONLY
 * ============================================================
 *
 * Uses future references to identify the theoretically
 * optimal victim.
 *
 * IMPORTANT:
 * This function is NEVER used to make the AI replacement
 * decision. It is used only to measure how closely AIPage
 * follows OPT.
 *
 * OPT selects the resident page whose next access is
 * farthest in the future.
 *
 * ============================================================
 */



/*
 * ============================================================
 * AI VICTIM
 * ============================================================
 *//*
 * ============================================================
 * OPT VICTIM - EVALUATION ONLY
 * ============================================================
 *
 * Uses future references to identify the theoretically
 * optimal victim.
 *
 * IMPORTANT:
 * This function is NEVER used to make the AI replacement
 * decision. It is used only to measure how closely AIPage
 * follows OPT.
 *
 * OPT selects the resident page whose next access is
 * farthest in the future.
 *
 * ============================================================
 */

/*
 * ============================================================
 * OPT VICTIM - EVALUATION ONLY
 * ============================================================
 *
 * Uses future references to identify the theoretically
 * optimal victim.
 *
 * IMPORTANT:
 * This function is NEVER used to make the AI replacement
 * decision. It is used only to measure how closely AIPage
 * follows OPT.
 *
 * OPT selects the resident page whose next access is
 * farthest in the future.
 *
 * ============================================================
 */



/*
 * ============================================================
 * OPT VICTIM - EVALUATION ONLY
 * ============================================================
 *
 * Uses future references to identify the theoretically
 * optimal victim.
 *
 * IMPORTANT:
 * This function is NEVER used to make the AI replacement
 * decision. It is used only to measure how closely AIPage
 * follows OPT.
 *
 * OPT selects the resident page whose next access is
 * farthest in the future.
 *
 * ============================================================
 */


/*
 * ============================================================
 * OPT VICTIM - EVALUATION ONLY
 * ============================================================
 *
 * Uses future references to identify the theoretically
 * optimal victim.
 *
 * IMPORTANT:
 * This function is NEVER used to make the AI replacement
 * decision. It is used only to measure how closely AIPage
 * follows OPT.
 *
 * OPT selects the resident page whose next access is
 * farthest in the future.
 *
 * ============================================================
 */



/*
 * ============================================================
 * AI VICTIM
 * ============================================================
 */
int choose_ai_victim(
    const Model *model,
    const int trace[],
    int position,
    Frame frames[],
    int frame_count)
{
int victim = 0;

double highest_score = -1.0;
    for (int i = 0;
         i < frame_count;
         i++)
    {
        double score =
            ai_score(
                model,
                trace,
                position,
                frames[i].page
            );

/*
 * The model predicts:
 *
 *     P(OPT selects candidate as victim)
 *
 * Higher score means the candidate is
 * more likely to be the correct victim.
 */
if (score > highest_score)
{
    highest_score = score;
    victim = i;
}    }

    return victim;
}
/*
 * ============================================================
 * LRU VICTIM
 * ============================================================
 *
 * This function is retained as a baseline/reference
 * implementation.
 *
 * It selects the page whose most recent access is
 * farthest in the past.
 *
 * ============================================================
 */
int choose_lru_victim(
    const int trace[],
    int position,
    Frame frames[],
    int frame_count)
{
    int victim =
        0;


    int oldest =
        -1;


    for (int i = 0;
         i < frame_count;
         i++)
    {
        int page =
            frames[i].page;


        int last =
            -1;


        /*
         * Search backwards for the
         * most recent access.
         */
        for (int j = position - 1;
             j >= 0;
             j--)
        {
            if (trace[j] == page)
            {
                last =
                    j;

                break;
            }
        }


        /*
         * If the page has never been
         * accessed before, it is the
         * oldest possible candidate.
         */
        if (last < 0)
        {
            return i;
        }


        /*
         * Larger index distance means
         * less recently used.
         */
        if (
            oldest == -1 ||
            last < oldest
        )
        {
            oldest =
                last;


            victim =
                i;
        }
    }


    return victim;
}


/*
 * ============================================================
 * MAIN SIMULATION
 * ============================================================
 */
int main(
    int argc,
    char *argv[])
{
long ai_opt_correct = 0;
long ai_opt_total = 0;

    /*
     * --------------------------------------------------------
     * Command-line arguments
     * --------------------------------------------------------
     *
     * Usage:
     *
     *     ./bin/ai_replacer_v21 <trace> <frames>
     *
     * Example:
     *
     *     ./bin/ai_replacer_v21 \
     *         traces/locality.trace \
     *         4
     */
    if (argc < 3)
    {
        printf(
            "Usage: %s <trace> <frames>\n",
            argv[0]
        );


        return 1;
    }


    const char *trace_file =
        argv[1];


    int frame_count =
        atoi(argv[2]);


    /*
     * Validate frame count.
     */
    if (
        frame_count < 1 ||
        frame_count > MAX_FRAMES
    )
    {
        printf(
            "Invalid frame count. "
            "Use 1-%d.\n",
            MAX_FRAMES
        );


        return 1;
    }


    /*
     * --------------------------------------------------------
     * Load trained model
     * --------------------------------------------------------
     */
    Model model =
        {0};


    if (
        !load_model(
            "model_v21.txt",
            &model
        )
    )
    {
        return 1;
    }


    /*
     * --------------------------------------------------------
     * Open trace
     * --------------------------------------------------------
     */
    FILE *file =
        fopen(
            trace_file,
            "r"
        );


    if (file == NULL)
    {
        perror(
            "Could not open trace"
        );


        return 1;
    }


    /*
     * --------------------------------------------------------
     * Allocate trace memory
     * --------------------------------------------------------
     */
    int *trace =
        malloc(
            sizeof(int)
            * MAX_TRACE
        );


    if (trace == NULL)
    {
        fclose(file);


        fprintf(
            stderr,
            "Memory allocation failed.\n"
        );


        return 1;
    }


    /*
     * --------------------------------------------------------
     * Load trace
     * --------------------------------------------------------
     */
    int trace_length =
        0;


    while (
        trace_length < MAX_TRACE &&
        fscanf(
            file,
            "%d",
            &trace[trace_length]
        ) == 1
    )
    {
        trace_length++;
    }


    fclose(file);


    /*
     * --------------------------------------------------------
     * Initialize frames
     * --------------------------------------------------------
     */
    Frame frames[MAX_FRAMES];


    for (int i = 0;
         i < frame_count;
         i++)
    {
        frames[i].valid =
            0;


        frames[i].page =
            -1;
    }


    int resident_count =
        0;


    long hits =
        0;


    long faults =
        0;


    /*
     * --------------------------------------------------------
     * Page-reference simulation
     * --------------------------------------------------------
     */
    for (int position = 0;
         position < trace_length;
         position++)
    {
        int page =
            trace[position];


        /*
         * Check whether page is already
         * resident in memory.
         */
        int existing =
            find_page(
                frames,
                resident_count,
                page
            );


        /*
         * ----------------------------------------------------
         * PAGE HIT
         * ----------------------------------------------------
         */
        if (existing != -1)
        {
            hits++;

            continue;
        }


        /*
         * ----------------------------------------------------
         * PAGE FAULT
         * ----------------------------------------------------
         */
        faults++;


        /*
         * ----------------------------------------------------
         * FREE FRAME AVAILABLE
         * ----------------------------------------------------
         */
        if (
            resident_count
            < frame_count
        )
        {
            frames[resident_count].page =
                page;


            frames[resident_count].valid =
                1;


            resident_count++;


            continue;
        }


        /*
         * ----------------------------------------------------
         * MEMORY FULL
         *
         * AI chooses the victim.
         * ----------------------------------------------------
         */
        int victim =
            choose_ai_victim(
                &model,
                trace,
                position,
                frames,
                frame_count
            );
/*
 * ----------------------------------------------------
 * EVALUATION: AI vs OPT
 * ----------------------------------------------------
 *
 * OPT is calculated only for evaluation.
 * The actual replacement decision remains AI-based.
 */
int optimal_victim =
    find_optimal_victim(
        trace,
        trace_length,
        position,
        frames,
        frame_count
    );

ai_opt_total++;

if (victim == optimal_victim)
{
    ai_opt_correct++;
}

        /*
         * Replace selected page.
         */
        frames[victim].page =
            page;


        frames[victim].valid =
            1;
    }


    /*
     * --------------------------------------------------------
     * Calculate hit rate
     * --------------------------------------------------------
     */
    double hit_rate =
        trace_length > 0
            ? (double)hits
              / trace_length
            : 0.0;
double ai_opt_agreement =
    ai_opt_total > 0
        ? (double)ai_opt_correct / ai_opt_total
        : 0.0;

    /*
     * --------------------------------------------------------
     * Final results
     * --------------------------------------------------------
     */
    printf(
        "\n"
        "============================================================\n"
        "                    AIPage v2.1\n"
        "============================================================\n"
        "Trace       : %s\n"
        "Frames      : %d\n"
        "Accesses    : %d\n"
        "Hits        : %ld\n"
        "Page Faults : %ld\n"
        "Hit Rate    : %.4f\n"
"AI vs OPT decisions : %ld / %ld\n"
"AI vs OPT agreement : %.4f\n"
        "============================================================\n",
        trace_file,
        frame_count,
        trace_length,
        hits,
        faults,
        hit_rate,
 ai_opt_correct,
    ai_opt_total,
ai_opt_agreement
    );


    /*
     * --------------------------------------------------------
     * Cleanup
     * --------------------------------------------------------
     */
    free(trace);


    return 0;
}
