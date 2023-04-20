#ifndef RACC_TOKEN_H
#define RACC_TOKEN_H

#include <stddef.h>

enum token_type {
	/* misc */
	TOK_NONE,
	TOK_EOF,
	TOK_SPACE,
	TOK_COMMENT_SINGLE,
	TOK_COMMENT_MULTI,
	TOK_IDENTIFIER,
	/* literals */
	TOK_INT,
	TOK_DOUBLE,
	TOK_STRING,
	TOK_CHAR,
	TOK_BOOL,
	/* assignment */
	TOK_EQ,
	TOK_COLON,
	/* arithmetic */
	TOK_ADD,
	TOK_SUB,
	TOK_MUL,
	TOK_DIV,
	/* comparison */
	TOK_LT,
	TOK_GT,
	TOK_LT_EQ,
	TOK_GT_EQ,
	TOK_EQ_EQ,
	TOK_NE,
	/* type */
	TOK_COLON_COLON,
	TOK_ARROW,
	/* function */
	TOK_PIPE,
	TOK_LAMBDA,
	/* keywords */
	TOK_IF,
	TOK_DATA,
	TOK_LET,
	TOK_IN,
	TOK_WHERE
};

struct token {
	enum token_type type;
	char *lexeme;
	size_t lexeme_len;
	size_t line;
	size_t column;
};

#endif
