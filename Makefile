DIR_SRC = src
DIR_BIN = bin
DIR_LIB = lib
DIR_OBJ = obj
DIR_OBJ_MAIN = $(DIR_OBJ)/main
DIR_OBJ_DEBUG = $(DIR_OBJ)/debug
DIR_OBJ_TEST = $(DIR_OBJ)/test

EXE_MAIN  = $(DIR_BIN)/racc
EXE_DEBUG = $(DIR_BIN)/racc_debug
EXE_TEST  = $(DIR_BIN)/racc_test

C_FLAGS  = -Wall -Wextra -pedantic -std=c89
C_FLAGS += -I$(DIR_LIB)/ctest/include
C_FLAGS += -I$(DIR_LIB)/arena/include
C_FLAGS += -I$(DIR_LIB)/fixint/include

MAIN_C = $(DIR_SRC)/main.c
TEST_C = $(DIR_SRC)/test.c

TESTS   = $(wildcard $(DIR_SRC)/*_test.h)
HEADERS = $(filter-out $(TESTS),$(wildcard $(DIR_SRC)/*.h))

SOURCES_ALL   = $(wildcard $(DIR_SRC)/*.c)
SOURCES_MAIN  = $(filter-out $(TEST_C),$(SOURCES_ALL))
SOURCES_TEST = $(filter-out $(MAIN_C),$(SOURCES_ALL))

OBJECTS_MAIN_TEMP  = $(patsubst %.c,%.o,$(SOURCES_MAIN))
OBJECTS_TEST_TEMP = $(patsubst %.c,%.o,$(SOURCES_TEST))

OBJECTS_MAIN  = $(subst $(DIR_SRC),$(DIR_OBJ_MAIN),$(OBJECTS_MAIN_TEMP))
OBJECTS_DEBUG = $(subst $(DIR_SRC),$(DIR_OBJ_DEBUG),$(OBJECTS_MAIN_TEMP))
OBJECTS_TEST  = $(subst $(DIR_SRC),$(DIR_OBJ_TEST),$(OBJECTS_TEST_TEMP))

OBJECTS_LIBS_MAIN  = $(DIR_LIB)/arena/obj/lib/arena.o
OBJECTS_LIBS_MAIN += $(DIR_LIB)/ctest/obj/lib/ctest.o

OBJECTS_LIBS_DEBUG  = $(DIR_LIB)/arena/obj/debug/arena.o
OBJECTS_LIBS_DEBUG += $(DIR_LIB)/ctest/obj/debug/ctest.o

.PHONY: main
main: $(EXE_MAIN)

.PHONY: debug
debug: $(EXE_DEBUG)

.PHONY: test
test: $(EXE_TEST)

.PHONY: libs
libs:
	cd $(DIR_LIB)/arena && make lib
	cd $(DIR_LIB)/ctest && make lib

.PHONY: libs-debug
libs-debug:
	cd $(DIR_LIB)/arena && make debug
	cd $(DIR_LIB)/ctest && make debug

$(EXE_MAIN): libs $(OBJECTS_MAIN) $(HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -O3 -o $@ $(OBJECTS_MAIN) $(OBJECTS_LIBS_MAIN)

$(EXE_DEBUG): libs-debug $(OBJECTS_DEBUG) $(HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -g -o $@ $(OBJECTS_DEBUG) $(OBJECTS_LIBS_DEBUG)

$(EXE_TEST): libs-debug $(OBJECTS_TEST) $(HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -g -o $@ $(OBJECTS_TEST) $(OBJECTS_LIBS_DEBUG)

$(DIR_OBJ_MAIN)/%.o: $(DIR_SRC)/%.c
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(C_FLAGS) -O3

$(DIR_OBJ_DEBUG)/%.o: $(DIR_SRC)/%.c
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(C_FLAGS) -g

$(DIR_OBJ_TEST)/%.o: $(DIR_SRC)/%.c $(TESTS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(C_FLAGS) -g

.PHONY: clean
clean:
	cd $(DIR_LIB)/arena && make clean
	cd $(DIR_LIB)/ctest && make clean
	rm -rf $(DIR_BIN) $(DIR_OBJ)
