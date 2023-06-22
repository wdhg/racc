/* TODO
 * this code is in desperate need of a rework.
 * much of the ugliness is from generating code from the AST.
 * adding an IR would go a long way to improving it.
 * also, generating a C AST would likely be better then generating raw strings.
 */
#include "code_gen.h"
#include "ast.h"
#include "list.h"
#include "map.h"
#include "set.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef size_t vid; /* var id */
typedef size_t rid; /* region id */

struct value {
	struct dec_type *dec_type;
	struct list *def_values;
	struct list *thunks_to_release;
};

struct code_generator {
	struct arena *arena;
	struct error_log *log;

	FILE *fptr;
	struct map *values;            /* char* -> struct value* */
	struct map *identifier_to_rid; /* char* -> rid */
	struct map *region_var_to_id;  /* char* -> rid */
	rid rid_state;
};

static char *translate_type_name(char *fn_name) {
	if (strcmp(fn_name, "[]") == 0) {
		return "List";
	}
	return fn_name;
}

static char *translate_identifier_name(char *fn_name) {
	if (strcmp(fn_name, "+") == 0) {
		return "add";
	}
	if (strcmp(fn_name, "-") == 0) {
		return "sub";
	}
	if (strcmp(fn_name, "[]") == 0) {
		return "Null";
	}
	if (strcmp(fn_name, ":") == 0) {
		return "Cons";
	}
	return fn_name;
}

static void add_value_dec(struct code_generator *cg,
                          struct dec_type *dec_type) {
	struct value *value      = arena_push_struct_zero(cg->arena, struct value);
	value->dec_type          = dec_type;
	value->def_values        = list_new(cg->arena);
	value->thunks_to_release = list_new(cg->arena);
	map_put_str(cg->values, dec_type->name, value);
}

static void add_value_def(struct code_generator *cg,
                          struct def_value *def_value,
                          struct list *thunks_to_release_in_def_value) {
	struct value *value = map_get_str(cg->values, def_value->name);
	assert(value != NULL);
	list_append(value->def_values, def_value);
	list_prepend_all(value->thunks_to_release, thunks_to_release_in_def_value);
}

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

		/* function */
		fprintf(cg->fptr,
		        "void *fn_%s(struct thunk **args, struct region *region) {\n",
		        dec_constructor->name);
		fprintf(cg->fptr, "\tstruct data_%s *value;\n", data_name);

		fprintf(cg->fptr, "\tif (region == NULL) {\n");
		fprintf(
			cg->fptr, "\tvalue = calloc(1, sizeof(struct data_%s));\n", data_name);
		fprintf(cg->fptr, "\t} else {\n");
		fprintf(cg->fptr,
		        "\t\tvalue = region_push_struct(region, struct data_%s);\n",
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

		/* closure */
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
	fprintf(cg->fptr, "};\n");

	/* type struct */
	fprintf(cg->fptr, "struct data_%s {\n", dec_data->name);
	fprintf(cg->fptr, "\tenum data_%s_type type;\n", dec_data->name);
	fprintf(cg->fptr, "\tunion {\n");
	list_for_each(dec_data->dec_constructors,
	              struct dec_constructor *,
	              code_gen_dec_constructor_struct(cg, _value));
	fprintf(cg->fptr, "\t} v;\n");
	fprintf(cg->fptr, "};\n");

	/* copy function */
	fprintf(cg->fptr,
	        "void *value_copy_%s(void *value, struct region *region) {\n",
	        dec_data->name);
	fprintf(cg->fptr, "\tstruct data_%s *data = value;\n", dec_data->name);
	fprintf(
		cg->fptr,
		"\tstruct data_%s *copy = region_push_struct(region, struct data_%s);\n",
		dec_data->name,
		dec_data->name);
	fprintf(cg->fptr, "\tcopy->type = data->type;\n");
	list_for_each(
		dec_data->dec_constructors, struct dec_constructor *, size_t i = 0;
		struct dec_constructor *dec_constructor = _value;
		if (dec_constructor->type_params == NULL ||
	      list_length(dec_constructor->type_params) == 0) continue;
		fprintf(cg->fptr,
	          "\tif (data->type == DATA_%s_%s) {\n",
	          dec_data->name,
	          dec_constructor->name);
		list_for_each(dec_constructor->type_params, struct type *, (void)_value);
		fprintf(
			cg->fptr,
			"\t\tcopy->v.%s.param_%ld = thunk_copy(data->v.%s.param_%ld, region);\n",
			dec_constructor->name,
			i,
			dec_constructor->name,
			i);
		fprintf(cg->fptr, "\t}\n"););
	fprintf(cg->fptr, "\treturn copy;\n");
	fprintf(cg->fptr, "}\n");
	fprintf(cg->fptr, "\n");

	/* constructor functions */
	list_for_each(dec_data->dec_constructors,
	              struct dec_constructor *,
	              code_gen_dec_constructor_func(cg, dec_data->name, _value));
}

