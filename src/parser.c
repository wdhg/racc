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
			report_error_at(p->log, error_msg, token->lexeme_index);                 \
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
	expr->source_index  = token->lexeme_index;
	switch (token->type) {
	case TOK_IDENTIFIER:
		expr->expr_type = EXPR_IDENTIFIER;
		expr->v.identifier =
			copy_text(p->arena, token->lexeme, token->lexeme_len + 1);
		break;
	case TOK_INT:
		expr->expr_type = EXPR_LIT_INT;
		sscanf(token->lexeme, "%d", &expr->v.lit_int);
		break;
	case TOK_DOUBLE:
		expr->expr_type = EXPR_LIT_DOUBLE;
		sscanf(token->lexeme, "%lf", &expr->v.lit_double);
		break;
	case TOK_STRING: {
		expr->expr_type = EXPR_LIT_STRING;
		expr->v.lit_string =
			copy_text(p->arena, &token->lexeme[1], token->lexeme_len - 2); /* -2 " */
		break;
	}
	case TOK_CHAR:
		expr->expr_type = EXPR_LIT_CHAR;
		/* TODO escaped characters */
		sscanf(token->lexeme, "'%c'", &expr->v.lit_char);
		break;
	case TOK_BOOL:
		expr->expr_type  = EXPR_LIT_BOOL;
		expr->v.lit_bool = strcmp(token->lexeme, "True") == 0;
		break;
	case TOK_PAREN_L: {
		struct list *sub_exprs = list_new(arena_alloc());
		size_t sub_exprs_len;

		do {
			struct expr *sub_expr = parse_expr(p);
			if (sub_expr == NULL) {
				return NULL;
			}
			list_append(sub_exprs, sub_expr);
		} while (match(p, TOK_COMMA));

		sub_exprs_len = list_length(sub_exprs);

		if (sub_exprs_len == 0) {
			assert(0);
		} else if (sub_exprs_len == 1) {
			expr->expr_type  = EXPR_GROUPING;
			expr->v.grouping = list_to_array(sub_exprs, p->arena)[0];
		} else {
			size_t i;
			size_t fn_len   = 2 + sub_exprs_len - 1;
			expr->expr_type = EXPR_APPLICATION;
			expr->v.application.fn =
				arena_push_array_zero(p->arena, fn_len + 1, char);
			expr->v.application.fn[0] = '(';
			for (i = 0; i < sub_exprs_len - 1; i++) {
				expr->v.application.fn[1 + i] = ',';
			}
			expr->v.application.fn[fn_len - 1] = ')';
			expr->v.application.fn[fn_len]     = '\0';
			expr->v.application.expr_args      = list_copy(sub_exprs, p->arena);
		}

		CONSUME(TOK_PAREN_R, "Expected ')' after expression");
		list_free(sub_exprs);
		break;
	}
	case TOK_SQUARE_L: {
		struct list *sub_exprs;
		struct list_iter sub_exprs_iter;

		expr->expr_type = EXPR_LIST_NULL;

		if (match(p, TOK_SQUARE_R)) {
			break;
		}

		sub_exprs = list_new(arena_alloc());

		do {
			struct expr *sub_expr = parse_expr(p);
			if (sub_expr == NULL) {
				return NULL;
			}
			list_append(sub_exprs, sub_expr);
		} while (match(p, TOK_COMMA));

		sub_exprs_iter = list_iterate_reverse(sub_exprs);

		while (!list_iter_at_end(&sub_exprs_iter)) {
			struct expr *sub_expr  = list_iter_next(&sub_exprs_iter);
			struct expr *cons_expr = arena_push_struct_zero(p->arena, struct expr);
			cons_expr->expr_type   = EXPR_APPLICATION;
			cons_expr->v.application.fn        = ":";
			cons_expr->v.application.expr_args = list_new(p->arena);
			list_append(cons_expr->v.application.expr_args, sub_expr);
			list_append(cons_expr->v.application.expr_args, expr);
			expr = cons_expr;
		}

		CONSUME(TOK_SQUARE_R, "Expected ']' after expression");
		list_free(sub_exprs);
		break;
	}
	case TOK_EOF:
		report_error_at(p->log, "Unexpected end of file", token->lexeme_index);
		return NULL;
	default:
		report_error_at(p->log, "Unexpected token", token->lexeme_index);
		return NULL;
	}

	return expr;
}

