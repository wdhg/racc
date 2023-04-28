#ifndef RACC_PARSER_H
#define RACC_PARSER_H

#include <arena.h>
#include <stddef.h>

#include "ast.h"
#include "token.h"

struct parser {
	struct token **tokens;
	size_t current;
	struct arena *arena;
	struct error_log *error_log;
};

struct expr *parse_expr(struct parser *p);

#endif
