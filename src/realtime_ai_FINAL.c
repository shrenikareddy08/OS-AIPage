#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>
#include <time.h>
#include <signal.h>

#define MAX_RECENT 32
#define FRAME_COUNT 4
#define PAGE_TABLE_SIZE 8192
#define TRANSITION_TABLE_SIZE 16384

typedef struct
{
    uint64_t page;
    unsigned long frequency;
    unsigned long last_seen;
    unsigned long total_reuse_distance;
    unsigned long reuse_count;
    int used;
} PageStats;

typedef struct
{
    uint64_t page;
    int valid;
} Frame;

typedef struct
{
    uint64_t from;
    uint64_t to;
    unsigned long count;
    int used;
} Transition;

static PageStats page_table[PAGE_TABLE_SIZE];
static Transition transitions[TRANSITION_TABLE_SIZE];
static Frame frames[FRAME_COUNT];

static uint64_t recent[MAX_RECENT];
static int recent_count = 0;

static uint64_t previous_page = 0;
static int have_previous_page = 0;

static uint64_t previous_prediction = 0;
static int have_previous_prediction = 0;

static unsigned long access_count = 0;
static unsigned long hit_count = 0;
static unsigned long fault_count = 0;
static unsigned long replacement_count = 0;

static unsigned long next_prediction_count = 0;
static unsigned long next_prediction_correct = 0;

/* Last completed next-page prediction evaluation for the dashboard. */
static int latest_prediction_result_valid = 0;
static int latest_prediction_correct = 0;
static uint64_t latest_predicted_page = 0;
static uint64_t latest_actual_page = 0;

/* Latest access decision for live_state.json */
static int latest_hit = 0;
static int latest_victim_valid = 0;
static uint64_t latest_victim_page = 0;
static char latest_decision[64] = "WAITING";
static char latest_reason[256] = "No access processed yet";

/* --------------------------------------------------------- */
/* LIVE STATE JSON                                            */
/* --------------------------------------------------------- */

static volatile sig_atomic_t aipage_running = 1;

static uint64_t predict_next(int *valid);
static double prediction_confidence(uint64_t predicted, int valid);

