#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_LINE 1024
#define MAX_SAMPLES 250000

#define EPOCHS 1200
#define LEARNING_RATE 0.02

#define NUM_FEATURES 7

typedef struct
{
    double x[NUM_FEATURES];
    int y;
} Sample;

typedef struct
{
    double mean[NUM_FEATURES];
    double std[NUM_FEATURES];

    double weight[NUM_FEATURES];
    double bias;
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
        return 1.0 / (1.0 + e);
    }

    double e = exp(z);
    return e / (1.0 + e);
}


/*
 * ============================================================
 * SHUFFLE
 * ============================================================
 */
void shuffle_samples(
    Sample data[],
    int count)
{
    for (int i = count - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);

        Sample temp =
            data[i];

        data[i] =
            data[j];

        data[j] =
            temp;
    }
}


/*
 * ============================================================
 * LOAD DATASET
 * ============================================================
 */
int load_dataset(
    const char *filename,
    Sample data[])
{
    FILE *file =
        fopen(filename, "r");

    if (file == NULL)
    {
        perror("Could not open training_data_v21.csv");
        return 0;
    }

    char line[MAX_LINE];

    /*
     * Skip header.
     */
    if (fgets(
            line,
            sizeof(line),
            file) == NULL)
    {
        fclose(file);
        return 0;
    }

    int count = 0;

    while (
        count < MAX_SAMPLES &&
        fgets(
            line,
            sizeof(line),
            file) != NULL)
    {
        char workload[32];

        int frames;
        int position;
        int page;

        int frequency_10;
        int frequency_50;
        int frequency_100;

        int recency;
        int last_reuse_distance;

        double avg_reuse_distance;

        int reuse_count;
        int target;

        int fields =
            sscanf(
                line,
                "%31[^,],%d,%d,%d,"
                "%d,%d,%d,"
                "%d,%d,%lf,%d,%d",

                workload,

                &frames,
                &position,
                &page,

                &frequency_10,
                &frequency_50,
                &frequency_100,

                &recency,
                &last_reuse_distance,

                &avg_reuse_distance,

                &reuse_count,
                &target
            );

        if (fields != 12)
            continue;

        /*
         * Seven runtime features.
         *
         * NOTE:
         * workload, page and position are intentionally
         * NOT used by the model.
         */
        data[count].x[0] =
            frequency_10;

        data[count].x[1] =
            frequency_50;

        data[count].x[2] =
            frequency_100;

        data[count].x[3] =
            recency;

        data[count].x[4] =
            last_reuse_distance;

        data[count].x[5] =
            avg_reuse_distance;

        data[count].x[6] =
            reuse_count;

        data[count].y =
            target;

        count++;
    }

    fclose(file);

    return count;
}


/*
 * ============================================================
 * STANDARDIZATION
 * ============================================================
 */
void calculate_normalization(
    Sample data[],
    int start,
    int end,
    Model *model)
{
    int count =
        end - start;

    for (int f = 0;
         f < NUM_FEATURES;
         f++)
    {
        model->mean[f] =
            0.0;
    }

    for (int i = start;
         i < end;
         i++)
    {
        for (int f = 0;
             f < NUM_FEATURES;
             f++)
        {
            model->mean[f] +=
                data[i].x[f];
        }
    }

    for (int f = 0;
         f < NUM_FEATURES;
         f++)
    {
        model->mean[f] /=
            count;
    }

    for (int f = 0;
         f < NUM_FEATURES;
         f++)
    {
        model->std[f] =
            0.0;
    }

    for (int i = start;
         i < end;
         i++)
    {
        for (int f = 0;
             f < NUM_FEATURES;
             f++)
        {
            double d =
                data[i].x[f]
                - model->mean[f];

            model->std[f] +=
                d * d;
        }
    }

    for (int f = 0;
         f < NUM_FEATURES;
         f++)
    {
        model->std[f] =
            sqrt(
                model->std[f]
                / count
            );

        if (model->std[f] == 0.0)
            model->std[f] = 1.0;
    }
}


/*
 * ============================================================
 * TRAIN
 * ============================================================
 */
