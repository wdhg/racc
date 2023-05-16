#include "parser.h"
#include "arena.h"
#include "ast.h"
#include "error.h"
#include "list.h"
#include "token.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RACC_MAX_FUNC_ARGS 32

/* program           -> stmt* EOF
 *
 * stmt              -> declaration | definition
 *
 * declaration       -> declrClass | declrData | declrType
 * declrClass        -> "class" IDENTIFIER IDENTIFIER+ "{" declrType+ "}"
 * declrData         -> "data" IDENTIFIER IDENTIFIER* "{"
 *                      constructor ("|" constructor)+ "}"
 * constructor       -> IDENTIFIER IDENTIFIER* ";"
 * declrType         -> IDENTIFIER "::" type ("->" type)* ";"
 * definition        -> defInstance | defValue
 * defInstance       -> "instance" IDENTIFIER IDENTIFIER+ "{" defValue+ "}"
 * defValue          -> IDENTIFIER IDENTIFIER "=" expr ";"
 *
 * type              -> typeContext? typeArrow
 * typeContext       -> "[" typeName typeName+ ("," typeName typeName+ )* "]"
 *                      "=>"
 * typeArrow         -> typeParameterized ( "->" typeArrow )*
 * typeParameterized -> typeName typePrimary+ | typePrimary
 * typePrimary       -> typeName | "(" type ")"
 * typeName          -> IDENTIFIER
 *
 * expr              -> exprEquality
 * exprEquality      -> exprComparison ( ( "/=" | "==" ) exprComparison )*
 * exprComparison    -> exprTerm ( ( ">" | "<" | ">=" | "<=" ) exprTerm )*
 * exprTerm          -> exprFactor ( ( "-" | "+" ) exprFactor )*
 * exprFactor        -> exprUnary ( ( "/" | "*" ) exprUnary )*
 * exprUnary         ->  "-" exprUnary  |  exprApplication
 * exprApplication   -> IDENTIFIER exprPrimary+ | exprPrimary
 * exprPrimary       -> IDENTIFIER | INT | DOUBLE | STRING | "true" | "false"
 *                    | "(" expr ")"
 */

/* ========== PARSER ========== */

static int is_at_end(struct parser *p) {
	return p->tokens[p->current]->type == TOK_EOF;
}

static struct token *advance(struct parser *p) {
	struct token *token;
	token = p->tokens[p->current];
	p->current++;
	return token;
}

static struct token *peek(struct parser *p) { return p->tokens[p->current]; }

static struct token *peek_next(struct parser *p) {
	assert(!is_at_end(p));
	return p->tokens[p->current + 1];
}

static enum token_type peek_type(struct parser *p) { return peek(p)->type; }

static enum token_type peek_type_next(struct parser *p) {
	return peek_next(p)->type;
}

static int match(struct parser *p, enum token_type token_type) {
	if (peek_type(p) == token_type) {
		advance(p);
		return 1;
	}
	return 0;
}

static struct token *previous(struct parser *p) {
	assert(p->current > 0);
	return p->tokens[p->current - 1];
}

#define CONSUME(token_type, error_msg)                                         \
	{                                                                            \
		if (!match(p, token_type)) {                                               \
			struct token *token = peek(p);                                           \
			report_error_at(p->error_log, error_msg, token->lexeme_index);           \
			return NULL;                                                             \
		}                                                                          \
	}

#define PARSE_IDENTIFIER(var, error_msg)                                       \
	{                                                                            \
		struct token *token;                                                       \
		CONSUME(TOK_IDENTIFIER, error_msg);                                        \
		token = previous(p);                                                       \
		var   = copy_lexeme(p->arena, token);                                      \
	}

static char *copy_text(struct arena *arena, char *text, size_t len) {
	char *text_copy = arena_push_array_zero(arena, len + 1, char);
	strncpy(text_copy, text, len);
	text_copy[len] = '\0'; /* ensure null terminator */
	return text_copy;
}

