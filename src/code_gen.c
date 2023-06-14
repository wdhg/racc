#include "code_gen.h"
#include "ast.h"
#include "list.h"
#include "map.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

struct code_generator {
	struct arena *arena;
	struct error_log *log;

	FILE *fptr;
	struct map *global_functions; /* char* -> list of struct def_value* */
	struct map *global_values;    /* char* -> struct def_value* */
};

static void
code_gen_dec_constructor_struct(struct code_generator *cg,
                                struct dec_constructor *dec_constructor) {
	size_t param_index;

	if (list_length(dec_constructor->type_params) == 0) {
		return;
	}

	param_index = 0;
	fprintf(cg->fptr, "\t\tstruct {\n");
	list_for_each(
		dec_constructor->type_params, struct type *, (void)_value;
		fprintf(cg->fptr, "\t\t\tstruct thunk *param_%ld;\n", param_index++););
	fprintf(cg->fptr, "\t\t} %s;\n", dec_constructor->name);
}

static void
code_gen_dec_constructor_func(struct code_generator *cg,
                              char *data_name,
                              struct dec_constructor *dec_constructor) {
	size_t arity = list_length(dec_constructor->type_params);

	if (arity == 0) {
		fprintf(cg->fptr,
		        "struct data_%s _data_%s_%s = {\n",
		        data_name,
		        data_name,
		        dec_constructor->name);
		fprintf(
			cg->fptr, "\t.type = DATA_%s_%s,\n", data_name, dec_constructor->name);
		fprintf(cg->fptr, "};\n");

		fprintf(cg->fptr, "struct thunk _val_%s = {\n", dec_constructor->name);
		fprintf(cg->fptr, "\t.evaluated = 1,\n");
		fprintf(cg->fptr, "\t.closure   = NULL,\n");
		fprintf(cg->fptr,
		        "\t.value     = &_data_%s_%s,\n",
		        data_name,
		        dec_constructor->name);
		fprintf(cg->fptr, "};\n");

		fprintf(cg->fptr,
		        "struct thunk *val_%s = &_val_%s;\n",
		        dec_constructor->name,
		        dec_constructor->name);
	} else {
		size_t i;
		fprintf(
			cg->fptr,
			"void *fn_%s(struct thunk **args, struct region_tree *region_tree) {\n",
			dec_constructor->name);
		fprintf(cg->fptr, "\tstruct data_%s *value;\n", data_name);

		fprintf(cg->fptr, "\tif (region == NULL) {\n");
		fprintf(
			cg->fptr, "\tvalue = calloc(1, sizeof(struct data_%s));\n", data_name);
		fprintf(cg->fptr, "\t} else {\n");
		fprintf(
			cg->fptr,
			"\t\tvalue = arena_push_struct_zero(region->v.arena, struct data_%s);\n",
			data_name);
		fprintf(cg->fptr, "\t}\n");

		fprintf(cg->fptr,
		        "\tvalue->type = DATA_%s_%s;\n",
		        data_name,
		        dec_constructor->name);
		for (i = 0; i < arity; i++) {
			fprintf(cg->fptr,
			        "\tvalue->v.%s.param_%ld = args[%ld];\n",
			        dec_constructor->name,
			        i,
			        i);
		}

		fprintf(cg->fptr, "\treturn value;\n");
		fprintf(cg->fptr, "}\n");

		fprintf(
			cg->fptr, "struct closure _closure_%s = {\n", dec_constructor->name);
		fprintf(cg->fptr, "\t.fn_arity = %ld,\n", arity);
		fprintf(cg->fptr, "\t.args_len = 0,\n");
		fprintf(cg->fptr, "\t.fn       = fn_%s,\n", dec_constructor->name);
		fprintf(cg->fptr, "\t.args     = NULL,\n");
		fprintf(cg->fptr, "};\n");
		fprintf(cg->fptr,
		        "struct closure *closure_%s = &_closure_%s;\n",
		        dec_constructor->name,
		        dec_constructor->name);
	}

	fprintf(cg->fptr, "\n");
}

