#include "type_check.h"
#include "arena.h"
#include "ast.h"
#include "error.h"
#include "list.h"
#include "set.h"
#include <assert.h>
#include <ctype.h>
#include <string.h>

/* Provided types: (), Bool, Int, Float, Char
 *
 * Provided data types: (->), (,), [a], IO a
 *
 * Provided classes: Num, Floating
 *
 *
 * Rules for type checking t1, t2:
 *
 *   - if t1 and t2 are monomorphic, then t1 must equal t2 exactly
 *   - if either t1 is monomorphic and t2 is polymorphic, t1 must be a form of
 *     t2
 *   - if t1 and t2 are polymorphic, t1 and t2 must have similar
 *
 *
 * Dealing with polymorphic types:
 *
 *   1. Go down the type and replace type variables with their bound types
 *   2.
 *
 */

struct type_info {
	struct type *type;
	struct kind *kind;
	struct list *instances;
};

struct value_info {
	struct type *type;
	int is_constructor;
};

struct type_checker {
	struct map *type_infos;  /* type name -> type info */
	struct map *value_infos; /* value name -> value_info */

	struct arena *arena_prog;
	struct arena *arena_tc;

	struct error_log *log;
	size_t curr_source_index;
};

/* ========== KIND BUILDING ========== */

struct kind kind_star = {
	.type = KIND_STAR,
	.lhs  = NULL,
	.rhs  = NULL,
};

struct kind kind_constraint = {
	.type = KIND_CONSTRAINT,
	.lhs  = NULL,
	.rhs  = NULL,
};

struct kind *
new_kind_arrow(struct arena *arena, struct kind *lhs, struct kind *rhs) {
	struct kind *kind = arena_push_struct_zero(arena, struct kind);
	kind->lhs         = lhs;
	kind->rhs         = rhs;
	return kind;
}

/* ========== TYPE BUILDING ========== */

#define TYPE_NULL   ("()")
#define TYPE_CHAR   ("Char")
#define TYPE_INT    ("Int")
#define TYPE_STRING ("String")
#define TYPE_FLOAT  ("Float")
#define TYPE_DOUBLE ("Double")
#define TYPE_BOOL   ("Bool)")

static struct type *new_type(struct arena *arena, char *name) {
	struct type *type = arena_push_struct_zero(arena, struct type);
	type->name        = arena_push_array_zero(arena, strlen(name) + 1, char);
	strcpy(type->name, name);
	return type;
}

static void
add_arg_to_type(struct arena *arena, struct type *type, struct type *arg) {
	if (type->type_args == NULL) {
		type->type_args = list_new(arena);
	}
	list_append(type->type_args, arg);
}

static struct type *new_type_unary(struct arena *arena, char *name, char *arg) {
	struct type *type     = new_type(arena, name);
	struct type *type_arg = new_type(arena, arg);
	add_arg_to_type(arena, type, type_arg);
	return type;
}

static void add_type_constraint(struct arena *arena,
                                struct type *type,
                                char *class,
                                char *arg) {
	struct type *constraint = new_type_unary(arena, class, arg);
	if (type->type_constraints == NULL) {
		type->type_constraints = list_new(arena);
	}
	list_append(type->type_constraints, constraint);
}

struct type *new_type_num(struct arena *arena) {
	struct type *type = new_type(arena, "a");
	add_type_constraint(arena, type, "Num", "a");
	return type;
}

struct type *new_type_floating(struct arena *arena) {
	struct type *type = new_type(arena, "a");
	add_type_constraint(arena, type, "Floating", "a");
	return type;
}

/* ========== TYPE CHECKER ACCESS ========== */

static struct type *get_type(struct type_checker *tc, char *name) {
	struct type_info *info = map_get(tc->type_infos, name);

	if (info == NULL) {
		return NULL;
	}

	return info->type;
}

static struct type *get_value_type(struct type_checker *tc, char *name) {
	struct value_info *info = map_get(tc->value_infos, name);

	if (info == NULL) {
		return NULL;
	}

	return info->type;
}

static void
add_value_type(struct type_checker *tc, char *name, struct type *type) {
	struct value_info *value_info =
		arena_push_struct_zero(tc->arena_tc, struct value_info);
	value_info->type           = type;
	value_info->is_constructor = 0;
	map_put(tc->value_infos, name, value_info);
}

static void
add_constructor_type(struct type_checker *tc, char *name, struct type *type) {
	struct value_info *value_info =
		arena_push_struct_zero(tc->arena_tc, struct value_info);
	value_info->type           = type;
	value_info->is_constructor = 1;
	map_put(tc->value_infos, name, value_info);
}