static void write_live_state(void)
{
    FILE *f = fopen("results/live_state.json", "w");

    if (!f)
        return;

    double hit_rate = 0.0;
    double fault_rate = 0.0;
    double prediction_accuracy = 0.0;

    if (access_count > 0)
    {
        hit_rate =
            ((double)hit_count /
             (double)access_count) * 100.0;

        fault_rate =
            ((double)fault_count /
             (double)access_count) * 100.0;
    }

    if (next_prediction_count > 0)
    {
        prediction_accuracy =
            ((double)next_prediction_correct /
             (double)next_prediction_count) * 100.0;
    }

    time_t now = time(NULL);

    fprintf(f, "{\n");

    fprintf(f, "  \"running\": %s,\n",
            aipage_running ? "true" : "false");

    fprintf(f, "  \"access_count\": %lu,\n",
            access_count);

    fprintf(f, "  \"current_page\": ");
    if (have_previous_page)
        fprintf(f, "%" PRIu64, previous_page);
    else
        fprintf(f, "null");
    fprintf(f, ",\n");

    int prediction_valid = 0;
    uint64_t prediction =
        predict_next(&prediction_valid);

    fprintf(f, "  \"predicted_page\": ");
    if (prediction_valid)
        fprintf(f, "%" PRIu64, prediction);
    else
        fprintf(f, "null");
    fprintf(f, ",\n");

    fprintf(f, "  \"confidence\": %.2f,\n",
            prediction_confidence(
                prediction,
                prediction_valid));

    fprintf(f, "  \"recent_pages\": [");

    int count =
        recent_count < 8
            ? recent_count
            : 8;

    for (int i = count - 1; i >= 0; i--)
    {
        fprintf(f, "%" PRIu64, recent[i]);

        if (i > 0)
            fprintf(f, ", ");
    }

    fprintf(f, "],\n");

    fprintf(f, "  \"resident_frames\": [");

    int first = 1;

    for (int i = 0; i < FRAME_COUNT; i++)
    {
        if (frames[i].valid)
        {
            if (!first)
                fprintf(f, ", ");

            fprintf(f, "%" PRIu64,
                    frames[i].page);

            first = 0;
        }
    }

    fprintf(f, "],\n");

    fprintf(f, "  \"hit_fault\": ");
    if (access_count > 0)
        fprintf(f, "\"%s\"", latest_hit ? "HIT" : "FAULT");
    else
        fprintf(f, "null");
    fprintf(f, ",\n");

    fprintf(f, "  \"victim_page\": ");
    if (latest_victim_valid)
        fprintf(f, "%" PRIu64, latest_victim_page);
    else
        fprintf(f, "null");
    fprintf(f, ",\n");

    fprintf(f, "  \"decision\": \"%s\",\n",
            latest_decision);

    fprintf(f, "  \"reason\": \"%s\",\n",
            latest_reason);

    fprintf(f, "  \"page_hits\": %lu,\n",
            hit_count);

    fprintf(f, "  \"page_faults\": %lu,\n",
            fault_count);

    fprintf(f, "  \"replacements\": %lu,\n",
            replacement_count);

    fprintf(f, "  \"prediction_count\": %lu,\n",
            next_prediction_count);

    fprintf(f, "  \"prediction_correct\": %lu,\n",
            next_prediction_correct);

    fprintf(f, "  \"hit_rate\": %.2f,\n",
            hit_rate);

    fprintf(f, "  \"fault_rate\": %.2f,\n",
            fault_rate);

    fprintf(f, "  \"prediction_accuracy\": %.2f,\n",
            prediction_accuracy);

    fprintf(f, "  \"last_prediction_result\": ");
    if (latest_prediction_result_valid)
    {
        fprintf(f, "{\"predicted\": %" PRIu64 ", \"actual\": %" PRIu64 ", \"correct\": %s}",
                latest_predicted_page,
                latest_actual_page,
                latest_prediction_correct ? "true" : "false");
    }
    else
    {
        fprintf(f, "null");
    }
    fprintf(f, ",\n");

    fprintf(f, "  \"timestamp\": %ld\n",
            (long)now);

    fprintf(f, "}\n");

    fclose(f);
}

static void handle_aipage_signal(int sig)
{
    (void)sig;

    aipage_running = 0;
    write_live_state();

    _exit(0);
}


/* --------------------------------------------------------- */
/* HASH FUNCTIONS                                             */
/* --------------------------------------------------------- */

static uint64_t hash_page(uint64_t page)
{
    page ^= page >> 33;
    page *= UINT64_C(0xff51afd7ed558ccd);
    page ^= page >> 33;
    page *= UINT64_C(0xc4ceb9fe1a85ec53);
    page ^= page >> 33;

    return page;
}

static size_t page_index(uint64_t page)
{
    return (size_t)(hash_page(page) % PAGE_TABLE_SIZE);
}

static size_t transition_index(uint64_t from, uint64_t to)
{
    uint64_t h = hash_page(from);

    h ^= hash_page(to) + UINT64_C(0x9e3779b97f4a7c15)
         + (h << 6) + (h >> 2);

    return (size_t)(h % TRANSITION_TABLE_SIZE);
}

/* --------------------------------------------------------- */
/* PAGE TABLE                                                 */
/* --------------------------------------------------------- */

static PageStats *get_page_stats(uint64_t page, int create)
{
    size_t start = page_index(page);

    for (size_t i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        size_t index = (start + i) % PAGE_TABLE_SIZE;

        if (!page_table[index].used)
        {
            if (!create)
                return NULL;

            page_table[index].used = 1;
            page_table[index].page = page;
            page_table[index].frequency = 0;
            page_table[index].last_seen = 0;
            page_table[index].total_reuse_distance = 0;
            page_table[index].reuse_count = 0;

            return &page_table[index];
        }

        if (page_table[index].page == page)
            return &page_table[index];
    }

    return NULL;
}

