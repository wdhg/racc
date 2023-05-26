#ifndef RACC_TYPE_CHECK_H
#define RACC_TYPE_CHECK_H

#include "ast.h"
#include "error.h"
#include <arena.h>

void check_types(struct prog *prog,
                 struct arena *arena_prog,
                 struct error_log *log);

#endif