static struct expr *parse_expr_application(struct parser *p) {
	if (peek_type(p) == TOK_IDENTIFIER && is_expr_primary(peek_type_next(p))) {
		struct token *token    = advance(p);
		struct expr *expr      = arena_push_struct_zero(p->arena, struct expr);
		expr->expr_type        = EXPR_APPLICATION;
		expr->v.application.fn = token->lexeme;
		expr->v.application.expr_args = list_new(p->arena);
		while (is_expr_primary(peek_type(p))) {
			struct expr *arg = parse_expr_primary(p);
			if (arg == NULL) {
				return NULL;
			}
			list_append(expr->v.application.expr_args, arg);
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
		expr->expr_type        = EXPR_APPLICATION;
		expr->v.application.fn = copy_lexeme(p->arena, op);
		expr->v.application.expr_args = list_new(p->arena);
		list_append(expr->v.application.expr_args, rhs);
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
		expr->expr_type        = EXPR_APPLICATION;
		expr->v.application.fn = copy_lexeme(p->arena, op);
		expr->v.application.expr_args = list_new(p->arena);
		list_append(expr->v.application.expr_args, lhs);
		list_append(expr->v.application.expr_args, rhs);
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
		expr->expr_type        = EXPR_APPLICATION;
		expr->v.application.fn = copy_lexeme(p->arena, op);
		expr->v.application.expr_args = list_new(p->arena);
		list_append(expr->v.application.expr_args, lhs);
		list_append(expr->v.application.expr_args, rhs);
	}
	return expr;
}

static struct expr *parse_expr_cons_list(struct parser *p) {
	struct expr *expr = parse_expr_term(p);
	if (expr == NULL) {
		return NULL;
	}
	while (match(p, TOK_COLON)) {
		struct expr *lhs = expr;
		struct expr *rhs = parse_expr_cons_list(p);
		if (rhs == NULL) {
			return NULL;
		}
		expr                   = arena_push_struct_zero(p->arena, struct expr);
		expr->expr_type        = EXPR_APPLICATION;
		expr->v.application.fn = ":";
		expr->v.application.expr_args = list_new(p->arena);
		list_append(expr->v.application.expr_args, lhs);
		list_append(expr->v.application.expr_args, rhs);
	}
	return expr;
}

static struct expr *parse_expr_comparison(struct parser *p) {
	struct expr *expr = parse_expr_cons_list(p);
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
		expr->expr_type        = EXPR_APPLICATION;
		expr->v.application.fn = copy_lexeme(p->arena, op);
		expr->v.application.expr_args = list_new(p->arena);
		list_append(expr->v.application.expr_args, lhs);
		list_append(expr->v.application.expr_args, rhs);
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
		expr->expr_type        = EXPR_APPLICATION;
		expr->v.application.fn = copy_lexeme(p->arena, op);
		expr->v.application.expr_args = list_new(p->arena);
		list_append(expr->v.application.expr_args, lhs);
		list_append(expr->v.application.expr_args, rhs);
	}
	return expr;
}

static struct expr *parse_expr_let_in(struct parser *p) {
	struct expr *expr;

	if (!match(p, TOK_LET)) {
		return parse_expr_equality(p);
	}

	expr                 = arena_push_struct_zero(p->arena, struct expr);
	expr->expr_type      = EXPR_LET_IN;
	expr->v.let_in.stmts = list_new(p->arena);

	do {
		size_t source_index = peek(p)->lexeme_index;
		struct stmt *stmt   = parse_stmt(p);
		if (stmt == NULL) {
			return NULL;
		}
		switch (stmt->type) {
		case STMT_DEC_TYPE:
		case STMT_DEF_VALUE: list_append(expr->v.let_in.stmts, stmt); break;
		default:
			report_error_at(p->log, "Invalid statement type", source_index);
			return NULL;
		}
	} while (!match(p, TOK_IN));

	expr->v.let_in.value = parse_expr(p);

	if (expr->v.let_in.value == NULL) {
		return NULL;
	}

	return expr;
}

struct expr *parse_expr(struct parser *p) {
	struct expr *expr;
	size_t source_index = peek(p)->lexeme_index;
	expr                = parse_expr_let_in(p);
	if (expr != NULL) {
		expr->source_index = source_index;
	}
	return expr;
}

/* ========== TYPES ========== */

static struct type *parse_type_free(struct parser *p);

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
	struct list *sub_types = list_new(arena_alloc());

	CONSUME(TOK_PAREN_L, "Expected '('");

	do {
		struct type *sub_type = parse_type_free(p);
		if (sub_type == NULL) {
			break;
		}
		list_append(sub_types, sub_type);
	} while (match(p, TOK_COMMA));

	sub_types_len = list_length(sub_types);

	if (sub_types_len == 0) {
		type       = arena_push_struct_zero(p->arena, struct type);
		type->name = "()";
	} else if (sub_types_len == 1) {
		/* grouped type */
		type = list_to_array(sub_types, p->arena)[0];
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
		type->type_args          = list_copy(sub_types, p->arena);
	}

	list_free(sub_types);

	CONSUME(TOK_PAREN_R, "Expected ')' after type");

	return type;
}