static void code_gen_dec_type(struct code_generator *cg,
                              struct dec_type *dec_type) {
	rid region_id = (rid)map_get_str(cg->region_var_to_id, dec_type->region_var);

	add_value_dec(cg, dec_type);

	if (region_id == 0) {
		region_id = cg->rid_state++;
		map_put_str(cg->region_var_to_id, dec_type->region_var, (void *)region_id);
		fprintf(cg->fptr, "struct region r_%ld = {\n", region_id);
		fprintf(cg->fptr, "\t.arena           = NULL,\n");
		fprintf(cg->fptr, "\t.reference_count = 0,\n");
		fprintf(cg->fptr, "};\n");
	}

	map_put_str(cg->identifier_to_rid, dec_type->name, (void *)region_id);

	fprintf(cg->fptr, "struct closure *closure_%s;\n", dec_type->name);
	fprintf(cg->fptr, "struct thunk *val_%s;\n", dec_type->name);
	fprintf(cg->fptr, "\n");
}

static void code_gen_stmt(struct code_generator *cg, struct stmt *stmt);

/* moves declarations/definitions in let..in exprs to top most level
 * replaces groupings with their inner expr */
static struct expr *flatten_expr(struct code_generator *cg,
                                 struct expr *expr,
                                 struct list *thunks_to_release_in_def_value) {
	switch (expr->expr_type) {
	case EXPR_LET_IN:
		list_for_each(
			expr->v.let_in.stmts, struct stmt *, code_gen_stmt(cg, _value);
			if (_value->type == STMT_DEC_TYPE)
				list_append(thunks_to_release_in_def_value, _value->v.dec_type->name));
		return flatten_expr(
			cg, expr->v.let_in.value, thunks_to_release_in_def_value);
		break;
	case EXPR_APPLICATION: {
		struct list *expr_args_new = list_new(cg->arena);
		list_map(expr_args_new,
		         expr->v.application.expr_args,
		         struct expr *,
		         flatten_expr(cg, _value, thunks_to_release_in_def_value));
		expr->v.application.expr_args = expr_args_new;
		return expr;
	}
	case EXPR_GROUPING:
		return flatten_expr(cg, expr->v.grouping, thunks_to_release_in_def_value);
	case EXPR_IDENTIFIER:
	case EXPR_LIT_INT:
	case EXPR_LIT_DOUBLE:
	case EXPR_LIT_STRING:
	case EXPR_LIT_CHAR:
	case EXPR_LIT_BOOL:
	case EXPR_LIST_NULL: return expr;
	}
}

static void code_gen_def_value(struct code_generator *cg,
                               struct def_value *def_value) {
	struct list *thunks_to_release_in_def_value = list_new(NULL);
	def_value->value =
		flatten_expr(cg, def_value->value, thunks_to_release_in_def_value);
	add_value_def(cg, def_value, thunks_to_release_in_def_value);
	list_free(thunks_to_release_in_def_value);
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
	fprintf(cg->fptr,
	        "void *value_copy_Int(void *value, struct region *region) {\n");
	fprintf(cg->fptr, "\t(void)region;\n");
	fprintf(cg->fptr, "\treturn (int)value;\n");
	fprintf(cg->fptr, "}\n");
	fprintf(cg->fptr, "\n");
	list_for_each(prog->stmts, struct stmt *, code_gen_stmt(cg, _value));
}

static vid vid_next(vid *var_id_state) {
	vid next = *var_id_state;
	*var_id_state += 1;
	return next;
}

static vid vid_curr(vid *var_id_state) { return *var_id_state - 1; }

