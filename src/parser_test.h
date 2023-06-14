#include "arena.h"
#include "ast.h"
#include "lexer.h"
#include "list.h"
#include "parser.h"
#include <ctest.h>
#include <string.h>

#define EXPECT_TYPE_NODE_EQUALS(                                               \
	type, expected_name, expected_args_len, expected_region_var)                 \
	{                                                                            \
		EXPECT(type != NULL);                                                      \
		EXPECT(strcmp(type->name, expected_name) == 0);                            \
		if (expected_args_len == 0) {                                              \
			EXPECT(type->type_args == NULL);                                         \
		} else {                                                                   \
			EXPECT(type->type_args != NULL);                                         \
			EXPECT(list_length(type->type_args) == expected_args_len);               \
		}                                                                          \
		if (expected_region_var != NULL) {                                         \
			EXPECT(type->region_var != NULL);                                        \
			EXPECT(strcmp(type->region_var, expected_region_var) == 0);              \
		}                                                                          \
	}

#define EXPECT_TYPE_NODE_EQUALS_DFS(                                           \
	queue, expected_name, expected_params_len, expected_region_var)              \
	{                                                                            \
		struct type *type = list_pop_head(queue);                                  \
		EXPECT_TYPE_NODE_EQUALS(                                                   \
			type, expected_name, expected_params_len, expected_region_var);          \
		if (type->type_args != NULL) {                                             \
			list_prepend_all(queue, type->type_args);                                \
		}                                                                          \
	}

#define _EXPECT_EXPR_EQUALS_DFS_VALUE(                                         \
	queue, expected_type, member, expected_value)                                \
	{                                                                            \
		struct expr *expr = list_pop_head(queue);                                  \
		EXPECT(expr != NULL);                                                      \
		EXPECT(expr->expr_type == expected_type);                                  \
		EXPECT(expr->v.member == expected_value);                                  \
	}

#define _EXPECT_EXPR_EQUALS_DFS_STRING(                                        \
	queue, expected_type, member, expected_string)                               \
	{                                                                            \
		struct expr *expr = list_pop_head(queue);                                  \
		EXPECT(expr != NULL);                                                      \
		EXPECT(expr->expr_type == expected_type);                                  \
		EXPECT(strcmp(expr->v.member, expected_string) == 0);                      \
	}

#define EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(queue, expected_identifier)          \
	_EXPECT_EXPR_EQUALS_DFS_STRING(                                              \
		queue, EXPR_IDENTIFIER, identifier, expected_identifier)

#define EXPECT_EXPR_EQUALS_DFS_APPLICATION(                                    \
	queue, expected_fn, expected_args_len)                                       \
	{                                                                            \
		struct expr *expr = list_pop_head(queue);                                  \
		EXPECT(expr != NULL);                                                      \
		EXPECT(expr->expr_type == EXPR_APPLICATION);                               \
		EXPECT(strcmp(expr->v.application.fn, expected_fn) == 0);                  \
		EXPECT(expr->v.application.expr_args != NULL);                             \
		EXPECT(list_length(expr->v.application.expr_args) == expected_args_len);   \
		list_prepend_all(queue, expr->v.application.expr_args);                    \
	}

#define EXPECT_EXPR_EQUALS_DFS_LIT_INT(queue, expected_value)                  \
	_EXPECT_EXPR_EQUALS_DFS_VALUE(queue, EXPR_LIT_INT, lit_int, expected_value)

#define EXPECT_EXPR_EQUALS_DFS_LIT_DOUBLE(queue, expected_value)               \
	_EXPECT_EXPR_EQUALS_DFS_VALUE(                                               \
		queue, EXPR_LIT_DOUBLE, lit_double, expected_value)

#define EXPECT_EXPR_EQUALS_DFS_LIT_STRING(queue, expected_value)               \
	_EXPECT_EXPR_EQUALS_DFS_STRING(                                              \
		queue, EXPR_LIT_STRING, lit_string, expected_value);

#define EXPECT_EXPR_EQUALS_DFS_LIT_CHAR(queue, expected_value)                 \
	_EXPECT_EXPR_EQUALS_DFS_VALUE(queue, EXPR_LIT_CHAR, lit_char, expected_value)

