import csv
import matplotlib.pyplot as plt
from collections import defaultdict

file = "results/v21_three_way_comparison.csv"

data = defaultdict(list)

with open(file, newline="") as f:
    reader = csv.DictReader(f)

    for row in reader:
        data[row["workload"]].append({
            "frames": int(row["frames"]),
            "ai_rate": float(row["ai_hit_rate"]),
            "lru_rate": float(row["lru_hit_rate"]),
            "fifo_rate": float(row["fifo_hit_rate"]),
            "ai_faults": int(row["ai_faults"]),
            "lru_faults": int(row["lru_faults"]),
            "fifo_faults": int(row["fifo_faults"])
        })

workloads = ["sequential", "strided", "random", "locality"]

for workload in workloads:

    rows = sorted(data[workload], key=lambda x: x["frames"])

    frames = [r["frames"] for r in rows]

    ai = [r["ai_rate"] * 100 for r in rows]
    lru = [r["lru_rate"] * 100 for r in rows]
    fifo = [r["fifo_rate"] * 100 for r in rows]

    plt.figure(figsize=(8, 5))

    plt.plot(frames, ai, marker="o", label="AIPage")
    plt.plot(frames, lru, marker="o", label="LRU")
    plt.plot(frames, fifo, marker="o", label="FIFO")

    plt.title(f"Hit Rate vs Frames — {workload.capitalize()} Workload")
    plt.xlabel("Number of Frames")
    plt.ylabel("Hit Rate (%)")
    plt.xticks(frames)
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    plt.savefig(
        f"results/v21_{workload}_hit_rate.png",
        dpi=200
    )

    plt.close()

    ai = [r["ai_faults"] for r in rows]
    lru = [r["lru_faults"] for r in rows]
    fifo = [r["fifo_faults"] for r in rows]

    plt.figure(figsize=(8, 5))

    plt.plot(frames, ai, marker="o", label="AIPage")
    plt.plot(frames, lru, marker="o", label="LRU")
    plt.plot(frames, fifo, marker="o", label="FIFO")

    plt.title(f"Page Faults vs Frames — {workload.capitalize()} Workload")
    plt.xlabel("Number of Frames")
    plt.ylabel("Page Faults")
    plt.xticks(frames)
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    plt.savefig(
        f"results/v21_{workload}_page_faults.png",
        dpi=200
    )

    plt.close()


# OPT agreement
opt_file = "results/v21_opt_comparison.csv"

opt = defaultdict(list)

with open(opt_file, newline="") as f:
    reader = csv.DictReader(f)

    for row in reader:
        opt[row["workload"]].append(
            float(row["opt_agreement"])
        )

avg_opt = []

for workload in workloads:
    avg_opt.append(
        sum(opt[workload]) / len(opt[workload]) * 100
    )

plt.figure(figsize=(8, 5))

plt.bar(workloads, avg_opt)

plt.title("AIPage Victim Selection Agreement with OPT")
plt.xlabel("Workload")
plt.ylabel("OPT Agreement (%)")
plt.ylim(0, 100)
plt.grid(axis="y", alpha=0.3)

plt.tight_layout()

plt.savefig(
    "results/v21_opt_agreement.png",
    dpi=200
)

plt.close()

print("Generated final v2.1 graphs:")
print()

for workload in workloads:
    print(f"v21_{workload}_hit_rate.png")
    print(f"v21_{workload}_page_faults.png")

print("v21_opt_agreement.png")
