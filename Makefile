CC := clang
CFLAGS := -std=c11 -Wall -Wextra -O2 -Iinclude -g
OBJCFLAGS := -fobjc-arc -Iinclude -O2 -g
LDFLAGS := -framework Cocoa -framework AudioToolbox -lm

SRC_DIR := src
BUILD_DIR := build
INCLUDE_DIR := include
TEST_DIR := tests

C_SRCS := $(filter-out $(SRC_DIR)/frontend_cocoa.m, $(wildcard $(SRC_DIR)/*.c))
M_SRCS := $(wildcard $(SRC_DIR)/*.m)

C_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
M_OBJS := $(patsubst $(SRC_DIR)/%.m,$(BUILD_DIR)/%.o,$(M_SRCS))
ALL_OBJS := $(C_OBJS) $(M_OBJS)
CORE_OBJS := $(filter-out $(BUILD_DIR)/main.o, $(C_OBJS))

TARGET := $(BUILD_DIR)/zeff_station
ALIAS := $(BUILD_DIR)/gba_emulator
TEST_BIN := $(BUILD_DIR)/test_runner

.PHONY: all clean run test roms

all: $(TARGET) $(ALIAS)

$(TARGET): $(ALL_OBJS) | $(BUILD_DIR)
	$(CC) $(ALL_OBJS) $(LDFLAGS) -o $@

$(ALIAS): $(TARGET)
	ln -sf zeff_station $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.m | $(BUILD_DIR)
	$(CC) $(OBJCFLAGS) -c $< -o $@

$(BUILD_DIR)/test_runner.o: $(TEST_DIR)/test_runner.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BIN): $(BUILD_DIR)/test_runner.o $(CORE_OBJS) | $(BUILD_DIR)
	$(CC) $^ -lm -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

roms:
	python3 $(TEST_DIR)/generate_test_roms.py

test: $(TEST_BIN) roms
	./$(TEST_BIN)

run: all
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET) $(ALIAS) $(TEST_BIN)
