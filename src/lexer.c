#include "lexer.h"

#include "list.h"
#include "token.h"
#include <arena.h>
#include <assert.h>
#include <string.h>

/* ========== SCANNER ========== */

static struct scanner new_scanner(char *source) {
	struct scanner s;
	s.source       = source;
	s.source_len   = strlen(source);
	s.current      = 0;
	s.lexeme_start = 0;
	return s;
}

static int is_at_end(struct scanner *s) { return s->current >= s->source_len; }

static char get_ch(struct scanner *s, size_t i) {
	if (i >= s->source_len) {
		return '\0';
	}
	return s->source[i];
}

static char advance(struct scanner *s) {
	char c = get_ch(s, s->current);
	s->current++;
	return c;
}

static char peek(struct scanner *s) { return get_ch(s, s->current); }

static int match(struct scanner *s, char c) {
	if (s->current >= s->source_len || peek(s) != c) {
		return 0;
	}
	assert(advance(s) == c);
	return 1;
}

/* ========== LEXER ========== */

static void scan_token_with_slash_prefix(struct token *token,
                                         struct scanner *s) {
	switch (peek(s)) {
	case '/':
		advance(s);
		while (!is_at_end(s) && peek(s) != '\n') {
			advance(s);
			if (is_at_end(s)) {
				/* TODO error */
			}
		}
		token->type = TOK_COMMENT_SINGLE;
		break;
	case '*':
		advance(s);
		while (!is_at_end(s) && !(advance(s) == '*' && advance(s) == '/')) {
			if (is_at_end(s)) {
				/* TODO error */
			}
		}
		token->type = TOK_COMMENT_MULTI;
		break;
	case '=':
		advance(s);
		token->type = TOK_NE;
		break;
	default: token->type = TOK_DIV; break;
	}
}

static void scan_token_with_equals_prefix(struct token *token,
                                          struct scanner *s) {
	switch (peek(s)) {
	case '=':
		advance(s);
		token->type = TOK_EQ_EQ;
		break;
	case '>':
		advance(s);
		token->type = TOK_EQ_ARROW;
		break;
	default: token->type = TOK_EQ; break;
	}
}

static void scan_token_string(struct token *token, struct scanner *s) {
	while (!is_at_end(s) && advance(s) != '"') {
		if (is_at_end(s)) {
			/* TODO error */
		}
	}
	token->type = TOK_STRING;
}

static void scan_token_char(struct token *token, struct scanner *s) {
	if (peek(s) == '\\') {
		/* escaped character */
		advance(s);
	}
	advance(s);
	if (!match(s, '\'')) {
		/* TODO error */
	}
	token->type = TOK_CHAR;
}

static void scan_token_number(struct token *token, struct scanner *s) {
	int is_double = 0;
	while (isnumber(peek(s))) {
		advance(s);
	}
	is_double = match(s, '.');
	if (is_double) {
		if (!isnumber(peek(s))) {
			/* TODO error */
		}
		while (isnumber(peek(s))) {
			advance(s);
		}
	}
	token->type = is_double ? TOK_DOUBLE : TOK_INT;
}

static void scan_token_identifier(struct token *token, struct scanner *s) {
	while (isalnum(peek(s)) || peek(s) == '_') {
		advance(s);
	}
	token->type = TOK_IDENTIFIER;
}

struct token *scan_token(struct scanner *s, struct arena *arena) {
	char c;
	struct token *token = arena_push_struct(arena, struct token);
	token->lexeme       = NULL;

	do {
		s->lexeme_start     = s->current;
		token->lexeme_index = s->current;
		c                   = advance(s);
	} while (isspace(c));

	switch (c) {
	case '\0': token->type = TOK_EOF; break;
	case '/': scan_token_with_slash_prefix(token, s); break;
	case '"': scan_token_string(token, s); break;
	case '\'': scan_token_char(token, s); break;
	case '(': token->type = TOK_PAREN_L; break;
	case ')': token->type = TOK_PAREN_R; break;
	case '+': token->type = TOK_ADD; break;
	case '-': token->type = match(s, '>') ? TOK_ARROW : TOK_SUB; break;
	case '*': token->type = TOK_MUL; break;
	case '<': token->type = match(s, '=') ? TOK_LT_EQ : TOK_LT; break;
	case '>': token->type = match(s, '=') ? TOK_GT_EQ : TOK_GT; break;
	case '=': scan_token_with_equals_prefix(token, s); break;
	case ':': token->type = match(s, ':') ? TOK_COLON_COLON : TOK_COLON; break;
	case '|': token->type = TOK_PIPE; break;
	case '\\': token->type = TOK_LAMBDA; break;
	case ';': token->type = TOK_SEMICOLON; break;
	case '{': token->type = TOK_CURLY_L; break;
	case '}': token->type = TOK_CURLY_R; break;
	case '[': token->type = TOK_SQUARE_L; break;
	case ']': token->type = TOK_SQUARE_R; break;
	case ',': token->type = TOK_COMMA; break;
	case '@': token->type = TOK_AT; break;
	default:
		if (isnumber(c)) {
			scan_token_number(token, s);
		} else if (isalpha(c) || c == '_') {
			scan_token_identifier(token, s);
		} else {
			token->type = TOK_NONE;
		}
	}

	if (token->type != TOK_NONE && token->type != TOK_EOF) {
		token->lexeme_len = s->current - s->lexeme_start;
		token->lexeme     = arena_push_array(arena, token->lexeme_len + 1, char);
		memcpy(token->lexeme, &s->source[s->lexeme_start], token->lexeme_len);
		token->lexeme[token->lexeme_len] = '\0'; /* null terminate */
	}

	if (token->type == TOK_IDENTIFIER) {
		if (strcmp(token->lexeme, "if") == 0) {
			token->type = TOK_IF;
		} else if (strcmp(token->lexeme, "class") == 0) {
			token->type = TOK_CLASS;
		} else if (strcmp(token->lexeme, "data") == 0) {
			token->type = TOK_DATA;
		} else if (strcmp(token->lexeme, "instance") == 0) {
			token->type = TOK_INSTANCE;
		} else if (strcmp(token->lexeme, "let") == 0) {
			token->type = TOK_LET;
		} else if (strcmp(token->lexeme, "in") == 0) {
			token->type = TOK_IN;
		} else if (strcmp(token->lexeme, "where") == 0) {
			token->type = TOK_WHERE;
		} else if (strcmp(token->lexeme, "True") == 0) {
			token->type = TOK_BOOL;
		} else if (strcmp(token->lexeme, "False") == 0) {
			token->type = TOK_BOOL;
		}
	}

	return token;
}

struct token **scan_tokens(char *source, struct arena *arena) {
	struct token **tokens;
	struct scanner s       = new_scanner(source);
	struct list token_list = list_new(arena_alloc());

	while (1) {
		struct token *token = scan_token(&s, arena);
		if (token->type != TOK_NONE) {
			list_append(&token_list, token);
		}
		if (token->type == TOK_EOF) {
			break;
		}
	}

	tokens = (struct token **)list_to_array(&token_list, arena);
	arena_free(token_list.arena);

	return tokens;
}
