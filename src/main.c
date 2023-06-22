#include "code_gen.h"
#include "lexer.h"
#include "parser.h"
#include "type_check.h"
#include <arena.h>
#include <stdio.h>

int main(int argc, char **argv) {
	struct arena *arena = arena_alloc();
	char source[1024];
	struct error_log *log;
	struct token **tokens;
	struct prog *prog;
	FILE *fptr;

	if (argc != 3) {
		return 1;
	}

	printf("Compiling %s...\n", argv[1]);

	fptr = fopen(argv[1], "r");

	if (fptr == NULL) {
		printf("Unable to read file '%s' :(\n", argv[1]);
		return 1;
	}

	fread(source, 1024, 1, fptr);

	log         = arena_push_struct_zero(arena, struct error_log);
	log->source = source;

	tokens = scan_tokens(source, arena, log);
	if (log->had_error)
		return 1;
	prog = parse(tokens, arena, log);
	if (log->had_error)
		return 1;
	type_check(prog, arena, log);
	if (log->had_error)
		return 1;
	code_gen(prog, arena, log, argv[2]);
	if (log->had_error)
		return 1;
	printf("Done :)\n");
	return 0;
}