void train_model(
    Sample data[],
    int train_count,
    Model *model)
{
    /*
     * Initialize.
     */
    model->bias = 0.0;

    for (int f = 0;
         f < NUM_FEATURES;
         f++)
    {
        model->weight[f] =
            0.0;
    }

    /*
     * Class balancing.
     */
    long positives = 0;

    for (int i = 0;
         i < train_count;
         i++)
    {
        if (data[i].y == 1)
            positives++;
    }

    long negatives =
        train_count - positives;

    double positive_weight =
        (double)train_count /
        (2.0 * positives);

    double negative_weight =
        (double)train_count /
        (2.0 * negatives);

    printf(
        "\nClass balance:\n"
        "  Positive: %ld\n"
        "  Negative: %ld\n"
        "  Positive weight: %.4f\n"
        "  Negative weight: %.4f\n\n",
        positives,
        negatives,
        positive_weight,
        negative_weight
    );

    for (int epoch = 0;
         epoch < EPOCHS;
         epoch++)
    {
        double gradient_bias =
            0.0;

        double gradient[NUM_FEATURES] = {
            0
        };

        double loss = 0.0;

        for (int i = 0;
             i < train_count;
             i++)
        {
            double x[NUM_FEATURES];

            for (int f = 0;
                 f < NUM_FEATURES;
                 f++)
            {
                x[f] =
                    (data[i].x[f]
                     - model->mean[f])
                    / model->std[f];
            }

            double z =
                model->bias;

            for (int f = 0;
                 f < NUM_FEATURES;
                 f++)
            {
                z +=
                    model->weight[f]
                    * x[f];
            }

            double prediction =
                sigmoid(z);

            double sample_weight =
                data[i].y == 1
                    ? positive_weight
                    : negative_weight;

            double error =
                prediction
                - data[i].y;

            gradient_bias +=
                sample_weight
                * error;

            for (int f = 0;
                 f < NUM_FEATURES;
                 f++)
            {
                gradient[f] +=
                    sample_weight
                    * error
                    * x[f];
            }

            double p =
                prediction;

            if (p < 1e-15)
                p = 1e-15;

            if (p > 1.0 - 1e-15)
                p = 1.0 - 1e-15;

            loss +=
                -sample_weight *
                (
                    data[i].y * log(p)
                    +
                    (1 - data[i].y)
                    * log(1.0 - p)
                );
        }

        gradient_bias /=
            train_count;

        model->bias -=
            LEARNING_RATE
            * gradient_bias;

        for (int f = 0;
             f < NUM_FEATURES;
             f++)
        {
            gradient[f] /=
                train_count;

            model->weight[f] -=
                LEARNING_RATE
                * gradient[f];
        }

        if (
            epoch == 0 ||
            (epoch + 1) % 100 == 0)
        {
            printf(
                "Epoch %4d | Loss %.6f\n",
                epoch + 1,
                loss / train_count
            );
        }
    }
}


/*
 * ============================================================
 * PREDICT
 * ============================================================
 */
double predict(
    const Model *model,
    const Sample *sample)
{
    double z =
        model->bias;

    for (int f = 0;
         f < NUM_FEATURES;
         f++)
    {
        double x =
            (sample->x[f]
             - model->mean[f])
            / model->std[f];

        z +=
            model->weight[f]
            * x;
    }

    return sigmoid(z);
}


/*
 * ============================================================
 * EVALUATION
 * ============================================================
 */
void evaluate(
    Sample data[],
    int start,
    int end,
    const Model *model)
{
    long tp = 0;
    long tn = 0;
    long fp = 0;
    long fn = 0;

    for (int i = start;
         i < end;
         i++)
    {
        double p =
            predict(
                model,
                &data[i]
            );

        int predicted =
            p >= 0.5 ? 1 : 0;

        if (
            predicted == 1 &&
            data[i].y == 1)
        {
            tp++;
        }
        else if (
            predicted == 0 &&
            data[i].y == 0)
        {
            tn++;
        }
        else if (
            predicted == 1 &&
            data[i].y == 0)
        {
            fp++;
        }
        else
        {
            fn++;
        }
    }

    long total =
        tp + tn + fp + fn;

    double accuracy =
        (double)(tp + tn)
        / total;

    double precision =
        tp + fp > 0
            ? (double)tp
              / (tp + fp)
            : 0.0;

    double recall =
        tp + fn > 0
            ? (double)tp
              / (tp + fn)
            : 0.0;

    double f1 =
        precision + recall > 0
            ? 2.0
              * precision
              * recall
              / (precision + recall)
            : 0.0;

    printf(
        "\n============================================================\n"
        "TEST RESULTS\n"
        "============================================================\n"
        "TP        : %ld\n"
        "TN        : %ld\n"
        "FP        : %ld\n"
        "FN        : %ld\n"
        "Accuracy  : %.4f\n"
        "Precision : %.4f\n"
        "Recall    : %.4f\n"
        "F1        : %.4f\n"
        "============================================================\n",
        tp,
        tn,
        fp,
        fn,
        accuracy,
        precision,
        recall,
        f1
    );
}


/*
 * ============================================================
 * SAVE MODEL
 * ============================================================
 */
