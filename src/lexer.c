#include "lexer.h"

#include "token.h"
#include <arena.h>
#include <assert.h>

/* ========== SCANNER ========== */

static char get_ch(struct scanner *s, size_t i) {
	if (i >= s->source_len) {
		return '\0';
	}
	return s->source[i];
}

static char advance(struct scanner *s) {
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

static char peek(struct scanner *s) { return get_ch(s, s->current); }

static char peek_next(struct scanner *s) { return get_ch(s, s->current + 1); }

static int match(struct scanner *s, char c) {
	if (s->current >= s->source_len || peek(s) != c) {
		return 0;
	}
	advance(s);
	return 1;
}

/* ========== LEXER ========== */

static struct token
scan_comment_single(struct scanner *s, struct arena *arena) {
	struct token token;
	token.type   = TOK_COMMENT_SINGLE;
	token.line   = s->line;
	token.column = s->column;
	while (peek(s) != '\n') {
		advance(s);
	}
	token.lexeme_len = 1 + s->current - s->lexeme_start;
	token.lexeme     = arena_push_array(arena, token.lexeme_len, char);
	strlcpy(token.lexeme, &s->source[s->lexeme_start], token.lexeme_len);
	return token;
}

static struct token scan_comment_multi(struct scanner *s, struct arena *arena) {
	struct token token;
	token.type   = TOK_COMMENT_MULTI;
	token.line   = s->line;
	token.column = s->column;
	while (!(advance(s) == '*' && advance(s) == '/')) {
	}
	token.lexeme_len = 1 + s->current - s->lexeme_start;
	token.lexeme     = arena_push_array(arena, token.lexeme_len, char);
	strlcpy(token.lexeme, &s->source[s->lexeme_start], token.lexeme_len);
	return token;
}

struct token scan_token(struct scanner *s, struct arena *arena) {
	s->lexeme_start = s->current;

	switch (peek(s)) {
	case '/':
		switch (peek_next(s)) {
		case '/':
			return scan_comment_single(s, arena);
		case '*':
			return scan_comment_multi(s, arena);
		default:
			assert(0);
		}
	default:
		assert(0);
	}
}
