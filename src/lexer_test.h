#include "lexer.h"
#include "token.h"

#include <ctest.h>

struct scanner test_scanner(char *source) {
	struct scanner s;
	s.source       = source;
	s.source_len   = strlen(source);
	s.lexeme_start = 0;
	s.current      = 0;
	s.line         = 0;
	s.column       = 0;
	return s;
}

#define SCAN_TOKEN_HELPER(expected_token_type, expected_lexeme)                \
	{                                                                            \
		struct scanner s    = test_scanner(expected_lexeme "\n");                  \
		struct arena *arena = arena_alloc();                                       \
		struct token token  = scan_token(&s, arena);                               \
		EXPECT(token.type == expected_token_type);                                 \
		if (expected_lexeme == NULL) {                                             \
			EXPECT(token.lexeme == NULL);                                            \
		} else {                                                                   \
			size_t expected_lexeme_len = strlen(expected_lexeme);                    \
			EXPECT(token.lexeme_len == expected_lexeme_len);                         \
			EXPECT(strcmp(token.lexeme, expected_lexeme) == 0);                      \
			EXPECT(s.current == expected_lexeme_len);                                \
		}                                                                          \
		arena_free(arena);                                                         \
		PASS();                                                                    \
	}

test scan_token_scans_eof(void) {
	struct scanner s    = test_scanner("");
	struct arena *arena = arena_alloc();
	struct token token  = scan_token(&s, arena);
	EXPECT(token.type == TOK_EOF);
	EXPECT(token.lexeme == NULL);
	arena_free(arena);
	PASS();
}

test scan_token_scans_single_line_comments(void) {
	SCAN_TOKEN_HELPER(TOK_COMMENT_SINGLE, "// this is a comment");
}

test scan_token_scans_multi_line_comments(void) {
	SCAN_TOKEN_HELPER(TOK_COMMENT_MULTI, "/* this is a comment */");
}

test scan_token_scans_identifiers(void) {
	SCAN_TOKEN_HELPER(TOK_IDENTIFIER, "myVariable");
}

test scan_token_scans_ints(void) { SCAN_TOKEN_HELPER(TOK_INT, "12345"); }

test scan_token_scans_double(void) {
	SCAN_TOKEN_HELPER(TOK_DOUBLE, "12345.6789");
}

test scan_token_scans_strings(void) {
	SCAN_TOKEN_HELPER(TOK_STRING, "\"my\nstring \"");
}

test scan_token_scans_chars(void) { SCAN_TOKEN_HELPER(TOK_CHAR, "\'a\'"); }

test scan_token_scans_escaped_chars(void) {
	SCAN_TOKEN_HELPER(TOK_CHAR, "\'\\n\'");
}

test scan_token_scans_true(void) { SCAN_TOKEN_HELPER(TOK_BOOL, "true"); }

test scan_token_scans_false(void) { SCAN_TOKEN_HELPER(TOK_BOOL, "false"); }

test scan_token_scans_add_symbols(void) { SCAN_TOKEN_HELPER(TOK_ADD, "+"); }

test scan_token_scans_subtract_symbols(void) {
	SCAN_TOKEN_HELPER(TOK_SUB, "-");
}

test scan_token_scans_multiply_symbols(void) {
	SCAN_TOKEN_HELPER(TOK_MUL, "*");
}

test scan_token_scans_divide_symbols(void) { SCAN_TOKEN_HELPER(TOK_DIV, "/"); }

test scan_token_scans_less_than_symbols(void) {
	SCAN_TOKEN_HELPER(TOK_LT, "<");
}

test scan_token_scans_greater_than_symbols(void) {
	SCAN_TOKEN_HELPER(TOK_GT, ">");
}

test scan_token_scans_less_than_or_equal_symbols(void) {
	SCAN_TOKEN_HELPER(TOK_LT_EQ, "<=");
}

test scan_token_scans_greater_than_or_equal_symbols(void) {
	SCAN_TOKEN_HELPER(TOK_GT_EQ, ">=");
}

test scan_token_scans_colon(void) { SCAN_TOKEN_HELPER(TOK_COLON, ":"); }

test scan_token_scans_colon_colon(void) {
	SCAN_TOKEN_HELPER(TOK_COLON_COLON, "::");
}

test scan_token_scans_arrow(void) { SCAN_TOKEN_HELPER(TOK_ARROW, "->"); }

test scan_token_scans_keyword_if(void) { SCAN_TOKEN_HELPER(TOK_IF, "if"); }

test scan_token_scans_keyword_data(void) {
	SCAN_TOKEN_HELPER(TOK_DATA, "data");
}

test scan_token_scans_keyword_let(void) { SCAN_TOKEN_HELPER(TOK_LET, "let"); }

test scan_token_scans_keyword_in(void) { SCAN_TOKEN_HELPER(TOK_IN, "in"); }

test scan_token_scans_keyword_where(void) {
	SCAN_TOKEN_HELPER(TOK_WHERE, "where");
}

void test_lexer_h(void) {
	TEST(scan_token_scans_eof);
	TEST(scan_token_scans_single_line_comments);
	TEST(scan_token_scans_multi_line_comments);
	TEST(scan_token_scans_identifiers);
	TEST(scan_token_scans_ints);
	TEST(scan_token_scans_double);
	TEST(scan_token_scans_strings);
	TEST(scan_token_scans_chars);
	TEST(scan_token_scans_escaped_chars);
	TEST(scan_token_scans_true);
	TEST(scan_token_scans_false);
	TEST(scan_token_scans_add_symbols);
	TEST(scan_token_scans_subtract_symbols);
	TEST(scan_token_scans_multiply_symbols);
	TEST(scan_token_scans_divide_symbols);
	TEST(scan_token_scans_less_than_symbols);
	TEST(scan_token_scans_greater_than_symbols);
	TEST(scan_token_scans_less_than_or_equal_symbols);
	TEST(scan_token_scans_greater_than_or_equal_symbols);
	TEST(scan_token_scans_colon);
	TEST(scan_token_scans_colon_colon);
	TEST(scan_token_scans_arrow);
	TEST(scan_token_scans_keyword_if);
	TEST(scan_token_scans_keyword_data);
	TEST(scan_token_scans_keyword_let);
	TEST(scan_token_scans_keyword_in);
	TEST(scan_token_scans_keyword_where);
}
