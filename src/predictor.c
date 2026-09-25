#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512

/*
 * AIPage v0.4
 *
 * Simple interpretable reuse predictor.
 *
 * Input features:
 *   frequency
 *   recency
 *   average reuse distance
 *
 * Output:
 *   predicted future reuse: 0 or 1
 *
 * This is intentionally a baseline.
 * It is NOT the final AI replacement policy.
 */

int predict_reuse(int frequency,
                 int recency,
                 double avg_reuse_distance)
{
    /*
     * Higher frequency increases the probability
     * that the page will be accessed again.
     */
    double score = 0.0;

    if (frequency >= 5)
        score += 1.0;

    if (frequency >= 10)
        score += 1.0;

    /*
     * Recently accessed pages are more likely
     * to exhibit temporal locality.
     */
    if (recency <= 20)
        score += 1.0;

    if (recency <= 5)
        score += 1.0;

    /*
     * Smaller reuse distance indicates stronger
     * temporal locality.
     */
    if (avg_reuse_distance <= 20.0)
        score += 1.0;

    if (avg_reuse_distance <= 10.0)
        score += 1.0;

    /*
     * Require enough evidence before predicting reuse.
     */
    return score >= 3.0 ? 1 : 0;
}

int main(void)
{
    FILE *file = fopen("training_data.csv", "r");

    if (file == NULL)
    {
        perror("Could not open training_data.csv");
        return 1;
    }

    char line[MAX_LINE];

    /*
     * Skip CSV header.
     */
    if (fgets(line, sizeof(line), file) == NULL)
    {
        printf("Dataset is empty.\n");
        fclose(file);
        return 1;
    }

    long total = 0;
    long correct = 0;

    long true_positive = 0;
    long true_negative = 0;
    long false_positive = 0;
    long false_negative = 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char workload[32];

        int page;
        int frequency;
        int recency;
        double avg_reuse_distance;
        int actual;

        int fields = sscanf(line,
                            "%31[^,],%d,%d,%d,%lf,%d",
                            workload,
                            &page,
                            &frequency,
                            &recency,
                            &avg_reuse_distance,
                            &actual);

        if (fields != 6)
            continue;

        int prediction = predict_reuse(
            frequency,
            recency,
            avg_reuse_distance
        );

        total++;

        if (prediction == actual)
            correct++;

        if (prediction == 1 && actual == 1)
            true_positive++;
        else if (prediction == 0 && actual == 0)
            true_negative++;
        else if (prediction == 1 && actual == 0)
            false_positive++;
        else if (prediction == 0 && actual == 1)
            false_negative++;
    }

    fclose(file);

    if (total == 0)
    {
        printf("No valid examples found.\n");
        return 1;
    }

    double accuracy =
        (double)correct / total * 100.0;

    double precision = 0.0;

    if (true_positive + false_positive > 0)
    {
        precision =
            (double)true_positive /
            (true_positive + false_positive) * 100.0;
    }

    double recall = 0.0;

    if (true_positive + false_negative > 0)
    {
        recall =
            (double)true_positive /
            (true_positive + false_negative) * 100.0;
    }

    double f1 = 0.0;

    if (precision + recall > 0.0)
    {
        f1 =
            2.0 * precision * recall /
            (precision + recall);
    }

    printf("========================================\n");
    printf("       AIPage Reuse Predictor v0.4\n");
    printf("========================================\n");

    printf("Total examples : %ld\n", total);

    printf("\nConfusion Matrix\n");
    printf("----------------------------------------\n");
    printf("True Positive  : %ld\n", true_positive);
    printf("True Negative  : %ld\n", true_negative);
    printf("False Positive : %ld\n", false_positive);
    printf("False Negative : %ld\n", false_negative);

    printf("\nPrediction Metrics\n");
    printf("----------------------------------------\n");
    printf("Accuracy  : %.2f%%\n", accuracy);
    printf("Precision : %.2f%%\n", precision);
    printf("Recall    : %.2f%%\n", recall);
    printf("F1 Score  : %.2f\n", f1);

    printf("\n========================================\n");

    return 0;
}
