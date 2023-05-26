#include "lexer_test.h"
#include "parser_test.h"
#include "type_check_test.h"

int main(void) {
	TESTS(test_lexer_h);
	TESTS(test_parser_h);
	TESTS(test_type_check_h);
	return tests_summarize();
}
