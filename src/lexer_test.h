#include "lexer.h"
#include "token.h"

#include <ctest.h>

struct scanner test_scanner(char *source) {
	struct scanner s;
	s.source       = source;
	s.source_len   = strlen(source);
	s.lexeme_start = 0;
	s.current      = 0;
	return s;
}

#define SCAN_TOKEN_HELPER(expected_token_type, expected_lexeme)                \
	{                                                                            \
		struct scanner s    = test_scanner(expected_lexeme "\n");                  \
		struct arena *arena = arena_alloc();                                       \
		struct token *token = scan_token(&s, arena);                               \
		EXPECT(token->type == expected_token_type);                                \
		if (expected_lexeme == NULL) {                                             \
			EXPECT(token->lexeme == NULL);                                           \
		} else {                                                                   \
			size_t expected_lexeme_len = strlen(expected_lexeme);                    \
			EXPECT(token->lexeme_len == expected_lexeme_len);                        \
			EXPECT(strcmp(token->lexeme, expected_lexeme) == 0);                     \
			EXPECT(s.current == expected_lexeme_len);                                \
		}                                                                          \
		arena_free(arena);                                                         \
		PASS();                                                                    \
	}

test scan_token_scans_eof(void) {
	struct scanner s    = test_scanner("");
	struct arena *arena = arena_alloc();
	struct token *token = scan_token(&s, arena);
	EXPECT(token->type == TOK_EOF);
	EXPECT(token->lexeme == NULL);
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

test scan_token_scans_left_parenthesis(void) {
	SCAN_TOKEN_HELPER(TOK_PAREN_L, "(");
}

test scan_token_scans_right_parenthesis(void) {
	SCAN_TOKEN_HELPER(TOK_PAREN_R, ")");
}

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

test scan_token_scans_equal_equal_symbols(void) {
	SCAN_TOKEN_HELPER(TOK_EQ_EQ, "==");
}

test scan_token_scans_not_equal_symbols(void) {
	SCAN_TOKEN_HELPER(TOK_NE, "/=");
}

test scan_token_scans_colon(void) { SCAN_TOKEN_HELPER(TOK_COLON, ":"); }

test scan_token_scans_colon_colon(void) {
	SCAN_TOKEN_HELPER(TOK_COLON_COLON, "::");
}

test scan_token_scans_arrow(void) { SCAN_TOKEN_HELPER(TOK_ARROW, "->"); }

test scan_token_scans_equal_arrow_symbols(void) {
	SCAN_TOKEN_HELPER(TOK_EQ_ARROW, "=>");
}

test scan_token_scans_keyword_if(void) { SCAN_TOKEN_HELPER(TOK_IF, "if"); }

test scan_token_scans_keyword_class(void) {
	SCAN_TOKEN_HELPER(TOK_CLASS, "class");
}

test scan_token_scans_keyword_data(void) {
	SCAN_TOKEN_HELPER(TOK_DATA, "data");
}

test scan_token_scans_keyword_instance(void) {
	SCAN_TOKEN_HELPER(TOK_INSTANCE, "instance");
}

test scan_token_scans_keyword_let(void) { SCAN_TOKEN_HELPER(TOK_LET, "let"); }

test scan_token_scans_keyword_in(void) { SCAN_TOKEN_HELPER(TOK_IN, "in"); }

test scan_token_scans_keyword_where(void) {
	SCAN_TOKEN_HELPER(TOK_WHERE, "where");
}

test scan_token_scans_semicolons(void) {
	SCAN_TOKEN_HELPER(TOK_SEMICOLON, ";");
}

test scan_token_scans_left_curly_bracket(void) {
	SCAN_TOKEN_HELPER(TOK_CURLY_L, "{");
}

test scan_token_scans_right_curly_bracket(void) {
	SCAN_TOKEN_HELPER(TOK_CURLY_R, "}");
}

test scan_tokens_scans_a_sequence_of_tokens(void) {
	struct token **tokens;
	char *source        = "let x = 300 in\ny*x ==600";
	struct arena *arena = arena_alloc();
	tokens              = scan_tokens(source, arena);

	EXPECT(tokens[0]->type == TOK_LET);
	EXPECT(tokens[0]->lexeme_len == 3);
	EXPECT(tokens[0]->lexeme_index == 0);
	EXPECT(strcmp(tokens[0]->lexeme, "let") == 0);

	EXPECT(tokens[1]->type == TOK_IDENTIFIER);
	EXPECT(tokens[1]->lexeme_len == 1);
	EXPECT(tokens[1]->lexeme_index == 4);
	EXPECT(strcmp(tokens[1]->lexeme, "x") == 0);

	EXPECT(tokens[2]->type == TOK_EQ);
	EXPECT(tokens[2]->lexeme_len == 1);
	EXPECT(tokens[2]->lexeme_index == 6);
	EXPECT(strcmp(tokens[2]->lexeme, "=") == 0);

	EXPECT(tokens[3]->type == TOK_INT);
	EXPECT(tokens[3]->lexeme_len == 3);
	EXPECT(tokens[3]->lexeme_index == 8);
	EXPECT(strcmp(tokens[3]->lexeme, "300") == 0);

	EXPECT(tokens[4]->type == TOK_IN);
	EXPECT(tokens[4]->lexeme_len == 2);
	EXPECT(tokens[4]->lexeme_index == 12);
	EXPECT(strcmp(tokens[4]->lexeme, "in") == 0);

	EXPECT(tokens[5]->type == TOK_IDENTIFIER);
	EXPECT(tokens[5]->lexeme_len == 1);
	EXPECT(tokens[5]->lexeme_index == 15);
	EXPECT(strcmp(tokens[5]->lexeme, "y") == 0);

	EXPECT(tokens[6]->type == TOK_MUL);
	EXPECT(tokens[6]->lexeme_len == 1);
	EXPECT(tokens[6]->lexeme_index == 16);
	EXPECT(strcmp(tokens[6]->lexeme, "*") == 0);

	EXPECT(tokens[7]->type == TOK_IDENTIFIER);
	EXPECT(tokens[7]->lexeme_len == 1);
	EXPECT(tokens[7]->lexeme_index == 17);
	EXPECT(strcmp(tokens[7]->lexeme, "x") == 0);

	EXPECT(tokens[8]->type == TOK_EQ_EQ);
	EXPECT(tokens[8]->lexeme_len == 2);
	EXPECT(tokens[8]->lexeme_index == 19);
	EXPECT(strcmp(tokens[8]->lexeme, "==") == 0);

	EXPECT(tokens[9]->type == TOK_INT);
	EXPECT(tokens[9]->lexeme_len == 3);
	EXPECT(tokens[9]->lexeme_index == 21);
	EXPECT(strcmp(tokens[9]->lexeme, "600") == 0);

	EXPECT(tokens[10]->type == TOK_EOF);
	EXPECT(tokens[10]->lexeme_index == 24);

	arena_free(arena);

	PASS();
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
	TEST(scan_token_scans_left_parenthesis);
	TEST(scan_token_scans_right_parenthesis);
	TEST(scan_token_scans_add_symbols);
	TEST(scan_token_scans_subtract_symbols);
	TEST(scan_token_scans_multiply_symbols);
	TEST(scan_token_scans_divide_symbols);
	TEST(scan_token_scans_less_than_symbols);
	TEST(scan_token_scans_greater_than_symbols);
	TEST(scan_token_scans_less_than_or_equal_symbols);
	TEST(scan_token_scans_greater_than_or_equal_symbols);
	TEST(scan_token_scans_equal_equal_symbols);
	TEST(scan_token_scans_not_equal_symbols);
	TEST(scan_token_scans_colon);
	TEST(scan_token_scans_colon_colon);
	TEST(scan_token_scans_arrow);
	TEST(scan_token_scans_equal_arrow_symbols);
	TEST(scan_token_scans_keyword_if);
	TEST(scan_token_scans_keyword_class);
	TEST(scan_token_scans_keyword_data);
	TEST(scan_token_scans_keyword_instance);
	TEST(scan_token_scans_keyword_let);
	TEST(scan_token_scans_keyword_in);
	TEST(scan_token_scans_keyword_where);
	TEST(scan_token_scans_semicolons);
	TEST(scan_token_scans_left_curly_bracket);
	TEST(scan_token_scans_right_curly_bracket);
	TEST(scan_tokens_scans_a_sequence_of_tokens);
}
