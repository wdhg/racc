#include "type_check.h"
#include "arena.h"
#include "ast.h"
#include "error.h"
#include "list.h"
#include "map.h"
#include "uid.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG
#undef DEBUG

struct type_checker {
	struct map *type_synonyms;  /* char* -> struct type* */
	struct list *type_scopes;   /* list of char* -> struct type* */
	struct list *type_contexts; /* list of char* -> struct type* */

	struct arena *arena;
	struct error_log *log;
	size_t source_index;
};

#ifdef DEBUG
int debug_indent_level = 0;
#endif

/* ========== BASIC KINDS/TYPES ========== */

struct kind kind_star = {
	KIND_STAR,
	NULL,
	NULL,
};
struct kind kind_unary = {
	KIND_ARROW,
	&kind_star,
	&kind_star,
};
struct kind kind_binary = {
	KIND_ARROW,
	&kind_star,
	&kind_unary,
};

struct kind *
kind_arrow(struct arena *arena, struct kind *lhs, struct kind *rhs) {
	struct kind *kind = arena_push_struct_zero(arena, struct kind);
	kind->type        = KIND_ARROW;
	kind->lhs         = lhs;
	kind->rhs         = rhs;
	return kind;
}

struct type type_int = {
	"Int",
	&kind_star,
	NULL,
	NULL,
	NULL,
};
struct type type_double = {
	"Double",
	&kind_star,
	NULL,
	NULL,
	NULL,
};
struct type type_char = {
	"Char",
	&kind_star,
	NULL,
	NULL,
	NULL,
};
struct type type_bool = {
	"Bool",
	&kind_star,
	NULL,
	NULL,
	NULL,
};

#define TYPE_VAR_A (new_type(tc, "a", &kind_star))
#define TYPE_VAR_B (new_type(tc, "b", &kind_star))
#define TYPE_LIST  (new_type(tc, "[]", &kind_unary))
#define TYPE_ARROW (new_type(tc, "->", &kind_binary))
#define TYPE_TUPLE (new_type(tc, "(,)", &kind_binary))

#define TYPE_STRING (apply_type(tc, TYPE_LIST, &type_char))
#define TYPE_LIST_A (apply_type(tc, TYPE_LIST, TYPE_VAR_A))
#define TYPE_TUPLE_A_B                                                         \
	(apply_type(tc, apply_type(tc, TYPE_TUPLE, TYPE_VAR_A), TYPE_VAR_B))

#define APPLY_A_ARROW_B(A, B) (apply_type(tc, apply_type(tc, TYPE_ARROW, A), B))
#define APPLY_A_ARROW_B_ARROW_C(A, B, C)                                       \
	APPLY_A_ARROW_B(A, APPLY_A_ARROW_B(B, C))

#define TYPE_EQ APPLY_A_ARROW_B_ARROW_C(TYPE_VAR_A, TYPE_VAR_A, &type_bool)

#define TYPE_CONSTRUCTOR_CONS                                                  \
	APPLY_A_ARROW_B_ARROW_C(TYPE_VAR_A, TYPE_LIST_A, TYPE_LIST_A)
#define TYPE_CONSTRUCTOR_TUPLE                                                 \
	APPLY_A_ARROW_B_ARROW_C(TYPE_VAR_A, TYPE_VAR_B, TYPE_TUPLE_A_B)

/* ========== EQUALITY CHECKING ========== */

static int kinds_equal(struct kind *k1, struct kind *k2) {
	if (k1->type != k2->type) {
		return 0;
	}

	switch (k1->type) {
	case KIND_CONSTRAINT:
	case KIND_STAR:
		assert(k1->lhs == NULL);
		assert(k1->rhs == NULL);
		assert(k2->lhs == NULL);
		assert(k2->rhs == NULL);
		return 1;
	case KIND_ARROW:
		assert(k1->lhs != NULL);
		assert(k1->rhs != NULL);
		assert(k2->lhs != NULL);
		assert(k2->rhs != NULL);
		return kinds_equal(k1->lhs, k2->lhs) && kinds_equal(k1->rhs, k2->rhs);
	}
	assert(0);
}

