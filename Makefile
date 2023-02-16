MAIN_EXE = racc
DEBUG_EXE = racc_debug
C_FLAGS = -Wall -Wextra -pedantic -std=c89
SOURCE_DIR = src
BUILD_DIR = build

.PHONY: all
all: $(BUILD_DIR)/$(MAIN_EXE)

.PHONY: debug
debug: $(BUILD_DIR)/$(DEBUG_EXE)

$(BUILD_DIR)/$(MAIN_EXE): $(SOURCE_DIR)/main.c | $(BUILD_DIR)
	$(CC) -o $@ $< $(C_FLAGS) -O2

$(BUILD_DIR)/$(DEBUG_EXE): $(SOURCE_DIR)/main.c | $(BUILD_DIR)
	$(CC) -o $@ $< $(C_FLAGS) -g

$(BUILD_DIR):
	mkdir $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
