import matplotlib.pyplot as plt

# ============================================================
# AIPage FINAL SAME-TRACE RESULTS
# 100 accesses, 4 frames
# ============================================================

algorithms = ["AIPage", "LRU", "FIFO"]
hits = [17, 4, 4]
faults = [83, 96, 96]
hit_rates = [17.00, 4.00, 4.00]
fault_rates = [83.00, 96.00, 96.00]

# ------------------------------------------------------------
# GRAPH 1: HIT RATE
# ------------------------------------------------------------

plt.figure(figsize=(8, 5))

bars = plt.bar(algorithms, hit_rates)

plt.title("AIPage vs LRU vs FIFO — Hit Rate")
plt.xlabel("Page Replacement Algorithm")
plt.ylabel("Hit Rate (%)")
plt.ylim(0, 20)

for bar, value in zip(bars, hit_rates):
    plt.text(
        bar.get_x() + bar.get_width() / 2,
        value + 0.5,
        f"{value:.2f}%",
        ha="center",
        va="bottom"
    )

plt.tight_layout()
plt.savefig(
    "final_results/FINAL_HIT_RATE_COMPARISON.png",
    dpi=300,
    bbox_inches="tight"
)
plt.close()

# ------------------------------------------------------------
# GRAPH 2: PAGE FAULTS
# ------------------------------------------------------------

plt.figure(figsize=(8, 5))

bars = plt.bar(algorithms, faults)

plt.title("AIPage vs LRU vs FIFO — Page Faults")
plt.xlabel("Page Replacement Algorithm")
plt.ylabel("Number of Page Faults")
plt.ylim(0, 105)

for bar, value in zip(bars, faults):
    plt.text(
        bar.get_x() + bar.get_width() / 2,
        value + 2,
        str(value),
        ha="center",
        va="bottom"
    )

plt.tight_layout()
plt.savefig(
    "final_results/FINAL_PAGE_FAULT_COMPARISON.png",
    dpi=300,
    bbox_inches="tight"
)
plt.close()

# ------------------------------------------------------------
# GRAPH 3: NEXT-PAGE PREDICTION
# ------------------------------------------------------------

predictions = 91
correct = 58
incorrect = predictions - correct
accuracy = 63.74

labels = ["Correct", "Incorrect"]
values = [correct, incorrect]

plt.figure(figsize=(8, 5))

bars = plt.bar(labels, values)

plt.title("AIPage Next-Page Prediction Performance")
plt.xlabel("Prediction Result")
plt.ylabel("Number of Predictions")
plt.ylim(0, 100)

for bar, value in zip(bars, values):
    plt.text(
        bar.get_x() + bar.get_width() / 2,
        value + 2,
        str(value),
        ha="center",
        va="bottom"
    )

plt.text(
    0.5,
    88,
    f"Accuracy = {accuracy:.2f}%\nPredictions = {predictions}",
    ha="center",
    va="center",
    fontsize=11
)

plt.tight_layout()
plt.savefig(
    "final_results/FINAL_PREDICTION_ACCURACY.png",
    dpi=300,
    bbox_inches="tight"
)
plt.close()

print("==============================================")
print("FINAL AIPage GRAPHS GENERATED")
print("==============================================")
print("1. FINAL_HIT_RATE_COMPARISON.png")
print("2. FINAL_PAGE_FAULT_COMPARISON.png")
print("3. FINAL_PREDICTION_ACCURACY.png")
print("==============================================")