/* --------------------------------------------------------- */
/* TRANSITION TABLE                                           */
/* --------------------------------------------------------- */

static Transition *get_transition(uint64_t from,
                                  uint64_t to,
                                  int create)
{
    size_t start = transition_index(from, to);

    for (size_t i = 0; i < TRANSITION_TABLE_SIZE; i++)
    {
        size_t index =
            (start + i) % TRANSITION_TABLE_SIZE;

        if (!transitions[index].used)
        {
            if (!create)
                return NULL;

            transitions[index].used = 1;
            transitions[index].from = from;
            transitions[index].to = to;
            transitions[index].count = 0;

            return &transitions[index];
        }

        if (transitions[index].from == from &&
            transitions[index].to == to)
        {
            return &transitions[index];
        }
    }

    return NULL;
}

static unsigned long transition_count(uint64_t from,
                                      uint64_t to)
{
    Transition *t =
        get_transition(from, to, 0);

    if (!t)
        return 0;

    return t->count;
}

static unsigned long transition_total(uint64_t from)
{
    unsigned long total = 0;

    for (size_t i = 0; i < TRANSITION_TABLE_SIZE; i++)
    {
        if (transitions[i].used &&
            transitions[i].from == from)
        {
            total += transitions[i].count;
        }
    }

    return total;
}

/* --------------------------------------------------------- */
/* RECENT HISTORY                                             */
/* --------------------------------------------------------- */

static void add_recent(uint64_t page)
{
    int limit =
        recent_count < MAX_RECENT
            ? recent_count
            : MAX_RECENT - 1;

    for (int i = limit; i > 0; i--)
        recent[i] = recent[i - 1];

    recent[0] = page;

    if (recent_count < MAX_RECENT)
        recent_count++;
}

/* --------------------------------------------------------- */
/* FRAME MANAGEMENT                                           */
/* --------------------------------------------------------- */

static int frame_contains(uint64_t page)
{
    for (int i = 0; i < FRAME_COUNT; i++)
    {
        if (frames[i].valid &&
            frames[i].page == page)
        {
            return i;
        }
    }

    return -1;
}

static int find_free_frame(void)
{
    for (int i = 0; i < FRAME_COUNT; i++)
    {
        if (!frames[i].valid)
            return i;
    }

    return -1;
}

/* --------------------------------------------------------- */
/* PAGE SCORE                                                 */
/* --------------------------------------------------------- */

static double page_score(uint64_t page)
{
    PageStats *s =
        get_page_stats(page, 0);

    if (!s || s->frequency == 0)
        return 0.0;

    double frequency =
        log(1.0 + (double)s->frequency);

    unsigned long age =
        access_count - s->last_seen;

    double recency =
        1.0 / (1.0 + (double)age);

    double reuse_score = 0.0;

    if (s->reuse_count > 0)
    {
        double average_distance =
            (double)s->total_reuse_distance /
            (double)s->reuse_count;

        reuse_score =
            1.0 / (1.0 + average_distance);
    }

    unsigned long recent_hits = 0;

    int limit =
        recent_count < MAX_RECENT
            ? recent_count
            : MAX_RECENT;

    for (int i = 0; i < limit; i++)
    {
        if (recent[i] == page)
            recent_hits++;
    }

    double recent_frequency =
        (double)recent_hits /
        (double)(limit > 0 ? limit : 1);

    double transition_score = 0.0;

    if (have_previous_page)
    {
        unsigned long total =
            transition_total(previous_page);

        unsigned long count =
            transition_count(previous_page, page);

        if (total > 0)
        {
            transition_score =
                (double)count /
                (double)total;
        }
    }

    return
        frequency * 0.20 +
        recency * 0.20 +
        transition_score * 0.40 +
        reuse_score * 0.15 +
        recent_frequency * 0.05;
}

/* --------------------------------------------------------- */
/* NEXT PAGE PREDICTION                                       */
/* --------------------------------------------------------- */

