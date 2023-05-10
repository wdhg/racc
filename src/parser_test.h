#include "arena.h"
#include "ast.h"
#include "parser.h"

#include <ctest.h>
#include <string.h>

#include "lexer.h"

#define EXPECT_TYPE_NODE_EQUALS(type, expected_name, expected_args_len)        \
	{                                                                            \
		EXPECT(type != NULL);                                                      \
		EXPECT(strcmp(type->name, expected_name) == 0);                            \
		EXPECT(type->args_len == expected_args_len);                               \
		EXPECT(expected_args_len > 0 || type->args == NULL);                       \
	}

struct parser test_parser(char *source) {
	struct parser p;
	p.arena                = arena_alloc();
	p.tokens               = scan_tokens(source, p.arena);
	p.current              = 0;
	p.error_log            = arena_push_struct_zero(p.arena, struct error_log);
	p.error_log->source    = source;
	p.error_log->had_error = 0;
	p.error_log->suppress_error_messages = 0;
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
	struct parser p   = test_parser("True");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_LIT_BOOL);
	EXPECT(expr->v.lit_bool == 1);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_bools_false(void) {
	struct parser p   = test_parser("False");
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
	EXPECT(expr->type == EXPR_APPLICATION);
	EXPECT(strcmp(expr->v.application.fn, "-") == 0);
	EXPECT(expr->v.application.args_len == 1);
	EXPECT(expr->v.application.args[0]->type == EXPR_LIT_INT);
	EXPECT(expr->v.application.args[0]->v.lit_int == 123);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_factors(void) {
	struct parser p   = test_parser("1.23 / 4.56");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_APPLICATION);
	EXPECT(strcmp(expr->v.application.fn, "/") == 0);
	EXPECT(expr->v.application.args[0]->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.application.args[0]->v.lit_double == 1.23);
	EXPECT(expr->v.application.args[1]->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.application.args[1]->v.lit_double == 4.56);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_terms(void) {
	struct parser p   = test_parser("1.23 - 4.56");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_APPLICATION);
	EXPECT(strcmp(expr->v.application.fn, "-") == 0);
	EXPECT(expr->v.application.args[0]->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.application.args[0]->v.lit_double == 1.23);
	EXPECT(expr->v.application.args[1]->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.application.args[1]->v.lit_double == 4.56);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_comparisons(void) {
	struct parser p   = test_parser("1.23 <= 4.56");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_APPLICATION);
	EXPECT(strcmp(expr->v.application.fn, "<=") == 0);
	EXPECT(expr->v.application.args[0]->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.application.args[0]->v.lit_double == 1.23);
	EXPECT(expr->v.application.args[1]->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.application.args[1]->v.lit_double == 4.56);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_equality(void) {
	struct parser p   = test_parser("1.23 /= 4.56");
	struct expr *expr = parse_expr(&p);
	EXPECT(expr->type == EXPR_APPLICATION);
	EXPECT(strcmp(expr->v.application.fn, "/=") == 0);
	EXPECT(expr->v.application.args[0]->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.application.args[0]->v.lit_double == 1.23);
	EXPECT(expr->v.application.args[1]->type == EXPR_LIT_DOUBLE);
	EXPECT(expr->v.application.args[1]->v.lit_double == 4.56);
	arena_free(p.arena);
	PASS();
}

test parse_expr_parses_grouped_expressions(void) {
	struct parser p   = test_parser("(123 + 456 == 789)");
	struct expr *expr = parse_expr(&p);
	struct expr *sub_expr;
	EXPECT(expr->type == EXPR_GROUPING);
	sub_expr = expr->v.grouping;
	EXPECT(sub_expr->type == EXPR_APPLICATION);
	EXPECT(strcmp(sub_expr->v.application.fn, "==") == 0);
	sub_expr = expr->v.grouping->v.application.args[0]; /* 123 + 456 */
	EXPECT(sub_expr->type == EXPR_APPLICATION);
	EXPECT(strcmp(sub_expr->v.application.fn, "+") == 0);
	sub_expr =
		expr->v.grouping->v.application.args[0]->v.application.args[0]; /* 123 */
	EXPECT(sub_expr->type == EXPR_LIT_INT);
	EXPECT(sub_expr->v.lit_int == 123);
	sub_expr =
		expr->v.grouping->v.application.args[0]->v.application.args[1]; /* 456 */
	EXPECT(sub_expr->type == EXPR_LIT_INT);
	EXPECT(sub_expr->v.lit_int == 456);
	sub_expr = expr->v.grouping->v.application.args[1]; /* 789 */
	EXPECT(sub_expr->type == EXPR_LIT_INT);
	EXPECT(sub_expr->v.lit_int == 789);
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
	EXPECT_TYPE_NODE_EQUALS(type, "String", 0);
	PASS();
}

test parse_type_parses_parameterized_concrete_types(void) {
	struct parser p   = test_parser("Maybe Int");
	struct type *type = parse_type(&p);
	EXPECT_TYPE_NODE_EQUALS(type, "Maybe", 1);
	EXPECT_TYPE_NODE_EQUALS(type->args[0], "Int", 0);
	PASS();
}

test parse_type_parses_basic_function_types(void) {
	struct parser p   = test_parser("Int -> String");
	struct type *type = parse_type(&p);
	EXPECT_TYPE_NODE_EQUALS(type, "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0], "Int", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1], "String", 0);
	PASS();
}

test parse_type_parses_parameterized_function_types(void) {
	struct parser p   = test_parser("Maybe Int -> Int");
	struct type *type = parse_type(&p);
	EXPECT_TYPE_NODE_EQUALS(type, "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0], "Maybe", 1);
	EXPECT_TYPE_NODE_EQUALS(type->args[0]->args[0], "Int", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1], "Int", 0);
	PASS();
}

test parse_type_parses_multiple_argument_function_types(void) {
	struct parser p   = test_parser("a -> b -> c");
	struct type *type = parse_type(&p);
	EXPECT_TYPE_NODE_EQUALS(type, "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1], "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0], "b", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1], "c", 0);
	PASS();
}

