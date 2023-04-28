#ifndef RACC_AST_H
#define RACC_AST_H

#include "token.h"

enum expr_type {
	EXPR_IDENTIFIER,
	EXPR_APPLICATION,
	EXPR_LIT_INT,
	EXPR_LIT_DOUBLE,
	EXPR_LIT_STRING,
	EXPR_LIT_CHAR,
	EXPR_LIT_BOOL,
	EXPR_UNARY,
	EXPR_BINARY,
	EXPR_GROUPING
};

struct expr {
	enum expr_type type;

	union {
		char *identifier;
		int lit_int;
		double lit_double;
		char *lit_string;
		char lit_char;
		int lit_bool;
		struct expr *grouping;

		struct {
			char *fn;
			struct expr **args;
			size_t args_len;
		} application;

		struct {
			struct token *op;
			struct expr *rhs;
		} unary;

		struct {
			struct expr *lhs;
			struct token *op;
			struct expr *rhs;
		} binary;

	} v;
};

#endif