static uint64_t predict_next(int *valid)
{
    *valid = 0;

    if (!have_previous_page)
        return 0;

    uint64_t current = previous_page;

    /* ----------------------------------------------------- */
    /* 1. STRONG SEQUENTIAL / STRIDE PATTERN                 */
    /* ----------------------------------------------------- */

    /*
     * recent[] is stored newest-first:
     *
     * recent[0] = newest page
     * recent[1] = previous page
     * recent[2] = older page
     * recent[3] = older page
     *
     * Therefore the actual sequence is:
     *
     * recent[3] -> recent[2] -> recent[1] -> recent[0]
     */

    if (recent_count >= 4)
    {
        int64_t d1 =
            (int64_t)recent[2] -
            (int64_t)recent[3];

        int64_t d2 =
            (int64_t)recent[1] -
            (int64_t)recent[2];

        int64_t d3 =
            (int64_t)recent[0] -
            (int64_t)recent[1];

        if (d1 == d2 &&
            d2 == d3 &&
            d1 != 0)
        {
            int64_t next =
                (int64_t)current + d1;

            if (next >= 0)
            {
                *valid = 1;
                return (uint64_t)next;
            }
        }
    }

    /* ----------------------------------------------------- */
    /* 2. LEARNED TRANSITION                                 */
    /* ----------------------------------------------------- */

    uint64_t candidate = 0;

    unsigned long best_count = 0;
    unsigned long total = 0;

    for (size_t i = 0;
         i < TRANSITION_TABLE_SIZE;
         i++)
    {
        if (!transitions[i].used)
            continue;

        if (transitions[i].from != current)
            continue;

        total += transitions[i].count;

        if (transitions[i].count > best_count)
        {
            best_count =
                transitions[i].count;

            candidate =
                transitions[i].to;
        }
    }

    /*
     * A transition observed repeatedly is strong evidence.
     */
    if (best_count >= 2 && total > 0)
    {
        double probability =
            (double)best_count /
            (double)total;

        if (probability >= 0.35 ||
            best_count >= 3)
        {
            *valid = 1;
            return candidate;
        }
    }

    /*
     * A transition observed once is still useful,
     * but is treated as weak evidence.
     *
     * This allows the real-time engine to begin
     * predicting earlier instead of remaining in
     * "learning..." for a long time.
     */
    if (best_count == 1)
    {
        *valid = 1;
        return candidate;
    }

    /* ----------------------------------------------------- */
    /* 3. NO RELIABLE PREDICTION                             */
    /* ----------------------------------------------------- */

    return 0;
}

/* --------------------------------------------------------- */
/* CONFIDENCE                                                */
/* --------------------------------------------------------- */

static double prediction_confidence(uint64_t predicted,
                                    int valid)
{
    if (!valid)
        return 0.0;

    if (!have_previous_page)
        return 0.0;

    unsigned long total =
        transition_total(previous_page);

    unsigned long best =
        transition_count(previous_page,
                         predicted);

    if (total > 0 && best > 0)
    {
        double confidence =
            ((double)best /
             (double)total) * 100.0;

        if (confidence > 99.0)
            confidence = 99.0;

        return confidence;
    }

    return 50.0;
}

/* --------------------------------------------------------- */
/* AI VICTIM SELECTION                                       */
/* --------------------------------------------------------- */

