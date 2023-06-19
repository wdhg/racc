LIB_OBJ = base.o

DIR_SRC = src
DIR_OBJ = obj
DIR_OBJ_LIB = $(DIR_OBJ)/lib
DIR_OBJ_DEBUG = $(DIR_OBJ)/debug

C_FLAGS  = -Wall -Wextra -pedantic -std=c99
C_FLAGS += -Iinclude
C_FLAGS += -I../arena/include

.PHONY: lib
lib: $(DIR_OBJ_LIB)/$(LIB_OBJ)

.PHONY: debug
debug: $(DIR_OBJ_DEBUG)/$(LIB_OBJ)

$(DIR_OBJ_LIB)/%.o: $(DIR_SRC)/%.c
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(C_FLAGS) -O3

$(DIR_OBJ_DEBUG)/%.o: $(DIR_SRC)/%.c
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(C_FLAGS) -g

.PHONY: clean
clean:
	rm -rf $(DIR_OBJ)