static char *copy_lexeme(struct arena *arena, struct token *token) {
	return copy_text(arena, token->lexeme, token->lexeme_len);
}

/* ========== EXPRESSIONS ========== */

static int is_expr_primary(enum token_type token_type) {
	switch (token_type) {
	case TOK_IDENTIFIER:
	case TOK_INT:
	case TOK_DOUBLE:
	case TOK_STRING:
	case TOK_CHAR:
	case TOK_BOOL:
	case TOK_PAREN_L: return 1;
	default: return 0;
	}
}

static struct expr *parse_expr_primary(struct parser *p) {
	struct token *token = advance(p);
	struct expr *expr   = arena_push_struct_zero(p->arena, struct expr);
	switch (token->type) {
	case TOK_IDENTIFIER:
		expr->type = EXPR_IDENTIFIER;
		expr->v.identifier =
			copy_text(p->arena, token->lexeme, token->lexeme_len + 1);
		break;
	case TOK_INT:
		expr->type = EXPR_LIT_INT;
		sscanf(token->lexeme, "%d", &expr->v.lit_int);
		break;
	case TOK_DOUBLE:
		expr->type = EXPR_LIT_DOUBLE;
		sscanf(token->lexeme, "%lf", &expr->v.lit_double);
		break;
	case TOK_STRING: {
		expr->type = EXPR_LIT_STRING;
		expr->v.lit_string =
			copy_text(p->arena, &token->lexeme[1], token->lexeme_len - 2); /* -2 " */
		break;
	}
	case TOK_CHAR:
		expr->type = EXPR_LIT_CHAR;
		/* TODO escaped characters */
		sscanf(token->lexeme, "'%c'", &expr->v.lit_char);
		break;
	case TOK_BOOL:
		expr->type       = EXPR_LIT_BOOL;
		expr->v.lit_bool = strcmp(token->lexeme, "True") == 0;
		break;
	case TOK_PAREN_L: {
		struct expr *sub_expr = parse_expr(p);
		if (sub_expr == NULL) {
			return NULL;
		}
		CONSUME(TOK_PAREN_R, "Expected ')' after expression");
		expr->type       = EXPR_GROUPING;
		expr->v.grouping = sub_expr;
		break;
	}
	case TOK_EOF:
		report_error_at(
			p->error_log, "Unexpected end of file", token->lexeme_index);
		return NULL;
	default:
		report_error_at(p->error_log, "Unexpected token", token->lexeme_index);
		return NULL;
	}

	return expr;
}

static struct expr *parse_expr_application(struct parser *p) {
	if (peek_type(p) == TOK_IDENTIFIER && is_expr_primary(peek_type_next(p))) {
		struct token *token    = advance(p);
		struct expr *expr      = arena_push_struct_zero(p->arena, struct expr);
		expr->type             = EXPR_APPLICATION;
		expr->v.application.fn = token->lexeme;
		expr->v.application.args =
			arena_push_array_zero(p->arena, RACC_MAX_FUNC_ARGS, struct expr *);
		expr->v.application.args_len = 0;
		while (is_expr_primary(peek_type(p))) {
			struct expr *arg = parse_expr_primary(p);
			if (arg == NULL) {
				return NULL;
			}
			expr->v.application.args[expr->v.application.args_len] = arg;
			expr->v.application.args_len++;
		}
		return expr;
	}
	return parse_expr_primary(p);
}

static struct expr *parse_expr_unary(struct parser *p) {
	if (match(p, TOK_SUB)) {
		struct expr *expr;
		struct token *op = previous(p);
		struct expr *rhs = parse_expr_unary(p);
		if (rhs == NULL) {
			return NULL;
		}
		expr                   = arena_push_struct_zero(p->arena, struct expr);
		expr->type             = EXPR_APPLICATION;
		expr->v.application.fn = copy_lexeme(p->arena, op);
		expr->v.application.args_len = 1;
		expr->v.application.args     = arena_push_array_zero(
      p->arena, expr->v.application.args_len, struct expr *);
		expr->v.application.args[0] = rhs;
		return expr;
	}
	return parse_expr_application(p);
}

