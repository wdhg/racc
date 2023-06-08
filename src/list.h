#ifndef RACC_LIST_H
#define RACC_LIST_H

#include <arena.h>
#include <stddef.h>

struct list_elem;
struct list;

struct list_iter {
	struct list_elem *curr, *next;
	struct list *list;
	int is_reverse;
};

struct list *list_new(struct arena *arena);
void list_free(struct list *list);
struct list *list_copy(struct list *list_from, struct arena *arena_to);

size_t list_length(struct list *list);
void **list_to_array(struct list *list, struct arena *arena);
void *list_head(struct list *list);
void *list_last(struct list *list);
void *list_get(struct list *list, size_t i);
void *list_pop_head(struct list *list);
void list_prepend(struct list *list, void *value);
void list_prepend_all(struct list *list_to, struct list *list_from);
void list_append(struct list *list, void *value);
void list_clear(struct list *list);

struct list_iter list_iterate(struct list *list);
struct list_iter list_iterate_reverse(struct list *list);
void *list_iter_curr(struct list_iter *iter);
void *list_iter_next(struct list_iter *iter);
void *list_iter_remove_curr(struct list_iter *iter);
int list_iter_at_end(struct list_iter *iter);

#define list_for_each(LIST, LIST_VALUE_TYPE, FUNCTION_CALL)                    \
	{                                                                            \
		struct list_iter _list_for_each_iter = list_iterate(LIST);                 \
		while (!list_iter_at_end(&_list_for_each_iter)) {                          \
			LIST_VALUE_TYPE _value = list_iter_next(&_list_for_each_iter);           \
			FUNCTION_CALL;                                                           \
		}                                                                          \
	}

#define list_map(LIST_DEST, LIST, LIST_VALUE_TYPE, FUNCTION_CALL)              \
	{                                                                            \
		list_clear(LIST_DEST);                                                     \
		list_for_each(                                                             \
			LIST, LIST_VALUE_TYPE, list_append(LIST_DEST, FUNCTION_CALL));           \
	}

#define list_zip_with(                                                         \
	LIST_1, LIST_1_VALUE_TYPE, LIST_2, LIST_2_VALUE_TYPE, FUNCTION_CALL)         \
	{                                                                            \
		struct list_iter _list_zip_with_iter_1 = list_iterate(LIST_1);             \
		struct list_iter _list_zip_with_iter_2 = list_iterate(LIST_2);             \
		while (!list_iter_at_end(&_list_zip_with_iter_1) &&                        \
		       !list_iter_at_end(&_list_zip_with_iter_2)) {                        \
			LIST_1_VALUE_TYPE _value_1 = list_iter_next(&_list_zip_with_iter_1);     \
			LIST_2_VALUE_TYPE _value_2 = list_iter_next(&_list_zip_with_iter_2);     \
			FUNCTION_CALL;                                                           \
		}                                                                          \
	}

#endif
