#ifndef RACC_TEST

#include <stdio.h>

int main(void) {
	printf("Hello world!\n");
	return 0;
}

#else /* ========== TESTS ========== */

#include <ctest.h>

#include "lexer_test.h"

int main(void) {
	TESTS(test_lexer_h);
	return ct_summarize_tests();
}

#endif
