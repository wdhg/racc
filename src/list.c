#include "list.h"

#include <assert.h>

struct list list_new(struct arena *arena) {
	struct list list;
	list.head  = NULL;
	list.last  = NULL;
	list.arena = arena;
	return list;
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

void list_append(struct list *list, void *elem) {
	struct list_elem *new_last;
	int list_is_not_empty = list->head != NULL && list->last != NULL;
	int list_is_empty     = list->head == NULL && list->last == NULL;

	assert(list_is_not_empty || list_is_empty);

	new_last       = arena_push_struct(list->arena, struct list_elem);
	new_last->v    = elem;
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