static int choose_victim(void)
{
    int victim = -1;
    double lowest_score = 1e100;

    /*
     * Use the prediction made BEFORE the current access.
     * Do not call predict_next() again here because that
     * would generate a prediction for the page AFTER the
     * current access.
     */
    int prediction_valid =
        have_previous_prediction;

    uint64_t predicted =
        previous_prediction;

    fprintf(stderr,
            "[PREDICT DEBUG] current=%" PRIu64
            " valid=%d predicted=%" PRIu64 "\n",
            previous_page,
            prediction_valid,
            predicted);

    for (int i = 0; i < FRAME_COUNT; i++)
    {
        if (!frames[i].valid)
            continue;

        uint64_t page =
            frames[i].page;

        PageStats *s =
            get_page_stats(page, 0);

        if (!s)
            continue;

        double score = 0.0;

        /* Frequency */
        double frequency =
            log(1.0 + (double)s->frequency);

        score += frequency * 0.25;

        /* Recency */
        unsigned long age =
            access_count - s->last_seen;

        double recency =
            1.0 / (1.0 + (double)age);

        score += recency * 0.30;

        /* Reuse behavior */
        if (s->reuse_count > 0)
        {
            double average_distance =
                (double)s->total_reuse_distance /
                (double)s->reuse_count;

            double reuse_score =
                1.0 / (1.0 + average_distance);

            score += reuse_score * 0.20;
        }

        /* Recent-window frequency */
        unsigned long recent_hits = 0;

        int limit =
            recent_count < MAX_RECENT
                ? recent_count
                : MAX_RECENT;

        for (int j = 0; j < limit; j++)
        {
            if (recent[j] == page)
                recent_hits++;
        }

        double recent_frequency =
            (double)recent_hits /
            (double)(limit > 0 ? limit : 1);

        score += recent_frequency * 0.15;

        /* Transition usefulness */
        if (have_previous_page)
        {
            unsigned long total =
                transition_total(previous_page);

            unsigned long count =
                transition_count(previous_page, page);

            if (total > 0 && count > 0)
            {
                double transition_score =
                    (double)count /
                    (double)total;

                score += transition_score * 0.10;
            }
        }

        /*
         * Strong prediction protection.
         *
         * A page predicted to be accessed next should be
         * very unlikely to be selected as the victim.
         */
        if (prediction_valid &&
            page == predicted)
        {
            score += 100.0;
        }

        /* Small tie-breaker */
        if (s->reuse_count == 0 &&
            s->frequency > 0)
        {
            score -= 0.05;
        }

        fprintf(stderr,
                "[AI SCORE] page=%" PRIu64
                " freq=%lu age=%lu reuse=%lu score=%.4f"
                " predicted=%s\n",
                page,
                s->frequency,
                age,
                s->reuse_count,
                score,
                (prediction_valid && page == predicted)
                    ? "YES"
                    : "NO");

        if (score < lowest_score)
        {
            lowest_score = score;
            victim = i;
        }
    }

    return victim;
}

/* --------------------------------------------------------- */
/* PRINTING                                                   */
/* --------------------------------------------------------- */

static void print_recent(void)
{
    printf("Recent Pages     : ");

    int count =
        recent_count < 8
            ? recent_count
            : 8;

    for (int i = count - 1;
         i >= 0;
         i--)
    {
        printf("%" PRIu64 " ", recent[i]);
    }

    printf("\n");
}

static void print_frames(void)
{
    printf("Resident Frames  : [ ");

    for (int i = 0; i < FRAME_COUNT; i++)
    {
        if (frames[i].valid)
            printf("%" PRIu64 " ",
                   frames[i].page);
        else
            printf("- ");
    }

    printf("]\n");
}

/* --------------------------------------------------------- */
/* PROCESS REAL MEMORY ACCESS                                 */
/* --------------------------------------------------------- */


