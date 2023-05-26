#include "arena.h"
#include "lexer.h"
#include "parser.h"
#include "type_check.h"
#include <ctest.h>

test check_types_does_not_report_errors_for_valid_dec_def_pair(void) {
	struct arena *arena = arena_alloc();
	char source[]       = "myFunc :: Int -> Int;\nmyFunc x = x;";
	struct error_log *log;
	struct token **tokens;
	struct prog *prog;

	log         = arena_push_struct_zero(arena, struct error_log);
	log->source = source;
	tokens      = scan_tokens(source, arena, log);
	prog        = parse(tokens, arena, log);

	check_types(prog, arena, log);

	EXPECT(log->had_error == 0);

	PASS();
}

test check_types_reports_errors_on_invalid_dec_def_pair(void) {
	struct arena *arena = arena_alloc();
	char source[]       = "myFunc :: Int -> Bool;\nmyFunc x = x;";
	struct error_log *log;
	struct token **tokens;
	struct prog *prog;

	log         = arena_push_struct_zero(arena, struct error_log);
	log->source = source;
	tokens      = scan_tokens(source, arena, log);
	prog        = parse(tokens, arena, log);

	check_types(prog, arena, log);

	EXPECT(log->had_error == 1);

	PASS();
}

void test_type_check_h(void) {
	TEST(check_types_does_not_report_errors_for_valid_dec_def_pair);
	TEST(check_types_reports_errors_on_invalid_dec_def_pair);
}
