#ifndef RACC_TOKEN_H
#define RACC_TOKEN_H

#include <stddef.h>

enum token_type {
	TOK_NONE,
	TOK_EOF,
	TOK_COMMENT_SINGLE,
	TOK_COMMENT_MULTI,
	TOK_IDENTIFIER,
	TOK_PAREN_L,
	TOK_PAREN_R,
	TOK_INT,
	TOK_DOUBLE,
	TOK_STRING,
	TOK_CHAR,
	TOK_BOOL,
	TOK_EQ,
	TOK_COLON,
	TOK_ADD,
	TOK_SUB,
	TOK_MUL,
	TOK_DIV,
	TOK_LT,
	TOK_GT,
	TOK_LT_EQ,
	TOK_GT_EQ,
	TOK_EQ_EQ,
	TOK_NE,
	TOK_COLON_COLON,
	TOK_ARROW,
	TOK_PIPE,
	TOK_LAMBDA,
	TOK_IF,
	TOK_DATA,
	TOK_LET,
	TOK_IN,
	TOK_WHERE,
	TOK_SEMICOLON,
	TOK_CURLY_L,
	TOK_CURLY_R

};

struct token {
	enum token_type type;
	char *lexeme;
	size_t lexeme_len;
	size_t lexeme_index; /* in source */
};

#endif