static void code_gen_dec_data(struct code_generator *cg,
                              struct dec_data *dec_data) {
	/* type enum */
	fprintf(cg->fptr, "enum data_%s_type {\n", dec_data->name);
	list_for_each(
		dec_data->dec_constructors,
		struct dec_constructor *,
		fprintf(cg->fptr, "\tDATA_%s_%s,\n", dec_data->name, _value->name));
	fprintf(cg->fptr, "};\n\n");

	/* type struct */
	fprintf(cg->fptr, "struct data_%s {\n", dec_data->name);
	fprintf(cg->fptr, "\tenum data_%s_type type;\n", dec_data->name);
	fprintf(cg->fptr, "\tunion {\n");
	list_for_each(dec_data->dec_constructors,
	              struct dec_constructor *,
	              code_gen_dec_constructor_struct(cg, _value));
	fprintf(cg->fptr, "\t} v;\n");
	fprintf(cg->fptr, "};\n\n");

	/* constructor functions */
	list_for_each(dec_data->dec_constructors,
	              struct dec_constructor *,
	              code_gen_dec_constructor_func(cg, dec_data->name, _value));
}

static void code_gen_dec_type(struct code_generator *cg,
                              struct dec_type *dec_type) {
	if (strcmp(dec_type->type->name, "->") == 0) {
		fprintf(cg->fptr, "struct closure *closure_%s;\n", dec_type->name);
	} else {
		fprintf(cg->fptr, "struct thunk *val_%s;\n", dec_type->name);
	}
	fprintf(cg->fptr, "\n");
}

static void code_gen_def_value(struct code_generator *cg,
                               struct def_value *def_value) {
	size_t arity = list_length(def_value->expr_params);

	if (arity > 0) {
		struct list *patterns = map_get_str(cg->global_functions, def_value->name);
		if (patterns == NULL) {
			patterns = list_new(NULL);
			assert(patterns != NULL);
			map_put_str(cg->global_functions, def_value->name, patterns);
		}
		list_append(patterns, def_value);
		assert(list_last(patterns) != NULL);
	} else {
		map_put_str(cg->global_values, def_value->name, def_value);
	}
}

static void code_gen_stmt(struct code_generator *cg, struct stmt *stmt) {
	switch (stmt->type) {
	case STMT_DEC_DATA: code_gen_dec_data(cg, stmt->v.dec_data); break;
	case STMT_DEC_TYPE: code_gen_dec_type(cg, stmt->v.dec_type); break;
	case STMT_DEF_VALUE: code_gen_def_value(cg, stmt->v.def_value); break;
	case STMT_DEC_CLASS: break;    /* TODO UNUSED */
	case STMT_DEF_INSTANCE: break; /* TODO UNUSED */
	}
}

static void code_gen_prog(struct code_generator *cg, struct prog *prog) {
	fprintf(cg->fptr, "#include <arena.h>\n");
	fprintf(cg->fptr, "#include <base.h>\n");
	fprintf(cg->fptr, "#include <stdio.h>\n");
	fprintf(cg->fptr, "#include <stdlib.h>\n");
	fprintf(cg->fptr, "\n");
	list_for_each(prog->stmts, struct stmt *, code_gen_stmt(cg, _value));
}