static struct expr *parse_expr_factor(struct parser *p) {
	struct expr *expr = parse_expr_unary(p);
	if (expr == NULL) {
		return NULL;
	}
	while (match(p, TOK_MUL) || match(p, TOK_DIV)) {
		struct expr *lhs = expr;
		struct token *op = previous(p);
		struct expr *rhs = parse_expr_unary(p);
		if (rhs == NULL) {
			return NULL;
		}
		expr                   = arena_push_struct_zero(p->arena, struct expr);
		expr->type             = EXPR_APPLICATION;
		expr->v.application.fn = copy_lexeme(p->arena, op);
		expr->v.application.args_len = 2;
		expr->v.application.args     = arena_push_array_zero(
      p->arena, expr->v.application.args_len, struct expr *);
		expr->v.application.args[0] = lhs;
		expr->v.application.args[1] = rhs;
	}
	return expr;
}

static struct expr *parse_expr_term(struct parser *p) {
	struct expr *expr = parse_expr_factor(p);
	if (expr == NULL) {
		return NULL;
	}
	while (match(p, TOK_ADD) || match(p, TOK_SUB)) {
		struct expr *lhs = expr;
		struct token *op = previous(p);
		struct expr *rhs = parse_expr_factor(p);
		if (rhs == NULL) {
			return NULL;
		}
		expr                   = arena_push_struct_zero(p->arena, struct expr);
		expr->type             = EXPR_APPLICATION;
		expr->v.application.fn = copy_lexeme(p->arena, op);
		expr->v.application.args_len = 2;
		expr->v.application.args     = arena_push_array_zero(
      p->arena, expr->v.application.args_len, struct expr *);
		expr->v.application.args[0] = lhs;
		expr->v.application.args[1] = rhs;
	}
	return expr;
}

static struct expr *parse_expr_comparison(struct parser *p) {
	struct expr *expr = parse_expr_term(p);
	if (expr == NULL) {
		return NULL;
	}
	while (match(p, TOK_LT) || match(p, TOK_LT_EQ) || match(p, TOK_GT) ||
	       match(p, TOK_GT_EQ)) {
		struct expr *lhs = expr;
		struct token *op = previous(p);
		struct expr *rhs = parse_expr_term(p);
		if (rhs == NULL) {
			return NULL;
		}
		expr                   = arena_push_struct_zero(p->arena, struct expr);
		expr->type             = EXPR_APPLICATION;
		expr->v.application.fn = copy_lexeme(p->arena, op);
		expr->v.application.args_len = 2;
		expr->v.application.args     = arena_push_array_zero(
      p->arena, expr->v.application.args_len, struct expr *);
		expr->v.application.args[0] = lhs;
		expr->v.application.args[1] = rhs;
	}
	return expr;
}

static struct expr *parse_expr_equality(struct parser *p) {
	struct expr *expr = parse_expr_comparison(p);
	if (expr == NULL) {
		return NULL;
	}
	while (match(p, TOK_EQ_EQ) || match(p, TOK_NE)) {
		struct expr *lhs = expr;
		struct token *op = previous(p);
		struct expr *rhs = parse_expr_comparison(p);
		if (rhs == NULL) {
			return NULL;
		}
		expr                   = arena_push_struct_zero(p->arena, struct expr);
		expr->type             = EXPR_APPLICATION;
		expr->v.application.fn = copy_lexeme(p->arena, op);
		expr->v.application.args_len = 2;
		expr->v.application.args     = arena_push_array_zero(
      p->arena, expr->v.application.args_len, struct expr *);
		expr->v.application.args[0] = lhs;
		expr->v.application.args[1] = rhs;
	}
	return expr;
}

