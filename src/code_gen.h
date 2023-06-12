#ifndef RACC_CODE_GEN_H
#define RACC_CODE_GEN_H

#include "ast.h"
#include "error.h"
#include <arena.h>

void code_gen(struct prog *prog,
              struct arena *arena,
              struct error_log *log,
              char *file_name);

#endif
