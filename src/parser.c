#include "parser.h"
#include "arena.h"
#include "ast.h"
#include "error.h"
#include "token.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RACC_MAX_FUNC_ARGS 32

/* expr        -> equality
 * equality    -> comparison ( ( "/=" | "==" ) comparison )*
 * comparison  -> term ( ( ">" | "<" | ">=" | "<=" ) term )*
 * term        -> factor ( ( "-" | "+" ) factor )*
 * factor      -> unary ( ( "/" | "*" ) unary )*
 * unary       ->  "-" unary  |  application
 * application -> IDENTIFIER primary+ | primary
 * primary     -> IDENTIFIER | INT | DOUBLE | STRING | "true" | "false"
 *              | "(" expr ")"
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
		expr->v.identifier = token->lexeme;
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
		size_t lit_string_len = token->lexeme_len - 1; /* -2 for ", +1 for \0 */
		expr->type            = EXPR_LIT_STRING;
		expr->v.lit_string    = arena_push_array(p->arena, lit_string_len, char);
		strlcpy(expr->v.lit_string, &token->lexeme[1], lit_string_len);
		expr->v.lit_string[lit_string_len - 1] = '\0'; /* null terminator */
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
		if (sub_expr == NULL || !consume(p, TOK_PAREN_R, "Expected ')' after expression.")) {
			return NULL;
		}
		expr->type       = EXPR_GROUPING;
		expr->v.grouping = sub_expr;
		break;
	}
	case TOK_EOF:
		report_error_at(
			p->error_log, "Unexpected end of file", token->lexeme_index
		);
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
