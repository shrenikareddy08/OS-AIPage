# AIPage v2.1 Final Run

## Ubuntu / Multipass

```bash
cd ~/AIPage
make final-binaries
./run_gui.sh
```

Open from the Mac browser:

`http://192.168.252.19:5000`

## Final GUI behavior

- Process Scheduler uses the first 5 live Linux processes reported by `/proc`.
- PID, process name, normalized arrival order and measured CPU execution time are shown.
- Quantum is visible only for Round Robin.
- Database Samples is read dynamically from SQLite `process_samples`.
- Live AI Memory Monitor reads real page-access state from the AIPage pipeline.
- Start/Stop controls operate the real-memory pipeline.

## Frozen 1000-access validation

AIPage: 281 hits / 719 faults / 28.10% hit rate.
LRU: 58 hits / 942 faults / 5.80% hit rate.
FIFO: 56 hits / 944 faults / 5.60% hit rate.
Next-page prediction accuracy: 55.27% on this 1000-access trace.

Do not overwrite the frozen result files when reproducing live runs.
