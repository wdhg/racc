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

static void scan_comment_single(
	struct token *token, struct scanner *s, struct arena *arena
) {
	token->type = TOK_COMMENT_SINGLE;
	assert(match(s, '/'));
	assert(match(s, '/'));
	while (peek(s) != '\n') {
		advance(s);
	}
	token->lexeme_len = 1 + s->current - s->lexeme_start;
	token->lexeme     = arena_push_array(arena, token->lexeme_len, char);
	strlcpy(token->lexeme, &s->source[s->lexeme_start], token->lexeme_len);
}

static void scan_comment_multi(
	struct token *token, struct scanner *s, struct arena *arena
) {
	token->type = TOK_COMMENT_MULTI;
	assert(match(s, '/'));
	assert(match(s, '*'));
	while (!(advance(s) == '*' && advance(s) == '/')) {
	}
	token->lexeme_len = 1 + s->current - s->lexeme_start;
	token->lexeme     = arena_push_array(arena, token->lexeme_len, char);
	strlcpy(token->lexeme, &s->source[s->lexeme_start], token->lexeme_len);
}

static void
scan_add(struct token *token, struct scanner *s, struct arena *arena) {
	token->type = TOK_ADD;
	assert(match(s, '+'));
	token->lexeme_len = 1;
	token->lexeme     = arena_push_array(arena, token->lexeme_len, char);
	token->lexeme[0]  = s->source[0];
}

static void
scan_sub(struct token *token, struct scanner *s, struct arena *arena) {
	token->type = TOK_SUB;
	assert(match(s, '-'));
	token->lexeme_len = 1;
	token->lexeme     = arena_push_array(arena, token->lexeme_len, char);
	token->lexeme[0]  = s->source[0];
}

static void
scan_mul(struct token *token, struct scanner *s, struct arena *arena) {
	token->type = TOK_MUL;
	assert(match(s, '*'));
	token->lexeme_len = 1;
	token->lexeme     = arena_push_array(arena, token->lexeme_len, char);
	token->lexeme[0]  = s->source[0];
}

static void
scan_div(struct token *token, struct scanner *s, struct arena *arena) {
	token->type = TOK_DIV;
	assert(match(s, '/'));
	token->lexeme_len = 1;
	token->lexeme     = arena_push_array(arena, token->lexeme_len, char);
	token->lexeme[0]  = s->source[0];
}

static void
scan_lt(struct token *token, struct scanner *s, struct arena *arena) {
	token->type = TOK_LT;
	assert(match(s, '<'));
	token->lexeme_len = 1;
	token->lexeme     = arena_push_array(arena, token->lexeme_len, char);
	token->lexeme[0]  = s->source[0];
}

struct token scan_token(struct scanner *s, struct arena *arena) {
	struct token token;
	token.line      = s->line;
	token.column    = s->column;
	s->lexeme_start = s->current;

	switch (peek(s)) {
	case '/':
		switch (peek_next(s)) {
		case '/':
			scan_comment_single(&token, s, arena);
			break;
		case '*':
			scan_comment_multi(&token, s, arena);
			break;
		default:
			scan_div(&token, s, arena);
			break;
		}
		break;
	case '+':
		scan_add(&token, s, arena);
		break;
	case '-':
		scan_sub(&token, s, arena);
		break;
	case '*':
		scan_mul(&token, s, arena);
		break;
	case '<':
		switch (peek_next(s)) {
		default:
			scan_lt(&token, s, arena);
			break;
		}
	default:
		token.type = TOK_NONE;
	}

	return token;
}
