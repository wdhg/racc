#include "arena.h"
#include "ast.h"
#include "parser.h"

#include <ctest.h>

#include "lexer.h"

struct parser test_parser(char *source) {
	struct parser p;
	p.arena                = arena_alloc();
	p.tokens               = scan_tokens(source, p.arena);
	p.current              = 0;
	p.error_log            = arena_push_struct_zero(p.arena, struct error_log);
	p.error_log->source    = source;
	p.error_log->had_error = 0;
	p.error_log->suppress_error_messages = 1;
	return p;
}

test parse_expr_parses_identifiers(void) {
	struct parser p   = test_parser("myVar");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_IDENTIFIER);
	EXPECT(strcmp(expr->v.identifier, "myVar") == 0);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_ints(void) {
	struct parser p   = test_parser("123");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_LIT_INT);
	EXPECT(expr->v.lit_int == 123);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_doubles(void) {
	struct parser p   = test_parser("123.456");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.lit_double == 123.456);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_strings(void) {
	struct parser p   = test_parser("\"my\ntest string ! \"");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_LIT_STRING);
	EXPECT(strcmp(expr->v.lit_string, "my\ntest string ! ") == 0);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_chars(void) {
	struct parser p   = test_parser("'a'");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_LIT_CHAR);
	EXPECT(expr->v.lit_char == 'a');
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_bools_true(void) {
	struct parser p   = test_parser("true");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_LIT_BOOL);
	EXPECT(expr->v.lit_bool == 1);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_bools_false(void) {
	struct parser p   = test_parser("false");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_LIT_BOOL);
	EXPECT(expr->v.lit_bool == 0);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_applications(void) {
	struct parser p   = test_parser("f x 123");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_APPLICATION);
	EXPECT(strcmp(expr->v.application.fn, "f") == 0);
	EXPECT(expr->v.application.args[0]->type == EXPR_IDENTIFIER);
	EXPECT(strcmp(expr->v.application.args[0]->v.identifier, "x") == 0);
	EXPECT(expr->v.application.args[1]->type == EXPR_LIT_INT);
	EXPECT(expr->v.application.args[1]->v.lit_int == 123);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_unarys(void) {
	struct parser p   = test_parser("-123");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_UNARY);
	EXPECT(expr->v.unary.op->type == TOK_SUB);
	EXPECT(expr->v.unary.rhs->type == EXPR_LIT_INT);
	EXPECT(expr->v.unary.rhs->v.lit_int == 123);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_factors(void) {
	struct parser p   = test_parser("1.23 / 4.56");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_BINARY);
	EXPECT(expr->v.binary.op->type == TOK_DIV);
	EXPECT(expr->v.binary.lhs->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.binary.lhs->v.lit_double == 1.23);
	EXPECT(expr->v.binary.rhs->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.binary.rhs->v.lit_double == 4.56);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_terms(void) {
	struct parser p   = test_parser("1.23 - 4.56");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_BINARY);
	EXPECT(expr->v.binary.op->type == TOK_SUB);
	EXPECT(expr->v.binary.lhs->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.binary.lhs->v.lit_double == 1.23);
	EXPECT(expr->v.binary.rhs->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.binary.rhs->v.lit_double == 4.56);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_comparisons(void) {
	struct parser p   = test_parser("1.23 <= 4.56");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_BINARY);
	EXPECT(expr->v.binary.op->type == TOK_LT_EQ);
	EXPECT(expr->v.binary.lhs->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.binary.lhs->v.lit_double == 1.23);
	EXPECT(expr->v.binary.rhs->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.binary.rhs->v.lit_double == 4.56);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_equality(void) {
	struct parser p   = test_parser("1.23 /= 4.56");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_BINARY);
	EXPECT(expr->v.binary.op->type == TOK_NE);
	EXPECT(expr->v.binary.lhs->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.binary.lhs->v.lit_double == 1.23);
	EXPECT(expr->v.binary.rhs->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.binary.rhs->v.lit_double == 4.56);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_grouped_expressions(void) {
	struct parser p   = test_parser("(123 + 456 == 789)");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_GROUPING);
	EXPECT(expr->v.grouping->type == EXPR_BINARY);
	EXPECT(expr->v.grouping->v.binary.op->type == TOK_EQ_EQ);
	EXPECT(expr->v.grouping->v.binary.lhs->type == EXPR_BINARY);
	EXPECT(expr->v.grouping->v.binary.lhs->v.binary.op->type == TOK_ADD);
	EXPECT(expr->v.grouping->v.binary.lhs->v.binary.lhs->type == EXPR_LIT_INT);
	EXPECT(expr->v.grouping->v.binary.lhs->v.binary.lhs->v.lit_int == 123);
	EXPECT(expr->v.grouping->v.binary.lhs->v.binary.rhs->type == EXPR_LIT_INT);
	EXPECT(expr->v.grouping->v.binary.lhs->v.binary.rhs->v.lit_int == 456);
	EXPECT(expr->v.grouping->v.binary.rhs->type == EXPR_LIT_INT);
	EXPECT(expr->v.grouping->v.binary.rhs->v.lit_int == 789);
	arena_free(p.arena);
	PASS();
}

