#ifndef RACC_SET_H
#define RACC_SET_H

#include "fixint.h"
#include "map.h"

struct set;

struct set *set_new(void);
void set_free(struct set *set);
void set_put(struct set *set, u8 *key, size_t key_len);
void set_pop(struct set *set, u8 *key, size_t key_len);
int set_has(struct set *set, u8 *key, size_t key_len);

#endif