#define EXPECT_EXPR_EQUALS_DFS_LIT_BOOL(queue, expected_value)                 \
	_EXPECT_EXPR_EQUALS_DFS_VALUE(queue, EXPR_LIT_BOOL, lit_bool, expected_value)

#define EXPECT_EXPR_EQUALS_DFS_LIST_NULL(queue)                                \
	{                                                                            \
		struct expr *expr = list_pop_head(queue);                                  \
		EXPECT(expr != NULL);                                                      \
		EXPECT(expr->expr_type == EXPR_LIST_NULL);                                 \
	}

#define EXPECT_EXPR_EQUALS_DFS_GROUPING(queue)                                 \
	{                                                                            \
		struct expr *expr = list_pop_head(queue);                                  \
		EXPECT(expr != NULL);                                                      \
		EXPECT(expr->expr_type == EXPR_GROUPING);                                  \
		EXPECT(expr->v.grouping != NULL);                                          \
		list_prepend(queue, expr->v.grouping);                                     \
	}

struct parser test_parser(char *source) {
	struct parser p;
	p.arena          = arena_alloc();
	p.current        = 0;
	p.log            = arena_push_struct_zero(p.arena, struct error_log);
	p.log->source    = source;
	p.log->had_error = 0;
	p.log->suppress_error_messages = 0;
	p.tokens                       = scan_tokens(source, p.arena, p.log);
	return p;
}

test parse_expr_parses_identifiers(void) {
	struct parser p   = test_parser("myVar");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->expr_type == EXPR_IDENTIFIER);
	EXPECT(strcmp(expr->v.identifier, "myVar") == 0);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_ints(void) {
	struct parser p   = test_parser("123");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->expr_type == EXPR_LIT_INT);
	EXPECT(expr->v.lit_int == 123);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_doubles(void) {
	struct parser p   = test_parser("123.456");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->expr_type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.lit_double == 123.456);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_strings(void) {
	struct parser p   = test_parser("\"my\ntest string ! \"");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->expr_type == EXPR_LIT_STRING);
	EXPECT(strcmp(expr->v.lit_string, "my\ntest string ! ") == 0);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_chars(void) {
	struct parser p   = test_parser("'a'");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->expr_type == EXPR_LIT_CHAR);
	EXPECT(expr->v.lit_char == 'a');
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_bools_true(void) {
	struct parser p   = test_parser("True");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->expr_type == EXPR_LIT_BOOL);
	EXPECT(expr->v.lit_bool == 1);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_bools_false(void) {
	struct parser p   = test_parser("False");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->expr_type == EXPR_LIT_BOOL);
	EXPECT(expr->v.lit_bool == 0);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_applications(void) {
	struct parser p         = test_parser("f x 123");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "f", 2);
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "x");
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 123);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_unarys(void) {
	struct parser p         = test_parser("-123");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "-", 1);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 123);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_factors(void) {
	struct parser p         = test_parser("1.23 / 4.56");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "/", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_DOUBLE(expr_queue, 1.23);
	EXPECT_EXPR_EQUALS_DFS_LIT_DOUBLE(expr_queue, 4.56);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_terms(void) {
	struct parser p         = test_parser("1.23 - 4.56");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "-", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_DOUBLE(expr_queue, 1.23);
	EXPECT_EXPR_EQUALS_DFS_LIT_DOUBLE(expr_queue, 4.56);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_comparisons(void) {
	struct parser p         = test_parser("1.23 <= 4.56");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "<=", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_DOUBLE(expr_queue, 1.23);
	EXPECT_EXPR_EQUALS_DFS_LIT_DOUBLE(expr_queue, 4.56);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_equality(void) {
	struct parser p         = test_parser("1.23 /= 4.56");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "/=", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_DOUBLE(expr_queue, 1.23);
	EXPECT_EXPR_EQUALS_DFS_LIT_DOUBLE(expr_queue, 4.56);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_grouped_expressions(void) {
	struct parser p         = test_parser("(123 + 456 == 789)");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_GROUPING(expr_queue);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "==", 2);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "+", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 123);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 456);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 789);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_tuple_expressions(void) {
	struct parser p         = test_parser("(123, 456 + 789, \"hello\")");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "(,,)", 3);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 123);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "+", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 456);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 789);
	EXPECT_EXPR_EQUALS_DFS_LIT_STRING(expr_queue, "hello");
	arena_free(p.arena);
	PASS();
}

