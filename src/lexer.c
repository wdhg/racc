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
	assert(match(s, '/'));
	assert(match(s, '/'));
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
	assert(match(s, '/'));
	assert(match(s, '*'));
	while (!(advance(s) == '*' && advance(s) == '/')) {
	}
	token.lexeme_len = 1 + s->current - s->lexeme_start;
	token.lexeme     = arena_push_array(arena, token.lexeme_len, char);
	strlcpy(token.lexeme, &s->source[s->lexeme_start], token.lexeme_len);
	return token;
}

static struct token scan_add(struct scanner *s, struct arena *arena) {
	struct token token;
	token.type   = TOK_ADD;
	token.line   = s->line;
	token.column = s->column;
	assert(match(s, '+'));
	token.lexeme_len = 1;
	token.lexeme     = arena_push_array(arena, token.lexeme_len, char);
	token.lexeme[0]  = s->source[0];
	return token;
}

static struct token scan_sub(struct scanner *s, struct arena *arena) {
	struct token token;
	token.type   = TOK_SUB;
	token.line   = s->line;
	token.column = s->column;
	assert(match(s, '-'));
	token.lexeme_len = 1;
	token.lexeme     = arena_push_array(arena, token.lexeme_len, char);
	token.lexeme[0]  = s->source[0];
	return token;
}

static struct token scan_mul(struct scanner *s, struct arena *arena) {
	struct token token;
	token.type   = TOK_MUL;
	token.line   = s->line;
	token.column = s->column;
	assert(match(s, '*'));
	token.lexeme_len = 1;
	token.lexeme     = arena_push_array(arena, token.lexeme_len, char);
	token.lexeme[0]  = s->source[0];
	return token;
}

static struct token scan_div(struct scanner *s, struct arena *arena) {
	struct token token;
	token.type   = TOK_DIV;
	token.line   = s->line;
	token.column = s->column;
	assert(match(s, '/'));
	token.lexeme_len = 1;
	token.lexeme     = arena_push_array(arena, token.lexeme_len, char);
	token.lexeme[0]  = s->source[0];
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
			return scan_div(s, arena);
		}
	case '+':
		return scan_add(s, arena);
	case '-':
		return scan_sub(s, arena);
	case '*':
		return scan_mul(s, arena);
	default:
		assert(0);
	}
}
