CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS = -lm
SQLITE_LIBS = -lsqlite3

SRC_DIR = src
BIN_DIR = bin

TARGETS = \
	$(BIN_DIR)/replacer \
	$(BIN_DIR)/real_memory_workload \
	$(BIN_DIR)/realtime_ai_FINAL

all: $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/replacer: $(SRC_DIR)/replacer.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/real_memory_workload: $(SRC_DIR)/real_memory_workload.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/realtime_ai_FINAL: $(SRC_DIR)/realtime_ai_FINAL.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGETS)

rebuild: clean all

final-binaries: $(BIN_DIR)/real_memory_workload $(BIN_DIR)/realtime_ai_FINAL
	@echo "Final AIPage binaries are ready."

.PHONY: all clean rebuild final-binaries
