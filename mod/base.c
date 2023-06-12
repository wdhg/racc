#include "base.h"
#include <assert.h>
#include <stdlib.h>

/* ========== CLOSURES/THUNKS ========== */

void *_thunk_eval(struct thunk *thunk) {
	if (!thunk->evaluated) {
		thunk->value     = closure_eval(thunk->closure);
		thunk->evaluated = 1;
	}
	return thunk->value;
}

struct thunk *closure_to_thunk(struct closure *closure) {
	/* TODO allocate thunk in arena */
	struct thunk *thunk;
	thunk            = calloc(1, sizeof(struct thunk));
	thunk->evaluated = 0;
	thunk->closure   = closure;
	return thunk;
}

struct closure *closure_copy(struct closure *closure) {
	struct closure *result;
	size_t i;
	/* TODO allocate in arena */
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

struct closure *closure_apply(struct closure *closure, struct thunk *arg) {
	struct closure *result;
	assert(closure->args_len < closure->fn_arity);
	result                         = closure_copy(closure);
	result->args[result->args_len] = arg;
	result->args_len++;
	return result;
}

void *closure_eval(struct closure *closure) {
	assert(closure->fn_arity == closure->args_len);
	return closure->fn(closure->args);
}

struct thunk *lit_thunk(void *value) {
	/* TODO allocate thunk in arena */
	struct thunk *thunk;
	thunk            = calloc(1, sizeof(struct thunk));
	thunk->evaluated = 1;
	thunk->value     = value;
	return thunk;
}

/* ========== CONTROL FLOW ========== */

/* ========== LANGUAGE DEFINED DATA TYPES ========== */

/* ========== LANGUAGE DEFINED FUNCTIONS ========== */

void *fn_add(struct thunk **args) {
	int v_0 = thunk_eval(args[0], int);
	int v_1 = thunk_eval(args[1], int);
	return v_0 + v_1;
}

void *fn_sub(struct thunk **args) {
	int v_0 = thunk_eval(args[0], int);
	int v_1 = thunk_eval(args[1], int);
	return v_0 - v_1;
}

void *fn_mul(struct thunk **args) {
	int v_0 = thunk_eval(args[0], int);
	int v_1 = thunk_eval(args[1], int);
	return v_0 * v_1;
}

void *fn_div(struct thunk **args) {
	int v_0 = thunk_eval(args[0], int);
	int v_1 = thunk_eval(args[1], int);
	return v_0 / v_1;
}
