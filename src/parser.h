#ifndef RACC_PARSER_H
#define RACC_PARSER_H

#include "ast.h"
#include "error.h"
#include "token.h"
#include <arena.h>
#include <stddef.h>

struct parser {
	struct token **tokens;
	size_t current;
	struct arena *arena;
	struct error_log *log;
};

struct expr *parse_expr(struct parser *p);
struct type *parse_type(struct parser *p);
struct stmt *parse_stmt(struct parser *p);
struct prog *parse_prog(struct parser *p);
struct prog *
parse(struct token **tokens, struct arena *arena, struct error_log *log);

#endif
