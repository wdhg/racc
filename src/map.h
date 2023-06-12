#ifndef RACC_MAP_H
#define RACC_MAP_H

#include "fixint.h"
#include "list.h"
#include <arena.h>

struct map;

struct map_iter {
	struct map *map;
	struct list_iter bucket_iter;
	size_t index_curr;
	size_t index_next;
};

struct map *map_new(void);

void map_free(struct map *map);

void map_put(struct map *map, u8 *key, size_t key_len, void *value);
void map_put_str(struct map *map, char *key, void *value);
void map_put_u64(struct map *map, u64 key, void *value);

void *map_pop(struct map *map, u8 *key, size_t key_len);
void *map_pop_str(struct map *map, char *key);
void *map_pop_u64(struct map *map, u64 key);

void *map_get(struct map *map, u8 *key, size_t key_len);
void *map_get_str(struct map *map, char *key);
void *map_get_u64(struct map *map, u64 key);

struct map_iter map_iterate(struct map *map);
void *map_iter_next(struct map_iter *iter);
int map_iter_at_end(struct map_iter *iter);

#define map_for_each(MAP, MAP_VALUE_TYPE, FUNCTION_CALL)                       \
	{                                                                            \
		struct map_iter _map_for_each_iter = map_iterate(MAP);                     \
		while (!map_iter_at_end(&_map_for_each_iter)) {                            \
			MAP_VALUE_TYPE _value = map_iter_next(&_map_for_each_iter);              \
			FUNCTION_CALL;                                                           \
		}                                                                          \
	}

#endif
