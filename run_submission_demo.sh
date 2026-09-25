#!/bin/bash

set -e

echo "============================================================"
echo "                 AIPage FINAL DEMONSTRATION"
echo "============================================================"

echo
echo "[1/4] Checking final trace..."
wc -l final_results/real_pages_1000.txt

echo
echo "[2/4] Running AIPage..."
./bin/AIPage_FINAL_SUBMISSION \
< final_results/real_pages_1000.txt \
> final_results/AIPage_FINAL_SUBMISSION.txt

echo
echo "[3/4] Running baseline algorithms..."

./bin/lru_real_baseline \
final_results/real_pages_1000.txt \
> final_results/LRU_FINAL.txt

./bin/fifo_real_baseline \
final_results/real_pages_1000.txt \
> final_results/FIFO_FINAL.txt

echo
echo "[4/4] FINAL COMPARISON"
echo

echo "---------------- AIPage ----------------"

grep -E \
"^(Total Accesses|Page Hits|Page Faults|Replacements|Next-Page Predictions|Next-Page Correct|Hit Rate|Fault Rate|Next-Page Accuracy)" \
final_results/AIPage_FINAL_SUBMISSION.txt

echo
echo "---------------- LRU ----------------"

grep -E \
"^(Accesses|Hits|Page Faults|Replacements|Hit Rate|Fault Rate)" \
final_results/LRU_FINAL.txt

echo
echo "---------------- FIFO ----------------"

grep -E \
"^(Accesses|Hits|Page Faults|Replacements|Hit Rate|Fault Rate)" \
final_results/FIFO_FINAL.txt

echo
echo "============================================================"
echo "              AIPage DEMONSTRATION COMPLETE"
echo "============================================================"