static struct type *get_type_local(struct type_checker *tc,
                                   char *type_identifier);
static void set_type(struct type_checker *tc, char *name, struct type *type);
static void type_scope_enter(struct type_checker *tc);
static void type_scope_exit(struct type_checker *tc);
static int
types_equal(struct type_checker *tc, struct type *t1, struct type *t2);
static struct type *get_type_synonym(struct type_checker *tc, char *name);

static int
type_args_equal(struct type_checker *tc, struct type *t1, struct type *t2) {
	list_zip_with(t1->type_args,
	              struct type *,
	              t2->type_args,
	              struct type *,
	              if (!types_equal(tc, _value_1, _value_2)) return 0);
	return 1;
}

static int
types_equal(struct type_checker *tc, struct type *t1, struct type *t2) {
	int args_are_equal;
	struct type *t1_synonym;
	struct type *t2_synonym;

	if (is_type_var(t1) && is_type_var(t2)) {
		report_error(tc->log, "Cannot bind two type variables");
		return 0;
	}

	if (is_type_var(t1)) {
		struct type *t1_new = get_type_local(tc, t1->name);
		if (t1_new == NULL) {
			set_type(tc, t1->name, t2);
			return 1;
		}
		t1 = t1_new;
	}

	t1_synonym = get_type_synonym(tc, t1->name);
	if (t1_synonym != NULL) {
		t1 = t1_synonym;
	}

	if (is_type_var(t2)) {
		struct type *t2_new = get_type_local(tc, t2->name);
		if (t2_new == NULL) {
			set_type(tc, t2->name, t1);
			return 1;
		}
		t2 = t2_new;
	}

	t2_synonym = get_type_synonym(tc, t2->name);
	if (t2_synonym != NULL) {
		t2 = t2_synonym;
	}

	if (!kinds_equal(t1->kind, t2->kind)) {
		return 0;
	}

	if (strcmp(t1->name, t2->name) != 0) {
		return 0;
	}

	if (t1->type_args == NULL && t2->type_args == NULL) {
		return 1;
	}
	if (t1->type_args == NULL && t2->type_args != NULL) {
		return 0;
	}
	if (t1->type_args != NULL && t2->type_args == NULL) {
		return 0;
	}

	args_are_equal = type_args_equal(tc, t1, t2);

	return args_are_equal;
}

static int check_types_equal(struct type_checker *tc,
                             struct type *t1,
                             struct type *t2,
                             int source_index) {
	assert(t1 != NULL);
	assert(t2 != NULL);

#ifdef DEBUG
	for (int i = 0; i < debug_indent_level; i++) {
		printf("> ");
	}
	printf("CHECK ");
	print_type(t1);
	printf(" = ");
	print_type(t2);
	printf("\n");
#endif

	if (!types_equal(tc, t1, t2)) {
		report_type_error(tc->log, t1, t2, source_index);
		return 0;
	}

	return 1;
}

/* ========== TYPES ========== */

static struct type *new_type(struct type_checker *tc,
                             char *type_identifier,
                             struct kind *type_kind) {
	struct type *type = arena_push_struct_zero(tc->arena, struct type);
	type->name =
		arena_push_array_zero(tc->arena, strlen(type_identifier) + 1, char);
	strcpy(type->name, type_identifier);
	type->kind             = type_kind;
	type->type_args        = NULL;
	type->region_var       = NULL;
	type->type_constraints = NULL;
	return type;
}

static struct type *
apply_type(struct type_checker *tc, struct type *type, struct type *type_arg) {
	assert(!kinds_equal(type->kind, &kind_star));
	assert(kinds_equal(type->kind->lhs, type_arg->kind));

	if (type->type_args == NULL) {
		type->type_args = list_new(tc->arena);
	}

	list_append(type->type_args, type_arg);
	type->kind = type->kind->rhs;

	return type;
}