/* checks if param (thunk stored in param_vid) matches expr */
static void code_gen_pattern_check_param(struct code_generator *cg,
                                         vid param_thunk_vid,
                                         struct expr *expr,
                                         vid *vid_state,
                                         size_t next_case_index,
                                         struct set *param_vars) {
	switch (expr->expr_type) {
	case EXPR_IDENTIFIER:
		if (expr->v.identifier[0] == '_') {
			break;
		}
		if (islower(expr->v.identifier[0])) {
			/* variable */
			fprintf(cg->fptr,
			        "\tstruct thunk *val_%s = v_%ld;\n",
			        expr->v.identifier,
			        param_thunk_vid);
			set_put_str(param_vars, expr->v.identifier);
		} else {
			/* data type (no params) */
			size_t var_id          = vid_next(vid_state);
			char *data_type_name   = translate_type_name(expr->type->name);
			char *constructor_name = translate_identifier_name(expr->v.identifier);
			fprintf(cg->fptr,
			        "\tstruct data_%s *v_%ld = thunk_eval(v_%ld, struct data_%s*);\n",
			        data_type_name,
			        var_id,
			        param_thunk_vid,
			        data_type_name);
			fprintf(cg->fptr,
			        "\tif(v_%ld->type != DATA_%s_%s) goto case_%ld;\n",
			        var_id,
			        data_type_name,
			        constructor_name,
			        next_case_index);
		}
		break;
	case EXPR_APPLICATION: {
		/* data type (with params) */
		size_t inner_param_index = 0;
		size_t var_id            = vid_next(vid_state);
		char *data_type_name     = translate_type_name(expr->type->name);
		char *constructor_name = translate_identifier_name(expr->v.application.fn);
		/* evaluate data structure */
		fprintf(cg->fptr,
		        "\tstruct data_%s *v_%ld = thunk_eval(v_%ld, struct data_%s*);\n",
		        data_type_name,
		        var_id,
		        param_thunk_vid,
		        data_type_name);
		/* check it is the right structure type */
		fprintf(cg->fptr,
		        "\tif(v_%ld->type != DATA_%s_%s) goto case_%ld;\n",
		        var_id,
		        data_type_name,
		        constructor_name,
		        next_case_index);
		/* recurse into its parameters */
		list_for_each(expr->v.application.expr_args,
		              struct expr *,
		              size_t inner_param_thunk_vid = vid_next(vid_state);
		              /* extract parameter into its own variable */
		              fprintf(cg->fptr,
		                      "\tstruct thunk *v_%ld = v_%ld->v.%s.param_%ld;\n",
		                      inner_param_thunk_vid,
		                      var_id,
		                      constructor_name,
		                      inner_param_index);
		              code_gen_pattern_check_param(cg,
		                                           inner_param_thunk_vid,
		                                           _value,
		                                           vid_state,
		                                           next_case_index,
		                                           param_vars);
		              inner_param_index++;);
		break;
	}
	case EXPR_LIT_INT: {
		size_t var_id = vid_next(vid_state);
		fprintf(cg->fptr,
		        "\tint v_%ld = thunk_eval(v_%ld, int);\n",
		        var_id,
		        param_thunk_vid);
		fprintf(cg->fptr,
		        "\tif (v_%ld != %d) goto case_%ld;\n",
		        var_id,
		        expr->v.lit_int,
		        next_case_index);
		break;
	}
	case EXPR_LIT_DOUBLE: {
		size_t var_id = vid_next(vid_state);
		fprintf(cg->fptr,
		        "\tdouble v_%ld = thunk_eval(v_%ld, double);\n",
		        var_id,
		        param_thunk_vid);
		fprintf(cg->fptr,
		        "\tif (v_%ld != %f) goto case_%ld;\n",
		        var_id,
		        expr->v.lit_double,
		        next_case_index);
		break;
	}

	case EXPR_LIT_STRING: break; /* TODO */
	case EXPR_LIT_CHAR: {
		size_t var_id = vid_next(vid_state);
		fprintf(cg->fptr,
		        "\tchar v_%ld = thunk_eval(v_%ld, char);\n",
		        var_id,
		        param_thunk_vid);
		fprintf(cg->fptr,
		        "\tif (v_%ld != %c) goto case_%ld;\n",
		        var_id,
		        expr->v.lit_char,
		        next_case_index);
		break;
	}

	case EXPR_LIT_BOOL: {
		size_t var_id = vid_next(vid_state);
		fprintf(cg->fptr,
		        "\tbool v_%ld = thunk_eval(v_%ld, bool);\n",
		        var_id,
		        param_thunk_vid);
		fprintf(cg->fptr,
		        "\tif (v_%ld != %i) goto case_%ld;\n",
		        var_id,
		        expr->v.lit_bool,
		        next_case_index);
		break;
	}

	case EXPR_GROUPING:
		code_gen_pattern_check_param(cg,
		                             param_thunk_vid,
		                             expr->v.grouping,
		                             vid_state,
		                             next_case_index,
		                             param_vars);
		break;
	case EXPR_LIST_NULL: {
		/* data type (no params) */
		size_t var_id = vid_next(vid_state);
		fprintf(
			cg->fptr,
			"\tstruct data_List *v_%ld = thunk_eval(v_%ld, struct data_List*);\n",
			var_id,
			param_thunk_vid);
		fprintf(cg->fptr,
		        "\tif(v_%ld->type != DATA_List_Null) goto case_%ld;\n",
		        var_id,
		        next_case_index);
		break;
	}
	case EXPR_LET_IN: assert(0); /* no let..in exprs in parameter patterns */
	}
}

