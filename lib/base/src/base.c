#include "base.h"
#include "arena.h"
#include <assert.h>
#include <stdlib.h>

/* ========== REGIONS ========== */

struct region *region_new(void) {
	struct region *region   = calloc(1, sizeof(struct region));
	region->arena           = arena_alloc();
	region->reference_count = 0;
	return region;
}

void region_free(struct region *region) {
	arena_free(region->arena);
	region->arena = NULL;
}

void region_retain(struct region *region) { region->reference_count++; }

void region_release(struct region *region) {
	region->reference_count--;
	if (region->reference_count == 0) {
		region_free(region);
	}
}

/* ========== CLOSURES/THUNKS ========== */

void *_thunk_eval(struct thunk *thunk) {
	if (!thunk->evaluated) {
		thunk->value     = thunk->closure->fn(thunk->closure->args, thunk->region);
		thunk->evaluated = 1;
	}
	return thunk->value;
}

static struct thunk *thunk_alloc(struct region *region) {
	struct thunk *thunk;
	if (region == NULL) {
		thunk = calloc(1, sizeof(struct thunk));
	} else {
		thunk = arena_push_struct_zero(region->arena, struct thunk);
	}
	thunk->region = region;
	region_retain(region);
	return thunk;
}

struct thunk *thunk_closure(struct closure *closure,
                            struct region *region,
                            void *(value_copy)(void *, struct region *region)) {
	struct thunk *thunk;
	thunk             = thunk_alloc(region);
	thunk->evaluated  = 0;
	thunk->value_copy = value_copy;
	thunk->closure    = closure;
	return thunk;
}

struct thunk *thunk_lit(void *value,
                        struct region *region,
                        void *(value_copy)(void *, struct region *region)) {
	struct thunk *thunk;
	thunk             = thunk_alloc(region);
	thunk->evaluated  = 1;
	thunk->value_copy = value_copy;
	thunk->value      = value;
	return thunk;
}

static struct closure *closure_copy(struct closure *closure) {
	struct closure *result;
	size_t i;
	result           = calloc(1, sizeof(struct closure));
	result->fn_arity = closure->fn_arity;
	result->fn       = closure->fn;
	result->args_len = closure->args_len;
	result->args     = calloc(result->fn_arity, sizeof(struct thunk *));
	for (i = 0; i < result->args_len; i++) {
		result->args[i] = closure->args[i]; /* we can resuse the thunks */
	}
	return result;
}

struct thunk *thunk_apply(struct thunk *thunk_closure,
                          struct thunk *thunk_arg) {
	struct thunk *result;
	result = thunk_copy(thunk_closure, thunk_arg->region);
	result->closure->args[result->closure->args_len] = thunk_arg;
	result->closure->args_len++;
	return result;
}

struct thunk *thunk_copy(struct thunk *thunk, struct region *region) {
	struct thunk *result;
	if (region == NULL) {
		result = calloc(1, sizeof(struct thunk));
	} else {
		if (region->arena == NULL) {
			region->arena = arena_alloc();
		}
		result = region_push_struct(region, struct thunk);
	}
	result->region     = region;
	result->evaluated  = thunk->evaluated;
	result->value_copy = thunk->value_copy;
	if (thunk->evaluated) {
		result->value = thunk->value_copy(thunk->value, region);
	} else {
		result->closure =
			thunk->closure == NULL ? NULL : closure_copy(thunk->closure);
	}
	return result;
}

void thunk_retain(struct thunk *thunk) { region_retain(thunk->region); }

void thunk_release(struct thunk *thunk) { region_release(thunk->region); }

/* ========== LANGUAGE DEFINED DATA TYPES ========== */

void *value_copy_List(void *value, struct region *region) {
	struct data_List *data = value;
	struct data_List *copy;
	if (region->arena == NULL) {
		region->arena = arena_alloc();
	}
	copy       = region_push_struct(region, struct data_List);
	copy->type = data->type;
	if (data->type == DATA_List_Cons) {
		copy->v.Cons.param_0 = thunk_copy(data->v.Cons.param_0, region);
		copy->v.Cons.param_1 = thunk_copy(data->v.Cons.param_1, region);
	}
	return copy;
}

struct data_List _data_List_Null = {
	.type = DATA_List_Null,
};
struct thunk _val_Null = {
	.evaluated = 1,
	.closure   = NULL,
	.value     = &_data_List_Null,
};
struct thunk *val_Null = &_val_Null;

void *fn_Cons(struct thunk **args, struct region *region) {
	struct data_List *value;
	if (region == NULL) {
		value = calloc(1, sizeof(struct data_List));
	} else {
		if (region->arena == NULL) {
			region->arena = arena_alloc();
		}
		value = region_push_struct(region, struct data_List);
	}
	value->type           = DATA_List_Cons;
	value->v.Cons.param_0 = args[0];
	value->v.Cons.param_1 = args[1];
	return value;
}
struct closure _closure_Cons = {
	.fn_arity = 2,
	.args_len = 0,
	.fn       = fn_Cons,
	.args     = NULL,
};
struct closure *closure_Cons = &_closure_Cons;

/* ========== LANGUAGE DEFINED FUNCTIONS ========== */

void *value_copy_Int(void *value, struct region *region) {
	(void)region;
	return (int)value;
}

void *value_copy_Char(void *value, struct region *region) {
	(void)region;
	return (char)value;
}

void *value_copy_Bool(void *value, struct region *region) {
	(void)region;
	return (char)value;
}

void *fn_add(struct thunk **args, struct region *region) {
	(void)region; /* we don't need to allocate */
	int v_0 = thunk_eval(args[0], int);
	int v_1 = thunk_eval(args[1], int);
	return v_0 + v_1;
}

void *fn_sub(struct thunk **args, struct region *region) {
	(void)region; /* we don't need to allocate */
	int v_0 = thunk_eval(args[0], int);
	int v_1 = thunk_eval(args[1], int);
	return v_0 - v_1;
}

void *fn_mul(struct thunk **args, struct region *region) {
	(void)region; /* we don't need to allocate */
	int v_0 = thunk_eval(args[0], int);
	int v_1 = thunk_eval(args[1], int);
	return v_0 * v_1;
}

void *fn_div(struct thunk **args, struct region *region) {
	(void)region; /* we don't need to allocate */
	int v_0 = thunk_eval(args[0], int);
	int v_1 = thunk_eval(args[1], int);
	return v_0 / v_1;
}

struct closure _closure_add = {
	.fn_arity = 2,
	.args_len = 0,
	.fn       = fn_add,
	.args     = NULL,
};
struct closure _closure_sub = {
	.fn_arity = 2,
	.args_len = 0,
	.fn       = fn_sub,
	.args     = NULL,
};
struct closure _closure_mul = {
	.fn_arity = 2,
	.args_len = 0,
	.fn       = fn_mul,
	.args     = NULL,
};
struct closure _closure_div = {
	.fn_arity = 2,
	.args_len = 0,
	.fn       = fn_div,
	.args     = NULL,
};

struct closure *closure_add = &_closure_add;
struct closure *closure_sub = &_closure_sub;
struct closure *closure_mul = &_closure_mul;
struct closure *closure_div = &_closure_div;