static struct type *get_type(struct type_checker *tc, char *type_identifier) {
	list_for_each(tc->type_scopes,
	              struct map *,
	              struct type *type = map_get_str(_value, type_identifier);
	              if (type != NULL) return type);
	return NULL;
}

static struct type *get_type_local(struct type_checker *tc,
                                   char *type_identifier) {
	struct map *type_scope_local = list_head(tc->type_scopes);
	return map_get_str(type_scope_local, type_identifier);
}

static void set_type(struct type_checker *tc, char *name, struct type *type) {
	struct map *type_scope_local = list_head(tc->type_scopes);

#ifdef DEBUG
	for (int i = 0; i < debug_indent_level; i++) {
		printf("> ");
	}
	printf("[%ld] %s => ", list_length(tc->type_scopes), name);
	print_type(type);
	printf("\n");
#endif

	map_put_str(type_scope_local, name, type);
}

static struct kind *copy_kind(struct type_checker *tc, struct kind *kind) {
	struct kind *copy;
	assert(kind != NULL);
	copy       = arena_push_struct_zero(tc->arena, struct kind);
	copy->type = kind->type;
	if (kind->lhs != NULL) {
		copy->lhs = copy_kind(tc, kind->lhs);
	}
	if (kind->rhs != NULL) {
		copy->rhs = copy_kind(tc, kind->rhs);
	}
	return copy;
}

static struct type *copy_type(struct type_checker *tc, struct type *type) {
	struct type *copy;
	assert(type != NULL);
	copy       = arena_push_struct_zero(tc->arena, struct type);
	copy->name = arena_push_array_zero(tc->arena, strlen(type->name) + 1, char);
	strcpy(copy->name, type->name);
	copy->kind = copy_kind(tc, type->kind);
	if (type->type_args != NULL) {
		copy->type_args = list_new(tc->arena);
		list_map(
			copy->type_args, type->type_args, struct type *, copy_type(tc, _value));
	} else {
		copy->type_args = NULL;
	}
	return copy;
}

static void register_type(struct type_checker *tc, struct type *type) {
	struct map *type_scope_global = list_last(tc->type_scopes);

#ifdef DEBUG
	printf("> ");
	print_type(type);
	printf(" :: ");
	print_kind(type->kind);
	printf("\n");
#endif

	map_put_str(type_scope_global, type->name, type);
}

static void
register_type_synonym(struct type_checker *tc, char *name, struct type *type) {
	map_put_str(tc->type_synonyms, name, type);
}

static struct type *get_type_synonym(struct type_checker *tc, char *name) {
	return map_get_str(tc->type_synonyms, name);
}

static struct type *
_substitute_type(struct type_checker *tc, struct type *type, int local) {
	struct type *synonym_type;

	if (is_type_var(type)) {
		struct type *substitution;
		if (local) {
			substitution = get_type_local(tc, type->name);
		} else {
			substitution = get_type(tc, type->name);
		}
		if (substitution == NULL) {
			return type;
		}
		type = substitution;
	}

	synonym_type = get_type_synonym(tc, type->name);
	if (synonym_type != NULL) {
		type = synonym_type;
	}

	assert(type != NULL);

	if (type->type_args != NULL) {
		struct type *copy = copy_type(tc, type);
		list_map(copy->type_args,
		         type->type_args,
		         struct type *,
		         _value = _substitute_type(tc, _value, local));
		type = copy;
	}

	return type;
}

static struct type *substitute_type(struct type_checker *tc,
                                    struct type *type) {
	return _substitute_type(tc, type, 0);
}

static struct type *substitute_type_local(struct type_checker *tc,
                                          struct type *type) {
	return _substitute_type(tc, type, 1);
}

/* ========== TYPE CONTEXTS ========== */

static void type_context_enter(struct type_checker *tc) {
	struct map *type_context = map_new();
	list_prepend(tc->type_contexts, type_context);
}

static void type_context_exit(struct type_checker *tc) {
	struct map *type_context = list_pop_head(tc->type_contexts);
	map_free(type_context);
}

