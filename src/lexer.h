#ifndef RACC_LEXER_H
#define RACC_LEXER_H

#include "error.h"
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
	struct error_log *log;
};

struct token *scan_token(struct scanner *s, struct arena *arena);
struct token **
scan_tokens(char *source, struct arena *arena, struct error_log *log);

#endif
