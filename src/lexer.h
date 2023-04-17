#ifndef RACC_LEXER_H
#define RACC_LEXER_H

#include <stddef.h>

struct scanner {
	char *source;
	size_t source_len;
	size_t current;      /* index of current character */
	size_t lexeme_start; /* index of start of current lexeme */
	size_t line, column; /* position in source */
};

char get_ch(struct scanner *s, size_t i);
int is_at_end(struct scanner *s);
char advance(struct scanner *s);
char peek(struct scanner *s);
char peek_next(struct scanner *s);
int match(struct scanner *s, char c);

#endif
