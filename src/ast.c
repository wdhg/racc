#include "ast.h"
#include "list.h"
#include "string.h"
#include <stdio.h>

void print_type(struct type *type) {
	if (type->type_constraints != NULL) {
		struct list_iter iter = list_iterate(type->type_constraints);
		printf("<");
		while (!list_iter_at_end(&iter)) {
			struct type *type_constraint = list_iter_next(&iter);
			print_type(type_constraint);
			if (!list_iter_next(&iter)) {
				printf(", ");
			}
		}
		printf("> => ");
	}

	if (strcmp(type->name, "->") == 0) {
		struct type *lhs = list_head(type->type_args);
		struct type *rhs = list_last(type->type_args);
		printf("(");
		print_type(lhs);
		printf(" -> ");
		print_type(rhs);
		printf(")");
	} else if (strcmp(type->name, "[]") == 0) {
		struct type *type_arg = list_head(type->type_args);
		printf("[");
		print_type(type_arg);
		printf("]");
	} else if (strcmp(type->name, "(,)") == 0) {
		struct type *lhs = list_head(type->type_args);
		struct type *rhs = list_last(type->type_args);
		printf("(");
		print_type(lhs);
		printf(",");
		print_type(rhs);
		printf(")");
	} else {
		printf("%s", type->name);
		if (type->type_args != NULL) {
			struct list_iter iter = list_iterate(type->type_args);
			while (!list_iter_at_end(&iter)) {
				struct type *type = list_iter_next(&iter);
				printf(" ");
				print_type(type);
			}
		}
	}
}