/* ========== TYPE CHECKER INITIALIZATION ========== */

static void init_type_ordinary(struct type_checker *tc, char *name) {
	struct type_info *type_info =
		arena_push_struct_zero(tc->arena_tc, struct type_info);
	type_info->type = new_type(tc->arena_tc, name);
	type_info->kind = &kind_star;
	map_put(tc->type_infos, name, type_info);
}

static void init_type_data_unary(struct type_checker *tc, char *name) {
	struct type_info *type_info =
		arena_push_struct_zero(tc->arena_tc, struct type_info);
	type_info->type = new_type(tc->arena_tc, name);
	type_info->kind = new_kind_arrow(tc->arena_tc, &kind_star, &kind_star);
	map_put(tc->type_infos, name, type_info);
}

static void init_type_synonym_unary_data(struct type_checker *tc,
                                         char *name,
                                         char *data,
                                         char *var) {
	struct type_info *type_info =
		arena_push_struct_zero(tc->arena_tc, struct type_info);
	type_info->type = new_type(tc->arena_tc, data);
	add_arg_to_type(tc->arena_tc, type_info->type, new_type(tc->arena_tc, var));
	type_info->kind = &kind_star;
	map_put(tc->type_infos, name, type_info);
}

static void add_type_data_binary(struct type_checker *tc, char *name) {
	struct type_info *type_info =
		arena_push_struct_zero(tc->arena_tc, struct type_info);
	type_info->type = new_type(tc->arena_tc, name);
	add_arg_to_type(tc->arena_tc, type_info->type, new_type(tc->arena_tc, "a"));
	add_arg_to_type(tc->arena_tc, type_info->type, new_type(tc->arena_tc, "b"));
	type_info->kind =
		new_kind_arrow(tc->arena_tc,
	                 &kind_star,
	                 new_kind_arrow(tc->arena_tc, &kind_star, &kind_star));
	map_put(tc->type_infos, name, type_info);
}

static void init_type_class_unary(struct type_checker *tc, char *name) {
	struct type_info *type_info =
		arena_push_struct_zero(tc->arena_tc, struct type_info);
	type_info->type = new_type(tc->arena_tc, name);
	add_arg_to_type(tc->arena_tc, type_info->type, new_type(tc->arena_tc, "a"));
	type_info->kind = new_kind_arrow(tc->arena_tc, &kind_star, &kind_constraint);
	map_put(tc->type_infos, name, type_info);
}

static void init_type_class_unary_instance(struct type_checker *tc,
                                           char *class_name,
                                           char *type_name) {}

static void init_function_type(struct type_checker *tc, char *name) {
	struct type *type = new_type(tc->arena_tc, name);
}

static void init_function_param_type(struct type_checker *tc,
                                     char *fn_name,
                                     char *arg_name,
                                     struct type *type) {}

void init_type_checker(struct type_checker *tc) {
	init_type_ordinary(tc, TYPE_NULL);
	init_type_ordinary(tc, TYPE_INT);
	init_type_ordinary(tc, TYPE_FLOAT);
	init_type_ordinary(tc, TYPE_DOUBLE);
	init_type_ordinary(tc, TYPE_CHAR);
	init_type_ordinary(tc, TYPE_BOOL);

	init_type_data_unary(tc, "[]");
	init_type_data_unary(tc, "IO");
	add_type_data_binary(tc, "->");
	add_type_data_binary(tc, "(,)");

	init_type_synonym_unary_data(tc, "String", "[]", "Char");

	init_type_class_unary(tc, "Num");
	init_type_class_unary_instance(tc, "Num", TYPE_INT);
	init_type_class_unary_instance(tc, "Num", TYPE_FLOAT);
	init_type_class_unary_instance(tc, "Num", TYPE_DOUBLE);

	init_type_class_unary(tc, "Floating");
	init_type_class_unary_instance(tc, "Floating", TYPE_FLOAT);
	init_type_class_unary_instance(tc, "Floating", TYPE_DOUBLE);

	init_function_type(tc, "+");
	init_function_param_type(tc, "+", "x", new_type(tc->arena_tc, "a"));
	init_function_param_type(tc, "+", "y", new_type(tc->arena_tc, "a"));
}

/* ========== LOW LEVEL TYPE CHECK FUNCS ========== */

static int is_type_var(char *type_name) { return islower(type_name[0]); }

