#include "map.h"
#include "fixint.h"
#include "list.h"
#include <stdlib.h>
#include <string.h>

#define INIT_BUCKET_BIT  (8)
#define RESIZE_THRESHOLD (0.7)

struct map {
	size_t count;
	size_t buckets_bit;
	struct list **buckets;
};

struct map_list_elem {
	u64 hash;
	void *value;
};

/* FNV-1 hash function */
static u64 hash(char *key) {
	u64 h = 0xcbf29ce484222325;
	size_t i;
	for (i = 0; key[i] != '\0'; i++) {
		h *= 0x100000001b3;
		h ^= (u64)key[i];
	}
	return h;
}

static struct list *get_bucket(struct map *map, u64 hash) {
	u64 mask     = (1 << (map->buckets_bit + 1)) - 1;
	size_t index = (size_t)(hash & mask);
	if (map->buckets[index] == NULL) {
		map->buckets[index] = list_new(NULL);
	}
	return map->buckets[index];
}

int find_in_bucket(struct list_iter *iter, u64 key_hash) {
	while (!list_iter_at_end(iter)) {
		struct map_list_elem *elem = list_iter_next(iter);
		if (elem->hash == key_hash) {
			return 1;
		}
	}
	return 0;
}

struct map *map_new(void) {
	struct map *map;
	size_t num_buckets = 1 << INIT_BUCKET_BIT;

	map              = calloc(1, sizeof(struct map));
	map->buckets     = calloc(num_buckets, sizeof(struct list *));
	map->count       = 0;
	map->buckets_bit = INIT_BUCKET_BIT;

	return map;
}

void map_free(struct map *map) {
	size_t buckets_len = 1 << map->buckets_bit;
	size_t i;
	for (i = 0; i < buckets_len; i++) {
		struct list *bucket = map->buckets[i];

		if (bucket == NULL) {
			continue;
		}

		list_free(bucket);
	}
	free(map);
}

void map_put(struct map *map, char *key, void *value) {
	size_t key_hash = hash(key);
	struct list *bucket;
	struct list_iter iter;
	struct map_list_elem *new_elem;

	map->count++;

	if (((float)map->count) / ((float)(1 << map->buckets_bit)) >
	    RESIZE_THRESHOLD) {
		/* TODO resize when count/buckets > 0.7 */
	}

	bucket = get_bucket(map, key_hash);
	iter   = list_iterate(bucket);

	if (find_in_bucket(&iter, key_hash)) {
		struct map_list_elem *elem = list_iter_curr(&iter);
		elem->value                = value;
		return;
	}

	new_elem        = calloc(1, sizeof(struct map_list_elem));
	new_elem->hash  = key_hash;
	new_elem->value = value;
	list_append(bucket, new_elem);
}

void *map_pop(struct map *map, char *key) {
	u64 key_hash          = hash(key);
	struct list *bucket   = get_bucket(map, key_hash);
	struct list_iter iter = list_iterate(bucket);

	if (find_in_bucket(&iter, key_hash)) {
		map->count--;
		return list_iter_remove_curr(&iter);
	}

	return NULL;
}

void *map_get(struct map *map, char *key) {
	u64 key_hash          = hash(key);
	struct list *bucket   = get_bucket(map, key_hash);
	struct list_iter iter = list_iterate(bucket);

	if (find_in_bucket(&iter, key_hash)) {
		struct map_list_elem *elem = list_iter_curr(&iter);
		return elem->value;
	}

	return NULL;
}
