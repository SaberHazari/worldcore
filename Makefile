# Compiler
CC = clang

# Libraries
LIBS =

# Directories
PROJECT_DIR     := $(HOME)/life-sim
SRC_DIR         := src
TEST_DIR        := $(SRC_DIR)/tests
BUILD_DIR       := $(PROJECT_DIR)/build

DEBUG_DIR       := $(BUILD_DIR)/debug
DEBUG_OBJ_DIR   := $(DEBUG_DIR)/obj
RELEASE_DIR     := $(BUILD_DIR)/release
RELEASE_OBJ_DIR := $(RELEASE_DIR)/obj
TEST_OUT_DIR    := $(BUILD_DIR)/test
TEST_OBJ_DIR    := $(TEST_OUT_DIR)/obj

# Flags
COMMON_FLAGS  = -Wall -Wextra -std=c17 -Isrc/
DEBUG_FLAGS   = $(COMMON_FLAGS) -g -O0
RELEASE_FLAGS = $(COMMON_FLAGS) -O2
TEST_FLAGS    = $(COMMON_FLAGS) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer

# Source discovery
MAIN_SRC      := $(SRC_DIR)/main.c
GAME_SRCS     := $(shell find $(SRC_DIR) -type f -name '*.c' -not -path '$(TEST_DIR)/*')
GAME_LIB_SRCS := $(filter-out $(MAIN_SRC),$(GAME_SRCS))
TEST_SRCS     := $(shell find $(TEST_DIR) -type f -name '*.c')

# Object mapping (preserves subdirectories)
to_objs = $(patsubst $(SRC_DIR)/%.c,$(1)/%.o,$(2))

DEBUG_OBJS   := $(call to_objs,$(DEBUG_OBJ_DIR),$(GAME_SRCS))
RELEASE_OBJS := $(call to_objs,$(RELEASE_OBJ_DIR),$(GAME_SRCS))
TEST_OBJS    := $(call to_objs,$(TEST_OBJ_DIR),$(GAME_LIB_SRCS) $(TEST_SRCS))

# Dependencies
DEBUG_DEPS   := $(DEBUG_OBJS:.o=.d)
RELEASE_DEPS := $(RELEASE_OBJS:.o=.d)
TEST_DEPS    := $(TEST_OBJS:.o=.d)

# Binaries
DEBUG_BIN   := $(DEBUG_DIR)/main
RELEASE_BIN := $(RELEASE_DIR)/main
TEST_BIN    := $(TEST_OUT_DIR)/test

# Phony targets
.PHONY: all run clean debug debug-run release release-run test test-run

all: debug
run: debug-run

debug: $(DEBUG_BIN)
debug-run: debug
	@"$(DEBUG_BIN)"

release: $(RELEASE_BIN)
release-run: release
	@"$(RELEASE_BIN)"

test: $(TEST_BIN)
	@"$(TEST_BIN)"

test-build: $(TEST_BIN)

clean:
	@rm -rf $(BUILD_DIR)

# Directory creation
$(DEBUG_DIR) $(RELEASE_DIR) $(TEST_OUT_DIR):
	@mkdir -p $@

# Generic compile rules — one per configuration, preserving subpaths
$(DEBUG_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(DEBUG_FLAGS) -MMD -MP -c $< -o $@

$(RELEASE_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(RELEASE_FLAGS) -MMD -MP -c $< -o $@

$(TEST_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(TEST_FLAGS) -MMD -MP -c $< -o $@

# Linking
$(DEBUG_BIN): $(DEBUG_OBJS) | $(DEBUG_DIR)
	$(CC) $(DEBUG_FLAGS) $^ $(LIBS) -o $@

$(RELEASE_BIN): $(RELEASE_OBJS) | $(RELEASE_DIR)
	$(CC) $(RELEASE_FLAGS) $^ $(LIBS) -o $@

$(TEST_BIN): $(TEST_OBJS) | $(TEST_OUT_DIR)
	$(CC) $(TEST_FLAGS) $^ $(LIBS) -o $@

# Include auto-generated dependencies
-include $(DEBUG_DEPS)
-include $(RELEASE_DEPS)
-include $(TEST_DEPS)