static void type_scope_enter(struct type_checker *tc) {
	struct map *type_scope = map_new();
	list_prepend(tc->type_scopes, type_scope);

#ifdef DEBUG
	for (int i = 0; i < debug_indent_level; i++) {
		printf("> ");
	}
	printf("SCOPE ENTER\n");
	debug_indent_level++;
#endif
}

static void type_scope_exit(struct type_checker *tc) {
	struct map *type_scope = list_pop_head(tc->type_scopes);
	map_free(type_scope);

#ifdef DEBUG
	debug_indent_level--;
	for (int i = 0; i < debug_indent_level; i++) {
		printf("> ");
	}
	printf("SCOPE EXIT\n");
#endif
}

static struct type *get_value_type(struct type_checker *tc,
                                   char *value_identifier) {
	list_for_each(tc->type_contexts,
	              struct map *,
	              struct type *value_type = map_get_str(_value, value_identifier);
	              if (value_type != NULL) return value_type);
	return NULL;
}

static void set_value_type(struct type_checker *tc,
                           char *value_identifier,
                           struct type *value_type) {
	struct map *type_context = list_head(tc->type_contexts);

#ifdef DEBUG
	for (int i = 0; i < debug_indent_level; i++) {
		printf("> ");
	}
	printf("%s :: ", value_identifier);
	print_type(value_type);
	printf("\n");
#endif

	map_put_str(type_context, value_identifier, value_type);
}

static int value_exists_in_current_context(struct type_checker *tc,
                                           char *value_identifier) {
	struct map *local_type_context = list_head(tc->type_contexts);
	return map_get_str(local_type_context, value_identifier) != NULL;
}

/* ========== HIGH LEVEL TYPE CHECKING ========== */

static int is_constructor(char *fn_identifier) {
	return isupper(fn_identifier[0]) || strcmp(fn_identifier, "->") == 0 ||
	       strcmp(fn_identifier, "[]") == 0 || strcmp(fn_identifier, ":") == 0 ||
	       strcmp(fn_identifier, "(,)") == 0;
}

static int type_is_valid(struct type_checker *tc, struct type *type) {
	struct type *substitution_type = get_type_synonym(tc, type->name);

	if (substitution_type == NULL) {
		substitution_type = get_type(tc, type->name);
	}

	if (substitution_type == NULL) {
		return 0;
	}

	if (type->kind == NULL) {
		type->kind = substitution_type->kind;
	}

	if (type->type_args != NULL) {
		list_for_each(type->type_args,
		              struct type *,
		              if (kinds_equal(type->kind, &kind_star)) return 0;
		              if (!type_is_valid(tc, _value)) return 0;
		              if (!kinds_equal(type->kind->lhs, _value->kind)) return 0;
		              type->kind = type->kind->rhs;);
	}

	if (!kinds_equal(type->kind, &kind_star)) {
		return 0;
	}

	return 1;
}

static void type_check_stmt(struct type_checker *tc, struct stmt *stmt);

static struct type *get_expr_type(struct type_checker *tc, struct expr *expr) {
#ifdef DEBUG
	for (int i = 0; i < debug_indent_level; i++) {
		printf("> ");
	}
	printf("GET TYPE %d", expr->expr_type);
	printf("\n");
#endif