test parse_expr_reports_errors_on_unclosed_subexpressions(void) {
	struct parser p   = test_parser("(123 + 456 ==");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr == NULL);
	EXPECT(p.error_log->had_error);
	arena_free(p.arena);
	PASS();
}

test parse_type_parses_basic_concrete_types(void) {
	struct parser p   = test_parser("String");
	struct type *type = parse_type(&p);
	EXPECT(strcmp(type->name, "String") == 0);
	EXPECT(type->args_len == 0);
	EXPECT(type->args == NULL);
	PASS();
}

test parse_type_parses_parameterized_concrete_types(void) {
	struct parser p   = test_parser("Maybe Int");
	struct type *type = parse_type(&p);
	EXPECT(strcmp(type->name, "Maybe") == 0);
	EXPECT(type->args_len == 1);
	EXPECT(strcmp(type->args[0]->name, "Int") == 0);
	EXPECT(type->args[0]->args_len == 0);
	EXPECT(type->args[0]->args == NULL);
	PASS();
}

test parse_type_parses_basic_function_types(void) {
	struct parser p   = test_parser("Int -> String");
	struct type *type = parse_type(&p);
	EXPECT(strcmp(type->name, "->") == 0);
	EXPECT(type->args_len == 2);
	EXPECT(strcmp(type->args[0]->name, "Int") == 0);
	EXPECT(type->args[0]->args_len == 0);
	EXPECT(type->args[0]->args == NULL);
	EXPECT(strcmp(type->args[1]->name, "String") == 0);
	EXPECT(type->args[1]->args_len == 0);
	EXPECT(type->args[1]->args == NULL);
	PASS();
}

test parse_type_parses_parameterized_function_types(void) {
	struct parser p   = test_parser("Maybe Int -> Int");
	struct type *type = parse_type(&p);
	EXPECT(strcmp(type->name, "->") == 0);
	EXPECT(type->args_len == 2);
	EXPECT(strcmp(type->args[0]->name, "Maybe") == 0);
	EXPECT(type->args[0]->args_len == 1);
	EXPECT(strcmp(type->args[0]->args[0]->name, "Int") == 0)
	EXPECT(type->args[0]->args[0]->args_len == 0);
	EXPECT(type->args[0]->args[0]->args == NULL);
	EXPECT(strcmp(type->args[1]->name, "Int") == 0);
	EXPECT(type->args[1]->args_len == 0);
	EXPECT(type->args[1]->args == NULL);
	PASS();
}

test parse_type_parses_multiple_argument_function_types(void) {
	struct parser p   = test_parser("a -> b -> c");
	struct type *type = parse_type(&p);
	EXPECT(strcmp(type->name, "->") == 0);
	EXPECT(type->args_len == 2);
	EXPECT(strcmp(type->args[0]->name, "a") == 0);
	EXPECT(type->args[0]->args_len == 0);
	EXPECT(type->args[0]->args == NULL);
	EXPECT(strcmp(type->args[1]->name, "->") == 0);
	EXPECT(type->args[1]->args_len == 2);
	EXPECT(strcmp(type->args[1]->args[0]->name, "b") == 0);
	EXPECT(type->args[1]->args[0]->args_len == 0);
	EXPECT(type->args[1]->args[0]->args == NULL);
	EXPECT(strcmp(type->args[1]->args[1]->name, "c") == 0);
	EXPECT(type->args[1]->args[1]->args_len == 0);
	EXPECT(type->args[1]->args[1]->args == NULL);
	PASS();
}

