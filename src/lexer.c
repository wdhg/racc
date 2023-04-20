#include "lexer.h"

#include "token.h"
#include <arena.h>
#include <assert.h>

/* ========== SCANNER ========== */

static int is_at_end(struct scanner *s) { return s->current >= s->source_len; }

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
	assert(advance(s) == c);
	return 1;
}

/* ========== LEXER ========== */

static void
scan_token_with_slash_prefix(struct token *token, struct scanner *s) {
	switch (peek(s)) {
	case '/':
		advance(s);
		while (!is_at_end(s) && peek(s) != '\n') {
			advance(s);
		}
		token->type = TOK_COMMENT_SINGLE;
		break;
	case '*':
		advance(s);
		while (!is_at_end(s) && !(advance(s) == '*' && advance(s) == '/')) {
		}
		token->type = TOK_COMMENT_MULTI;
		break;
	default: token->type = TOK_DIV; break;
	}
}

struct token scan_token(struct scanner *s, struct arena *arena) {
	struct token token;
	token.line      = s->line;
	token.column    = s->column;
	s->lexeme_start = s->current;

	switch (advance(s)) {
	case '/': scan_token_with_slash_prefix(&token, s); break;
	case '+': token.type = TOK_ADD; break;
	case '-': token.type = TOK_SUB; break;
	case '*': token.type = TOK_MUL; break;
	case '<': token.type = match(s, '=') ? TOK_LT_EQ : TOK_LT; break;
	case '>': token.type = match(s, '=') ? TOK_GT_EQ : TOK_GT; break;
	default: token.type = TOK_NONE;
	}

	if (token.type != TOK_NONE) {
		token.lexeme_len = s->current - s->lexeme_start;
		token.lexeme     = arena_push_array(arena, token.lexeme_len + 1, char);
		memcpy(token.lexeme, &s->source[s->lexeme_start], token.lexeme_len);
		token.lexeme[token.lexeme_len] = '\0'; /* null terminate */
	}

	return token;
}
