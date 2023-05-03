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

static int
consume(struct parser *p, enum token_type token_type, char *error_msg) {
	int result;
	assert(!is_at_end(p));
	result = match(p, token_type);
	if (!result) {
		struct token *token = peek(p);
		report_error_at(p->error_log, error_msg, token->lexeme_index);
	}
	return result;
}

static char *copy_text(struct parser *p, char *text, size_t len) {
	char *text_copy = arena_push_array_zero(p->arena, len + 1, char);
	strncpy(text_copy, text, len);
	text_copy[len] = '\0'; /* ensure null terminator */
	return text_copy;
}

static char *parse_identifier(struct parser *p, char *error_msg) {
	struct token *token;
	if (!consume(p, TOK_IDENTIFIER, error_msg)) {
		return NULL;
	}
	token = previous(p);
	return copy_text(p, token->lexeme, token->lexeme_len);
}

/* ========== EXPRESSIONS ========== */

static int is_primary(enum token_type token_type) {
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

static struct expr *parse_primary(struct parser *p) {
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
		if (sub_expr == NULL ||
		    !consume(p, TOK_PAREN_R, "Expected ')' after expression.")) {
			return NULL;
		}
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

static struct expr *parse_application(struct parser *p) {
	if (peek(p)->type == TOK_IDENTIFIER && is_primary(peek_next(p)->type)) {
		struct token *token    = advance(p);
		struct expr *expr      = arena_push_struct_zero(p->arena, struct expr);
		expr->type             = EXPR_APPLICATION;
		expr->v.application.fn = token->lexeme;
		expr->v.application.args =
			arena_push_array_zero(p->arena, RACC_MAX_FUNC_ARGS, struct expr *);
		expr->v.application.args_len = 0;
		while (!is_at_end(p) && is_primary(peek(p)->type)) {
			struct expr *arg = parse_primary(p);
			if (arg == NULL) {
				return NULL;
			}
			expr->v.application.args[expr->v.application.args_len] = arg;
			expr->v.application.args_len++;
		}
		return expr;
	}
	return parse_primary(p);
}

static struct expr *parse_unary(struct parser *p) {
	if (match(p, TOK_SUB)) {
		struct expr *expr;
		struct token *op = previous(p);
		struct expr *rhs = parse_unary(p);
		if (rhs == NULL) {
			return NULL;
		}
		expr              = arena_push_struct_zero(p->arena, struct expr);
		expr->type        = EXPR_UNARY;
		expr->v.unary.op  = op;
		expr->v.unary.rhs = rhs;
		return expr;
	}
	return parse_application(p);
}

static struct expr *parse_factor(struct parser *p) {
	struct expr *expr = parse_unary(p);
	if (expr == NULL) {
		return NULL;
	}
	while (match(p, TOK_MUL) || match(p, TOK_DIV)) {
		struct expr *lhs = expr;
		struct token *op = previous(p);
		struct expr *rhs = parse_unary(p);
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

static struct expr *parse_term(struct parser *p) {
	struct expr *expr = parse_factor(p);
	if (expr == NULL) {
		return NULL;
	}
	while (match(p, TOK_ADD) || match(p, TOK_SUB)) {
		struct expr *lhs = expr;
		struct token *op = previous(p);
		struct expr *rhs = parse_factor(p);
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

static struct expr *parse_comparison(struct parser *p) {
	struct expr *expr = parse_term(p);
	if (expr == NULL) {
		return NULL;
	}
	while (match(p, TOK_LT) || match(p, TOK_LT_EQ) || match(p, TOK_GT) ||
	       match(p, TOK_GT_EQ)) {
		struct expr *lhs = expr;
		struct token *op = previous(p);
		struct expr *rhs = parse_term(p);
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

static struct expr *parse_equality(struct parser *p) {
	struct expr *expr = parse_comparison(p);
	if (expr == NULL) {
		return NULL;
	}
	while (match(p, TOK_EQ_EQ) || match(p, TOK_NE)) {
		struct expr *lhs = expr;
		struct token *op = previous(p);
		struct expr *rhs = parse_comparison(p);
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
	expr = parse_equality(p);
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
	if (!consume(p, TOK_IDENTIFIER, "Expected type identifier")) {
		return NULL;
	}
	token_name = previous(p);
	type->name = copy_text(p, token_name->lexeme, token_name->lexeme_len);
	return type;
}

static struct type *parse_type_primary(struct parser *p) {
	if (match(p, TOK_PAREN_L)) {
		struct type *type = parse_type(p);
		if (!consume(p, TOK_PAREN_R, "Expected ')' after expression.")) {
			return NULL;
		}
		return type;
	}

	return parse_type_name(p);
}

static struct type *parse_type_parameterized(struct parser *p) {
	if (peek(p)->type == TOK_IDENTIFIER && is_type_primary(peek_next(p)->type)) {
		struct type *type = parse_type_name(p);
		struct list list  = list_new(arena_alloc());
		while (!is_at_end(p) && is_type_primary(peek(p)->type)) {
			list_append(&list, parse_type_name(p));
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
	constraint       = arena_push_struct_zero(p->arena, struct type_constraint);
	constraint_args  = list_new(arena_alloc());
	constraint->name = parse_identifier(p, "Expected class name");
	if (constraint->name == NULL) {
		return NULL;
	}
	while (peek(p)->type == TOK_IDENTIFIER) {
		char *arg = parse_identifier(p, "Expected type variable");
		if (arg == NULL) {
			return NULL;
		}
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
		return NULL;
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
	if (!consume(p, TOK_SQUARE_R, "Missing closing ']' after type context") ||
	    !consume(p, TOK_EQ_ARROW, "Expected '=>' after type context")) {
		return NULL;
	};
	return context;
}

struct type *parse_type(struct parser *p) {
	struct type *type;
	struct type_context *context;
	context = parse_type_context(p);
	if (p->error_log->had_error) {
		return NULL;
	}
	type = parse_type_arrow(p);
	if (type != NULL) {
		type->context = context;
	}
	return type;
}

/* ========== STATEMENTS ========== */

struct dec_type *parse_dec_type(struct parser *p) {
	struct dec_type *dec_type = arena_push_struct_zero(p->arena, struct dec_type);
	struct token *token;
	if (!consume(p, TOK_IDENTIFIER, "Expected identifier")) {
		return NULL;
	}
	token          = previous(p);
	dec_type->name = copy_text(p, token->lexeme, token->lexeme_len);
	if (!consume(p, TOK_COLON_COLON, "Expected '::' after identifier")) {
		return NULL;
	}
	dec_type->type = parse_type(p);
	if (!consume(p, TOK_SEMICOLON, "Expected ';' after type")) {
		return NULL;
	}
	return dec_type;
}

static struct stmt *parse_stmt_class(struct parser *p) {
	struct stmt *stmt          = arena_push_struct_zero(p->arena, struct stmt);
	struct list declarations   = list_new(arena_alloc());
	stmt->type                 = STMT_DEC_CLASS;
	stmt->v.dec_class.name     = parse_identifier(p, "Expected class name");
	stmt->v.dec_class.type_var = parse_identifier(p, "Expected type variable");
	if (!consume(p, TOK_CURLY_L, "Expected '{' before class declaration")) {
		return NULL;
	}
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
	if (!consume(p, TOK_CURLY_R, "Expected '}' after class declaration")) {
		return NULL;
	}
	return stmt;
}

struct stmt *parse_stmt(struct parser *p) {
	struct token *token = advance(p);

	switch (token->type) {
	case TOK_CLASS: return parse_stmt_class(p);
	default: assert(0);
	}
}
