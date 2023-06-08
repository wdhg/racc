#include "arena.h"
#include "lexer.h"
#include "parser.h"
#include "type_check.h"
#include <ctest.h>

#define TYPE_CHECK_TEST(name, _source, _expected_had_error)                    \
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
		EXPECT(log->had_error == _expected_had_error);                             \
		arena_free(arena);                                                         \
		PASS();                                                                    \
	}

#define TYPE_CHECK_TEST_ACCEPT(name, _source) TYPE_CHECK_TEST(name, _source, 0)
#define TYPE_CHECK_TEST_REJECT(name, _source) TYPE_CHECK_TEST(name, _source, 1)

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_basic_type_declarations,
                       "myFunc :: Int -> Bool;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_duplicated_type_declarations,
                       "myFunc :: Int -> Bool;\n"
                       "myFunc :: Int -> Bool;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_different_type_declarations,
                       "myFunc :: Int -> Bool;\n"
                       "myFunc :: Int -> Bool;")

TYPE_CHECK_TEST_ACCEPT(
	type_check_accepts_matching_type_declaration_and_definitions,
	"myFunc :: Int -> Bool;\n"
	"myFunc _ = True;")

TYPE_CHECK_TEST_REJECT(
	type_check_rejects_mismatching_type_declaration_and_definitions,
	"myFunc :: Int -> Bool;\n"
	"myFunc x = x;")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_multiple_valid_definitions,
                       "myFunc :: Int -> Int;\n"
                       "myFunc 0 = 1;\n"
                       "myFunc x = x;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_multiple_invalid_definitions,
                       "myFunc :: Int -> Int;\n"
                       "myFunc 0 = 1;\n"
                       "myFunc x = False;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_not_enough_parameters_in_function_def,
                       "myFunc :: Int -> Int;\n"
                       "myFunc = 1;\n")

TYPE_CHECK_TEST_REJECT(type_check_rejects_too_many_parameters_in_function_def,
                       "myFunc :: Int -> Int;\n"
                       "myFunc x y = 1;\n")

TYPE_CHECK_TEST_REJECT(type_check_rejects_unknown_identifiers_in_function_def,
                       "myFunc :: Int -> Int;\n"
                       "myFunc x = y;\n")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_data_structures_in_declaration,
                       "myFunc :: [Int] -> Int;\n")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_data_structures_in_definitions,
                       "myFunc :: [Int] -> Int;\n"
                       "myFunc (x:_) = x;")

TYPE_CHECK_TEST_ACCEPT(
	type_check_accepts_valid_nested_data_structures_in_definitions,
	"myFunc :: [(Int, Bool)] -> Int;\n"
	"myFunc ((x,_):_) = x;")

TYPE_CHECK_TEST_REJECT(
	type_check_rejects_invalid_nested_data_structures_in_definitions,
	"myFunc :: [(Int, Bool)] -> Int;\n"
	"myFunc ((_,x):_) = x;")

TYPE_CHECK_TEST_ACCEPT(
	type_check_accepts_valid_complex_nested_data_structures_in_definitions,
	"myFunc :: [([(Int,Char)], ((Int,Bool), [Char]))] -> (Int, [Char]);\n"
	"myFunc ((((x,_):_),(_,y)):_) = (x,y);")

TYPE_CHECK_TEST_REJECT(
	type_check_rejects_invalid_complex_nested_data_structures_in_definitions,
	"myFunc :: [([(Int,Char)], ((Int,Bool), [Char]))] -> (Int, [Char]);\n"
	"myFunc ((((x,_):_),(_,y)):_) = (y,x);")

TYPE_CHECK_TEST_ACCEPT(
	type_check_accepts_valid_complex_nested_data_structures_in_definitions_2,
	"myFunc :: [([(Int,Char)], ((Int,Bool), [Char]))] -> ((Int, Int), "
	"[[Char]]);\n"
	"myFunc ((((x,_):_),(_,y)):_) = ((x,x),[y]);")

TYPE_CHECK_TEST_REJECT(
	type_check_rejects_invalid_complex_nested_data_structures_in_definitions_2,
	"myFunc :: [([(Int,Char)], ((Int,Bool), [Char]))] -> ((Int, Int), "
	"[[Char]]);\n"
	"myFunc ((((x,_):_),(_,y)):_) = (x,y);")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_generic_uses_of_equal,
                       "myFunc :: Int -> Int -> Bool;\n"
                       "myFunc x y = x == y;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_invalid_generic_uses_of_equal,
                       "myFunc :: Int -> Char -> Bool;\n"
                       "myFunc x y = x == y;")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_empty_list_patterns,
                       "myFunc :: [Int] -> Int;\n"
                       "myFunc (x:_) = x;\n"
                       "myFunc [] = 0;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_invalid_empty_list_patterns,
                       "myFunc :: Int -> Int;\n"
                       "myFunc x = x;\n"
                       "myFunc [] = 0;")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_use_of_string_type,
                       "myFunc :: String -> Char;\n"
                       "myFunc \"\" = ' ';\n"
                       "myFunc (x:_) = x;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_invalid_use_of_string_type,
                       "myFunc :: String -> Int;\n"
                       "myFunc (x:_) = x;")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_use_of_literal_strings,
                       "myFunc :: String;\n"
                       "myFunc = \"hello\";")

TYPE_CHECK_TEST_REJECT(type_check_rejects_invalid_use_of_literal_strings,
                       "myFunc :: Int;\n"
                       "myFunc = \"hello\";")

void test_type_check_h(void) {
	TEST(type_check_accepts_basic_type_declarations);
	TEST(type_check_rejects_duplicated_type_declarations);
	TEST(type_check_rejects_different_type_declarations);
	TEST(type_check_accepts_matching_type_declaration_and_definitions);
	TEST(type_check_rejects_mismatching_type_declaration_and_definitions);
	TEST(type_check_accepts_multiple_valid_definitions);
	TEST(type_check_rejects_multiple_invalid_definitions);
	TEST(type_check_rejects_not_enough_parameters_in_function_def);
	TEST(type_check_rejects_too_many_parameters_in_function_def);
	TEST(type_check_rejects_unknown_identifiers_in_function_def);
	TEST(type_check_accepts_valid_data_structures_in_declaration);
	TEST(type_check_accepts_valid_data_structures_in_definitions);
	TEST(type_check_accepts_valid_nested_data_structures_in_definitions);
	TEST(type_check_rejects_invalid_nested_data_structures_in_definitions);
	TEST(type_check_accepts_valid_complex_nested_data_structures_in_definitions);
	TEST(
		type_check_rejects_invalid_complex_nested_data_structures_in_definitions);
	TEST(
		type_check_accepts_valid_complex_nested_data_structures_in_definitions_2);
	TEST(
		type_check_rejects_invalid_complex_nested_data_structures_in_definitions_2);
	TEST(type_check_accepts_valid_generic_uses_of_equal);
	TEST(type_check_rejects_invalid_generic_uses_of_equal);
	TEST(type_check_accepts_valid_empty_list_patterns);
	TEST(type_check_rejects_invalid_empty_list_patterns);
	TEST(type_check_accepts_valid_use_of_string_type);
	TEST(type_check_rejects_invalid_use_of_string_type);
	TEST(type_check_accepts_valid_use_of_literal_strings);
	TEST(type_check_rejects_invalid_use_of_literal_strings);
}
