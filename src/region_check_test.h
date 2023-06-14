#include "arena.h"
#include "lexer.h"
#include "parser.h"
#include "region_check.h"
#include "type_check.h"
#include <ctest.h>

#define REGION_CHECK_TEST(name, _source, _expected_had_error)                  \
	test name(void) {                                                            \
		struct arena *arena = arena_alloc();                                       \
		char *source        = _source;                                             \
		struct error_log *log;                                                     \
		struct token **tokens;                                                     \
		struct prog *prog;                                                         \
		log         = arena_push_struct_zero(arena, struct error_log);             \
		log->source = source;                                                      \
		tokens      = scan_tokens(source, arena, log);                             \
		assert(log->had_error == 0);                                               \
		prog = parse(tokens, arena, log);                                          \
		assert(log->had_error == 0);                                               \
		type_check(prog, arena, log);                                              \
		assert(log->had_error == 0);                                               \
		check_regions(prog, arena, log);                                           \
		EXPECT(log->had_error == _expected_had_error);                             \
		arena_free(arena);                                                         \
		PASS();                                                                    \
	}

#define REGION_CHECK_TEST_ACCEPT(name, _source)                                \
	REGION_CHECK_TEST(name, _source, 0)
#define REGION_CHECK_TEST_REJECT(name, _source)                                \
	REGION_CHECK_TEST(name, _source, 1)

REGION_CHECK_TEST_ACCEPT(check_regions_accepts_basic_input,
                         "data Maybe a {\n"
                         "  Maybe a\n"
                         "}\n"
                         "getInt :: Maybe Int 'r -> Int'r;\n"
                         "getInt (Maybe x) = x;\n"
                         "getInt Nothing = 0;\n")

void test_region_check_h(void) { TEST(check_regions_accepts_basic_input); }
