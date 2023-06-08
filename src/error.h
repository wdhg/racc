#ifndef RACC_ERROR_H
#define RACC_ERROR_H

#include "ast.h"
#include "token.h"
#include <arena.h>
#include <stddef.h>

struct error_log {
	char *source;
	int had_error;
	int suppress_error_messages;
};

void report_error(struct error_log *log, char *error_msg);
void report_error_at(struct error_log *log, char *error_msg, size_t index);
void report_type_error(struct error_log *log,
                       struct type *type_a,
                       struct type *type_b,
                       size_t index);

#endif