static void process_access(uint64_t real_page)
{
    PageStats *stats =
        get_page_stats(real_page, 1);

    if (!stats)
    {
        fprintf(stderr,
                "AIPage: dynamic page table full.\n");
        return;
    }

    access_count++;

    /* Evaluate prediction made for this access. */
    if (have_previous_prediction)
    {
        next_prediction_count++;

        latest_prediction_result_valid = 1;
        latest_predicted_page = previous_prediction;
        latest_actual_page = real_page;
        latest_prediction_correct =
            (previous_prediction == real_page);

        if (latest_prediction_correct)
            next_prediction_correct++;
    }
    else
    {
        latest_prediction_result_valid = 0;
    }

    int frame_index =
        frame_contains(real_page);

    int hit =
        frame_index >= 0;

    if (hit)
        hit_count++;
    else
        fault_count++;

    int victim = -1;
    uint64_t victim_page = 0;

    if (hit)
    {
        /* Page already resident. */
    }
    else
    {
        int free_frame =
            find_free_frame();

        if (free_frame >= 0)
        {
            frames[free_frame].valid = 1;
            frames[free_frame].page = real_page;
        }
        else
        {
            int victim_frame =
                choose_victim();

            if (victim_frame < 0)
                return;

            victim = victim_frame;

            victim_page = frames[victim_frame].page;

            frames[victim_frame].page =
                real_page;

            replacement_count++;
        }
    }

    /* Learn transition from previous real page. */
    if (have_previous_page &&
        previous_page != real_page)
    {
        Transition *t =
            get_transition(previous_page,
                           real_page,
                           1);

        if (t)
            t->count++;
    }

    /* Learn reuse distance. */
    if (stats->frequency > 0)
    {
        unsigned long distance =
            access_count -
            stats->last_seen;

        stats->total_reuse_distance +=
            distance;

        stats->reuse_count++;
    }

    stats->frequency++;
    stats->last_seen =
        access_count;

    previous_page =
        real_page;

    have_previous_page = 1;

    add_recent(real_page);

    /* Predict the NEXT actual real page. */
    int prediction_valid = 0;

    uint64_t prediction =
        predict_next(&prediction_valid);

    double confidence =
        prediction_confidence(prediction,
                               prediction_valid);

    if (prediction_valid)
    {
        previous_prediction =
            prediction;

        have_previous_prediction = 1;
    }
    else
    {
        have_previous_prediction = 0;
    }

    printf("\n");
    printf("============================================================\n");
    printf("AIPage REAL MEMORY DECISION | Access #%lu\n",
           access_count);
    printf("============================================================\n");

    printf("Actual Real Page : %" PRIu64 "\n",
           real_page);

    if (prediction_valid)
    {
        printf("Next Prediction  : %" PRIu64 "\n",
               prediction);

        printf("Confidence       : %.1f%%\n",
               confidence);
    }
    else
    {
        printf("Next Prediction  : learning...\n");
        printf("Confidence       : 0.0%%\n");
    }

    print_recent();
    print_frames();

    printf("Hit/Fault        : %s\n",
           hit ? "HIT" : "FAULT");

    if (victim != -1)
    {
        printf("Victim Real Page : %" PRIu64 "\n",
               victim_page);
    }
    else
    {
        printf("Victim Real Page : NONE\n");
    }

    if (hit)
    {
        printf("Decision         : KEEP\n");
        printf("Reason           : Real page already resident\n");

        latest_hit = 1;
        latest_victim_valid = 0;
        latest_victim_page = 0;

        snprintf(latest_decision,
                 sizeof(latest_decision),
                 "KEEP");

        snprintf(latest_reason,
                 sizeof(latest_reason),
                 "Real page already resident");
    }
    else if (victim != -1)
    {
        printf("Decision         : REPLACE %" PRIu64
               " -> %" PRIu64 "\n",
               victim_page,
               real_page);

        printf("Reason           : AI selected lowest-future-usefulness page\n");

        latest_hit = 0;
        latest_victim_valid = 1;
        latest_victim_page = victim_page;

        snprintf(latest_decision,
                 sizeof(latest_decision),
                 "REPLACE");

        snprintf(latest_reason,
                 sizeof(latest_reason),
                 "AI selected lowest-future-usefulness page");
    }
    else
    {
        printf("Decision         : LOAD %" PRIu64 "\n",
               real_page);

        printf("Reason           : Free resident frame available\n");

        latest_hit = 0;
        latest_victim_valid = 0;
        latest_victim_page = 0;

        snprintf(latest_decision,
                 sizeof(latest_decision),
                 "LOAD");

        snprintf(latest_reason,
                 sizeof(latest_reason),
                 "Free resident frame available");
    }

    write_live_state();

    printf("============================================================\n");

    fflush(stdout);
}

/* --------------------------------------------------------- */
/* INPUT PARSER                                               */
/* --------------------------------------------------------- */