struct expr *parse_expr(struct parser *p) {
	struct expr *expr;
	expr = parse_expr_equality(p);
	return expr;
}

/* ========== TYPES ========== */

static struct type *parse_type_free(struct parser *p);

static struct region_sort *parse_type_region_sort(struct parser *p) {
	struct region_sort *region_sort =
		arena_push_struct_zero(p->arena, struct region_sort);

	if (peek_type(p) == TOK_IDENTIFIER) {
		PARSE_IDENTIFIER(region_sort->name, "Expected region variable");
	}

	if (match(p, TOK_AT)) {
		struct list sub_sorts = list_new(arena_alloc());
		CONSUME(TOK_PAREN_L, "Expected opening '(' for region sort");
		do {
			struct region_sort *sub_sort = parse_type_region_sort(p);
			list_append(&sub_sorts, sub_sort);
		} while (match(p, TOK_COMMA));
		CONSUME(TOK_PAREN_R, "Expected closing ')' for region sort");
		region_sort->sub_sorts =
			(struct region_sort **)list_to_array(&sub_sorts, p->arena);
		region_sort->sub_sorts_len = list_length(&sub_sorts);
		arena_free(sub_sorts.arena);
	}

	return region_sort;
}

static int is_type_primary(enum token_type type) {
	switch (type) {
	case TOK_IDENTIFIER:
	case TOK_SQUARE_L:
	case TOK_PAREN_L: return 1;
	default: return 0;
	}
}

static struct type *parse_type_name(struct parser *p) {
	struct type *type = arena_push_struct_zero(p->arena, struct type);
	PARSE_IDENTIFIER(type->name, "Expected type name");
	return type;
}

static struct type *parse_type_primary_bracketed(struct parser *p) {
	struct type *type;
	size_t sub_types_len;
	struct list sub_types = list_new(arena_alloc());

	CONSUME(TOK_PAREN_L, "Expected '('");

	do {
		struct type *sub_type = parse_type_free(p);
		if (sub_type == NULL) {
			break;
		}
		list_append(&sub_types, sub_type);
	} while (match(p, TOK_COMMA));

	sub_types_len = list_length(&sub_types);

	if (sub_types_len == 0) {
		type           = arena_push_struct_zero(p->arena, struct type);
		type->name     = "()";
		type->args_len = 0;
	} else if (sub_types_len == 1) {
		/* grouped type */
		type = list_to_array(&sub_types, p->arena)[0];
	} else {
		/* tuple type */
		size_t i;
		size_t name_len = 2 + sub_types_len - 1;
		type            = arena_push_struct_zero(p->arena, struct type);
		type->name      = arena_push_array_zero(p->arena, name_len + 1, char);
		type->name[0]   = '(';
		for (i = 0; i < sub_types_len; i++) {
			type->name[1 + i] = ',';
		}
		type->name[name_len - 1] = ')';
		type->name[name_len]     = '\0';
		type->args     = (struct type **)list_to_array(&sub_types, p->arena);
		type->args_len = list_length(&sub_types);
	}

	arena_free(sub_types.arena);

	CONSUME(TOK_PAREN_R, "Expected ')' after type");

	return type;
}

static struct type *parse_type_primary_list(struct parser *p) {
	struct type *type;
	CONSUME(TOK_SQUARE_L, "Expected '['");
	type           = arena_push_struct_zero(p->arena, struct type);
	type->name     = "[]";
	type->args_len = 1;
	type->args     = arena_push_array_zero(p->arena, 1, struct type *);
	type->args[0]  = parse_type_free(p);
	CONSUME(TOK_SQUARE_R, "Missing closing ']'");
	return type;
}

static struct type *parse_type_primary(struct parser *p) {
	switch (peek_type(p)) {
	case TOK_PAREN_L: return parse_type_primary_bracketed(p);
	case TOK_SQUARE_L: return parse_type_primary_list(p);
	case TOK_IDENTIFIER: return parse_type_name(p);
	default: assert(0);
	}
}