test parse_expr_reports_errors_on_unclosed_subexpressions(void) {
	struct parser p   = test_parser("(123 + 456 ==");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr == NULL);
	EXPECT(p.log->had_error);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_null_list_expressions(void) {
	struct parser p         = test_parser("[]");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_LIST_NULL(expr_queue);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_singleton_list_expressions(void) {
	struct parser p         = test_parser("[1]");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, ":", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 1);
	EXPECT_EXPR_EQUALS_DFS_LIST_NULL(expr_queue);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_longer_list_expressions(void) {
	struct parser p         = test_parser("[1,2,3,4]");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, ":", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 1);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, ":", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 2);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, ":", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 3);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, ":", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 4);
	EXPECT_EXPR_EQUALS_DFS_LIST_NULL(expr_queue);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_cons_list_expressions(void) {
	struct parser p         = test_parser("1:2:3:4:[]");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, ":", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 1);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, ":", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 2);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, ":", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 3);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, ":", 2);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 4);
	EXPECT_EXPR_EQUALS_DFS_LIST_NULL(expr_queue);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_cons_pattern(void) {
	struct parser p         = test_parser("(x:_)");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_GROUPING(expr_queue);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, ":", 2);
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "x");
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "_");
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_complex_data_structures(void) {
	struct parser p         = test_parser("((((x,_):_),(_,y)):_)");
	struct expr *expr       = parse_expr(&p);
	struct list *expr_queue = list_new(p.arena);
	list_prepend(expr_queue, expr);
	EXPECT_EXPR_EQUALS_DFS_GROUPING(expr_queue);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, ":", 2);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "(,)", 2);
	EXPECT_EXPR_EQUALS_DFS_GROUPING(expr_queue);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, ":", 2);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "(,)", 2);
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "x");
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "_");
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "_");
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "(,)", 2);
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "_");
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "y");
	PASS();
}

