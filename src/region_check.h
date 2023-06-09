#ifndef RACC_REGION_CHECK_H
#define RACC_REGION_CHECK_H

#include "ast.h"
#include "error.h"
#include <arena.h>

void check_regions(struct prog *prog,
                   struct arena *arena_prog,
                   struct error_log *log);

#endif
