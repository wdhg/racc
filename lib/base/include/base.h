#ifndef RACC_BASE_H
#define RACC_BASE_H

#include <arena.h>
#include <stddef.h>

/* ========== REGIONS ========== */

struct region {
	struct arena *arena;
	unsigned int reference_count;
};

struct region *region_new(void);
void region_free(struct region *);
void region_retain(struct region *);
void region_release(struct region *);

#define region_push_struct(REGION, TYPE)                                       \
	(arena_push_struct_zero(REGION->arena, TYPE))

/* ========== CLOSURES/THUNKS ========== */

struct thunk;

struct closure {
	size_t fn_arity;
	size_t args_len;
	struct thunk **args;
	void *(*fn)(struct thunk **, struct region *);
};

struct thunk {
	struct region *region;
	int evaluated;
	void *(*value_copy)(void *, struct region *);

	/* evaluated = 1 */
	void *value;

	/* evaluated = 0 */
	struct closure *closure;
};

void *_thunk_eval(struct thunk *);
#define thunk_eval(THUNK, TYPE) ((TYPE)_thunk_eval(THUNK))
struct thunk *thunk_closure(struct closure *,
                            struct region *,
                            void *(*)(void *, struct region *));
struct thunk *
thunk_lit(void *, struct region *, void *(*)(void *, struct region *));
struct thunk *thunk_apply(struct thunk *, struct thunk *);
struct thunk *thunk_copy(struct thunk *, struct region *);
void thunk_retain(struct thunk *);
void thunk_release(struct thunk *);

/* ========== CONTROL FLOW ========== */

/* ========== LANGUAGE DEFINED DATA TYPES ========== */

/* lists */
/* TODO rename to avoid potential collisions with user defined structures */

enum data_List_type { DATA_List_Null, DATA_List_Cons };

struct data_List {
	enum data_List_type type;

	union {
		struct {
			struct thunk *param_0;
			struct thunk *param_1;
		} Cons;
	} v;
};

void *value_copy_List(void *, struct region *);

struct thunk *val_Null;
struct closure *closure_Cons;

/* tuples */

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
void *fn_add(struct thunk **, struct region *);
void *fn_sub(struct thunk **, struct region *);
void *fn_mul(struct thunk **, struct region *);
void *fn_div(struct thunk **, struct region *);

struct closure *closure_add;
struct closure *closure_sub;
struct closure *closure_mul;
struct closure *closure_div;

/* comparisons */
void *fn_eq(struct thunk **, struct region *);
void *fn_lt(struct thunk **, struct region *);
void *fn_gt(struct thunk **, struct region *);
void *fn_lte(struct thunk **, struct region *);
void *fn_gte(struct thunk **, struct region *);

struct closure *closure_eq;
struct closure *closure_lt;
struct closure *closure_gt;
struct closure *closure_lte;
struct closure *closure_gte;

#endif