test parse_type_parses_grouped_function_types(void) {
	struct parser p   = test_parser("(a -> b) -> a -> b");
	struct type *type = parse_type(&p);
	EXPECT(strcmp(type->name, "->") == 0);
	EXPECT(type->args_len == 2);
	EXPECT(strcmp(type->args[0]->name, "->") == 0);
	EXPECT(type->args[0]->args_len == 2);
	EXPECT(strcmp(type->args[0]->args[0]->name, "a") == 0)
	EXPECT(type->args[0]->args[0]->args_len == 0);
	EXPECT(type->args[0]->args[0]->args == NULL);
	EXPECT(strcmp(type->args[0]->args[1]->name, "b") == 0)
	EXPECT(type->args[0]->args[1]->args_len == 0);
	EXPECT(type->args[0]->args[1]->args == NULL);
	EXPECT(strcmp(type->args[1]->name, "->") == 0);
	EXPECT(type->args[1]->args_len == 2);
	EXPECT(strcmp(type->args[1]->args[0]->name, "a") == 0)
	EXPECT(type->args[1]->args[0]->args_len == 0);
	EXPECT(type->args[1]->args[0]->args == NULL);
	EXPECT(strcmp(type->args[1]->args[1]->name, "b") == 0)
	EXPECT(type->args[1]->args[1]->args_len == 0);
	EXPECT(type->args[1]->args[1]->args == NULL);
	PASS();
}

test parse_type_parses_complex_types(void) {
	struct parser p =
		test_parser("Either a b -> (b -> Either a c) -> Either a c");
	struct type *type = parse_type(&p);
	EXPECT(strcmp(type->name, "->") == 0);
	EXPECT(type->args_len == 2);
	EXPECT(strcmp(type->args[0]->name, "Either") == 0);
	EXPECT(type->args[0]->args_len == 2);
	EXPECT(strcmp(type->args[0]->args[0]->name, "a") == 0);
	EXPECT(type->args[0]->args[0]->args_len == 0);
	EXPECT(type->args[0]->args[0]->args == NULL);
	EXPECT(strcmp(type->args[0]->args[1]->name, "b") == 0);
	EXPECT(type->args[0]->args[1]->args_len == 0);
	EXPECT(type->args[0]->args[1]->args == NULL);
	EXPECT(strcmp(type->args[1]->name, "->") == 0);
	EXPECT(type->args[1]->args_len == 2);
	EXPECT(strcmp(type->args[1]->args[0]->name, "->") == 0);
	EXPECT(type->args[1]->args[0]->args_len == 2);
	EXPECT(strcmp(type->args[1]->args[0]->args[0]->name, "b") == 0);
	EXPECT(type->args[1]->args[0]->args[0]->args_len == 0);
	EXPECT(type->args[1]->args[0]->args[0]->args == NULL);
	EXPECT(strcmp(type->args[1]->args[0]->args[1]->name, "Either") == 0);
	EXPECT(type->args[1]->args[0]->args[1]->args_len == 2);
	EXPECT(strcmp(type->args[1]->args[0]->args[1]->args[0]->name, "a") == 0);
	EXPECT(type->args[1]->args[0]->args[1]->args[0]->args_len == 0);
	EXPECT(type->args[1]->args[0]->args[1]->args[0]->args == NULL);
	EXPECT(strcmp(type->args[1]->args[0]->args[1]->args[1]->name, "c") == 0);
	EXPECT(type->args[1]->args[0]->args[1]->args[1]->args_len == 0);
	EXPECT(type->args[1]->args[0]->args[1]->args[1]->args == NULL);
	EXPECT(strcmp(type->args[1]->args[1]->name, "Either") == 0);
	EXPECT(type->args[1]->args[1]->args_len == 2);
	EXPECT(strcmp(type->args[1]->args[1]->args[0]->name, "a") == 0);
	EXPECT(type->args[1]->args[1]->args[0]->args_len == 0);
	EXPECT(type->args[1]->args[1]->args[0]->args == NULL);
	EXPECT(strcmp(type->args[1]->args[1]->args[1]->name, "c") == 0);
	EXPECT(type->args[1]->args[1]->args[1]->args_len == 0);
	EXPECT(type->args[1]->args[1]->args[1]->args == NULL);
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
	TEST(parse_expr_parses_grouped_expressions);
	TEST(parse_expr_reports_errors_on_unclosed_subexpressions);
	TEST(parse_type_parses_basic_concrete_types);
	TEST(parse_type_parses_parameterized_concrete_types);
	TEST(parse_type_parses_basic_function_types);
	TEST(parse_type_parses_parameterized_function_types);
	TEST(parse_type_parses_multiple_argument_function_types);
	TEST(parse_type_parses_grouped_function_types);
	TEST(parse_type_parses_complex_types);
}
