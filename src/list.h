#ifndef RACC_LIST_H
#define RACC_LIST_H

#include <arena.h>
#include <stddef.h>

struct list_elem {
	void *v;
	struct list_elem *prev;
	struct list_elem *next;
};

struct list {
	struct list_elem *head;
	struct list_elem *last;
	struct arena *arena;
};

struct list list_new(struct arena *arena);
size_t list_length(struct list *list);
void **list_to_array(struct list *list, struct arena *arena);
void list_append(struct list *list, void *elem);

#endif