static int parse_memory_access(const char *line,
                               uint64_t *page)
{
    unsigned long long address = 0;
    unsigned long long parsed_page = 0;
    int pid = 0;

    /*
     * Expected:
     *
     * MEMACCESS | addr=0xf... | page=123456 | pid=1234
     */

    if (sscanf(line,
               "MEMACCESS | addr=%llx | page=%llu | pid=%d",
               &address,
               &parsed_page,
               &pid) == 3)
    {
        (void)address;
        (void)pid;

        *page =
            (uint64_t)parsed_page;

        return 1;
    }

    /*
     * Also accept:
     *
     * ACCESS | ... | page=...
     */

    unsigned long long time_ms = 0;
    unsigned long long offset = 0;
    int id = 0;

    if (sscanf(line,
               "ACCESS | id=%d | page=%llu | offset=%llu | time_ms=%llu",
               &id,
               &parsed_page,
               &offset,
               &time_ms) == 4)
    {
        (void)id;
        (void)offset;
        (void)time_ms;

        *page =
            (uint64_t)parsed_page;

        return 1;
    }

    /*
     * Finally accept a plain numeric page number.
     */
    if (sscanf(line,
               "%llu",
               &parsed_page) == 1)
    {
        *page =
            (uint64_t)parsed_page;

        return 1;
    }

    return 0;
}

/* --------------------------------------------------------- */
/* MAIN                                                       */
/* --------------------------------------------------------- */

int main(void)
{

    signal(SIGINT, handle_aipage_signal);
    signal(SIGTERM, handle_aipage_signal);

    aipage_running = 1;
    write_live_state();


    char line[512];

    printf("\n");
    printf("============================================================\n");
    printf("AIPage REAL-TIME INTELLIGENT PAGE REPLACEMENT ENGINE\n");
    printf("============================================================\n");
    printf("Frame Capacity   : %d\n",
           FRAME_COUNT);
    printf("Input            : REAL PROCESS MEMORY ACCESS STREAM\n");
    printf("Page IDs         : ACTUAL 64-BIT VIRTUAL PAGES\n");
    printf("Learning         : FREQUENCY + RECENCY + REUSE + TRANSITIONS\n");
    printf("Prediction       : NEXT REAL MEMORY PAGE\n");
    printf("Replacement      : AI-GUIDED\n");
    printf("Mode             : REAL-TIME\n");
    printf("============================================================\n");

    while (fgets(line,
                  sizeof(line),
                  stdin))
    {
        uint64_t page;

        if (parse_memory_access(line,
                                &page))
        {
            process_access(page);
        }
    }

    /* Mark the live dashboard state as stopped after normal EOF. */
    aipage_running = 0;
    write_live_state();

    printf("\n");
    printf("============================================================\n");
    printf("AIPage FINAL REAL MEMORY ANALYSIS\n");
    printf("============================================================\n");

    printf("Total Accesses        : %lu\n",
           access_count);

    printf("Page Hits             : %lu\n",
           hit_count);

    printf("Page Faults           : %lu\n",
           fault_count);

    printf("Replacements          : %lu\n",
           replacement_count);

    printf("Next-Page Predictions : %lu\n",
           next_prediction_count);

    printf("Next-Page Correct     : %lu\n",
           next_prediction_correct);

    if (access_count > 0)
    {
        double hit_rate =
            ((double)hit_count /
             (double)access_count) *
            100.0;

        double fault_rate =
            ((double)fault_count /
             (double)access_count) *
            100.0;

        printf("Hit Rate              : %.2f%%\n",
               hit_rate);

        printf("Fault Rate            : %.2f%%\n",
               fault_rate);
    }

    if (next_prediction_count > 0)
    {
        double accuracy =
            ((double)next_prediction_correct /
             (double)next_prediction_count) *
            100.0;

        printf("Next-Page Accuracy    : %.2f%%\n",
               accuracy);
    }

    printf("============================================================\n");
    printf("Real-memory AIPage demonstration complete.\n");
    printf("============================================================\n");

    return 0;
}
