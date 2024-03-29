#include "map.h"
#include "fixint.h"
#include "list.h"
#include <assert.h>
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
static u64 hash(u8 *key, size_t key_len) {
	u64 h = 0xcbf29ce484222325;
	size_t i;
	for (i = 0; i < key_len; i++) {
		h *= 0x100000001b3;
		h ^= (u64)key[i];
	}
	return h;
}

static struct list *get_bucket(struct map *map, u64 hash) {
	u64 mask     = (1 << map->buckets_bit) - 1;
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

void map_put(struct map *map, u8 *key, size_t key_len, void *value) {
	size_t key_hash = hash(key, key_len);
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

void map_put_str(struct map *map, char *key, void *value) {
	map_put(map, (u8 *)key, strlen(key), value);
}

void map_put_u64(struct map *map, u64 key, void *value) {
	map_put(map, (u8 *)&key, sizeof(u64) / sizeof(u8), value);
}

void *map_pop(struct map *map, u8 *key, size_t key_len) {
	u64 key_hash          = hash(key, key_len);
	struct list *bucket   = get_bucket(map, key_hash);
	struct list_iter iter = list_iterate(bucket);

	if (find_in_bucket(&iter, key_hash)) {
		map->count--;
		return list_iter_remove_curr(&iter);
	}

	return NULL;
}

void *map_pop_str(struct map *map, char *key) {
	return map_pop(map, (u8 *)key, strlen(key));
}

void *map_pop_u64(struct map *map, u64 key) {
	return map_pop(map, (u8 *)&key, sizeof(u64) / sizeof(u8));
}

void *map_get(struct map *map, u8 *key, size_t key_len) {
	u64 key_hash          = hash(key, key_len);
	struct list *bucket   = get_bucket(map, key_hash);
	struct list_iter iter = list_iterate(bucket);

	if (find_in_bucket(&iter, key_hash)) {
		struct map_list_elem *elem = list_iter_curr(&iter);
		return elem->value;
	}

	return NULL;
}

void *map_get_str(struct map *map, char *key) {
	return map_get(map, (u8 *)key, strlen(key));
}

void *map_get_u64(struct map *map, u64 key) {
	return map_get(map, (u8 *)&key, sizeof(u64) / sizeof(u8));
}

struct map_iter map_iterate(struct map *map) {
	struct map_iter iter;
	iter.map                    = map;
	iter.bucket_iter.curr       = NULL;
	iter.bucket_iter.next       = NULL;
	iter.bucket_iter.list       = NULL;
	iter.bucket_iter.is_reverse = 0;
	iter.index_curr             = 0;

	for (iter.index_next = 0; iter.index_next < (1 << iter.map->buckets_bit);
	     iter.index_next++) {
		if (iter.map->buckets[iter.index_next] != NULL) {
			break;
		}
	}

	return iter;
}

void *map_iter_next(struct map_iter *iter) {
	assert(!map_iter_at_end(iter));

	if (list_iter_at_end(&iter->bucket_iter)) {
		iter->index_curr  = iter->index_next;
		iter->bucket_iter = list_iterate(iter->map->buckets[iter->index_curr]);

		iter->index_next++; /* increment so index_curr != index_next */
		for (; iter->index_next < (1 << iter->map->buckets_bit);
		     iter->index_next++) {
			if (iter->map->buckets[iter->index_next] != NULL) {
				break;
			}
		}
	}

	return ((struct map_list_elem *)list_iter_next(&iter->bucket_iter))->value;
}

int map_iter_at_end(struct map_iter *iter) {
	return iter->index_next >= (1 << iter->map->buckets_bit);
}
