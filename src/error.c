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

void report_error(struct error_log *log, char *error_msg) {
	log->had_error = 1;
	printf("ERROR: %s\n", error_msg);
}

void report_error_at(struct error_log *log, char *error_msg, size_t index) {
	size_t char_index = 0;
	size_t line_index = 0;
	int line          = 0;
	int column        = 0;
	int line_length   = 0;
	int line_number_char_width;
	size_t source_len;

	assert(log != NULL);
	assert(error_msg != NULL);
	source_len = strlen(log->source);
	assert(index <= source_len); /* index may include EOF past last index */

	log->had_error = 1;

	if (log->suppress_error_messages) {
		return;
	}

	/* find the line and column */
	for (char_index = 0; char_index < index; char_index++) {
		assert(char_index < source_len);
		if (log->source[char_index] == '\n') {
			line++;
			column     = 0;
			line_index = char_index + 1;
		} else {
			column++;
		}
	}

	line_number_char_width = count_digits(line);

	/* calculate the line length */
	while (char_index < source_len && log->source[char_index] != '\n') {
		char_index++;
	}
	line_length = char_index - line_index;

	printf("\nERROR: %s\n", error_msg);
	printf("\n");
	printf("\t%*s |\n", line_number_char_width, "");
	printf("\t%d | %.*s\n", line + 1, line_length, &log->source[line_index]);
	printf("\t%*s | %*s^\n", line_number_char_width, "", column, "");
	printf("\n");
}
