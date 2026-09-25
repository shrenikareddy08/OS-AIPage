# AIPage v2.1 — Intelligent Page Replacement

## Project

AIPage is a machine-learning-assisted page replacement prototype implemented in C for Linux.

The system uses page-access history and a trained logistic model to assist victim-page selection during page replacement.

## Workloads

The final evaluation uses four synthetic memory-access workloads:

- Sequential
- Strided
- Random
- Locality

## Frame configurations

Each workload was evaluated with:

2, 3, 4, 5, 6, 8 and 10 frames.

Total configurations:

28

## Baselines

AIPage was compared against:

- LRU
- FIFO

An offline OPT oracle was additionally used to evaluate victim-selection agreement.

## Final Results

Average hit rate:

AIPage : 0.110564
LRU    : 0.090196
FIFO   : 0.090300

AIPage achieved the highest hit rate in:

16 / 28 configurations

LRU achieved the highest hit rate in:

2 / 28 configurations

FIFO achieved the highest hit rate in:

6 / 28 configurations

Ties:

4

Average OPT victim-selection agreement:

59.24%

## Key Result

AIPage performs particularly well on workloads containing predictable locality.

For the locality workload with 10 frames:

AIPage : 62.14% hit rate
LRU    : 60.84%
FIFO   : 60.84%

Corresponding page faults:

AIPage : 1051
LRU    : 1087
FIFO   : 1087

## Limitations

AIPage does not outperform conventional policies on every workload.

Random workloads remain challenging because future page reuse is difficult to predict.

Therefore, the system is presented as a workload-aware intelligent page replacement prototype rather than a universally optimal replacement algorithm.

## Reproducibility

Source:

ai_replacer_v21.c
logistic_v21.c

Model:

model_v21.txt

Traces:

sequential.trace
strided.trace
random.trace
locality.trace

Experiment results:

v21_comparison.csv
v21_three_way_comparison.csv
v21_opt_comparison.csv

## Platform

Linux / Ubuntu

Implementation language:

C

Compiler:

GCC

Optimization:

-O2

Warnings:

-Wall -Wextra

## Evaluation Principle

OPT is used only as an offline oracle because it has knowledge of future references that is unavailable to a real-time page replacement algorithm.
