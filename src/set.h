#ifndef RACC_SET_H
#define RACC_SET_H

#include "fixint.h"
#include "map.h"

struct set;

struct set *set_new(void);
void set_free(struct set *set);
void set_put(struct set *set, char *key);
void set_pop(struct set *set, char *key);
int set_has(struct set *set, char *key);

#endif