	if (expr->type != NULL) {
		return expr->type;
	}
	switch (expr->expr_type) {
	case EXPR_IDENTIFIER:
		expr->type = get_value_type(tc, expr->v.identifier);
		if (expr->type == NULL) {
			report_error_at(tc->log, "Undefined identifier", expr->source_index);
		}
		break;
	case EXPR_APPLICATION: {
		struct type *func_type;
		struct list_iter expr_args_iter;

		func_type = get_value_type(tc, expr->v.application.fn);

		if (func_type == NULL) {
			report_error_at(tc->log, "Unknown function", expr->source_index);
			return NULL;
		}

		expr_args_iter = list_iterate(expr->v.application.expr_args);

		type_scope_enter(tc);
		while (!list_iter_at_end(&expr_args_iter)) {
			struct expr *expr_arg;
			struct type *expr_arg_type;
			struct type *func_arg_type;
			if (strcmp(func_type->name, "->") != 0) {
				report_error_at(
					tc->log, "Too many parameters function", expr->source_index);
				type_scope_exit(tc);
				return NULL;
			}

#ifdef DEBUG
			for (i = 0; i < debug_indent_level; i++) {
				printf("> ");
			}
			printf("FUNCTION TYPE ");
			print_type(func_type);
			printf("\n");
#endif

			expr_arg      = list_iter_next(&expr_args_iter);
			expr_arg_type = get_expr_type(tc, expr_arg);
			func_arg_type = list_head(func_type->type_args);
			func_type     = list_last(func_type->type_args);

			func_arg_type = substitute_type_local(tc, func_arg_type);

			if (expr_arg_type == NULL) {
				report_error_at(tc->log, "Expression has no type", expr->source_index);
				type_scope_exit(tc);
				return NULL;
			}
			if (!check_types_equal(
						tc, expr_arg_type, func_arg_type, expr_arg->source_index)) {
				type_scope_exit(tc);
				return NULL;
			}
		}
		expr->type = substitute_type(tc, func_type);
		type_scope_exit(tc);
		break;
	}
	case EXPR_LIT_INT: expr->type = &type_int; break;
	case EXPR_LIT_DOUBLE: expr->type = &type_double; break;
	case EXPR_LIT_STRING: expr->type = get_type_synonym(tc, "String"); break;
	case EXPR_LIT_CHAR: expr->type = &type_char; break;
	case EXPR_LIT_BOOL: expr->type = &type_bool; break;
	case EXPR_GROUPING: expr->type = get_expr_type(tc, expr->v.grouping); break;
	case EXPR_LIST_NULL:
		expr->type = substitute_type(tc, get_value_type(tc, "[]"));
		break;
	case EXPR_LET_IN:
		type_context_enter(tc);
		list_for_each(
			expr->v.let_in.stmts, struct stmt *, type_check_stmt(tc, _value));
		expr->type = get_expr_type(tc, expr->v.let_in.value);
		type_context_exit(tc);
		break;
	}

#ifdef DEBUG
	if (expr->type != NULL) {
		for (i = 0; i < debug_indent_level; i++) {
			printf("> ");
		}
		printf("GOT TYPE ");
		print_type(expr->type);
		printf("\n");
	}
#endif

	return expr->type;
}

static void bind_expr_param_to_type(struct type_checker *tc,
                                    struct expr *expr,
                                    struct type *type);

static void bind_expr_param_application_to_type(struct type_checker *tc,
                                                struct expr *expr,
                                                struct type *type) {
	struct type *constructor_type;
	struct type *data_type;
	struct list_iter expr_args_iter;

	if (!is_constructor(expr->v.application.fn)) {
		report_error_at(
			tc->log, "Not a data type constructor in pattern", expr->source_index);
		return;
	}
	constructor_type = get_value_type(tc, expr->v.application.fn);
	if (constructor_type == NULL) {
		report_error_at(
			tc->log, "Unknown data type constructor", expr->source_index);
		return;
	}

	expr_args_iter = list_iterate(expr->v.application.expr_args);
	data_type      = constructor_type;
	while (!list_iter_at_end(&expr_args_iter)) {
		if (strcmp(data_type->name, "->") != 0) {
			report_error_at(tc->log,
			                "Too many parameters in constructor pattern",
			                expr->source_index);
			return;
		}
		data_type = list_last(data_type->type_args);
		list_iter_next(&expr_args_iter);
	}

	check_types_equal(tc, type, data_type, expr->source_index);
	/* now all relevant type vars are bound to concrete types */

	expr_args_iter = list_iterate(expr->v.application.expr_args);
	while (!list_iter_at_end(&expr_args_iter)) {
		struct expr *expr_arg;
		struct type *expr_arg_type;
		expr_arg         = list_iter_next(&expr_args_iter);
		expr_arg_type    = list_head(constructor_type->type_args);
		expr_arg_type    = substitute_type(tc, expr_arg_type);
		constructor_type = list_last(constructor_type->type_args);
		type_scope_enter(tc);
		bind_expr_param_to_type(tc, expr_arg, expr_arg_type);
		type_scope_exit(tc);
	}
}

