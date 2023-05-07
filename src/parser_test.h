#include "arena.h"
#include "ast.h"
#include "parser.h"

#include <ctest.h>
#include <string.h>

#include "lexer.h"

static int type_node_equals(struct type *type,
                            char *expected_name,
                            size_t expected_args_len) {
	return strcmp(type->name, expected_name) == 0 &&
	       type->args_len == expected_args_len &&
	       (type->args_len > 0 || type->args == NULL);
}

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

test parse_type_parses_type_contexts(void) {
	struct parser p   = test_parser("[Eq a, Functor f] => f a -> f a -> Bool");
	struct type *type = parse_type(&p);
	EXPECT(type != NULL);
	EXPECT(type->context != NULL);
	EXPECT(type->context->constraints_len == 2);
	EXPECT(strcmp(type->context->constraints[0]->name, "Eq") == 0);
	EXPECT(type->context->constraints[0]->args_len == 1);
	EXPECT(type->context->constraints[0]->args != NULL);
	EXPECT(strcmp(type->context->constraints[0]->args[0], "a") == 0);
	EXPECT(strcmp(type->context->constraints[1]->name, "Functor") == 0);
	EXPECT(type->context->constraints[1]->args_len == 1);
	EXPECT(type->context->constraints[1]->args != NULL);
	EXPECT(strcmp(type->context->constraints[1]->args[0], "f") == 0);

	EXPECT(type_node_equals(type, "->", 2));
	EXPECT(type_node_equals(type->args[0], "f", 1));
	EXPECT(type_node_equals(type->args[0]->args[0], "a", 0));
	EXPECT(type_node_equals(type->args[1], "->", 2));
	EXPECT(type_node_equals(type->args[1]->args[0], "f", 1));
	EXPECT(type_node_equals(type->args[1]->args[0]->args[0], "a", 0));
	EXPECT(type_node_equals(type->args[1]->args[1], "Bool", 0));
	PASS();
}

test parse_stmt_parses_basic_class_declarations(void) {
	struct parser p =
		test_parser("class Functor a {\n  fmap :: (a -> b) -> f a -> f b;\n} ");
	struct stmt *stmt = parse_stmt(&p);
	struct type *type;
	EXPECT(stmt != NULL);
	EXPECT(stmt->type == STMT_DEC_CLASS);
	EXPECT(strcmp(stmt->v.dec_class.name, "Functor") == 0);
	EXPECT(strcmp(stmt->v.dec_class.type_var, "a") == 0);
	EXPECT(stmt->v.dec_class.declarations_len == 1);
	EXPECT(strcmp(stmt->v.dec_class.declarations[0]->name, "fmap") == 0);
	type = stmt->v.dec_class.declarations[0]->type;
	EXPECT(type_node_equals(type, "->", 2));
	EXPECT(type_node_equals(type->args[0], "->", 2));
	EXPECT(type_node_equals(type->args[0]->args[0], "a", 0));
	EXPECT(type_node_equals(type->args[0]->args[1], "b", 0));
	EXPECT(type_node_equals(type->args[1], "->", 2));
	EXPECT(type_node_equals(type->args[1]->args[0], "f", 1));
	EXPECT(type_node_equals(type->args[1]->args[0]->args[0], "a", 0));
	EXPECT(type_node_equals(type->args[1]->args[1], "f", 1));
	EXPECT(type_node_equals(type->args[1]->args[1]->args[0], "b", 0));
	PASS();
}

test parse_stmt_parses_class_declarations(void) {
	struct parser p =
		test_parser("class Applicative a {\n  pure :: a -> f a;\n  liftA2 :: (a -> "
	              "b -> c) -> f a -> f b -> f c;\n} ");
	struct stmt *stmt = parse_stmt(&p);
	struct type *type;
	EXPECT(stmt != NULL);
	EXPECT(stmt->type == STMT_DEC_CLASS);
	EXPECT(strcmp(stmt->v.dec_class.name, "Applicative") == 0);
	EXPECT(strcmp(stmt->v.dec_class.type_var, "a") == 0);
	EXPECT(stmt->v.dec_class.declarations_len == 2);
	EXPECT(strcmp(stmt->v.dec_class.declarations[0]->name, "pure") == 0);
	type = stmt->v.dec_class.declarations[0]->type;
	EXPECT(type_node_equals(type, "->", 2));
	EXPECT(type_node_equals(type->args[0], "a", 0));
	EXPECT(type_node_equals(type->args[1], "f", 1));
	EXPECT(type_node_equals(type->args[1]->args[0], "a", 0));
	EXPECT(strcmp(stmt->v.dec_class.declarations[1]->name, "liftA2") == 0);
	type = stmt->v.dec_class.declarations[1]->type;
	EXPECT(type_node_equals(type, "->", 2));
	EXPECT(type_node_equals(type->args[0], "->", 2));
	EXPECT(type_node_equals(type->args[0]->args[0], "a", 0));
	EXPECT(type_node_equals(type->args[0]->args[1], "->", 2));
	EXPECT(type_node_equals(type->args[0]->args[1]->args[0], "b", 0));
	EXPECT(type_node_equals(type->args[0]->args[1]->args[1], "c", 0));
	EXPECT(type_node_equals(type->args[1], "->", 2));
	EXPECT(type_node_equals(type->args[1]->args[0], "f", 1));
	EXPECT(type_node_equals(type->args[1]->args[0]->args[0], "a", 0));
	EXPECT(type_node_equals(type->args[1]->args[1], "->", 2));
	EXPECT(type_node_equals(type->args[1]->args[1]->args[0], "f", 1));
	EXPECT(type_node_equals(type->args[1]->args[1]->args[0]->args[0], "b", 0));
	EXPECT(type_node_equals(type->args[1]->args[1]->args[1], "f", 1));
	EXPECT(type_node_equals(type->args[1]->args[1]->args[1]->args[0], "c", 0));
	PASS();
}

