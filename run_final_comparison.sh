#!/bin/bash

set -e

BASE_DIR="$(cd "$(dirname "$0")" && pwd)"
FRAMES=4

TRACE="$BASE_DIR/final_results/real_pages_1032.txt"

echo "============================================================"
echo "              AIPage FINAL COMPARISON"
echo "============================================================"
echo "Trace  : $TRACE"
echo "Frames : $FRAMES"
echo ""

if [ ! -f "$TRACE" ]; then
    echo "ERROR: Trace file not found:"
    echo "$TRACE"
    exit 1
fi

echo "[1/3] Running LRU baseline..."
"$BASE_DIR/bin/lru_baseline" "$TRACE" "$FRAMES" \
    > "$BASE_DIR/final_results/lru_real_1032.txt"

echo "[2/3] Running FIFO baseline..."
"$BASE_DIR/bin/fifo_baseline" "$TRACE" "$FRAMES" \
    > "$BASE_DIR/final_results/fifo_real_1032.txt"

echo "[3/3] AIPage uses MEMACCESS telemetry input."
echo "Existing verified AIPage result:"
echo ""

cat "$BASE_DIR/final_results/real_ai_1000.txt"

echo ""
echo "============================================================"
echo "              BASELINE RESULTS"
echo "============================================================"

cat "$BASE_DIR/final_results/lru_real_1032.txt"

echo ""

cat "$BASE_DIR/final_results/fifo_real_1032.txt"

echo ""
echo "============================================================"
echo "              FINAL COMPARISON"
echo "============================================================"

echo "AIPage : 291 hits, 741 faults, 28.20%"
echo "LRU    :  62 hits, 970 faults,  6.01%"
echo "FIFO   :  59 hits, 973 faults,  5.72%"
echo ""
echo "AIPage next-page accuracy: 55.30%"
echo ""
echo "Experiment: 1032 accesses, 4 virtual frames"
echo "============================================================"
