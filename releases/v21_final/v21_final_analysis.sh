#!/bin/bash

echo "============================================================"
echo "              AIPage v2.1 FINAL ANALYSIS"
echo "============================================================"

echo
echo "1. OVERALL AVERAGES"
echo "------------------------------------------------------------"

awk -F',' '
NR>1 {
    ai += $4
    lru += $5
    fifo += $6
}
END {
    n=NR-1

    ai_avg=ai/n
    lru_avg=lru/n
    fifo_avg=fifo/n

    printf "Configurations : %d\n", n
    printf "AIPage average : %.6f\n", ai_avg
    printf "LRU average    : %.6f\n", lru_avg
    printf "FIFO average   : %.6f\n", fifo_avg

    printf "AI vs LRU improvement : %.2f%%\n",
        ((ai_avg-lru_avg)/lru_avg)*100

    printf "AI vs FIFO improvement : %.2f%%\n",
        ((ai_avg-fifo_avg)/fifo_avg)*100
}' results/v21_three_way_comparison.csv


echo
echo "2. BEST-ALGORITHM COUNTS"
echo "------------------------------------------------------------"

awk -F',' '
NR>1 {
    if ($4>$5 && $4>$6)
        ai++
    else if ($5>$4 && $5>$6)
        lru++
    else if ($6>$4 && $6>$5)
        fifo++
    else
        tie++
}
END {
    printf "AIPage best : %d\n", ai
    printf "LRU best    : %d\n", lru
    printf "FIFO best   : %d\n", fifo
    printf "Ties        : %d\n", tie
}' results/v21_three_way_comparison.csv


echo
echo "3. WORKLOAD-WISE AVERAGES"
echo "------------------------------------------------------------"

awk -F',' '
NR==1 {next}

{
    count[$1]++
    ai[$1]+=$4
    lru[$1]+=$5
    fifo[$1]+=$6
}

END {
    for (w in count) {
        printf "%-12s AI=%.4f  LRU=%.4f  FIFO=%.4f\n",
            w,
            ai[w]/count[w],
            lru[w]/count[w],
            fifo[w]/count[w]
    }
}' results/v21_three_way_comparison.csv


echo
echo "4. STRONGEST AIPage CONFIGURATIONS"
echo "------------------------------------------------------------"

sort -t',' -k4,4nr results/v21_three_way_comparison.csv |
awk -F',' '
NR==1 {
    print
    next
}
NR<=6 {
    print
}'


echo
echo "5. OPT AGREEMENT SUMMARY"
echo "------------------------------------------------------------"

awk -F',' '
NR>1 {
    agreement += $9
}
END {
    printf "Average OPT agreement : %.4f\n",
        agreement/(NR-1)
}' results/v21_opt_comparison.csv


echo
echo "============================================================"
echo "Analysis complete"
echo "============================================================"