test parse_type_parses_grouped_function_types(void) {
	struct parser p   = test_parser("(a -> b) -> a -> b");
	struct type *type = parse_type(&p);
	EXPECT_TYPE_NODE_EQUALS(type, "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0], "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0]->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[0]->args[1], "b", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1], "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1], "b", 0);
	PASS();
}

test parse_type_parses_complex_types(void) {
	struct parser p =
		test_parser("Either a b -> (b -> Either a c) -> Either a c");
	struct type *type = parse_type(&p);
	EXPECT_TYPE_NODE_EQUALS(type, "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0], "Either", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0]->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[0]->args[1], "b", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1], "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0], "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0]->args[0], "b", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0]->args[1], "Either", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0]->args[1]->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0]->args[1]->args[1], "c", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1], "Either", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1]->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1]->args[1], "c", 0);
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
	EXPECT_TYPE_NODE_EQUALS(type, "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0], "f", 1);
	EXPECT_TYPE_NODE_EQUALS(type->args[0]->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1], "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0], "f", 1);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0]->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1], "Bool", 0);
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
	EXPECT_TYPE_NODE_EQUALS(type, "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0], "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0]->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[0]->args[1], "b", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1], "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0], "f", 1);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0]->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1], "f", 1);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1]->args[0], "b", 0);
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
	EXPECT_TYPE_NODE_EQUALS(type, "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1], "f", 1);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0], "a", 0);
	EXPECT(strcmp(stmt->v.dec_class.declarations[1]->name, "liftA2") == 0);
	type = stmt->v.dec_class.declarations[1]->type;
	EXPECT_TYPE_NODE_EQUALS(type, "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0], "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0]->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[0]->args[1], "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0]->args[1]->args[0], "b", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[0]->args[1]->args[1], "c", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1], "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0], "f", 1);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[0]->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1], "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1]->args[0], "f", 1);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1]->args[0]->args[0], "b", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1]->args[1], "f", 1);
	EXPECT_TYPE_NODE_EQUALS(type->args[1]->args[1]->args[1]->args[0], "c", 0);
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
	EXPECT_TYPE_NODE_EQUALS(stmt->v.dec_data.constructors[1]->args[0], "a", 0);
	EXPECT_TYPE_NODE_EQUALS(stmt->v.dec_data.constructors[1]->args[1], "List", 1);
	EXPECT_TYPE_NODE_EQUALS(
		stmt->v.dec_data.constructors[1]->args[1]->args[0], "a", 0);
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
	EXPECT_TYPE_NODE_EQUALS(type, "->", 2);
	EXPECT_TYPE_NODE_EQUALS(type->args[0], "Int", 0);
	EXPECT_TYPE_NODE_EQUALS(type->args[1], "String", 0);
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

