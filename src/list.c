#include "list.h"

#include <assert.h>
#include <stdlib.h>

struct list_elem {
	void *v;
	struct list_elem *prev;
	struct list_elem *next;
};

struct list {
	struct arena *arena;
	struct list_elem *head;
	struct list_elem *last;
};

struct list *list_new(struct arena *arena) {
	struct list *list;

	if (arena == NULL) {
		list        = calloc(1, sizeof(struct list));
		list->arena = NULL;
	} else {
		list        = arena_push_struct_zero(arena, struct list);
		list->arena = arena;
	}

	list->head = NULL;
	list->last = NULL;
	return list;
}

void list_free(struct list *list) {
	if (list->arena == NULL) {
		list_clear(list);
		free(list);
	} else {
		arena_free(list->arena);
	}
}

struct list *list_copy(struct list *list_from, struct arena *arena_to) {
	struct list *list_to  = list_new(arena_to);
	struct list_iter iter = list_iterate(list_from);

	while (!list_iter_at_end(&iter)) {
		void *value = list_iter_next(&iter);
		list_append(list_to, value);
	}

	return list_to;
}

size_t list_length(struct list *list) {
	size_t length             = 0;
	struct list_elem *current = list->head;

	while (current != NULL) {
		length++;
		current = current->next;
	}

	return length;
}

void **list_to_array(struct list *list, struct arena *arena) {
	size_t length             = list_length(list);
	void **array              = arena_push_array(arena, length, void *);
	struct list_elem *current = list->head;
	size_t i;

	for (i = 0; i < length; i++) {
		array[i] = current->v;
		current  = current->next;
	}

	return array;
}

void *list_get(struct list *list, size_t i) {
	struct list_iter iter = list_iterate(list);

	while (i > 0) {
		i--;
		list_iter_next(&iter);

		if (list_iter_at_end(&iter)) {
			return NULL; /* TODO consider error instead */
		}
	}

	return list_iter_next(&iter); /* compenstate for starting on curr = NULL */
}

void *list_pop_head(struct list *list) {
	struct list_iter iter = list_iterate(list);
	list_iter_next(&iter);
	return list_iter_remove_curr(&iter);
}

void list_prepend(struct list *list, void *value) {
	struct list_elem *new_first;
	int list_is_not_empty = list->head != NULL && list->last != NULL;
	int list_is_empty     = list->head == NULL && list->last == NULL;

	assert(list_is_not_empty || list_is_empty);

	if (list->arena == NULL) {
		new_first = calloc(1, sizeof(struct list_elem));
	} else {
		new_first = arena_push_struct(list->arena, struct list_elem);
	}

	new_first->v    = value;
	new_first->prev = NULL;
	new_first->next = NULL;

	if (list_is_not_empty) {
		new_first->next  = list->head;
		list->head->prev = new_first;
	} else if (list_is_empty) {
		list->last = new_first;
	}

	list->head = new_first;
}

void list_prepend_all(struct list *list_to, struct list *list_from) {
	struct list_iter iter = list_iterate_reverse(list_from);

	while (!list_iter_at_end(&iter)) {
		void *value = list_iter_next(&iter);
		list_prepend(list_to, value);
	}
}

void list_append(struct list *list, void *value) {
	struct list_elem *new_last;
	int list_is_not_empty = list->head != NULL && list->last != NULL;
	int list_is_empty     = list->head == NULL && list->last == NULL;

	assert(list_is_not_empty || list_is_empty);

	if (list->arena == NULL) {
		new_last = calloc(1, sizeof(struct list_elem));
	} else {
		new_last = arena_push_struct(list->arena, struct list_elem);
	}

	new_last->v    = value;
	new_last->prev = NULL;
	new_last->next = NULL;

	if (list_is_not_empty) {
		new_last->prev   = list->last;
		list->last->next = new_last;
	} else if (list_is_empty) {
		list->head = new_last;
	}

	list->last = new_last;
}

void list_clear(struct list *list) {
	struct list_elem *curr = list->head;

	while (curr != NULL) {
		struct list_elem *next = curr->next;
		if (list->arena == NULL) {
			free(curr);
		}
		curr = next;
	}

	list->head = NULL;
	list->last = NULL;
}

struct list_iter list_iterate(struct list *list) {
	struct list_iter iter;
	iter.curr       = NULL;
	iter.next       = list->head;
	iter.list       = list;
	iter.is_reverse = 0;
	return iter;
}

struct list_iter list_iterate_reverse(struct list *list) {
	struct list_iter iter;
	iter.curr       = NULL;
	iter.next       = list->last;
	iter.list       = list;
	iter.is_reverse = 1;
	return iter;
}

void *list_iter_curr(struct list_iter *iter) {
	assert(iter->curr != NULL);
	return iter->curr->v;
}

void *list_iter_next(struct list_iter *iter) {
	assert(iter->next != NULL);
	iter->curr = iter->next;
	iter->next = iter->is_reverse ? iter->next->prev : iter->next->next;
	return iter->curr->v;
}

void *list_iter_remove_curr(struct list_iter *iter) {
	struct list_elem *curr, *prev, *next;
	void *value;

	curr = iter->curr;
	assert(curr != NULL);
	prev = curr->prev;
	next = curr->next;

	if (prev == NULL) {
		iter->list->head = next;
	} else {
		prev->next = next;
	}

	if (next == NULL) {
		iter->list->last = prev;
	} else {
		next->prev = prev;
	}

	iter->curr = iter->is_reverse ? prev : next;
	if (iter->curr != NULL) {
		iter->next = iter->is_reverse ? iter->curr->prev : iter->curr->next;
	}
	value = curr->v;

	if (iter->list->arena == NULL) {
		free(curr);
	}

	return value;
}

int list_iter_at_end(struct list_iter *iter) { return iter->next == NULL; }