static struct type *parse_type_parameterized(struct parser *p) {
	struct type *type;
	struct list args;
	int is_parameterized_type =
		peek_type(p) == TOK_IDENTIFIER && is_type_primary(peek_type_next(p));

	if (!is_parameterized_type) {
		return parse_type_primary(p);
	}

	type = parse_type_name(p);
	args = list_new(arena_alloc());

	while (is_type_primary(peek_type(p))) {
		struct type *arg = parse_type_primary(p);
		if (arg == NULL) {
			return NULL;
		}
		list_append(&args, arg);
	}

	type->args     = (struct type **)list_to_array(&args, p->arena);
	type->args_len = list_length(&args);
	arena_free(args.arena);

	return type;
}

static struct type *parse_type_annotated(struct parser *p) {
	struct type *type = parse_type_parameterized(p);
	if (type != NULL && match(p, TOK_TICK)) {
		type->region_sort = parse_type_region_sort(p);
	}
	return type;
}

static struct type *parse_type_arrow(struct parser *p) {
	struct type *type = parse_type_annotated(p);

	while (match(p, TOK_ARROW)) {
		struct type *lhs = type;
		struct type *rhs = arena_push_struct_zero(p->arena, struct type);
		rhs              = parse_type_arrow(p);
		type             = arena_push_struct_zero(p->arena, struct type);
		type->name       = "->";
		type->args       = arena_push_array_zero(p->arena, 2, struct type *);
		type->args[0]    = lhs;
		type->args[1]    = rhs;
		type->args_len   = 2;
	}

	return type;
}

static struct type_constraint *parse_type_constraint(struct parser *p) {
	struct type_constraint *constraint;
	struct list constraint_args;
	constraint      = arena_push_struct_zero(p->arena, struct type_constraint);
	constraint_args = list_new(arena_alloc());
	PARSE_IDENTIFIER(constraint->name, "Expected class name");
	while (peek_type(p) == TOK_IDENTIFIER) {
		char *arg;
		PARSE_IDENTIFIER(arg, "Expected type variable");
		list_append(&constraint_args, arg);
	}
	constraint->args     = (char **)list_to_array(&constraint_args, p->arena);
	constraint->args_len = list_length(&constraint_args);
	arena_free(constraint_args.arena);
	return constraint;
}

struct type_context *parse_type_context(struct parser *p) {
	struct type_context *context;
	struct list constraints;
	struct type_constraint *constraint;
	if (!match(p, TOK_LT)) {
		return NULL; /* type doesn't have a context */
	}
	context     = arena_push_struct_zero(p->arena, struct type_context);
	constraints = list_new(arena_alloc());
	constraint  = parse_type_constraint(p);
	if (constraint == NULL) {
		return NULL;
	}
	list_append(&constraints, constraint);
	while (match(p, TOK_COMMA)) {
		constraint = parse_type_constraint(p);
		if (constraint == NULL) {
			return NULL;
		}
		list_append(&constraints, constraint);
	}
	context->constraints =
		(struct type_constraint **)list_to_array(&constraints, p->arena);
	context->constraints_len = list_length(&constraints);
	arena_free(constraints.arena);
	CONSUME(TOK_GT, "Missing closing '>' after type context");
	CONSUME(TOK_EQ_ARROW, "Expected '=>' after type context");
	return context;
}

static struct type *parse_type_free(struct parser *p) {
	return parse_type_arrow(p);
}

struct type *parse_type(struct parser *p) {
	struct type *type;
	struct type_context *context;
	context = parse_type_context(p);
	if (p->error_log->had_error) {
		return NULL;
	}
	type = parse_type_free(p);
	if (type != NULL) {
		type->context = context;
	}
	return type;
}

/* ========== STATEMENTS ========== */

