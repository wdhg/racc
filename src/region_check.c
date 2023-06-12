#include "region_check.h"
#include "arena.h"
#include "ast.h"
#include "list.h"
#include "map.h"
#include <assert.h>
#include <string.h>

struct region_checker {
	struct map *type_to_region_sort; /* struct type -> struct region_sort */

	struct arena *arena;

	struct error_log *log;
	size_t curr_source_index;
};

/* ========== REGION SORT BUILDING ========== */

static struct region_sort *new_region_sort(struct arena *arena, char *name) {
	struct region_sort *region_sort =
		arena_push_struct_zero(arena, struct region_sort);
	region_sort->name = arena_push_array_zero(arena, strlen(name) + 1, char);
	strcpy(region_sort->name, name);
	return region_sort;
}

static struct region_sort *
add_region_sort(struct arena *arena,
                struct region_sort *region_sort,
                struct region_sort *region_sort_arg) {
	if (region_sort->region_sort_params == NULL) {
		region_sort->region_sort_params = list_new(arena);
	}
	list_append(region_sort->region_sort_params, region_sort_arg);
	return region_sort;
}

/* ========== REGION CHECKER INITIALIZATION ========== */

struct region_sort region_sort_empty = {
	.name               = NULL,
	.region_sort_params = NULL,
};

struct region_sort region_sort_basic = {
	.name               = "r",
	.region_sort_params = NULL,
};

#define REGION_SORT_R1               (new_region_sort(rc->arena, "r1"))
#define REGION_SORT_R2               (new_region_sort(rc->arena, "r2"))
#define ADD_REGION_SORT(REGION, ARG) (add_region_sort(rc->arena, REGION, ARG))
#define REGION_SORT_ARROW                                                      \
	(ADD_REGION_SORT(ADD_REGION_SORT(&region_sort_empty, REGION_SORT_R1),        \
	                 REGION_SORT_R2))

static void register_region_sort(struct region_checker *rc,
                                 char *name,
                                 struct region_sort *region_sort) {

	map_put_str(rc->type_to_region_sort, name, region_sort);
}

/* ========== REGION INFERENCE RULES ========== */

/* ========== HIGH LEVEL REGION CHECK FUNCS ========== */

static void check_regions_in_type(struct region_checker *rc,
                                  struct type *type,
                                  size_t source_index) {
	if (type->region_sort != NULL) {
		return;
	}

	type->region_sort = map_get_str(rc->type_to_region_sort, type->name);

	if (type->region_sort == NULL) {
		report_error_at(rc->log, "Type doesn't have a region sort", source_index);
		return;
	}

	if (type->type_args != NULL) {
		list_for_each(type->type_args,
		              struct type *,
		              check_regions_in_type(rc, _value, source_index));
	}
}

static void
check_regions_in_dec_constructor(struct region_checker *rc,
                                 struct dec_constructor *dec_constructor) {
	list_for_each(
		dec_constructor->type_params,
		struct type *,
		check_regions_in_type(rc, _value, dec_constructor->source_index));
}

static void check_regions_in_dec_data(struct region_checker *rc,
                                      struct dec_data *dec_data) {
	assert(dec_data->region_sort != NULL);

	map_put_str(rc->type_to_region_sort, dec_data->name, dec_data->region_sort);

	list_for_each(dec_data->dec_constructors,
	              struct dec_constructor *,
	              check_regions_in_dec_constructor(rc, _value));
}

static void check_regions_in_dec_type(struct region_checker *rc,
                                      struct dec_type *dec_type,
                                      size_t source_index) {
	check_regions_in_type(rc, dec_type->type, source_index);
}

static void check_regions_in_def_value(struct region_checker *rc,
                                       struct def_value *def_value,
                                       size_t source_index) {
	list_for_each(def_value->expr_params,
	              struct expr *,
	              check_regions_in_type(rc, _value->type, source_index));
	check_regions_in_type(rc, def_value->value->type, source_index);
}

static void check_regions_in_stmt(struct region_checker *rc,
                                  struct stmt *stmt) {
	rc->curr_source_index = stmt->source_index;
	switch (stmt->type) {
	case STMT_DEC_DATA: check_regions_in_dec_data(rc, stmt->v.dec_data); break;
	case STMT_DEC_TYPE:
		check_regions_in_dec_type(rc, stmt->v.dec_type, stmt->source_index);
		break;
	case STMT_DEF_VALUE:
		check_regions_in_def_value(rc, stmt->v.def_value, stmt->source_index);
		break;
	case STMT_DEC_CLASS: break;    /* TODO UNUSED */
	case STMT_DEF_INSTANCE: break; /* TODO UNUSED */
	default: assert(0);
	}
}

static void check_regions_in_prog(struct region_checker *rc,
                                  struct prog *prog) {
	struct list_iter iter = list_iterate(prog->stmts);
	while (!list_iter_at_end(&iter)) {
		struct stmt *stmt = list_iter_next(&iter);
		check_regions_in_stmt(rc, stmt);
	}
}

void check_regions(struct prog *prog,
                   struct arena *arena,
                   struct error_log *log) {
	struct region_checker *rc =
		arena_push_struct_zero(arena, struct region_checker);
	rc->type_to_region_sort = map_new();
	rc->arena               = arena;
	rc->log                 = log;

	register_region_sort(rc, "Int", &region_sort_basic);
	register_region_sort(rc, "Double", &region_sort_basic);
	register_region_sort(rc, "Char", &region_sort_basic);
	register_region_sort(rc, "Bool", &region_sort_basic);
	register_region_sort(rc, "->", REGION_SORT_ARROW);

	check_regions_in_prog(rc, prog);

	map_free(rc->type_to_region_sort);
}
