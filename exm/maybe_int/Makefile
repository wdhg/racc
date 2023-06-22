LIB_DIR = ../../lib

RACC_MAIN  = ../../bin/racc
RACC_DEBUG = ../../bin/racc_debug

OBJECTS_MAIN  = $(LIB_DIR)/base/obj/lib/base.o
OBJECTS_MAIN += $(LIB_DIR)/arena/obj/lib/arena.o

OBJECTS_DEBUG  = $(LIB_DIR)/base/obj/debug/base.o
OBJECTS_DEBUG += $(LIB_DIR)/arena/obj/debug/arena.o

C_FLAGS  = -std=c99
C_FLAGS += -I$(LIB_DIR)/base/include
C_FLAGS += -I$(LIB_DIR)/arena/include


.PHONY: all
all: out/main.c
	$(CC) -o out/main out/main.c $(OBJECTS_MAIN) $(C_FLAGS)

.PHONY: debug
debug: out/main.c | out
	$(CC) -o out/main out/main.c $(OBJECTS_DEBUG) $(C_FLAGS) -g

.PHONY: clean
clean:
	rm -rf out

out/main.c: main.rc | out
	$(RACC_MAIN) main.rc out/main.c

out:
	mkdir -p out