static int
check_types_match(struct type_checker *tc, struct type *a, struct type *b) {
	/* TODO functions/data types, type variables, type classes */
	int ok = strcmp(a->name, b->name) == 0;
	if (!ok) {
		/* TODO write better messages */
		report_error_at(tc->log, "Types don't match", tc->curr_source_index);
	}
	return ok;
}

static size_t calc_type_arity(struct type *type) {
	size_t arity = 0;
	while (strcmp(type->name, "->") == 0) {
		arity++;
		type = list_get(type->type_args, 1);
	}
	return arity;
}

static struct type *get_type_of_application(struct type_checker *tc,
                                            struct expr *app) {
	struct type *app_type;
	size_t arity, i;

	assert(app->type == EXPR_APPLICATION);

	app_type = get_value_type(tc, app->v.application.fn);

	if (app_type == NULL) {
		return NULL;
	}

	arity = list_length(app->v.application.expr_args);

	if (arity != calc_type_arity(app_type)) {
		/* TODO better message */
		report_error_at(
			tc->log, "Mismatched function arity", tc->curr_source_index);
		return NULL;
	}

	/* remove types of all args */
	for (i = 0; i < arity; i++) {
		app_type = list_get(app_type->type_args, 1);
	}

	return app_type;
}

static struct type *get_type_of_expr(struct type_checker *tc,
                                     struct expr *expr) {
	expr->source_index = expr->source_index;
	switch (expr->type) {
	case EXPR_IDENTIFIER: return get_value_type(tc, expr->v.identifier);
	case EXPR_APPLICATION: return get_type_of_application(tc, expr);
	case EXPR_LIT_INT: return new_type_num(tc->arena_tc);
	case EXPR_LIT_DOUBLE: return new_type_floating(tc->arena_tc);
	case EXPR_LIT_STRING: return get_type(tc, TYPE_STRING);
	case EXPR_LIT_CHAR: return get_type(tc, TYPE_CHAR);
	case EXPR_LIT_BOOL: return get_type(tc, TYPE_BOOL);
	case EXPR_GROUPING: return get_type_of_expr(tc, expr->v.grouping);
	default: assert(0);
	}
}

static struct type *
get_return_type(struct type_checker *tc, struct type *type, size_t arity) {
	size_t i;

	for (i = 0; i < arity; i++) {
		if (strcmp(type->name, "->") != 0) {
			/* TODO better message */
			report_error_at(tc->log, "Incorrect arity", tc->curr_source_index);
			return NULL;
		}

		type = list_get(type->type_args, 1);
	}

	return type;
}

/* ========== TYPE VARIABLE BINDING ========== */

static void bind_parameters(struct type_checker *tc,
                            struct list *params,
                            struct type *type);
static void unbind_parameters(struct type_checker *tc, struct list *params);

static void
bind_identifier(struct type_checker *tc, char *identifier, struct type *type) {
	if (get_value_type(tc, identifier) != NULL) {
		/* TODO better message */
		report_error_at(
			tc->log, "Redefinition of identifier", tc->curr_source_index);
		return;
	}
	add_value_type(tc, identifier, type);
}

static void unbind_identifier(struct type_checker *tc, char *identifier) {
	map_pop(tc->value_infos, identifier);
}

static int check_is_data_type(struct type_checker *tc, struct expr *app) {
	struct type *data_type = get_type(tc, app->v.application.fn);

	if (data_type == NULL) {
		/* TODO better message */
		report_error_at(tc->log, "Expected data type", tc->curr_source_index);
		return 0;
	}

	return 1;
}

static void
bind_application(struct type_checker *tc, struct expr *app, struct type *type) {
	if (!check_is_data_type(tc, app)) {
		return;
	}

	bind_parameters(tc, app->v.application.expr_args, type);
}

static void unbind_application(struct type_checker *tc, struct expr *app) {
	if (!check_is_data_type(tc, app)) {
		return;
	}

	unbind_parameters(tc, app->v.application.expr_args);
}

static void
bind_pattern(struct type_checker *tc, struct expr *pattern, struct type *type) {
	switch (pattern->type) {
	case EXPR_IDENTIFIER: bind_identifier(tc, pattern->v.identifier, type); break;
	case EXPR_APPLICATION: bind_application(tc, pattern, type); break;
	case EXPR_GROUPING: bind_pattern(tc, pattern->v.grouping, type); break;
	case EXPR_LIT_INT:
	case EXPR_LIT_DOUBLE:
	case EXPR_LIT_STRING:
	case EXPR_LIT_CHAR:
	case EXPR_LIT_BOOL:
		check_types_match(tc, type, get_type_of_expr(tc, pattern));
	default: assert(0);
	}
}

