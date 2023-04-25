#ifndef RACC_LEXER_H
#define RACC_LEXER_H

#include "token.h"
#include <arena.h>
#include <ctype.h>
#include <stddef.h>
#include <string.h>

struct scanner {
	char *source;
	size_t source_len;
	size_t current;      /* index of current character */
	size_t lexeme_start; /* index of start of current lexeme */
	size_t line, column; /* position in source */
};

struct token *scan_token(struct scanner *s, struct arena *arena);

#endif
