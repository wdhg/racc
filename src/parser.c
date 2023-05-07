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

static int match(struct parser *p, enum token_type token_type) {
	if (peek(p)->type == token_type) {
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
		var   = copy_text(p, token->lexeme, token->lexeme_len);                    \
	}

static char *copy_text(struct parser *p, char *text, size_t len) {
	char *text_copy = arena_push_array_zero(p->arena, len + 1, char);
	strncpy(text_copy, text, len);
	text_copy[len] = '\0'; /* ensure null terminator */
	return text_copy;
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
		expr->type         = EXPR_IDENTIFIER;
		expr->v.identifier = copy_text(p, token->lexeme, token->lexeme_len + 1);
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
			copy_text(p, &token->lexeme[1], token->lexeme_len - 2); /* -2 " */
		break;
	}
	case TOK_CHAR:
		expr->type = EXPR_LIT_CHAR;
		/* TODO escaped characters */
		sscanf(token->lexeme, "'%c'", &expr->v.lit_char);
		break;
	case TOK_BOOL:
		expr->type       = EXPR_LIT_BOOL;
		expr->v.lit_bool = strcmp(token->lexeme, "true") == 0;
		break;
	case TOK_PAREN_L: {
		struct expr *sub_expr = parse_expr(p);
		if (sub_expr == NULL) {
			return NULL;
		}
		CONSUME(TOK_PAREN_R, "Expected ')' after expression.");
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
	if (peek(p)->type == TOK_IDENTIFIER && is_expr_primary(peek_next(p)->type)) {
		struct token *token    = advance(p);
		struct expr *expr      = arena_push_struct_zero(p->arena, struct expr);
		expr->type             = EXPR_APPLICATION;
		expr->v.application.fn = token->lexeme;
		expr->v.application.args =
			arena_push_array_zero(p->arena, RACC_MAX_FUNC_ARGS, struct expr *);
		expr->v.application.args_len = 0;
		while (!is_at_end(p) && is_expr_primary(peek(p)->type)) {
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
		expr              = arena_push_struct_zero(p->arena, struct expr);
		expr->type        = EXPR_UNARY;
		expr->v.unary.op  = op;
		expr->v.unary.rhs = rhs;
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
		expr               = arena_push_struct_zero(p->arena, struct expr);
		expr->type         = EXPR_BINARY;
		expr->v.binary.lhs = lhs;
		expr->v.binary.op  = op;
		expr->v.binary.rhs = rhs;
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
		expr               = arena_push_struct_zero(p->arena, struct expr);
		expr->type         = EXPR_BINARY;
		expr->v.binary.lhs = lhs;
		expr->v.binary.op  = op;
		expr->v.binary.rhs = rhs;
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
		expr               = arena_push_struct_zero(p->arena, struct expr);
		expr->type         = EXPR_BINARY;
		expr->v.binary.lhs = lhs;
		expr->v.binary.op  = op;
		expr->v.binary.rhs = rhs;
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
		expr               = arena_push_struct_zero(p->arena, struct expr);
		expr->type         = EXPR_BINARY;
		expr->v.binary.lhs = lhs;
		expr->v.binary.op  = op;
		expr->v.binary.rhs = rhs;
	}
	return expr;
}

struct expr *parse_expr(struct parser *p) {
	struct expr *expr;
	expr = parse_expr_equality(p);
	return expr;
}

/* ========== TYPES ========== */

static int is_type_primary(enum token_type type) {
	switch (type) {
	case TOK_IDENTIFIER:
	case TOK_PAREN_L: return 1;
	default: return 0;
	}
}

static struct type *parse_type_name(struct parser *p) {
	struct type *type = arena_push_struct_zero(p->arena, struct type);
	struct token *token_name;
	CONSUME(TOK_IDENTIFIER, "Expected type identifier");
	token_name = previous(p);
	type->name = copy_text(p, token_name->lexeme, token_name->lexeme_len);
	return type;
}

static struct type *parse_type_free(struct parser *p);

static struct type *parse_type_primary(struct parser *p) {
	if (match(p, TOK_PAREN_L)) {
		struct type *type = parse_type_free(p);
		CONSUME(TOK_PAREN_R, "Expected ')' after expression.");
		return type;
	}

	return parse_type_name(p);
}

static struct type *parse_type_parameterized(struct parser *p) {
	if (peek(p)->type == TOK_IDENTIFIER && is_type_primary(peek_next(p)->type)) {
		struct type *type = parse_type_name(p);
		struct list list  = list_new(arena_alloc());
		while (!is_at_end(p) && is_type_primary(peek(p)->type)) {
			struct type *type = parse_type_name(p);
			if (type == NULL) {
				return NULL;
			}
			list_append(&list, type);
		}
		type->args     = (struct type **)list_to_array(&list, p->arena);
		type->args_len = list_length(&list);
		arena_free(list.arena);
		return type;
	}
	return parse_type_primary(p);
}

static struct type *parse_type_arrow(struct parser *p) {
	struct type *type = parse_type_parameterized(p);

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
	while (peek(p)->type == TOK_IDENTIFIER) {
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
	if (!match(p, TOK_SQUARE_L)) {
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
	CONSUME(TOK_SQUARE_R, "Missing closing ']' after type context");
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
	CONSUME(TOK_CLASS, "Expected class keyword");
	PARSE_IDENTIFIER(stmt->v.dec_class.name, "Expected class name");
	PARSE_IDENTIFIER(stmt->v.dec_class.type_var, "Expected type variable");
	CONSUME(TOK_CURLY_L, "Expected '{' before class declaration");
	while (!is_at_end(p) && peek(p)->type == TOK_IDENTIFIER) {
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
	while (is_type_primary(peek(p)->type)) {
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
	CONSUME(TOK_DATA, "Expected data keyword");
	PARSE_IDENTIFIER(stmt->v.dec_data.name, "Expected data identifier");
	while (peek(p)->type == TOK_IDENTIFIER) {
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

struct stmt *parse_stmt(struct parser *p) {
	switch (peek(p)->type) {
	case TOK_CLASS: return parse_stmt_class(p);
	case TOK_DATA: return parse_stmt_data(p);
	case TOK_IDENTIFIER:
		return peek_next(p)->type == TOK_COLON_COLON ? parse_stmt_dec_type(p)
		                                             : parse_stmt_def_value(p);
	default: assert(0);
	}
}
