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
	} v;
};

struct type_constraint {
	char *name;
	char **args;
	size_t args_len;
};

struct type_context {
	struct type_constraint **constraints;
	size_t constraints_len;
};

struct type {
	struct type_context *context;
	char *name;
	struct type **args;
	size_t args_len;
};

struct dec_type {
	char *name;
	struct type *type;
};

struct dec_constructor {
	char *name;
	struct type **args;
	size_t args_len;
};

struct def_value {
	char *name;
	struct expr **args;
	size_t args_len;
	struct expr *value;
};

enum stmt_type {
	STMT_DEC_CLASS,
	STMT_DEC_DATA,
	STMT_DEC_TYPE,
	STMT_DEF_INSTANCE,
	STMT_DEF_VALUE
};

struct stmt {
	enum stmt_type type;

	union {
		struct {
			char *name;
			char *type_var;
			struct dec_type **declarations;
			size_t declarations_len;
		} dec_class;

		struct {
			char *name;
			char **type_vars;
			size_t type_vars_len;
			struct dec_constructor **constructors;
			size_t constructors_len;
		} dec_data;

		struct dec_type *dec_type;

		struct def_value *def_value;

		struct {
			char *class_name;
			struct type_context *context;
			struct type **args;
			size_t args_len;
			struct def_value **defs;
			size_t defs_len;
		} def_instance;
	} v;
};

struct prog {
	struct stmt **stmts;
	size_t stmts_len;
};

#endif
