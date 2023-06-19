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
                       "myFunc :: Int -> Bool 'r;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_duplicated_type_declarations,
                       "myFunc :: Int -> Bool 'r;\n"
                       "myFunc :: Int -> Bool 'r;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_different_type_declarations,
                       "myFunc :: Int -> Bool 'r;\n"
                       "myFunc :: Int -> Bool 'r;")

TYPE_CHECK_TEST_ACCEPT(
	type_check_accepts_matching_type_declaration_and_definitions,
	"myFunc :: Int -> Bool 'r;\n"
	"myFunc _ = True;")

TYPE_CHECK_TEST_REJECT(
	type_check_rejects_mismatching_type_declaration_and_definitions,
	"myFunc :: Int -> Bool 'r;\n"
	"myFunc x = x;")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_multiple_valid_definitions,
                       "myFunc :: Int -> Int 'r;\n"
                       "myFunc 0 = 1;\n"
                       "myFunc x = x;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_multiple_invalid_definitions,
                       "myFunc :: Int -> Int 'r;\n"
                       "myFunc 0 = 1;\n"
                       "myFunc x = False;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_not_enough_parameters_in_function_def,
                       "myFunc :: Int -> Int 'r;\n"
                       "myFunc = 1;\n")

TYPE_CHECK_TEST_REJECT(type_check_rejects_too_many_parameters_in_function_def,
                       "myFunc :: Int -> Int 'r;\n"
                       "myFunc x y = 1;\n")

TYPE_CHECK_TEST_REJECT(type_check_rejects_unknown_identifiers_in_function_def,
                       "myFunc :: Int -> Int 'r;\n"
                       "myFunc x = y;\n")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_data_structures_in_declaration,
                       "myFunc :: [Int] -> Int 'r;\n")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_data_structures_in_definitions,
                       "myFunc :: [Int] -> Int 'r;\n"
                       "myFunc (x:_) = x;")

TYPE_CHECK_TEST_ACCEPT(
	type_check_accepts_valid_nested_data_structures_in_definitions,
	"myFunc :: [(Int, Bool)] -> Int 'r;\n"
	"myFunc ((x,_):_) = x;")

TYPE_CHECK_TEST_REJECT(
	type_check_rejects_invalid_nested_data_structures_in_definitions,
	"myFunc :: [(Int, Bool)] -> Int 'r;\n"
	"myFunc ((_,x):_) = x;")

TYPE_CHECK_TEST_ACCEPT(
	type_check_accepts_valid_complex_nested_data_structures_in_definitions,
	"myFunc :: [([(Int,Char)], ((Int,Bool), [Char]))] -> (Int, [Char]) 'r;\n"
	"myFunc ((((x,_):_),(_,y)):_) = (x,y);")

TYPE_CHECK_TEST_REJECT(
	type_check_rejects_invalid_complex_nested_data_structures_in_definitions,
	"myFunc :: [([(Int,Char)], ((Int,Bool), [Char]))] -> (Int, [Char]) 'r;\n"
	"myFunc ((((x,_):_),(_,y)):_) = (y,x);")

TYPE_CHECK_TEST_ACCEPT(
	type_check_accepts_valid_complex_nested_data_structures_in_definitions_2,
	"myFunc :: [([(Int,Char)], ((Int,Bool), [Char]))] -> ((Int, Int), "
	"[[Char]]) 'r;\n"
	"myFunc ((((x,_):_),(_,y)):_) = ((x,x),[y]);")

TYPE_CHECK_TEST_REJECT(
	type_check_rejects_invalid_complex_nested_data_structures_in_definitions_2,
	"myFunc :: [([(Int,Char)], ((Int,Bool), [Char]))] -> ((Int, Int), "
	"[[Char]]) 'r;\n"
	"myFunc ((((x,_):_),(_,y)):_) = (x,y);")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_generic_uses_of_equal,
                       "myFunc :: Int -> Int -> Bool 'r;\n"
                       "myFunc x y = x == y;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_invalid_generic_uses_of_equal,
                       "myFunc :: Int -> Char -> Bool 'r;\n"
                       "myFunc x y = x == y;")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_empty_list_patterns,
                       "myFunc :: [Int] -> Int 'r;\n"
                       "myFunc (x:_) = x;\n"
                       "myFunc [] = 0;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_invalid_empty_list_patterns,
                       "myFunc :: Int -> Int 'r;\n"
                       "myFunc x = x;\n"
                       "myFunc [] = 0;")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_use_of_string_type,
                       "myFunc :: String -> Char 'r;\n"
                       "myFunc \"\" = ' ';\n"
                       "myFunc (x:_) = x;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_invalid_use_of_string_type,
                       "myFunc :: String -> Int 'r;\n"
                       "myFunc (x:_) = x;")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_use_of_literal_strings,
                       "myFunc :: String 'r;\n"
                       "myFunc = \"hello\";")