static struct dec_type *parse_dec_type(struct parser *p) {
	struct dec_type *dec_type = arena_push_struct_zero(p->arena, struct dec_type);
	PARSE_IDENTIFIER(dec_type->name, "Expected declaration identifier");
	CONSUME(TOK_COLON_COLON, "Expected '::' after identifier");
	dec_type->type = parse_type(p);
	CONSUME(TOK_SEMICOLON, "Expected ';' after type");
	return dec_type;
}

static struct def_value *parse_def_value(struct parser *p) {
	struct def_value *def_value =
		arena_push_struct_zero(p->arena, struct def_value);
	struct list args = list_new(arena_alloc());
	PARSE_IDENTIFIER(def_value->name, "Expected definition identifier");
	while (!match(p, TOK_EQ)) {
		struct expr *arg = parse_expr_primary(p);
		if (arg == NULL) {
			return NULL;
		}
		list_append(&args, arg);
	}
	def_value->args     = (struct expr **)list_to_array(&args, p->arena);
	def_value->args_len = list_length(&args);
	arena_free(args.arena);
	def_value->value = parse_expr(p);
	CONSUME(TOK_SEMICOLON, "Expected ';' after expression");
	return def_value;
}

static struct stmt *parse_stmt_class(struct parser *p) {
	struct stmt *stmt        = arena_push_struct_zero(p->arena, struct stmt);
	struct list declarations = list_new(arena_alloc());
	stmt->type               = STMT_DEC_CLASS;
	CONSUME(TOK_CLASS, "Expected 'class' keyword");
	PARSE_IDENTIFIER(stmt->v.dec_class.name, "Expected class name");
	PARSE_IDENTIFIER(stmt->v.dec_class.type_var, "Expected type variable");
	CONSUME(TOK_CURLY_L, "Expected '{' before class declaration");
	while (!is_at_end(p) && peek_type(p) == TOK_IDENTIFIER) {
		struct dec_type *dec_type = parse_dec_type(p);
		if (dec_type == NULL) {
			return NULL;
		}
		list_append(&declarations, dec_type);
	}
	stmt->v.dec_class.declarations =
		(struct dec_type **)list_to_array(&declarations, p->arena);
	stmt->v.dec_class.declarations_len = list_length(&declarations);
	arena_free(declarations.arena);
	CONSUME(TOK_CURLY_R, "Expected '}' after class declaration");
	return stmt;
}

static struct dec_constructor *parse_dec_constructor(struct parser *p) {
	struct dec_constructor *constructor =
		arena_push_struct_zero(p->arena, struct dec_constructor);
	struct list args = list_new(arena_alloc());
	PARSE_IDENTIFIER(constructor->name, "Expected constructor");
	while (is_type_primary(peek_type(p))) {
		struct type *type_arg = parse_type_primary(p);
		if (type_arg == NULL) {
			return constructor;
		}
		list_append(&args, type_arg);
	}
	constructor->args     = (struct type **)list_to_array(&args, p->arena);
	constructor->args_len = list_length(&args);
	arena_free(args.arena);
	return constructor;
}

static struct stmt *parse_stmt_data(struct parser *p) {
	struct stmt *stmt        = arena_push_struct_zero(p->arena, struct stmt);
	struct list type_vars    = list_new(arena_alloc());
	struct list constructors = list_new(arena_alloc());
	stmt->type               = STMT_DEC_DATA;
	CONSUME(TOK_DATA, "Expected 'data' keyword");
	stmt->v.dec_data.name = parse_type_name(p);
	while (peek_type(p) == TOK_IDENTIFIER) {
		char *type_var;
		PARSE_IDENTIFIER(type_var, "Expected type variable");
		list_append(&type_vars, type_var);
	}
	stmt->v.dec_data.type_vars     = (char **)list_to_array(&type_vars, p->arena);
	stmt->v.dec_data.type_vars_len = list_length(&type_vars);
	arena_free(type_vars.arena);
	CONSUME(TOK_CURLY_L, "Expected '{' before data declaration");
	if (match(p, TOK_CURLY_R)) {
		return stmt;
	}
	do {
		struct dec_constructor *constructor = parse_dec_constructor(p);
		if (constructor == NULL) {
			return NULL;
		}
		list_append(&constructors, constructor);
	} while (match(p, TOK_PIPE));
	stmt->v.dec_data.constructors =
		(struct dec_constructor **)list_to_array(&constructors, p->arena);
	stmt->v.dec_data.constructors_len = list_length(&constructors);
	arena_free(constructors.arena);
	CONSUME(TOK_CURLY_R, "Expected closing '}' after data declaration");
	return stmt;
}

