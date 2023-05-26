#ifndef RACC_MAP_H
#define RACC_MAP_H

#include "list.h"
#include <arena.h>

struct map;

struct map *map_new(void);
void map_free(struct map *map);
void map_put(struct map *map, char *key, void *value);
void *map_pop(struct map *map, char *key);
void *map_get(struct map *map, char *key);

#endif
