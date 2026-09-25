#!/bin/bash

set -e

echo "============================================================"
echo "              AIPage FINAL DEMONSTRATION"
echo "============================================================"

echo
echo "[1/4] Generating real process memory trace..."

./bin/real_memory_workload | \
grep '^MEMACCESS' | \
head -1000 | \
sed -E 's/.*page=([0-9]+).*/\1/' \
> final_results/real_pages_1000.txt

echo "Trace generated:"
wc -l final_results/real_pages_1000.txt

echo
echo "[2/4] Running AIPage..."

./bin/realtime_ai_FINAL \
< final_results/real_pages_1000.txt \
> final_results/real_ai_1000.txt

echo
echo "[3/4] Running LRU and FIFO baselines..."

./bin/lru_real_baseline \
final_results/real_pages_1000.txt \
> final_results/lru_real_1000.txt

./bin/fifo_real_baseline \
final_results/real_pages_1000.txt \
> final_results/fifo_real_1000.txt

echo
echo "[4/4] FINAL RESULTS"
echo

echo "---------------- AIPage ----------------"

grep -E \
"^(Total Accesses|Page Hits|Page Faults|Replacements|Next-Page Predictions|Next-Page Correct|Hit Rate|Fault Rate|Next-Page Accuracy)" \
final_results/real_ai_1000.txt

echo
echo "---------------- LRU ----------------"

grep -E \
"^(Accesses|Hits|Page Faults|Replacements|Hit Rate|Fault Rate)" \
final_results/lru_real_1000.txt

echo
echo "---------------- FIFO ----------------"

grep -E \
"^(Accesses|Hits|Page Faults|Replacements|Hit Rate|Fault Rate)" \
final_results/fifo_real_1000.txt

echo
echo "============================================================"
echo "              AIPage DEMONSTRATION COMPLETE"
echo "============================================================"