static void code_gen_pattern_check_case(struct code_generator *cg,
                                        struct list *expr_params,
                                        size_t *vid_state,
                                        size_t next_case_index,
                                        struct set *param_vars) {
	/* the first few vids are each function param thunk */
	size_t param_thunk_vid = 1;
	list_for_each(
		expr_params,
		struct expr *,
		code_gen_pattern_check_param(
			cg, param_thunk_vid, _value, vid_state, next_case_index, param_vars);
		param_thunk_vid++;);
}

static struct type *get_return_type(struct type *type) {
	while (strcmp(type->name, "->") == 0) {
		type = list_last(type->type_args);
	}
	return type;
}

static void code_gen_expr(struct code_generator *cg,
                          struct expr *expr,
                          vid *vid_state,
                          struct set *param_vars) {
	switch (expr->expr_type) {
	case EXPR_IDENTIFIER: {
		char *name = translate_identifier_name(expr->v.identifier);
		if (param_vars == NULL || !set_has_str(param_vars, name)) {
			rid region_id = (rid)map_get_str(cg->identifier_to_rid, name);
			fprintf(cg->fptr, "\tif (val_%s == NULL) {\n", name);
			fprintf(cg->fptr,
			        "\t\tif (r_%ld.arena == NULL) r_%ld.arena = arena_alloc();\n",
			        region_id,
			        region_id);
			fprintf(
				cg->fptr,
				"\t\tval_%s = thunk_closure(closure_%s, &r_%ld, value_copy_%s);\n",
				name,
				name,
				region_id,
				translate_type_name(get_return_type(expr->type)->name));
			fprintf(cg->fptr, "\t}\n");
		}
		fprintf(
			cg->fptr, "\tstruct thunk *v_%ld = val_%s;\n", vid_next(vid_state), name);
		break;
	}
	case EXPR_APPLICATION: {
		size_t args_left  = list_length(expr->v.application.expr_args);
		vid *arg_indicies = calloc(args_left, sizeof(vid));
		char *fn_name     = translate_identifier_name(expr->v.application.fn);
		size_t i;

		i = 0;
		list_for_each(expr->v.application.expr_args,
		              struct expr *,
		              code_gen_expr(cg, _value, vid_state, param_vars);
		              arg_indicies[i] = vid_curr(vid_state);
		              i++);

		fprintf(cg->fptr, "\tstruct thunk *v_%ld = ", vid_next(vid_state));

		for (i = 0; i < args_left; i++) {
			fprintf(cg->fptr, "thunk_apply(");
		}

		fprintf(cg->fptr,
		        "thunk_closure(closure_%s, region, value_copy_%s), ",
		        fn_name,
		        translate_type_name(expr->type->name));

		for (i = 0; i < args_left; i++) {
			fprintf(cg->fptr, "v_%ld)", arg_indicies[i]);
			if (i < args_left - 1) {
				fprintf(cg->fptr, ", ");
			}
		}

		free(arg_indicies);
		fprintf(cg->fptr, ";\n");
		break;
	}
	case EXPR_LIT_INT: {
		fprintf(
			cg->fptr,
			"\tstruct thunk *v_%ld = thunk_lit((void*)%d, region, value_copy_Int);\n",
			vid_next(vid_state),
			expr->v.lit_int);
		break;
	}
	case EXPR_LIT_DOUBLE:
		fprintf(cg->fptr,
		        "\tstruct thunk *v_%ld = thunk_lit((void*)%f, region);\n",
		        vid_next(vid_state),
		        expr->v.lit_double);
		break;
	case EXPR_LIT_STRING: break; /* TODO */
	case EXPR_LIT_CHAR:
		fprintf(cg->fptr,
		        "\tstruct thunk *v_%ld = thunk_lit((void*)%c, region);\n",
		        vid_next(vid_state),
		        expr->v.lit_char);
		break;
	case EXPR_LIT_BOOL:
		fprintf(cg->fptr,
		        "\tstruct thunk *v_%ld = thunk_lit((void*)%d, region);\n",
		        vid_next(vid_state),
		        expr->v.lit_bool);
		break;
	case EXPR_LIST_NULL:
		fprintf(
			cg->fptr, "\tstruct thunk *v_%ld = val_Null;\n", vid_next(vid_state));
		break;
	case EXPR_GROUPING: assert(0); /* should be removed by flatten_expr */
	case EXPR_LET_IN: assert(0);   /* should be removed by flatten_expr */
	default: assert(0);            /* unknown expr_type */
	}
}