static void code_gen_pattern_check_param(struct code_generator *cg,
                                         struct expr *expr,
                                         struct map *identifier_to_var_index,
                                         size_t *next_var_index,
                                         size_t next_case_index,
                                         size_t param_index) {
	switch (expr->expr_type) {
	case EXPR_IDENTIFIER:
		if (islower(expr->v.identifier[0])) {
			size_t var_index = *next_var_index;
			*next_var_index += 1;
			fprintf(
				cg->fptr, "\tstruct thunk *v_%ld = v_%ld;\n", var_index, param_index);
			map_put_str(
				identifier_to_var_index, expr->v.identifier, (void *)var_index);
		} else {
			size_t var_index = *next_var_index;
			*next_var_index += 1;
			fprintf(cg->fptr,
			        "\tstruct data_%s *v_%ld = thunk_eval(v_%ld, struct data_%s*);\n",
			        expr->type->name,
			        var_index,
			        param_index,
			        expr->type->name);
			fprintf(cg->fptr,
			        "\tif(v_%ld->type != DATA_%s_%s) goto case_%ld;\n",
			        var_index,
			        expr->type->name,
			        expr->v.application.fn,
			        next_case_index);
		}
		break;
	case EXPR_APPLICATION: {
		size_t inner_param_index = 0;
		size_t var_index         = *next_var_index;
		*next_var_index += 1;
		fprintf(cg->fptr,
		        "\tstruct data_%s *v_%ld = thunk_eval(v_%ld, struct data_%s*);\n",
		        expr->type->name,
		        var_index,
		        param_index,
		        expr->type->name);
		fprintf(cg->fptr,
		        "\tif(v_%ld->type != DATA_%s_%s) goto case_%ld;\n",
		        var_index,
		        expr->type->name,
		        expr->v.application.fn,
		        next_case_index);
		list_for_each(expr->v.application.expr_args,
		              struct expr *,
		              size_t inner_var_index = *next_var_index;
		              *next_var_index += 1;
		              fprintf(cg->fptr,
		                      "\tstruct thunk *v_%ld = v_%ld->v.%s.param_%ld;\n",
		                      inner_var_index,
		                      var_index,
		                      expr->v.application.fn,
		                      inner_param_index);
		              code_gen_pattern_check_param(cg,
		                                           _value,
		                                           identifier_to_var_index,
		                                           next_var_index,
		                                           next_case_index,
		                                           inner_var_index);
		              inner_param_index++;);
		break;
	}
	case EXPR_LIT_INT: {
		size_t var_index = *next_var_index;
		*next_var_index += 1;
		fprintf(cg->fptr,
		        "\tint v_%ld = thunk_eval(v_%ld, int);\n",
		        var_index,
		        param_index);
		fprintf(cg->fptr,
		        "\tif (v_%ld != %d) goto case_%ld;\n",
		        var_index,
		        expr->v.lit_int,
		        next_case_index);
		break;
	}
	case EXPR_LIT_DOUBLE: {
		size_t var_index = *next_var_index;
		*next_var_index += 1;
		fprintf(cg->fptr,
		        "\tdouble v_%ld = thunk_eval(v_%ld, double);\n",
		        var_index,
		        param_index);
		fprintf(cg->fptr,
		        "\tif (v_%ld != %f) goto case_%ld;\n",
		        var_index,
		        expr->v.lit_double,
		        next_case_index);
		break;
	}

	case EXPR_LIT_STRING: break; /* TODO */
	case EXPR_LIT_CHAR: {
		size_t var_index = *next_var_index;
		*next_var_index += 1;
		fprintf(cg->fptr,
		        "\tchar v_%ld = thunk_eval(v_%ld, char);\n",
		        var_index,
		        param_index);
		fprintf(cg->fptr,
		        "\tif (v_%ld != %c) goto case_%ld;\n",
		        var_index,
		        expr->v.lit_char,
		        next_case_index);
		break;
	}

	case EXPR_LIT_BOOL: {
		size_t var_index = *next_var_index;
		*next_var_index += 1;
		fprintf(cg->fptr,
		        "\tbool v_%ld = thunk_eval(v_%ld, bool);\n",
		        var_index,
		        param_index);
		fprintf(cg->fptr,
		        "\tif (v_%ld != %i) goto case_%ld;\n",
		        var_index,
		        expr->v.lit_bool,
		        next_case_index);
		break;
	}

	case EXPR_GROUPING:
		code_gen_pattern_check_param(cg,
		                             expr->v.grouping,
		                             identifier_to_var_index,
		                             next_var_index,
		                             next_case_index,
		                             param_index);
		break;
	case EXPR_LIST_NULL: break; /* TODO */
	}
}

static void code_gen_pattern_check_case(struct code_generator *cg,
                                        struct list *expr_params,
                                        struct map *identifier_to_var_index,
                                        size_t *next_var_index,
                                        size_t next_case_index) {
	size_t expr_params_index = 0;
	list_for_each(expr_params,
	              struct expr *,
	              code_gen_pattern_check_param(cg,
	                                           _value,
	                                           identifier_to_var_index,
	                                           next_var_index,
	                                           next_case_index,
	                                           expr_params_index);
	              expr_params_index++;);
}

static char *translate_function_name(char *fn_name) {
	if (strcmp(fn_name, "+") == 0) {
		return "add";
	}
	if (strcmp(fn_name, "-") == 0) {
		return "sub";
	}
	return fn_name;
}