test parse_type_parses_basic_concrete_types(void) {
	struct parser p   = test_parser("String");
	struct type *type = parse_type(&p);
	EXPECT_TYPE_NODE_EQUALS(type, "String", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_type_parses_parameterized_concrete_types(void) {
	struct parser p         = test_parser("Maybe Int");
	struct type *type       = parse_type(&p);
	struct list *type_queue = list_new(p.arena);
	list_prepend(type_queue, type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Maybe", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Int", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_type_parses_basic_function_types(void) {
	struct parser p         = test_parser("Int -> String");
	struct type *type       = parse_type(&p);
	struct list *type_queue = list_new(p.arena);
	list_prepend(type_queue, type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Int", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "String", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_type_parses_parameterized_function_types(void) {
	struct parser p         = test_parser("Maybe Int -> Int");
	struct type *type       = parse_type(&p);
	struct list *type_queue = list_new(p.arena);
	list_prepend(type_queue, type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Maybe", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Int", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Int", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_type_parses_multiple_argument_function_types(void) {
	struct parser p         = test_parser("a -> b -> c");
	struct type *type       = parse_type(&p);
	struct list *type_queue = list_new(p.arena);
	list_prepend(type_queue, type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "c", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_type_parses_grouped_function_types(void) {
	struct parser p         = test_parser("(a -> b) -> a -> b");
	struct type *type       = parse_type(&p);
	struct list *type_queue = list_new(p.arena);
	list_prepend(type_queue, type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_type_parses_list_types(void) {
	struct parser p         = test_parser("(a -> b) -> [a] -> [b]");
	struct type *type       = parse_type(&p);
	struct list *type_queue = list_new(p.arena);
	list_prepend(type_queue, type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "[]", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "[]", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_type_parses_tuple_types(void) {
	struct parser p         = test_parser("(a, b, c) -> (a, b)");
	struct type *type       = parse_type(&p);
	struct list *type_queue = list_new(p.arena);
	list_prepend(type_queue, type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "(,,)", 3, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "c", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "(,)", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_type_parses_complex_types(void) {
	struct parser p =
		test_parser("Either a b -> (b -> Either a c) -> Either a c");
	struct type *type       = parse_type(&p);
	struct list *type_queue = list_new(p.arena);
	list_prepend(type_queue, type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Either", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Either", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "c", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Either", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "c", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_type_parses_complex_types_2(void) {
	struct parser p = test_parser(
		"[([(Int,Char)], ((Int,Bool), [Char]))] -> ((Int, Int),[[Char]])");
	struct type *type       = parse_type(&p);
	struct list *type_queue = list_new(p.arena);
	list_prepend(type_queue, type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "[]", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "(,)", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "[]", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "(,)", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Int", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Char", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "(,)", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "(,)", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Int", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Bool", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "[]", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Char", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "(,)", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "(,)", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Int", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Int", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "[]", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "[]", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Char", 0, NULL);
	PASS();
}

test parse_type_parses_type_contexts(void) {
	struct parser p   = test_parser("<Eq a, Functor f> => f a -> f a -> Bool");
	struct type *type = parse_type(&p);
	struct list *type_queue = list_new(p.arena);
	EXPECT(type != NULL);
	EXPECT(type->type_constraints != NULL);
	EXPECT(list_length(type->type_constraints) == 2);
	list_prepend_all(type_queue, type->type_constraints);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Eq", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Functor", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "f", 0, NULL);
	list_prepend(type_queue, type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "f", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "f", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Bool", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_type_parses_region_sorts(void) {
	struct parser p         = test_parser("(a, b) 'r -> a 'r");
	struct type *type       = parse_type(&p);
	struct list *type_queue = list_new(p.arena);
	list_prepend(type_queue, type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "(,)", 2, "r");
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, "r");
	EXPECT(list_length(type_queue) == 0);
	list_prepend(type_queue, type);
	arena_free(p.arena);
	PASS();
}

test parse_stmt_parses_basic_class_declarations(void) {
	struct parser p =
		test_parser("class Functor a {\n  fmap :: (a -> b) -> f a -> f b;\n} ");
	struct stmt *stmt       = parse_stmt(&p);
	struct list *type_queue = list_new(p.arena);
	struct dec_type *dec_type;
	EXPECT(stmt != NULL);
	EXPECT(stmt->type == STMT_DEC_CLASS);
	EXPECT(strcmp(stmt->v.dec_class->name, "Functor") == 0);
	EXPECT(strcmp(stmt->v.dec_class->type_var, "a") == 0);
	EXPECT(list_length(stmt->v.dec_class->dec_types) == 1);
	dec_type = list_get(stmt->v.dec_class->dec_types, 0);
	EXPECT(strcmp(dec_type->name, "fmap") == 0);
	list_prepend(type_queue, dec_type->type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "f", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "f", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_stmt_parses_class_declarations(void) {
	struct parser p =
		test_parser("class Applicative a {\n  pure :: a -> f a;\n  liftA2 :: (a -> "
	              "b -> c) -> f a -> f b -> f c;\n} ");
	struct stmt *stmt       = parse_stmt(&p);
	struct list *type_queue = list_new(p.arena);
	struct dec_type *dec_type;
	EXPECT(stmt != NULL);
	EXPECT(stmt->type == STMT_DEC_CLASS);
	EXPECT(strcmp(stmt->v.dec_class->name, "Applicative") == 0);
	EXPECT(strcmp(stmt->v.dec_class->type_var, "a") == 0);
	EXPECT(list_length(stmt->v.dec_class->dec_types) == 2);

	/* pure */
	dec_type = list_get(stmt->v.dec_class->dec_types, 0);
	EXPECT(strcmp(dec_type->name, "pure") == 0);
	list_prepend(type_queue, dec_type->type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "f", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT(list_length(type_queue) == 0);

	/* liftA2 */
	dec_type = list_get(stmt->v.dec_class->dec_types, 1);
	EXPECT(strcmp(dec_type->name, "liftA2") == 0);
	list_prepend(type_queue, dec_type->type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "c", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "f", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "f", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "b", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "f", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "c", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_stmt_parses_data_declarations(void) {
	struct parser p =
		test_parser("data List a\n  { Null\n  | Cons a (List a)\n  }");
	struct stmt *stmt       = parse_stmt(&p);
	struct list *type_queue = list_new(p.arena);
	struct dec_constructor *constructor;
	EXPECT(p.log->had_error == 0);
	EXPECT(stmt != NULL);
	EXPECT(stmt->type == STMT_DEC_DATA);
	EXPECT(strcmp(stmt->v.dec_data->name, "List") == 0);
	EXPECT(stmt->v.dec_data->type_vars != NULL);
	EXPECT(list_length(stmt->v.dec_data->type_vars) == 1);
	EXPECT(strcmp(list_get(stmt->v.dec_data->type_vars, 0), "a") == 0);
	EXPECT(stmt->v.dec_data->dec_constructors != NULL);
	EXPECT(list_length(stmt->v.dec_data->dec_constructors) == 2);

	constructor = list_get(stmt->v.dec_data->dec_constructors, 0);
	EXPECT(strcmp(constructor->name, "Null") == 0);
	EXPECT(constructor->type_params != NULL);
	EXPECT(list_length(constructor->type_params) == 0);

	constructor = list_get(stmt->v.dec_data->dec_constructors, 1);
	EXPECT(strcmp(constructor->name, "Cons") == 0);
	EXPECT(constructor->type_params != NULL);
	EXPECT(list_length(constructor->type_params) == 2);
	list_prepend_all(type_queue, constructor->type_params);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "List", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT(list_length(type_queue) == 0);

	arena_free(p.arena);
	PASS();
}

test parse_stmt_parses_type_declaration(void) {
	struct parser p         = test_parser("myFunc :: Int -> String;");
	struct stmt *stmt       = parse_stmt(&p);
	struct list *type_queue = list_new(p.arena);
	struct type *type;
	EXPECT(p.log->had_error == 0);
	EXPECT(stmt != NULL);
	EXPECT(stmt->type == STMT_DEC_TYPE);
	EXPECT(strcmp(stmt->v.dec_type->name, "myFunc") == 0);
	type = stmt->v.dec_type->type;
	list_prepend(type_queue, type);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "->", 2, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Int", 0, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "String", 0, NULL);
	arena_free(p.arena);
	PASS();
}

test parse_stmt_parses_basic_value_definitions(void) {
	struct parser p         = test_parser("myFunc 0 = \"zero\";");
	struct stmt *stmt       = parse_stmt(&p);
	struct list *expr_queue = list_new(p.arena);
	EXPECT(p.log->had_error == 0);
	EXPECT(stmt != NULL);
	EXPECT(stmt->type == STMT_DEF_VALUE);
	EXPECT(strcmp(stmt->v.def_value->name, "myFunc") == 0);
	list_prepend_all(expr_queue, stmt->v.def_value->expr_params);
	EXPECT_EXPR_EQUALS_DFS_LIT_INT(expr_queue, 0);
	EXPECT(list_length(expr_queue) == 0);
	list_prepend(expr_queue, stmt->v.def_value->value);
	EXPECT_EXPR_EQUALS_DFS_LIT_STRING(expr_queue, "zero");
	arena_free(p.arena);
	PASS();
}

test parse_stmt_parses_instance_definitions(void) {
	struct parser p         = test_parser("instance <Eq a> => Eq (Maybe a) {\n"
	                                      "  equal Nothing Nothing = True;\n"
	                                      "  equal (Just a) (Just b) = equal a b;\n"
	                                      "  equal _ _ = False;\n"
	                                      "}");
	struct stmt *stmt       = parse_stmt(&p);
	struct list *type_queue = list_new(p.arena);
	struct list *expr_queue = list_new(p.arena);
	struct def_value *def;
	EXPECT(p.log->had_error == 0);
	EXPECT(stmt != NULL);
	EXPECT(stmt->type == STMT_DEF_INSTANCE);
	EXPECT(stmt->v.def_instance != NULL);

	EXPECT(stmt->v.def_instance->type_constraints != NULL);
	EXPECT(list_length(stmt->v.def_instance->type_constraints) == 1);
	list_prepend_all(type_queue, stmt->v.def_instance->type_constraints);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Eq", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT(list_length(type_queue) == 0);

	EXPECT(strcmp(stmt->v.def_instance->class_name, "Eq") == 0);

	EXPECT(list_length(stmt->v.def_instance->type_args) == 1);
	list_prepend_all(type_queue, stmt->v.def_instance->type_args);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "Maybe", 1, NULL);
	EXPECT_TYPE_NODE_EQUALS_DFS(type_queue, "a", 0, NULL);
	EXPECT(list_length(type_queue) == 0);

	EXPECT(list_length(stmt->v.def_instance->def_values) == 3);

	/* equal Nothing Nothing = True */
	def = list_get(stmt->v.def_instance->def_values, 0);
	EXPECT(strcmp(def->name, "equal") == 0);
	EXPECT(list_length(def->expr_params) == 2);
	list_prepend_all(expr_queue, def->expr_params);
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "Nothing");
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "Nothing");
	EXPECT(list_length(expr_queue) == 0);
	list_prepend(expr_queue, def->value);
	EXPECT_EXPR_EQUALS_DFS_LIT_BOOL(expr_queue, 1);
	EXPECT(list_length(expr_queue) == 0);

	/* equal (Just a) (Just b) = equal a b */
	def = list_get(stmt->v.def_instance->def_values, 1);
	EXPECT(strcmp(def->name, "equal") == 0);
	EXPECT(list_length(def->expr_params) == 2);
	list_prepend_all(expr_queue, def->expr_params);
	EXPECT_EXPR_EQUALS_DFS_GROUPING(expr_queue);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "Just", 1);
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "a");
	EXPECT_EXPR_EQUALS_DFS_GROUPING(expr_queue);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "Just", 1);
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "b");
	EXPECT(list_length(expr_queue) == 0);
	list_prepend(expr_queue, def->value);
	EXPECT_EXPR_EQUALS_DFS_APPLICATION(expr_queue, "equal", 2);
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "a");
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "b");

	/* equal _ _ = False */
	def = list_get(stmt->v.def_instance->def_values, 2);
	EXPECT(strcmp(def->name, "equal") == 0);
	EXPECT(list_length(def->expr_params) == 2);
	list_prepend_all(expr_queue, def->expr_params);
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "_");
	EXPECT_EXPR_EQUALS_DFS_IDENTIFIER(expr_queue, "_");
	EXPECT(list_length(expr_queue) == 0);
	list_prepend(expr_queue, def->value);
	EXPECT_EXPR_EQUALS_DFS_LIT_BOOL(expr_queue, 0);

	arena_free(p.arena);
	PASS();
}

void test_parser_h(void) {
	TEST(parse_expr_parses_identifiers);
	TEST(parse_expr_parses_ints);
	TEST(parse_expr_parses_doubles);
	TEST(parse_expr_parses_strings);
	TEST(parse_expr_parses_chars);
	TEST(parse_expr_parses_bools_true);
	TEST(parse_expr_parses_bools_false);
	TEST(parse_expr_parses_applications);
	TEST(parse_expr_parses_unarys);
	TEST(parse_expr_parses_factors);
	TEST(parse_expr_parses_terms);
	TEST(parse_expr_parses_comparisons);
	TEST(parse_expr_parses_equality);
	TEST(parse_expr_parses_tuple_expressions);
	TEST(parse_expr_parses_grouped_expressions);
	TEST(parse_expr_reports_errors_on_unclosed_subexpressions);
	TEST(parse_expr_parses_null_list_expressions);
	TEST(parse_expr_parses_singleton_list_expressions);
	TEST(parse_expr_parses_longer_list_expressions);
	TEST(parse_expr_parses_cons_list_expressions);
	TEST(parse_expr_parses_cons_pattern);
	TEST(parse_expr_parses_complex_data_structures);
	TEST(parse_type_parses_basic_concrete_types);
	TEST(parse_type_parses_parameterized_concrete_types);
	TEST(parse_type_parses_basic_function_types);
	TEST(parse_type_parses_parameterized_function_types);
	TEST(parse_type_parses_multiple_argument_function_types);
	TEST(parse_type_parses_grouped_function_types);
	TEST(parse_type_parses_list_types);
	TEST(parse_type_parses_tuple_types);
	TEST(parse_type_parses_complex_types);
	TEST(parse_type_parses_complex_types_2);
	TEST(parse_type_parses_type_contexts);
	TEST(parse_type_parses_region_sorts);
	TEST(parse_stmt_parses_basic_class_declarations);
	TEST(parse_stmt_parses_class_declarations);
	TEST(parse_stmt_parses_data_declarations);
	TEST(parse_stmt_parses_type_declaration);
	TEST(parse_stmt_parses_basic_value_definitions);
	TEST(parse_stmt_parses_instance_definitions);
}
