/* JSON-lines harness for the w34_holders_028cc0 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_holders_028cc0.h"

static char line[1048576];

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *arg_str(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static long arg_long(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return 0;
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in || in->type != JSON_OBJECT) { json_free(in); continue; }
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "looks_like_python_executable")) {
            out = json_bool(w34_holders_028cc0_looks_like_python_executable(arg_str(in, "program")));
        } else if (!strcmp(func, "python_execution_target")) {
            out = w34_holders_028cc0_python_execution_target(json_obj_get(in, "argv"));
        } else if (!strcmp(func, "looks_like_hermes")) {
            out = json_bool(w34_holders_028cc0_looks_like_hermes(json_obj_get(in, "argv")));
        } else if (!strcmp(func, "argv_flag_value")) {
            out = w34_holders_028cc0_argv_flag_value(json_obj_get(in, "argv"), json_obj_get(in, "flags"));
        } else if (!strcmp(func, "argv_env_home")) {
            out = w34_holders_028cc0_argv_env_home(json_obj_get(in, "argv"));
        } else if (!strcmp(func, "sqlite_family")) {
            out = w34_holders_028cc0_sqlite_family(arg_str(in, "base"));
        } else if (!strcmp(func, "norm_path")) {
            out = w34_holders_028cc0_norm_path(arg_str(in, "value"));
        } else if (!strcmp(func, "canonical_sqlite_path")) {
            out = w34_holders_028cc0_canonical_sqlite_path(arg_str(in, "path"));
        } else if (!strcmp(func, "argv_path_tokens")) {
            out = w34_holders_028cc0_argv_path_tokens(json_obj_get(in, "argv"));
        } else if (!strcmp(func, "names_other_profile")) {
            out = json_bool(w34_holders_028cc0_names_other_profile(
                arg_str(in, "normalized"), arg_str(in, "install_root"), arg_str(in, "our_profile")));
        } else if (!strcmp(func, "argv_home_selection")) {
            out = w34_holders_028cc0_argv_home_selection(
                json_obj_get(in, "argv"), arg_str(in, "this_home"),
                arg_str(in, "install_root"), arg_str(in, "our_profile"));
        } else if (!strcmp(func, "argv_scoped_to_other_home")) {
            out = json_bool(w34_holders_028cc0_argv_scoped_to_other_home(
                json_obj_get(in, "argv"), arg_str(in, "db_path"),
                arg_str(in, "install_root"), arg_str(in, "our_profile")));
        } else if (!strcmp(func, "describe_holder_pid")) {
            out = w34_holders_028cc0_describe_holder_pid(arg_long(in, "pid"), json_obj_get(in, "argv"));
        } else if (!strcmp(func, "held_store_refusal")) {
            out = w34_holders_028cc0_held_store_refusal(
                arg_str(in, "db_path"), json_obj_get(in, "holders"), arg_str(in, "command"),
                arg_str(in, "profile_arg"), arg_str(in, "docs_url"), arg_str(in, "force_hint"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            json_t *v;
            if (!k || !strcmp(k, "func")) continue;
            v = json_copy(json_object_get_at(in, i));
            fputs(",", stdout);
            emit_key(k, v);
            json_free(v);
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