static void code_gen_expr(struct code_generator *cg,
                          struct expr *expr,
                          struct map *identifier_to_var_index,
                          struct map *region_var_to_var_index) {
	switch (expr->expr_type) {
	case EXPR_IDENTIFIER: {
		size_t var_index = 0;
		if (identifier_to_var_index != NULL &&
		    (var_index = (size_t)map_get_str(identifier_to_var_index,
		                                     expr->v.identifier)) != 0) {
			fprintf(cg->fptr, "v_%ld", var_index);
		} else {
			fprintf(cg->fptr, "%s", expr->v.identifier);
		}
		break;
	}
	case EXPR_APPLICATION: {
		size_t args_left = list_length(expr->v.application.expr_args);
		char *fn_name    = expr->v.application.fn;
		fprintf(cg->fptr, "closure_to_thunk(");
		list_for_each(expr->v.application.expr_args, struct expr *, (void)_value;
		              fprintf(cg->fptr, "closure_apply("));
		fprintf(cg->fptr, "closure_%s, ", translate_function_name(fn_name));
		list_for_each(
			expr->v.application.expr_args, struct expr *, (void)_value; code_gen_expr(
				cg, _value, identifier_to_var_index, region_var_to_var_index);
			fprintf(cg->fptr, ")");
			args_left--;
			if (args_left > 0) fprintf(cg->fptr, ", "););
		fprintf(cg->fptr, ")");
		break;
	}
	case EXPR_LIT_INT: {
		if (region_var_to_var_index != NULL) {
			size_t region_var_index =
				(size_t)map_get_str(region_var_to_var_index, expr->type->region_var);
			fprintf(cg->fptr,
			        "lit_thunk((void*)%d, v_%ld)",
			        expr->v.lit_int,
			        region_var_index);
		} else {
			fprintf(cg->fptr, "lit_thunk((void*)%d, NULL)", expr->v.lit_int);
		}
		break;
	}
	case EXPR_LIT_DOUBLE:
		fprintf(cg->fptr, "lit_thunk((void*)%f)", expr->v.lit_double);
		break;
	case EXPR_LIT_STRING: break; /* TODO */
	case EXPR_LIT_CHAR:
		fprintf(cg->fptr, "lit_thunk((void*)%c)", expr->v.lit_char);
		break;
	case EXPR_LIT_BOOL:
		fprintf(cg->fptr, "lit_thunk((void*)%d)", expr->v.lit_bool);
		break;
	case EXPR_GROUPING:
		code_gen_expr(
			cg, expr->v.grouping, identifier_to_var_index, region_var_to_var_index);
		break;
	case EXPR_LIST_NULL: break; /* TODO */
	}
}

// static void _code_gen_region_var_index_assignments(
// 	struct code_generator *cg,
// 	struct region_sort *region_sort,
// 	size_t arg_index,
// 	size_t *next_var_index,
// 	struct map *region_var_to_var_index,
// 	struct list *region_tree_indicies /* list of size_t */
// ) {
//
// 	if (region_sort->name != NULL) {
// 		size_t var_index = *next_var_index;
// 		*next_var_index += 1;
// 		fprintf(cg->fptr,
// 		        "\tstruct region_tree *v_%ld = args[%ld]->region_tree",
// 		        var_index,
// 		        arg_index);
// 		list_for_each_reverse(region_tree_indicies,
// 		                      size_t,
// 		                      fprintf(cg->fptr, "->v.tuple[%ld]", _value));
// 		fprintf(cg->fptr, ";\n");
// 		map_put_str(region_var_to_var_index, region_sort->name, (void
// *)var_index);
// 	}
//
// 	if (region_sort->region_sort_params != NULL) {
// 		size_t index = 0;
// 		list_for_each(
// 			region_sort->region_sort_params,
// 			struct region_sort *,
// 			list_prepend(region_tree_indicies, (void *)index++);
// 			_code_gen_region_var_index_assignments(cg,
// 		                                         _value,
// 		                                         arg_index,
// 		                                         next_var_index,
// 		                                         region_var_to_var_index,
// 		                                         region_tree_indicies);
// 			list_pop_head(region_tree_indicies););
// 	}
// }
//
// static void
// code_gen_region_var_index_assignments(struct code_generator *cg,
//                                       char *region_sort,
//                                       size_t arg_index,
//                                       size_t *next_var_index,
//                                       struct map *region_var_to_var_index) {
// 	struct list *region_tree_indicies = list_new(NULL);
// 	_code_gen_region_var_index_assignments(cg,
// 	                                       region_sort,
// 	                                       arg_index,
// 	                                       next_var_index,
// 	                                       region_var_to_var_index,
// 	                                       region_tree_indicies);
// 	list_free(region_tree_indicies);
// }