static struct type *parse_type_primary_list(struct parser *p) {
	struct type *type;
	CONSUME(TOK_SQUARE_L, "Expected '['");
	type            = arena_push_struct_zero(p->arena, struct type);
	type->name      = "[]";
	type->type_args = list_new(p->arena);
	list_append(type->type_args, parse_type_free(p));
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
	int is_parameterized_type =
		peek_type(p) == TOK_IDENTIFIER && is_type_primary(peek_type_next(p));

	if (!is_parameterized_type) {
		return parse_type_primary(p);
	}

	type            = parse_type_name(p);
	type->type_args = list_new(p->arena);

	while (is_type_primary(peek_type(p))) {
		struct type *arg = parse_type_primary(p);
		if (arg == NULL) {
			return NULL;
		}
		list_append(type->type_args, arg);
	}

	return type;
}

static struct type *parse_type_arrow(struct parser *p) {
	struct type *type = parse_type_parameterized(p);

	while (match(p, TOK_ARROW)) {
		struct type *lhs = type;
		struct type *rhs = arena_push_struct_zero(p->arena, struct type);
		rhs              = parse_type_arrow(p);
		type             = arena_push_struct_zero(p->arena, struct type);
		type->name       = "->";
		type->type_args  = list_new(p->arena);
		list_append(type->type_args, lhs);
		list_append(type->type_args, rhs);
	}

	return type;
}

static struct type *parse_type_constraint(struct parser *p) {
	return parse_type_parameterized(p);
}

struct list *parse_type_context(struct parser *p) {
	struct list *constraints;

	if (!match(p, TOK_LT)) {
		return NULL; /* type doesn't have a context */
	}

	constraints = list_new(p->arena);

	do {
		struct type *constraint = parse_type_constraint(p);
		if (constraint == NULL) {
			return NULL;
		}

		list_append(constraints, constraint);
	} while (match(p, TOK_COMMA));

	CONSUME(TOK_GT, "Missing closing '>' after type context");
	CONSUME(TOK_EQ_ARROW, "Expected '=>' after type context");

	return constraints;
}

static struct type *parse_type_free(struct parser *p) {
	struct type *type = parse_type_arrow(p);
	return type;
}

struct type *parse_type(struct parser *p) {
	struct list *constraints = parse_type_context(p);
	struct type *type;

	if (p->log->had_error) {
		return NULL;
	}

	type = parse_type_free(p);

	if (type != NULL) {
		type->type_constraints = constraints;
	}

	return type;
}

/* ========== STATEMENTS ========== */

static struct dec_type *parse_dec_type(struct parser *p) {
	struct dec_type *dec_type = arena_push_struct_zero(p->arena, struct dec_type);
	PARSE_IDENTIFIER(dec_type->name, "Expected declaration identifier");
	CONSUME(TOK_COLON_COLON, "Expected '::' after identifier");
	dec_type->type = parse_type(p);
	CONSUME(TOK_TICK, "Missing ' after value type");
	PARSE_IDENTIFIER(dec_type->region_var, "Expected region variable name");
	CONSUME(TOK_SEMICOLON, "Expected ';' after type");
	return dec_type;
}

static struct dec_class *parse_dec_class(struct parser *p) {
	struct dec_class *dec_class =
		arena_push_struct_zero(p->arena, struct dec_class);
	dec_class->dec_types = list_new(p->arena);

	CONSUME(TOK_CLASS, "Expected 'class' keyword");
	PARSE_IDENTIFIER(dec_class->name, "Expected class name");
	PARSE_IDENTIFIER(dec_class->type_var, "Expected type variable");
	CONSUME(TOK_CURLY_L, "Expected '{' before class declaration");

	while (!is_at_end(p) && peek_type(p) == TOK_IDENTIFIER) {
		struct dec_type *dec_type = parse_dec_type(p);
		if (dec_type == NULL) {
			return NULL;
		}
		list_append(dec_class->dec_types, dec_type);
	}

	CONSUME(TOK_CURLY_R, "Expected '}' after class declaration");

	return dec_class;
}

static struct def_value *parse_def_value(struct parser *p) {
	struct def_value *def_value =
		arena_push_struct_zero(p->arena, struct def_value);
	def_value->expr_params = list_new(p->arena);

	PARSE_IDENTIFIER(def_value->name, "Expected definition identifier");

	while (!match(p, TOK_EQ)) {
		struct expr *arg = parse_expr_primary(p);
		if (arg == NULL) {
			return NULL;
		}
		list_append(def_value->expr_params, arg);
	}

	def_value->value = parse_expr(p);
	CONSUME(TOK_SEMICOLON, "Expected ';' after expression");
	return def_value;
}

