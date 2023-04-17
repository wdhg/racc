DIR_SRC = src
DIR_OBJ = obj
DIR_BIN = bin
DIR_LIB = lib

EXE_MAIN  = $(DIR_BIN)/racc
EXE_DEBUG = $(DIR_BIN)/racc_debug
EXE_TEST  = $(DIR_BIN)/racc_test

C_FLAGS  = -Wall -Wextra -pedantic -std=c89
C_FLAGS += -I$(DIR_LIB)/ctest/include
C_FLAGS += -I$(DIR_LIB)/carena/include

MAIN_SRC = $(DIR_SRC)/main.c

SOURCES = $(filter-out $(MAIN_SRC),$(wildcard $(DIR_SRC)/*.c))
OBJECTS = $(subst $(DIR_SRC),$(DIR_OBJ),$(patsubst %.c,%.o,$(SRC)))
TESTS   = $(wildcard $(DIR_SRC)/*_test.h)
HEADERS = $(filter-out $(TESTS),$(wildcard $(DIR_SRC)/*.h))

.PHONY: all
all: $(EXE_MAIN)

.PHONY: debug
debug: $(EXE_DEBUG)

.PHONY: test
test: $(EXE_TEST)

$(EXE_MAIN): $(OBJECTS) $(MAIN_SRC) $(HEADERS)
	mkdir -p $(dir $@)
	$(CC) -o $@ $(OBJECTS) $(MAIN_SRC) $(C_FLAGS) -O2

$(EXE_DEBUG): $(OBJECTS) $(MAIN_SRC) $(HEADERS)
	mkdir -p $(dir $@)
	$(CC) -o $@ $(OBJ) $(MAIN_SRC) $(C_FLAGS) -g

$(EXE_TEST): $(OBJECTS) $(MAIN_SRC) $(HEADERS) $(TESTS)
	mkdir -p $(dir $@)
	$(CC) -D RACC_TEST -o $@ $(OBJ) $(MAIN_SRC) $(C_FLAGS) -g

$(DIR_OBJ)/%.o: $(DIR_SRC)/%.c
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(C_FLAGS)

.PHONY: clean
clean:
	rm -rf $(DIR_BIN) $(DIR_OBJ)