static void code_gen_function(struct code_generator *cg, struct list *cases) {
	size_t i;
	struct map *identifier_to_var_index;
	struct map *region_var_to_var_index = map_new();
	char *fn_name = ((struct def_value *)list_head(cases))->name;
	size_t arity =
		list_length(((struct def_value *)list_head(cases))->expr_params);
	size_t next_var_index = 0;
	size_t case_index     = 0;

	struct def_value *head_def = list_head(cases);

	assert(arity > 0);

	fprintf(cg->fptr, "void* fn_%s(struct thunk **args) {\n", fn_name);

	/* setup variables for each arg */
	// for (i = 0; i < arity; i++) {
	// 	fprintf(
	// 		cg->fptr, "\tstruct thunk *v_%ld = args[%ld];\n", next_var_index++, i);
	// 	code_gen_region_var_index_assignments(
	// 		cg,
	// 		((struct expr *)list_get(head_def->expr_params, i))->type->region_var,
	// 		i,
	// 		&next_var_index,
	// 		region_var_to_var_index);
	// }

	/* generate each case */
	list_for_each(
		cases, struct def_value *, identifier_to_var_index = map_new();
		fprintf(cg->fptr, "case_%ld : {\n", case_index++);
		code_gen_pattern_check_case(cg,
	                              _value->expr_params,
	                              identifier_to_var_index,
	                              &next_var_index,
	                              case_index);
		/* TODO assign in thunk and return */
		fprintf(cg->fptr, "\treturn thunk_eval(");
		code_gen_expr(
			cg, _value->value, identifier_to_var_index, region_var_to_var_index);
		fprintf(cg->fptr, ", void*);\n");
		fprintf(cg->fptr, "}\n");
		map_free(identifier_to_var_index));

	fprintf(cg->fptr, "case_%ld : {\n", case_index);
	fprintf(cg->fptr, "\tprintf(\"Unmatched pattern in function '");
	fprintf(cg->fptr, "%s", fn_name);
	fprintf(cg->fptr, "'\");\n");
	fprintf(cg->fptr, "\texit(1);\n");
	fprintf(cg->fptr, "}\n");

	/* TODO error if no case matched */

	fprintf(cg->fptr, "}\n");

	fprintf(cg->fptr, "struct closure _closure_%s = {\n", fn_name);
	fprintf(cg->fptr, "\t.fn_arity = %ld,\n", arity);
	fprintf(cg->fptr, "\t.args_len = 0,\n");
	fprintf(cg->fptr, "\t.fn       = &fn_%s,\n", fn_name);
	fprintf(cg->fptr, "\t.args     = NULL,\n");
	fprintf(cg->fptr, "};\n");

	fprintf(
		cg->fptr, "struct closure *closure_%s = &_closure_%s;\n", fn_name, fn_name);

	fprintf(cg->fptr, "\n");

	return;
}

static void code_gen_functions(struct code_generator *cg) {
	map_for_each(
		cg->global_functions, struct list *, code_gen_function(cg, _value));
}

static void code_gen_main(struct code_generator *cg) {
	fprintf(cg->fptr, "int main(void) {\n");
	map_for_each(cg->global_values,
	             struct def_value *,
	             fprintf(cg->fptr, "\tval_%s = ", _value->name);
	             code_gen_expr(cg, _value->value, NULL, NULL);
	             fprintf(cg->fptr, ";\n"););
	fprintf(cg->fptr, "\tint ret_val = thunk_eval(val_main, int);\n");
	fprintf(cg->fptr, "\tprintf(\"%%d\\n\", ret_val);\n");
	fprintf(cg->fptr, "\treturn 0;\n");
	fprintf(cg->fptr, "}\n");
}

void code_gen(struct prog *prog,
              struct arena *arena,
              struct error_log *log,
              char *file_name) {
	struct code_generator *cg =
		arena_push_struct_zero(arena, struct code_generator);

	cg->arena            = arena;
	cg->log              = log;
	cg->fptr             = fopen(file_name, "w");
	cg->global_functions = map_new();
	cg->global_values    = map_new();

	if (cg->fptr == NULL) {
		report_error(cg->log, "Unable to open file");
		return;
	}

	code_gen_prog(cg, prog);
	code_gen_functions(cg);
	code_gen_main(cg);

	/* TODO maybe free stuff? OS will clear it anyway so maybe not */
	fclose(cg->fptr);
}
