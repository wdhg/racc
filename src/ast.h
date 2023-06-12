#ifndef RACC_AST_H
#define RACC_AST_H

#include "token.h"
#include "uid.h"

enum expr_type {
	EXPR_IDENTIFIER,
	EXPR_APPLICATION,
	EXPR_LIT_INT,
	EXPR_LIT_DOUBLE,
	EXPR_LIT_STRING,
	EXPR_LIT_CHAR,
	EXPR_LIT_BOOL,
	EXPR_LIST_NULL,
	EXPR_GROUPING
};

struct expr {
	enum expr_type expr_type;
	size_t source_index;

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
			struct list *expr_args; /* list of struct expr */
		} application;
	} v;

	struct type *type;               /* set during type checking */
	struct region_sort *region_sort; /* set during region checking */
};

struct region_sort {
	char *name;
	struct list *region_sort_params; /* list of struct region_sort */
};

enum kind_type { KIND_STAR, KIND_ARROW, KIND_CONSTRAINT };

struct kind {
	enum kind_type type;
	/* kind arrow */
	struct kind *lhs;
	struct kind *rhs;
};

typedef uid scope_id;

struct type {
	char *name;
	struct kind *kind;
	struct list *type_args; /* list of struct type* */
	struct region_sort *region_sort;
	struct list *type_constraints; /* UNUSED list of struct type* */
};

struct dec_type {
	char *name;
	struct type *type;
};

struct dec_class {
	char *name;
	char *type_var;
	struct list *dec_types; /* list of struct dec_type */
};

struct dec_constructor {
	char *name;
	char *region_var;
	struct list *type_params; /* list of struct type */
	size_t source_index;
};

struct dec_data {
	char *name;
	struct region_sort *region_sort;
	struct list *type_vars;        /* list of char* */
	struct list *dec_constructors; /* list of struct dec_constructor */
};

struct def_value {
	char *name;
	struct list *expr_params; /* list of struct expr */
	struct expr *value;
};

struct def_instance {
	char *class_name;
	struct list *type_constraints; /* list of struct type */
	struct list *type_args;        /* list of struct type */
	struct list *def_values;       /* list of struct def_value */
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
	size_t source_index;

	union {
		struct dec_class *dec_class;
		struct dec_data *dec_data;
		struct dec_type *dec_type;
		struct def_value *def_value;
		struct def_instance *def_instance;
	} v;
};

struct prog {
	struct list *stmts; /* list of struct stmt */
};

void print_type(struct type *type);
void print_kind(struct kind *kind);

int is_type_var(struct type *type);

#endif