static void bind_expr_param_to_type(struct type_checker *tc,
                                    struct expr *expr,
                                    struct type *type) {
	expr->type = type;
	switch (expr->expr_type) {
	case EXPR_IDENTIFIER:
		if (expr->v.identifier[0] == '_') {
			return;
		}
		if (value_exists_in_current_context(tc, expr->v.identifier)) {
			report_error_at(tc->log,
			                "Identifier already bound in current scope",
			                expr->source_index);
			return;
		}
		set_value_type(tc, expr->v.identifier, type);
		break;
	case EXPR_APPLICATION:
		bind_expr_param_application_to_type(tc, expr, type);
		break;
	case EXPR_LIT_INT:
		check_types_equal(tc, type, &type_int, expr->source_index);
		break;
	case EXPR_LIT_DOUBLE:
		check_types_equal(tc, type, &type_double, expr->source_index);
		break;
	case EXPR_LIT_STRING:
		check_types_equal(
			tc, type, get_type_synonym(tc, "String"), expr->source_index);
		break;
	case EXPR_LIT_CHAR:
		check_types_equal(tc, type, &type_char, expr->source_index);
		break;
	case EXPR_LIT_BOOL:
		check_types_equal(tc, type, &type_bool, expr->source_index);
		break;
	case EXPR_GROUPING:
		bind_expr_param_to_type(tc, expr->v.grouping, type);
		break;
	case EXPR_LIST_NULL:
		check_types_equal(tc, type, get_value_type(tc, "[]"), expr->source_index);
		break;
	case EXPR_LET_IN:
		report_error_at(tc->log,
		                "Cannot pattern match against let .. in expression",
		                expr->source_index);
		break;
	}
}

static struct type *
get_dec_constructor_type(struct type_checker *tc,
                         struct dec_constructor *dec_constructor,
                         struct type *data_type) {
	struct type *constructor_type = data_type;

	list_for_each(
		dec_constructor->type_params,
		struct type *,
		int is_valid = type_is_valid(tc, _value);
		if (!is_valid) report_error_at(
			tc->log, "Invalid constructor", dec_constructor->source_index);
		if (!is_valid) return NULL;
		else constructor_type = APPLY_A_ARROW_B(_value, constructor_type));

	return constructor_type;
}

static void type_check_dec_data(struct type_checker *tc,
                                struct dec_data *dec_data,
                                size_t source_index) {
	struct kind *data_type_kind = &kind_star;
	struct type *data_type;

	list_for_each(dec_data->type_vars, char *, (void)_value;
	              data_type_kind =
	                kind_arrow(tc->arena, &kind_star, data_type_kind));

	data_type = new_type(tc, dec_data->name, data_type_kind);

	register_type(tc, data_type);
	data_type = copy_type(tc, data_type);

	type_scope_enter(tc);

	list_for_each(dec_data->type_vars, char *, struct type *type_var;
	              if (get_type_local(tc, _value) != NULL)
	                report_error_at(tc->log, "Duplicate type var", source_index);
	              if (get_type_local(tc, _value) != NULL) return;
	              type_var = new_type(tc, _value, &kind_star);
	              set_type(tc, _value, type_var);
	              apply_type(tc, data_type, type_var));

	list_for_each(dec_data->dec_constructors,
	              struct dec_constructor *,
	              struct type *constructor_type =
	                get_dec_constructor_type(tc, _value, data_type);
	              if (constructor_type != NULL)
	                set_value_type(tc, _value->name, constructor_type));

	type_scope_exit(tc);
}