static void unbind_pattern(struct type_checker *tc, struct expr *pattern) {
	switch (pattern->type) {
	case EXPR_IDENTIFIER: unbind_identifier(tc, pattern->v.identifier); break;
	case EXPR_APPLICATION: unbind_application(tc, pattern); break;
	case EXPR_GROUPING: unbind_pattern(tc, pattern->v.grouping); break;
	case EXPR_LIT_INT:
	case EXPR_LIT_DOUBLE:
	case EXPR_LIT_STRING:
	case EXPR_LIT_CHAR:
	case EXPR_LIT_BOOL: return;
	default: assert(0);
	}
}

static void bind_parameters(struct type_checker *tc,
                            struct list *params,
                            struct type *type) {
	size_t arity = list_length(params);
	size_t i;

	if (arity > calc_type_arity(type)) {
		/* TODO better message */
		report_error_at(
			tc->log, "Mismatched function arity", tc->curr_source_index);
		return;
	}

	for (i = 0; i < arity; i++) {
		struct type *arg_type = list_get(type->type_args, 0);
		type                  = list_get(type->type_args, 1);
		bind_pattern(tc, list_get(params, i), arg_type);
	}
}

static void unbind_parameters(struct type_checker *tc, struct list *params) {
	size_t arity = list_length(params);
	size_t i;

	for (i = 0; i < arity; i++) {
		unbind_pattern(tc, list_get(params, i));
	}
}

/* ========== HIGH LEVEL TYPE CHECK FUNCS ========== */

static void check_types_in_dec_class(struct type_checker *tc,
                                     struct dec_class *dec_class) {
	/* TODO */
}

static void check_types_in_dec_data(struct type_checker *tc,
                                    struct dec_data *dec_data) {
	/* TODO */
}

static void check_types_in_dec_type(struct type_checker *tc,
                                    struct dec_type *dec_type) {
	struct type *existing_type = get_value_type(tc, dec_type->name);
	if (existing_type == NULL) {
		add_value_type(tc, dec_type->name, dec_type->type);
		return;
	}
	check_types_match(tc, existing_type, dec_type->type);
}

static void check_types_in_def_instance(struct type_checker *tc,
                                        struct def_instance *def_instance) {
	/* TODO */
}

static void check_types_in_def_value(struct type_checker *tc,
                                     struct def_value *def_value) {
	struct type *identifier_type = get_value_type(tc, def_value->name);
	struct type *value_type;

	if (identifier_type != NULL) {
		bind_parameters(tc, def_value->expr_params, identifier_type);
	}

	value_type = get_type_of_expr(tc, def_value->value);

	if (identifier_type != NULL) {
		size_t arity             = list_length(def_value->expr_params);
		struct type *return_type = get_return_type(tc, identifier_type, arity);
		check_types_match(tc, value_type, return_type);
		unbind_parameters(tc, def_value->expr_params);
	} else {
		add_value_type(tc, def_value->name, value_type);
	}
}

void check_types_in_stmt(struct type_checker *tc, struct stmt *stmt) {
	tc->curr_source_index = stmt->source_index;
	switch (stmt->type) {
	case STMT_DEC_CLASS: check_types_in_dec_class(tc, stmt->v.dec_class); break;
	case STMT_DEC_DATA: check_types_in_dec_data(tc, stmt->v.dec_data); break;
	case STMT_DEC_TYPE: check_types_in_dec_type(tc, stmt->v.dec_type); break;
	case STMT_DEF_INSTANCE:
		check_types_in_def_instance(tc, stmt->v.def_instance);
		break;
	case STMT_DEF_VALUE: check_types_in_def_value(tc, stmt->v.def_value); break;
	default: assert(0);
	}
}

static void check_types_in_prog(struct type_checker *tc, struct prog *prog) {
	struct list_iter iter = list_iterate(prog->stmts);

	while (!list_iter_at_end(&iter)) {
		struct stmt *stmt = list_iter_next(&iter);
		check_types_in_stmt(tc, stmt);
	}
}

void check_types(struct prog *prog,
                 struct arena *arena_prog,
                 struct error_log *log) {
	struct arena *arena_tc = arena_alloc();
	struct type_checker *tc =
		arena_push_struct_zero(arena_tc, struct type_checker);

	tc->type_infos  = map_new();
	tc->value_infos = map_new();
	tc->arena_prog  = arena_prog;
	tc->arena_tc    = arena_tc;
	tc->log         = log;

	init_type_checker(tc);

	check_types_in_prog(tc, prog);

	map_free(tc->value_infos);
	map_free(tc->type_infos);
	arena_free(arena_tc);
}
