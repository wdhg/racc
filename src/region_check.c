#include "region_check.h"
#include "arena.h"
#include "ast.h"
#include "list.h"
#include "map.h"
#include <assert.h>
#include <string.h>

struct region_checker {
	struct arena *arena;

	struct error_log *log;
	size_t curr_source_index;
};

/* ========== REGION INFERENCE RULES ========== */

/* ========== HIGH LEVEL REGION CHECK FUNCS ========== */

static void check_regions_in_type(struct region_checker *rc,
                                  struct type *type,
                                  size_t source_index) {
	return;
}

static void
check_regions_in_dec_constructor(struct region_checker *rc,
                                 struct dec_constructor *dec_constructor) {}

static void check_regions_in_dec_data(struct region_checker *rc,
                                      struct dec_data *dec_data) {}

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
	rc->arena = arena;
	rc->log   = log;

	check_regions_in_prog(rc, prog);
}
