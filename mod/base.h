#ifndef RACC_BASE_H
#define RACC_BASE_H

#include <stddef.h>

/* ========== CLOSURES/THUNKS ========== */

struct thunk;
struct closure;

struct thunk {
	int evaluated;

	/* evaluated = 0 */
	struct closure *closure;

	/* evaluated = 1 */
	void *value;
};

struct closure {
	size_t fn_arity;
	size_t args_len;
	void *(*fn)(struct thunk **args);
	struct thunk **args;
};

void *_thunk_eval(struct thunk *thunk);
#define thunk_eval(THUNK, TYPE) ((TYPE)_thunk_eval(THUNK))

struct thunk *closure_to_thunk(struct closure *closure);

struct closure *closure_apply(struct closure *closure, struct thunk *arg);
void *closure_eval(struct closure *closure);

struct thunk *lit_thunk(void *value);

/* ========== CONTROL FLOW ========== */

/* ========== LANGUAGE DEFINED DATA TYPES ========== */

enum base_List_type { DATA_List_Null, DATA_List_Cons };

struct base_List {
	enum base_List_type type;

	union {
		struct {
			void *value;
			struct base_List *next;
		} Cons;
	} v;
};

struct base_List *data_List_Null(void);
struct base_List *data_List_Cons(void);

enum base_Tuple_type { DATA_Tuple_Tuple };

struct base_Tuple {
	enum base_Tuple_type type;

	union {
		struct {
			void *lhs;
			void *rhs;
		} Tuple;
	} v;
};

struct base_Tuple *data_Tuple_Tuple(void);

/* ========== LANGUAGE DEFINED FUNCTIONS ========== */

/* arithmetic */
void *fn_add(struct thunk **args);
void *fn_sub(struct thunk **args);
void *fn_mul(struct thunk **args);
void *fn_div(struct thunk **args);

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

/* comparisons */
void *fn_eq(struct thunk **args);
void *fn_lt(struct thunk **args);
void *fn_gt(struct thunk **args);
void *fn_lte(struct thunk **args);
void *fn_gte(struct thunk **args);

struct closure *closure_eq;
struct closure *closure_lt;
struct closure *closure_gt;
struct closure *closure_lte;
struct closure *closure_gte;

/* lists */

/* TODO */

/* tuples */

/* TODO */

#endif
