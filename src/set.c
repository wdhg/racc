#include "set.h"

#include "fixint.h"
#include <stdlib.h>

struct set {
	struct map *map;
};

struct set *set_new(void) {
	struct set *set = calloc(1, sizeof(struct set));
	set->map        = map_new();
	return set;
}

void set_free(struct set *set) {
	map_free(set->map);
	free(set);
}

void set_put(struct set *set, u8 *key, size_t key_len) {
	map_put(set->map, key, key_len, (void *)0x1);
}

void set_pop(struct set *set, u8 *key, size_t key_len) {
	map_pop(set->map, key, key_len);
}

int set_has(struct set *set, u8 *key, size_t key_len) {
	return map_get(set->map, key, key_len) != NULL;
}
