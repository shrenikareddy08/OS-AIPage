# AIPage: AI-Assisted Page Prediction and Virtual Memory Management

AIPage is an Operating Systems research prototype for intelligent page prediction, page replacement, and real Linux memory monitoring.

## Features

- Real Linux /proc process and memory telemetry
- AI-assisted page reuse prediction
- Page replacement decision engine
- FIFO and LRU baseline comparison
- Locality, sequential, strided, and random workloads
- SQLite experiment data
- Flask live dashboard
- Real-time memory workload
- Experimental results and graphs

## Architecture

Linux /proc Telemetry
        |
        v
Process and Page Access Monitor
        |
        v
Page Access History
        |
        v
AI Prediction Engine
        |
        v
Memory Manager
        |
        v
Page Replacement Decision

## Workloads

The evaluation uses four workload patterns:

- Locality
- Sequential
- Strided
- Random

Workload traces are stored in the traces/ directory.

## Experimental Results

The final three-way comparison is:

    results/v21_three_way_comparison.csv

Additional results and graphs are available in:

    final_results/

The experiments compare AI-assisted page replacement with LRU and FIFO
across multiple frame configurations.

## Live Dashboard

The Flask dashboard displays:

- Real Linux CPU and RAM telemetry
- Linux process information
- Page accesses and page faults
- AI page predictions
- Prediction confidence
- Resident-page status
- Memory-management decisions
- Process scheduling
- Experiment results

Start the dashboard with:

    ./run_gui.sh

Then open:

    http://<VM-IP>:5000/

## Build

Requirements:

- Linux
- GCC
- GNU Make
- Python 3
- SQLite3
- Flask

Build the final native binaries:

    make

The final executables are:

    bin/real_memory_workload
    bin/real_memory_workload
    bin/realtime_ai_FINAL

## Real Memory Pipeline

The real-memory demonstration uses:

    bin/real_memory_workload
            |
            v
    Linux memory/page activity
            |
            v
    realtime_ai_FINAL
            |
            v
    AI page prediction
            |
            v
    Memory-management decision
            |
            v
    Flask dashboard

## Project Structure

    bin/             Final executable binaries
    src/             C source code
    gui/             Flask dashboard
    traces/          Workload traces
    results/         Experiment data
    final_results/   Final results and graphs
    final_demo/      Demonstration outputs
    releases/        Versioned experiment results
    workloads/       Workload source code

## Important Note

AIPage is an academic research prototype. The AI prediction engine
provides an experimental page-reuse prediction signal and does not
replace the Linux kernel virtual-memory subsystem.

## Team

OSSP Team-8

- Shrenika Reddy
- Aswani Gayatri
- A Pavan Reddy
- Mohd Abdul Aiyaan