static struct stmt *parse_stmt_dec_type(struct parser *p) {
	struct stmt *stmt = arena_push_struct_zero(p->arena, struct stmt);
	stmt->type        = STMT_DEC_TYPE;
	stmt->v.dec_type  = parse_dec_type(p);
	return stmt;
}

static struct stmt *parse_stmt_def_value(struct parser *p) {
	struct stmt *stmt = arena_push_struct_zero(p->arena, struct stmt);
	stmt->type        = STMT_DEF_VALUE;
	stmt->v.def_value = parse_def_value(p);
	return stmt;
}

static struct stmt *parse_stmt_def_instance(struct parser *p) {
	struct stmt *stmt = arena_push_struct_zero(p->arena, struct stmt);
	struct list args  = list_new(arena_alloc());
	struct list defs  = list_new(arena_alloc());
	stmt->type        = STMT_DEF_INSTANCE;
	CONSUME(TOK_INSTANCE, "Expected 'instance' keyword");
	stmt->v.def_instance.context = parse_type_context(p);
	PARSE_IDENTIFIER(stmt->v.def_instance.class_name, "Expected class name");
	do {
		struct type *arg = parse_type_primary(p);
		if (arg == NULL) {
			return NULL;
		}
		list_append(&args, arg);
	} while (!match(p, TOK_CURLY_L));
	stmt->v.def_instance.args = (struct type **)list_to_array(&args, p->arena);
	stmt->v.def_instance.args_len = list_length(&args);
	arena_free(args.arena);
	do {
		struct def_value *def = parse_def_value(p);
		if (def == NULL) {
			return NULL;
		}
		list_append(&defs, def);
	} while (!match(p, TOK_CURLY_R));
	stmt->v.def_instance.defs =
		(struct def_value **)list_to_array(&defs, p->arena);
	stmt->v.def_instance.defs_len = list_length(&defs);
	arena_free(defs.arena);
	return stmt;
}

struct stmt *parse_stmt(struct parser *p) {
	switch (peek_type(p)) {
	case TOK_CLASS: return parse_stmt_class(p);
	case TOK_DATA: return parse_stmt_data(p);
	case TOK_INSTANCE: return parse_stmt_def_instance(p);
	case TOK_IDENTIFIER:
		return peek_type_next(p) == TOK_COLON_COLON ? parse_stmt_dec_type(p)
		                                            : parse_stmt_def_value(p);
	default: assert(0);
	}
}

static void advance_to_next_statement(struct parser *p) {
	while (!match(p, TOK_SEMICOLON) || !match(p, TOK_CURLY_R)) {
		advance(p);
	}
}

struct prog *parse_prog(struct parser *p) {
	struct prog *prog = arena_push_struct_zero(p->arena, struct prog);
	struct list stmts = list_new(arena_alloc());
	while (!match(p, TOK_EOF)) {
		struct stmt *stmt = parse_stmt(p);
		if (stmt == NULL) {
			advance_to_next_statement(p);
			continue;
		}
		list_append(&stmts, stmt);
	}
	prog->stmts     = (struct stmt **)list_to_array(&stmts, p->arena);
	prog->stmts_len = list_length(&stmts);
	arena_free(stmts.arena);
	return prog;
}
