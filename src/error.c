#include "error.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int count_digits(int num) {
	int count = 0;
	do {
		count++;
		num /= 10;
	} while (num != 0);
	return count;
}

static void report_error_code(char *source, size_t index) {
	size_t char_index = 0;
	size_t line_index = 0;
	int line          = 0;
	int column        = 0;
	int line_length   = 0;
	int line_number_char_width;
	size_t source_len;

	source_len = strlen(source);
	assert(index <= source_len); /* index may include EOF past last index */

	/* find the line and column */
	for (char_index = 0; char_index < index; char_index++) {
		assert(char_index < source_len);
		if (source[char_index] == '\n') {
			line++;
			column     = 0;
			line_index = char_index + 1;
		} else {
			column++;
		}
	}

	line_number_char_width = count_digits(line);

	/* calculate the line length */
	while (char_index < source_len && source[char_index] != '\n') {
		char_index++;
	}
	line_length = char_index - line_index;

	printf("\t%*s |\n", line_number_char_width, "");
	printf("\t%d | %.*s\n", line + 1, line_length, &source[line_index]);
	printf("\t%*s | %*s^\n", line_number_char_width, "", column, "");
	printf("\n");
}

void report_error(struct error_log *log, char *error_msg) {
	assert(log != NULL);
	assert(error_msg != NULL);
	log->had_error = 1;
	if (log->suppress_error_messages) {
		return;
	}
	printf("\nERROR: %s\n\n", error_msg);
}

void report_error_at(struct error_log *log, char *error_msg, size_t index) {
	report_error(log, error_msg);
	if (log->suppress_error_messages) {
		return;
	}
	report_error_code(log->source, index);
}

void report_type_error(struct error_log *log,
                       struct type *t1,
                       struct type *t2,
                       size_t index) {
	report_error(log, "Type error");
	if (log->suppress_error_messages) {
		return;
	}

	printf("\t Cannot match type '");
	print_type(t1);
	printf("' with type '");
	print_type(t2);
	printf("'\n\n");
	report_error_code(log->source, index);
}