test parse_stmt_parses_data_declarations(void) {
	struct parser p =
		test_parser("data List a\n  { Null\n  | Cons a (List a)\n  }");
	struct stmt *stmt = parse_stmt(&p);
	EXPECT(p.error_log->had_error == 0);
	EXPECT(stmt != NULL);
	EXPECT(stmt->type == STMT_DEC_DATA);
	EXPECT(strcmp(stmt->v.dec_data.name, "List") == 0);
	EXPECT(stmt->v.dec_data.type_vars_len == 1);
	EXPECT(stmt->v.dec_data.type_vars != NULL);
	EXPECT(strcmp(stmt->v.dec_data.type_vars[0], "a") == 0);
	EXPECT(stmt->v.dec_data.constructors_len == 2);
	EXPECT(stmt->v.dec_data.constructors != NULL);
	EXPECT(strcmp(stmt->v.dec_data.constructors[0]->name, "Null") == 0);
	EXPECT(stmt->v.dec_data.constructors[0]->args_len == 0);
	EXPECT(strcmp(stmt->v.dec_data.constructors[1]->name, "Cons") == 0);
	EXPECT(stmt->v.dec_data.constructors[1]->args_len == 2);
	EXPECT(type_node_equals(stmt->v.dec_data.constructors[1]->args[0], "a", 0));
	EXPECT(
		type_node_equals(stmt->v.dec_data.constructors[1]->args[1], "List", 1));
	EXPECT(type_node_equals(
		stmt->v.dec_data.constructors[1]->args[1]->args[0], "a", 0));
	PASS();
}

test parse_stmt_parses_type_declaration(void) {
	struct parser p   = test_parser("myFunc :: Int -> String;");
	struct stmt *stmt = parse_stmt(&p);
	struct type *type;
	EXPECT(p.error_log->had_error == 0);
	EXPECT(stmt != NULL);
	EXPECT(stmt->type == STMT_DEC_TYPE);
	EXPECT(strcmp(stmt->v.dec_type->name, "myFunc") == 0);
	type = stmt->v.dec_type->type;
	EXPECT(type_node_equals(type, "->", 2));
	EXPECT(type_node_equals(type->args[0], "Int", 0));
	EXPECT(type_node_equals(type->args[1], "String", 0));
	PASS();
}

test parse_stmt_parses_basic_value_definitions(void) {
	struct parser p   = test_parser("myFunc 0 = \"zero\";");
	struct stmt *stmt = parse_stmt(&p);
	EXPECT(p.error_log->had_error == 0);
	EXPECT(stmt != NULL);
	EXPECT(stmt->type == STMT_DEF_VALUE);
	EXPECT(strcmp(stmt->v.def_value->name, "myFunc") == 0);
	EXPECT(stmt->v.def_value->args_len == 1);
	EXPECT(stmt->v.def_value->args[0]->type == EXPR_LIT_INT);
	EXPECT(stmt->v.def_value->args[0]->v.lit_int == 0);
	EXPECT(stmt->v.def_value->value->type == EXPR_LIT_STRING);
	EXPECT(strcmp(stmt->v.def_value->value->v.lit_string, "zero") == 0);
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
	TEST(parse_type_parses_type_contexts);
	TEST(parse_stmt_parses_basic_class_declarations);
	TEST(parse_stmt_parses_class_declarations);
	TEST(parse_stmt_parses_data_declarations);
	TEST(parse_stmt_parses_type_declaration);
	TEST(parse_stmt_parses_basic_value_definitions);
}
