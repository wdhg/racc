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

#endif