static void type_check_dec_type(struct type_checker *tc,
                                struct dec_type *dec_type,
                                size_t source_index) {
	if (!type_is_valid(tc, dec_type->type)) {
		report_error_at(tc->log, "Invalid declaration", source_index);
		return;
	}
	if (value_exists_in_current_context(tc, dec_type->name)) {
		report_error_at(tc->log, "Redeclaration of value type", source_index);
		return;
	}
	set_value_type(tc, dec_type->name, dec_type->type);
}

static void type_check_def_value(struct type_checker *tc,
                                 struct def_value *def_value,
                                 size_t source_index) {
	struct type *dec_type = get_value_type(tc, def_value->name);
	struct type *def_type;
	struct list_iter expr_params_iter;
	if (dec_type == NULL) {
		report_error_at(
			tc->log, "Missing declaration for definition", source_index);
		return;
	}

	type_scope_enter(tc);
	expr_params_iter = list_iterate(def_value->expr_params);
	while (!list_iter_at_end(&expr_params_iter)) {
		struct expr *expr_param;
		struct type *expr_param_type;
		if (strcmp(dec_type->name, "->") != 0) {
			report_error_at(tc->log, "Too many parameters function", source_index);
			type_scope_exit(tc);
			return;
		}
		expr_param      = list_iter_next(&expr_params_iter);
		expr_param_type = list_head(dec_type->type_args);
		dec_type        = list_last(dec_type->type_args);
		bind_expr_param_to_type(tc, expr_param, expr_param_type);
	}
	type_scope_exit(tc);

	type_scope_enter(tc);
	def_type = get_expr_type(tc, def_value->value);
	if (def_type == NULL) {
		type_scope_exit(tc);
		return;
	}

	check_types_equal(tc, dec_type, def_type, source_index);
	type_scope_exit(tc);
}

static void type_check_stmt(struct type_checker *tc, struct stmt *stmt) {
	tc->source_index = stmt->source_index;
	switch (stmt->type) {
	case STMT_DEC_DATA:
		type_check_dec_data(tc, stmt->v.dec_data, stmt->source_index);
		break;
	case STMT_DEC_TYPE:
		type_check_dec_type(tc, stmt->v.dec_type, stmt->source_index);
		break;
	case STMT_DEF_VALUE:
		type_context_enter(tc);
		type_check_def_value(tc, stmt->v.def_value, stmt->source_index);
		type_context_exit(tc);
		break;
	case STMT_DEF_INSTANCE: break; /* TODO UNUSED */
	case STMT_DEC_CLASS: break;    /* TODO UNUSED */
	}
}

static void type_check_prog(struct type_checker *tc, struct prog *prog) {
	list_for_each(prog->stmts, struct stmt *, type_check_stmt(tc, _value));
}

void type_check(struct prog *prog, struct arena *arena, struct error_log *log) {
	struct type_checker *tc = arena_push_struct_zero(arena, struct type_checker);

	tc->type_synonyms = map_new();
	tc->type_scopes   = list_new(NULL);
	tc->type_contexts = list_new(NULL);
	tc->arena         = arena;
	tc->log           = log;

	type_context_enter(tc);
	type_scope_enter(tc);

	register_type(tc, &type_int);
	register_type(tc, &type_double);
	register_type(tc, &type_char);
	register_type(tc, &type_bool);
	register_type_synonym(tc, "String", TYPE_STRING);
	register_type(tc, TYPE_ARROW);
	register_type(tc, TYPE_TUPLE);
	register_type(tc, TYPE_LIST);

	set_value_type(tc, "==", TYPE_EQ);

	set_value_type(tc, "[]", TYPE_LIST_A);
	set_value_type(tc, ":", TYPE_CONSTRUCTOR_CONS);
	set_value_type(tc, "(,)", TYPE_CONSTRUCTOR_TUPLE);
	set_value_type(
		tc, "+", APPLY_A_ARROW_B_ARROW_C(&type_int, &type_int, &type_int));

	type_check_prog(tc, prog);

	type_scope_exit(tc);
	type_context_exit(tc);
}