static struct def_instance *parse_def_instance(struct parser *p) {
	struct def_instance *def_instance =
		arena_push_struct_zero(p->arena, struct def_instance);
	def_instance->type_args  = list_new(p->arena);
	def_instance->def_values = list_new(p->arena);

	CONSUME(TOK_INSTANCE, "Expected 'instance' keyword");
	def_instance->type_constraints = parse_type_context(p);
	PARSE_IDENTIFIER(def_instance->class_name, "Expected class name");

	do {
		struct type *arg = parse_type_primary(p);
		if (arg == NULL) {
			return NULL;
		}
		list_append(def_instance->type_args, arg);
	} while (!match(p, TOK_CURLY_L));

	do {
		struct def_value *def = parse_def_value(p);
		if (def == NULL) {
			return NULL;
		}
		list_append(def_instance->def_values, def);
	} while (!match(p, TOK_CURLY_R));

	return def_instance;
}

static struct dec_constructor *parse_dec_constructor(struct parser *p) {
	struct dec_constructor *constructor =
		arena_push_struct_zero(p->arena, struct dec_constructor);
	constructor->type_params  = list_new(p->arena);
	constructor->source_index = peek(p)->lexeme_index;

	PARSE_IDENTIFIER(constructor->name, "Expected constructor");

	while (is_type_primary(peek_type(p))) {
		struct type *type_arg = parse_type_primary(p);
		if (type_arg == NULL) {
			return constructor;
		}
		list_append(constructor->type_params, type_arg);
	}

	return constructor;
}

static struct dec_data *parse_dec_data(struct parser *p) {
	struct dec_data *dec_data;

	CONSUME(TOK_DATA, "Expected 'data' keyword");
	dec_data = arena_push_struct_zero(p->arena, struct dec_data);
	PARSE_IDENTIFIER(dec_data->name, "Expected data type name");

	dec_data->type_vars        = list_new(p->arena);
	dec_data->dec_constructors = list_new(p->arena);

	while (peek_type(p) == TOK_IDENTIFIER) {
		char *type_var;
		PARSE_IDENTIFIER(type_var, "Expected type variable");
		list_append(dec_data->type_vars, type_var);
	}

	CONSUME(TOK_CURLY_L, "Expected '{' before data declaration");

	if (match(p, TOK_CURLY_R)) {
		return dec_data;
	}

	do {
		struct dec_constructor *constructor = parse_dec_constructor(p);
		if (constructor == NULL) {
			return NULL;
		}
		list_append(dec_data->dec_constructors, constructor);
	} while (match(p, TOK_PIPE));

	CONSUME(TOK_CURLY_R, "Expected closing '}' after data declaration");

	return dec_data;
}

struct stmt *parse_stmt(struct parser *p) {
	struct stmt *stmt  = arena_push_struct_zero(p->arena, struct stmt);
	stmt->source_index = peek(p)->lexeme_index;
	switch (peek_type(p)) {
	case TOK_CLASS:
		stmt->type        = STMT_DEC_CLASS;
		stmt->v.dec_class = parse_dec_class(p);
		break;
	case TOK_DATA:
		stmt->type       = STMT_DEC_DATA;
		stmt->v.dec_data = parse_dec_data(p);
		break;
	case TOK_INSTANCE:
		stmt->type           = STMT_DEF_INSTANCE;
		stmt->v.def_instance = parse_def_instance(p);
		break;
	case TOK_IDENTIFIER:
		if (peek_type_next(p) == TOK_COLON_COLON) {
			stmt->type       = STMT_DEC_TYPE;
			stmt->v.dec_type = parse_dec_type(p);
		} else {
			stmt->type        = STMT_DEF_VALUE;
			stmt->v.def_value = parse_def_value(p);
		}
		break;
	default: assert(0);
	}

	return stmt;
}

static void advance_to_next_statement(struct parser *p) {
	while (!match(p, TOK_SEMICOLON) || !match(p, TOK_CURLY_R)) {
		advance(p);
	}
}

struct prog *parse_prog(struct parser *p) {
	struct prog *prog = arena_push_struct_zero(p->arena, struct prog);
	prog->stmts       = list_new(p->arena);

	while (!match(p, TOK_EOF)) {
		struct stmt *stmt = parse_stmt(p);
		if (stmt == NULL) {
			/* statement contains error, continue to next statement */
			advance_to_next_statement(p);
			continue;
		}
		list_append(prog->stmts, stmt);
	}

	return prog;
}

struct prog *
parse(struct token **tokens, struct arena *arena, struct error_log *log) {
	struct parser p;
	p.tokens  = tokens;
	p.current = 0;
	p.arena   = arena;
	p.log     = log;

	return parse_prog(&p);
}