static void code_gen_value(struct code_generator *cg, struct value *value) {
	char *name   = value->dec_type->name;
	size_t arity = list_length(
		((struct def_value *)list_head(value->def_values))->expr_params);

	vid _vid_state = 1;
	vid *vid_state = &_vid_state;

	fprintf(cg->fptr,
	        "void* fn_%s(struct thunk **args, struct region *region) {\n",
	        name);
	fprintf(cg->fptr, "\tvoid* ret_thunk;\n");

	if (arity == 0) {
		struct def_value *value_def_value = list_head(value->def_values);
		code_gen_expr(cg, value_def_value->value, vid_state, NULL);
		fprintf(cg->fptr, "\tret_thunk = v_%ld;\n", vid_curr(vid_state));
		fprintf(cg->fptr, "\tgoto ret;\n");
	} else {
		size_t i;
		size_t next_case_index = 0;

		/* setup variables for each arg */
		/* first v_0 .. v_n variables are the arguments */
		for (i = 0; i < arity; i++) {
			fprintf(cg->fptr,
			        "\tstruct thunk *v_%ld = args[%ld];\n",
			        vid_next(vid_state),
			        i);
		}

		fprintf(cg->fptr, "\tgoto case_0;\n");
		/* generate each case */
		list_for_each(
			value->def_values, struct def_value *, struct set *param_vars = set_new();
			fprintf(cg->fptr, "case_%ld : {\n", next_case_index++);
			code_gen_pattern_check_case(
				cg, _value->expr_params, vid_state, next_case_index, param_vars);
			code_gen_expr(cg, _value->value, vid_state, param_vars);
			fprintf(cg->fptr, "\tret_thunk = v_%ld;\n", vid_curr(vid_state));
			fprintf(cg->fptr, "\tgoto ret;\n");
			fprintf(cg->fptr, "}\n"));
		/* error case if no matches */
		fprintf(cg->fptr, "case_%ld : {\n", next_case_index);
		fprintf(cg->fptr, "\tprintf(\"Unmatched pattern in function '");
		fprintf(cg->fptr, "%s", name);
		fprintf(cg->fptr, "'\");\n");
		fprintf(cg->fptr, "\texit(1);\n");
		fprintf(cg->fptr, "}\n");
	}

	fprintf(cg->fptr, "ret: {\n");
	fprintf(cg->fptr, "\tvoid *ret_val = _thunk_eval(ret_thunk);\n");
	list_for_each(value->thunks_to_release,
	              char *,
	              fprintf(cg->fptr, "\tthunk_release(val_%s);\n", _value));
	fprintf(cg->fptr, "\treturn ret_val;\n");
	fprintf(cg->fptr, "}\n");

	fprintf(cg->fptr, "}\n"); /* end of function */

	fprintf(cg->fptr, "struct closure _closure_%s = {\n", name);
	fprintf(cg->fptr, "\t.fn_arity = %ld,\n", arity);
	fprintf(cg->fptr, "\t.args_len = 0,\n");
	fprintf(cg->fptr, "\t.fn       = &fn_%s,\n", name);
	fprintf(cg->fptr, "\t.args     = NULL,\n");
	fprintf(cg->fptr, "};\n");

	fprintf(cg->fptr, "struct closure *closure_%s = &_closure_%s;\n", name, name);

	fprintf(cg->fptr, "\n");

	return;
}

static void code_gen_values(struct code_generator *cg) {
	map_for_each(cg->values, struct value *, code_gen_value(cg, _value));
}

static void code_gen_main(struct code_generator *cg) {
	fprintf(cg->fptr, "int main(void) {\n");
	fprintf(cg->fptr, "\tstruct region *r_main = region_new();\n");
	/* TODO use actual region for main */
	fprintf(
		cg->fptr,
		"\tval_main = thunk_closure(closure_main, r_main, value_copy_Int);\n");
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

	cg->arena             = arena;
	cg->log               = log;
	cg->fptr              = fopen(file_name, "w");
	cg->values            = map_new();
	cg->identifier_to_rid = map_new();
	cg->region_var_to_id  = map_new();
	cg->rid_state         = 1; /* start at 1 as 0 == NULL */

	if (cg->fptr == NULL) {
		report_error(cg->log, "Unable to open file");
		return;
	}

	code_gen_prog(cg, prog);
	code_gen_values(cg);
	code_gen_main(cg);

	/* TODO maybe free stuff? OS will clear it anyway so maybe not */
	fclose(cg->fptr);
}
