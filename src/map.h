#ifndef RACC_MAP_H
#define RACC_MAP_H

#include "fixint.h"
#include "list.h"
#include <arena.h>

struct map;

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

#endif
