#ifndef RACC_LEXER_TEST_H
#define RACC_LEXER_TEST_H

#include "lexer.h"

#include <ctest.h>
#include <string.h>

struct scanner test_scanner(void) {
	struct scanner s;
	s.source = "hello\nworld!";
	s.source_len = strlen(s.source);
	s.lexeme_start = 0;
	s.current = 0;
	s.line = 0;
	s.column = 0;
	return s;
}

test get_ch_returns_correct_character(void) {
	struct scanner s = test_scanner();
	EXPECT(get_ch(&s, 0) == 'h');
	EXPECT(get_ch(&s, 1) == 'e');
	EXPECT(get_ch(&s, 4) == 'o');
	PASS();
}

test get_ch_returns_null_byte_when_out_of_bounds(void) {
	struct scanner s = test_scanner();
	EXPECT(get_ch(&s, 20) == '\0');
	PASS();
}

test advance_increments_current(void) {
	struct scanner s = test_scanner();
	advance(&s);
	EXPECT(s.current == 1);
	PASS();
}

test advance_increments_line_and_resets_column_on_newline(void) {
	struct scanner s = test_scanner();
	char c;
	c = advance(&s);
	EXPECT(c != '\n');
	EXPECT(s.line == 0);
	EXPECT(s.column == 1);
	c = advance(&s);
	EXPECT(c != '\n');
	EXPECT(s.line == 0);
	EXPECT(s.column == 2);
	s.current = 5;
	c = advance(&s);
	EXPECT(c == '\n');
	EXPECT(s.line == 1);
	EXPECT(s.column == 0);
	PASS();
}

test peek_returns_correct_character(void) {
	struct scanner s = test_scanner();
	EXPECT(peek(&s) == 'h');
	advance(&s);
	EXPECT(peek(&s) == 'e');
	advance(&s);
	EXPECT(peek(&s) == 'l');
	advance(&s);
	EXPECT(peek(&s) == 'l');
	s.current = 11;
	EXPECT(peek(&s) == '!');
	s.current = 12;
	EXPECT(peek(&s) == '\0');
	PASS();
}

test peek_next_returns_correct_character(void) {
	struct scanner s = test_scanner();
	EXPECT(peek_next(&s) == 'e');
	advance(&s);
	EXPECT(peek_next(&s) == 'l');
	advance(&s);
	EXPECT(peek_next(&s) == 'l');
	advance(&s);
	EXPECT(peek_next(&s) == 'o');
	s.current = 10;
	EXPECT(peek_next(&s) == '!');
	s.current = 11;
	EXPECT(peek_next(&s) == '\0');
	PASS();
}

test match_only_matches_the_correct_character(void) {
	struct scanner s = test_scanner();
	EXPECT(!match(&s, 'e'));
	EXPECT(!match(&s, ' '));
	EXPECT(!match(&s, '!'));
	EXPECT(!match(&s, '\0'));
	EXPECT(match(&s, 'h'));
	PASS();
}

test match_advances_when_character_matches(void) {
	struct scanner s = test_scanner();
	EXPECT(s.current == 0);
	EXPECT(match(&s, 'h'));
	EXPECT(s.current == 1);
	EXPECT(match(&s, 'e'));
	EXPECT(s.current == 2);
	PASS();
}

test match_does_not_match_out_of_bounds(void) {
	struct scanner s = test_scanner();
	s.current = 300;
	EXPECT(!match(&s, '!'));
	EXPECT(!match(&s, ' '));
	EXPECT(!match(&s, '\0'));
	PASS();
}

void test_lexer_h(void) {
	TEST(get_ch_returns_correct_character);
	TEST(get_ch_returns_null_byte_when_out_of_bounds);
	TEST(advance_increments_current);
	TEST(advance_increments_line_and_resets_column_on_newline);
	TEST(peek_returns_correct_character);
	TEST(peek_next_returns_correct_character);
	TEST(match_only_matches_the_correct_character);
	TEST(match_advances_when_character_matches);
	TEST(match_does_not_match_out_of_bounds);
}

#endif
