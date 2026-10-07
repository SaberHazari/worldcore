# Compiler
CC = clang

# Libraries
LIBS =

# Directories
PROJECT_DIR  := $(HOME)/worldcore
SRC_DIR      := src
TEST_DIR     := $(SRC_DIR)/tests
BUILD_DIR    := $(PROJECT_DIR)/build
TEST_OUT_DIR := $(BUILD_DIR)/test
TEST_OBJ_DIR := $(TEST_OUT_DIR)/obj
TEST_BIN     := $(TEST_OUT_DIR)/test

# Flags
WARN_FLAGS     = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes
TEST_SANITIZE ?= address,undefined
sanitize_flags = $(if $(1),-fsanitize=$(1) -fno-omit-frame-pointer)
TEST_FLAGS     = $(WARN_FLAGS) -std=c17 -I$(SRC_DIR) -g -O1 $(call sanitize_flags,$(TEST_SANITIZE))

# Sources
MAIN_SRC         := $(SRC_DIR)/main.c
PROJECT_LIB_SRCS := $(sort $(filter-out $(MAIN_SRC),$(shell find $(SRC_DIR) -type f -name '*.c' -not -path '$(TEST_DIR)/*')))
TEST_SRCS        := $(sort $(shell find $(TEST_DIR) -type f -name '*.c'))
TEST_OBJS        := $(patsubst $(SRC_DIR)/%.c,$(TEST_OBJ_DIR)/%.o,$(PROJECT_LIB_SRCS) $(TEST_SRCS))

.DEFAULT_GOAL := test
.DELETE_ON_ERROR:
.PHONY: test test-build clean

test: $(TEST_BIN)
	@"$(TEST_BIN)"

test-build: $(TEST_BIN)

clean:
	@rm -rf $(BUILD_DIR)

$(TEST_BIN): $(TEST_OBJS)
	@mkdir -p $(@D)
	$(CC) $(TEST_FLAGS) $^ $(LIBS) -o $@

$(TEST_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(TEST_FLAGS) -MMD -MP -c $< -o $@

-include $(TEST_OBJS:.o=.d)