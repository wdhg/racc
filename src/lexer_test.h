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

test scan_token_scans_single_line_comments(void) {
	struct scanner s    = test_scanner("// this is a comment\n");
	struct arena *arena = arena_alloc();
	struct token token  = scan_token(&s, arena);
	EXPECT(token.type == TOK_COMMENT_SINGLE);
	EXPECT(token.lexeme_len == 20);
	EXPECT(strcmp(token.lexeme, "// this is a comment") == 0);
	EXPECT(s.current == 20);
	PASS();
}

test scan_token_scans_multi_line_comments(void) {
	struct scanner s    = test_scanner("/* this is a comment */\n");
	struct arena *arena = arena_alloc();
	struct token token  = scan_token(&s, arena);
	EXPECT(token.type == TOK_COMMENT_MULTI);
	EXPECT(token.lexeme_len == 23);
	EXPECT(strcmp(token.lexeme, "/* this is a comment */") == 0);
	EXPECT(s.current == 23);
	PASS();
}

test scan_token_scans_add_symbols(void) {
	struct scanner s    = test_scanner("+\n");
	struct arena *arena = arena_alloc();
	struct token token  = scan_token(&s, arena);
	EXPECT(token.type == TOK_ADD);
	EXPECT(token.lexeme_len == 1);
	EXPECT(strcmp(token.lexeme, "+") == 0);
	EXPECT(s.current == 1);
	PASS();
}

test scan_token_scans_subtract_symbols(void) {
	struct scanner s    = test_scanner("-\n");
	struct arena *arena = arena_alloc();
	struct token token  = scan_token(&s, arena);
	EXPECT(token.type == TOK_SUB);
	EXPECT(token.lexeme_len == 1);
	EXPECT(strcmp(token.lexeme, "-") == 0);
	EXPECT(s.current == 1);
	PASS();
}

test scan_token_scans_multiply_symbols(void) {
	struct scanner s    = test_scanner("*\n");
	struct arena *arena = arena_alloc();
	struct token token  = scan_token(&s, arena);
	EXPECT(token.type == TOK_MUL);
	EXPECT(token.lexeme_len == 1);
	EXPECT(strcmp(token.lexeme, "*") == 0);
	EXPECT(s.current == 1);
	PASS();
}

test scan_token_scans_divide_symbols(void) {
	struct scanner s    = test_scanner("/\n");
	struct arena *arena = arena_alloc();
	struct token token  = scan_token(&s, arena);
	EXPECT(token.type == TOK_DIV);
	EXPECT(token.lexeme_len == 1);
	EXPECT(strcmp(token.lexeme, "/") == 0);
	EXPECT(s.current == 1);
	PASS();
}

test scan_token_scans_less_than_symbols(void) {
	struct scanner s    = test_scanner("<\n");
	struct arena *arena = arena_alloc();
	struct token token  = scan_token(&s, arena);
	EXPECT(token.type == TOK_LT);
	EXPECT(token.lexeme_len == 1);
	EXPECT(strcmp(token.lexeme, "<") == 0);
	EXPECT(s.current == 1);
	PASS();
}

test scan_token_scans_greater_than_symbols(void) {
	struct scanner s    = test_scanner(">\n");
	struct arena *arena = arena_alloc();
	struct token token  = scan_token(&s, arena);
	EXPECT(token.type == TOK_GT);
	EXPECT(token.lexeme_len == 1);
	EXPECT(strcmp(token.lexeme, ">") == 0);
	EXPECT(s.current == 1);
	PASS();
}

test scan_token_scans_less_than_or_equal_symbols(void) {
	struct scanner s    = test_scanner("<=\n");
	struct arena *arena = arena_alloc();
	struct token token  = scan_token(&s, arena);
	EXPECT(token.type == TOK_LT_EQ);
	EXPECT(token.lexeme_len == 2);
	EXPECT(strcmp(token.lexeme, "<=") == 0);
	EXPECT(s.current == 2);
	PASS();
}

test scan_token_scans_greater_than_or_equal_symbols(void) {
	struct scanner s    = test_scanner(">=\n");
	struct arena *arena = arena_alloc();
	struct token token  = scan_token(&s, arena);
	EXPECT(token.type == TOK_GT_EQ);
	EXPECT(token.lexeme_len == 2);
	EXPECT(strcmp(token.lexeme, ">=") == 0);
	EXPECT(s.current == 2);
	PASS();
}

void test_lexer_h(void) {
	TEST(scan_token_scans_single_line_comments);
	TEST(scan_token_scans_multi_line_comments);
	TEST(scan_token_scans_add_symbols);
	TEST(scan_token_scans_subtract_symbols);
	TEST(scan_token_scans_multiply_symbols);
	TEST(scan_token_scans_divide_symbols);
	TEST(scan_token_scans_less_than_symbols);
	TEST(scan_token_scans_greater_than_symbols);
	TEST(scan_token_scans_less_than_or_equal_symbols);
	TEST(scan_token_scans_greater_than_or_equal_symbols);
}
