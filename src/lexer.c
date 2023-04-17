#include "lexer.h"

char get_ch(struct scanner *s, size_t i) {
	if (i >= s->source_len) {
		return '\0';
	}
	return s->source[i];
}

char advance(struct scanner *s) {
	char c = get_ch(s, s->current);
	if (c == '\n') {
		s->line++;
		s->column = 0;
	} else {
		s->column++;
	}
	s->current++;
	return c;
}

char peek(struct scanner *s) { return get_ch(s, s->current); }

char peek_next(struct scanner *s) { return get_ch(s, s->current + 1); }

int match(struct scanner *s, char c) {
	if (s->current >= s->source_len || peek(s) != c) {
		return 0;
	}
	advance(s);
	return 1;
}