TYPE_CHECK_TEST_REJECT(type_check_rejects_invalid_use_of_literal_strings,
                       "myFunc :: Int 'r;\n"
                       "myFunc = \"hello\";")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_nested_generic_uses_of_equal,
                       "myFunc :: Int -> Int -> Bool 'r;\n"
                       "myFunc x y = (x == y) == False;")

TYPE_CHECK_TEST_REJECT(type_check_rejects_invalid_nested_generic_uses_of_equal,
                       "myFunc :: Int -> Int -> Bool 'r;\n"
                       "myFunc x y = (x == y) == 3;")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_basic_data_types,
                       "data YesNo { Yes | No }\n"
                       "yes :: YesNo 'r;\n"
                       "yes = Yes;")

TYPE_CHECK_TEST_ACCEPT(
	type_check_accepts_valid_basic_data_types_with_concrete_args,
	"data ID { ID Int }\n"
	"myID :: ID 'r;\n"
	"myID = ID 3;")

TYPE_CHECK_TEST_REJECT(
	type_check_rejects_invalid_basic_data_types_with_unknown_args,
	"data ID { ID a }\n")

TYPE_CHECK_TEST_ACCEPT(
	type_check_accepts_valid_basic_data_types_with_generic_args,
	"data ID a { ID a }\n")

TYPE_CHECK_TEST_ACCEPT(
	type_check_accepts_valid_basic_data_types_with_multiple_generic_args,
	"data Either a b { Left a | Right b }\n"
	"myInt :: Either Int Char 'r;\n"
	"myInt = Left 3;\n"
	"myChar :: Either Int Char 'r;\n"
	"myChar = Right 'a';\n")

TYPE_CHECK_TEST_REJECT(
	type_check_rejects_invalid_use_of_basic_data_types_with_multiple_generic_args,
	"data Either a b { Left a | Right b }\n"
	"myChar :: Either Int Char 'r;\n"
	"myChar = Left 'a';\n")

TYPE_CHECK_TEST_REJECT(
	type_check_rejects_duplicate_type_vars_in_data_declaration,
	"data Either a a { Left a | Right a }\n")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_basic_let_in_expressions,
                       "myFunc :: Int -> Int 'r;\n"
                       "myFunc x = let y :: Int 'r;\n"
                       "               y = x + 1;"
                       "           in y;\n")

TYPE_CHECK_TEST_ACCEPT(type_check_accepts_valid_shadowing_let_in_expressions,
                       "y :: Char 'r;\n"
                       "myFunc :: Int -> Int 'r;\n"
                       "myFunc x = let y :: Int 'r;\n"
                       "               y = x + 1;"
                       "           in y;\n")

TYPE_CHECK_TEST_REJECT(type_check_rejects_invalid_basic_let_in_expressions,
                       "myFunc :: Int -> Int 'r;\n"
                       "myFunc x = let y :: Char 'r;\n"
                       "               y = 'a';"
                       "           in y;\n")

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
	TEST(type_check_accepts_valid_nested_generic_uses_of_equal);
	TEST(type_check_rejects_invalid_nested_generic_uses_of_equal);
	TEST(type_check_accepts_valid_basic_data_types);
	TEST(type_check_accepts_valid_basic_data_types_with_concrete_args);
	TEST(type_check_rejects_invalid_basic_data_types_with_unknown_args);
	TEST(type_check_accepts_valid_basic_data_types_with_generic_args);
	TEST(type_check_accepts_valid_basic_data_types_with_multiple_generic_args);
	TEST(
		type_check_rejects_invalid_use_of_basic_data_types_with_multiple_generic_args);
	TEST(type_check_rejects_duplicate_type_vars_in_data_declaration);
	TEST(type_check_accepts_valid_basic_let_in_expressions);
	TEST(type_check_rejects_invalid_basic_let_in_expressions);
	TEST(type_check_accepts_valid_shadowing_let_in_expressions);
}