void save_model(
    const char *filename,
    const Model *model)
{
    FILE *file =
        fopen(filename, "w");

    if (file == NULL)
    {
        perror("Could not save model");
        return;
    }

    fprintf(
        file,
        "BIAS %.12f\n",
        model->bias
    );

    fprintf(
        file,
        "FREQUENCY_10_WEIGHT %.12f\n",
        model->weight[0]
    );

    fprintf(
        file,
        "FREQUENCY_50_WEIGHT %.12f\n",
        model->weight[1]
    );

    fprintf(
        file,
        "FREQUENCY_100_WEIGHT %.12f\n",
        model->weight[2]
    );

    fprintf(
        file,
        "RECENCY_WEIGHT %.12f\n",
        model->weight[3]
    );

    fprintf(
        file,
        "LAST_REUSE_DISTANCE_WEIGHT %.12f\n",
        model->weight[4]
    );

    fprintf(
        file,
        "AVG_REUSE_DISTANCE_WEIGHT %.12f\n",
        model->weight[5]
    );

    fprintf(
        file,
        "REUSE_COUNT_WEIGHT %.12f\n",
        model->weight[6]
    );

    fprintf(
        file,
        "FREQUENCY_10_MEAN %.12f\n",
        model->mean[0]
    );

    fprintf(
        file,
        "FREQUENCY_10_STD %.12f\n",
        model->std[0]
    );

    fprintf(
        file,
        "FREQUENCY_50_MEAN %.12f\n",
        model->mean[1]
    );

    fprintf(
        file,
        "FREQUENCY_50_STD %.12f\n",
        model->std[1]
    );

    fprintf(
        file,
        "FREQUENCY_100_MEAN %.12f\n",
        model->mean[2]
    );

    fprintf(
        file,
        "FREQUENCY_100_STD %.12f\n",
        model->std[2]
    );

    fprintf(
        file,
        "RECENCY_MEAN %.12f\n",
        model->mean[3]
    );

    fprintf(
        file,
        "RECENCY_STD %.12f\n",
        model->std[3]
    );

    fprintf(
        file,
        "LAST_REUSE_DISTANCE_MEAN %.12f\n",
        model->mean[4]
    );

    fprintf(
        file,
        "LAST_REUSE_DISTANCE_STD %.12f\n",
        model->std[4]
    );

    fprintf(
        file,
        "AVG_REUSE_DISTANCE_MEAN %.12f\n",
        model->mean[5]
    );

    fprintf(
        file,
        "AVG_REUSE_DISTANCE_STD %.12f\n",
        model->std[5]
    );

    fprintf(
        file,
        "REUSE_COUNT_MEAN %.12f\n",
        model->mean[6]
    );

    fprintf(
        file,
        "REUSE_COUNT_STD %.12f\n",
        model->std[6]
    );

    fclose(file);

    printf(
        "\nModel saved to %s\n",
        filename
    );
}


/*
 * ============================================================
 * MAIN
 * ============================================================
 */
int main(void)
{
    printf(
        "============================================================\n"
        "              AIPage v2.1 Trainer\n"
        "============================================================\n"
    );

    Sample *data =
        malloc(
            sizeof(Sample)
            * MAX_SAMPLES
        );

    if (data == NULL)
    {
        perror("Memory allocation failed");
        return 1;
    }

    int count =
        load_dataset(
            "training_data_v21.csv",
            data
        );

    if (count <= 0)
    {
        free(data);
        return 1;
    }

    printf(
        "\nLoaded %d training examples.\n",
        count
    );

    /*
     * Reproducible shuffle.
     */
    srand(42);

    shuffle_samples(
        data,
        count
    );

    int train_count =
        (int)(count * 0.80);

    int test_count =
        count - train_count;

    printf(
        "Training examples : %d\n"
        "Testing examples  : %d\n",
        train_count,
        test_count
    );

    Model model;

    calculate_normalization(
        data,
        0,
        train_count,
        &model
    );

    train_model(
        data,
        train_count,
        &model
    );

    evaluate(
        data,
        train_count,
        count,
        &model
    );

    save_model(
        "model_v21.txt",
        &model
    );

    printf(
        "\n============================================================\n"
        "Learned weights\n"
        "============================================================\n"
    );

    printf(
        "Frequency 10       : %.6f\n",
        model.weight[0]
    );

    printf(
        "Frequency 50       : %.6f\n",
        model.weight[1]
    );

    printf(
        "Frequency 100      : %.6f\n",
        model.weight[2]
    );

    printf(
        "Recency            : %.6f\n",
        model.weight[3]
    );

    printf(
        "Last reuse distance: %.6f\n",
        model.weight[4]
    );

    printf(
        "Average reuse dist.: %.6f\n",
        model.weight[5]
    );

    printf(
        "Reuse count        : %.6f\n",
        model.weight[6]
    );

    free(data);

    return 0;
}