test parse_stmt_parses_instance_definitions(void) {
	struct parser p   = test_parser("instance [Eq a] => Eq (Maybe a) {\n"
	                                "  equal Nothing Nothing = True;\n"
	                                "  equal (Just a) (Just b) = equal a b;\n"
	                                "  equal _ _ = False;\n"
	                                "}");
	struct stmt *stmt = parse_stmt(&p);
	struct def_value *def;
	struct expr *expr;
	EXPECT(p.error_log->had_error == 0);
	EXPECT(stmt != NULL);
	EXPECT(stmt->type == STMT_DEF_INSTANCE);
	EXPECT(stmt->v.def_instance.context->constraints_len == 1);
	EXPECT(strcmp(stmt->v.def_instance.context->constraints[0]->name, "Eq") == 0);
	EXPECT(stmt->v.def_instance.context->constraints[0]->args_len == 1);
	EXPECT(strcmp(stmt->v.def_instance.context->constraints[0]->args[0], "a") ==
	       0)
	EXPECT(strcmp(stmt->v.def_instance.class_name, "Eq") == 0);
	EXPECT(stmt->v.def_instance.args_len == 1);
	EXPECT_TYPE_NODE_EQUALS(stmt->v.def_instance.args[0], "Maybe", 1);
	EXPECT_TYPE_NODE_EQUALS(stmt->v.def_instance.args[0]->args[0], "a", 0);
	EXPECT(stmt->v.def_instance.defs_len == 3);
	/* equal Nothing Nothing = True */
	def = stmt->v.def_instance.defs[0];
	EXPECT(strcmp(def->name, "equal") == 0);
	EXPECT(def->args_len == 2);
	expr = def->args[0]; /* Nothing */
	EXPECT(expr->type == EXPR_IDENTIFIER);
	EXPECT(strcmp(expr->v.identifier, "Nothing") == 0);
	expr = def->args[1]; /* Nothing */
	EXPECT(expr->type == EXPR_IDENTIFIER);
	EXPECT(strcmp(expr->v.identifier, "Nothing") == 0);
	expr = def->value; /* True */
	EXPECT(expr->type == EXPR_LIT_BOOL);
	EXPECT(expr->v.lit_bool == 1);
	/* equal (Just a) (Just b) = equal a b */
	def = stmt->v.def_instance.defs[1];
	EXPECT(strcmp(def->name, "equal") == 0);
	EXPECT(def->args_len == 2);
	expr = def->args[0]; /* (Just a) */
	EXPECT(expr->type == EXPR_GROUPING);
	expr = def->args[0]->v.grouping; /* Just a */
	EXPECT(expr->type == EXPR_APPLICATION);
	EXPECT(strcmp(expr->v.application.fn, "Just") == 0);
	EXPECT(expr->v.application.args_len == 1);
	EXPECT(expr->v.application.args != NULL);
	expr = def->args[0]->v.grouping->v.application.args[0]; /* a */
	EXPECT(expr->type == EXPR_IDENTIFIER);
	EXPECT(strcmp(expr->v.identifier, "a") == 0);
	expr = def->args[1]; /* (Just b) */
	EXPECT(expr->type == EXPR_GROUPING);
	expr = def->args[1]->v.grouping; /* Just b */
	EXPECT(expr->type == EXPR_APPLICATION);
	EXPECT(strcmp(expr->v.application.fn, "Just") == 0);
	EXPECT(expr->v.application.args_len == 1);
	EXPECT(expr->v.application.args != NULL);
	expr = def->args[1]->v.grouping->v.application.args[0]; /* b */
	EXPECT(expr->type == EXPR_IDENTIFIER);
	EXPECT(strcmp(expr->v.identifier, "b") == 0);
	expr = def->value; /* equal a b */
	EXPECT(expr->type == EXPR_APPLICATION);
	EXPECT(strcmp(expr->v.application.fn, "equal") == 0);
	EXPECT(expr->v.application.args_len == 2);
	EXPECT(expr->v.application.args != NULL);
	expr = def->value->v.application.args[0]; /* a */
	EXPECT(expr->type == EXPR_IDENTIFIER);
	EXPECT(strcmp(expr->v.identifier, "a") == 0);
	expr = def->value->v.application.args[1]; /* b */
	EXPECT(expr->type == EXPR_IDENTIFIER);
	EXPECT(strcmp(expr->v.identifier, "b") == 0);
	/* equal _ _ = False */
	def = stmt->v.def_instance.defs[2];
	EXPECT(strcmp(def->name, "equal") == 0);
	EXPECT(def->args_len == 2);
	expr = def->args[0]; /* _ */
	EXPECT(expr->type == EXPR_IDENTIFIER);
	EXPECT(strcmp(expr->v.identifier, "_") == 0);
	expr = def->args[1]; /* _ */
	EXPECT(expr->type == EXPR_IDENTIFIER);
	EXPECT(strcmp(expr->v.identifier, "_") == 0);
	expr = def->value; /* False */
	EXPECT(expr->type == EXPR_LIT_BOOL);
	EXPECT(expr->v.lit_bool == 0);
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
	TEST(parse_stmt_parses_instance_definitions);
}
