/* JSON-lines harness for the w31_compressio_220b6d_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_compressio_220b6d_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int get_int(const json_t *in, const char *key, int def) {
    const json_t *v = json_obj_get(in, key);
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (int)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return def;
}

static double get_num(const json_t *in, const char *key, double def) {
    const json_t *v = json_obj_get(in, key);
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* A nullable TEXT argument: null stays NULL, a string passes through. */
static const char *opt_str(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "cooldown_row")) {
            out = w31_compressio_220b6d_p1_cooldown_row(
                json_get_bool(in, "session_exists", 0),
                json_obj_get(in, "cooldown_until"), json_obj_get(in, "error"));
        } else if (!strcmp(func, "walk_chain")) {
            out = w31_compressio_220b6d_p1_walk_chain(
                json_get_str(in, "session_id", ""), json_obj_get(in, "steps"),
                get_int(in, "cap", W31_COMPRESSIO_220B6D_CHAIN_CAP));
        } else if (!strcmp(func, "chain_tip")) {
            out = w31_compressio_220b6d_p1_chain_tip(
                json_get_str(in, "session_id", ""), json_obj_get(in, "steps"),
                get_int(in, "cap", W31_COMPRESSIO_220B6D_CHAIN_CAP));
        } else if (!strcmp(func, "unset_created")) {
            out = w31_compressio_220b6d_p1_unset_created(json_obj_get(in, "value"));
        } else if (!strcmp(func, "fork_child")) {
            out = w31_compressio_220b6d_p1_fork_child(
                json_obj_get(in, "session"), get_int(in, "include_reset", 0));
        } else if (!strcmp(func, "compression_child")) {
            out = w31_compressio_220b6d_p1_compression_child(
                json_obj_get(in, "child"), json_obj_get(in, "sessions"));
        } else if (!strcmp(func, "lineage")) {
            out = w31_compressio_220b6d_p1_lineage(
                json_get_str(in, "session_id", ""), json_obj_get(in, "sessions"),
                json_obj_get(in, "children"));
        } else if (!strcmp(func, "tool_root")) {
            out = w31_compressio_220b6d_p1_tool_root(
                json_get_str(in, "session_id", ""), json_obj_get(in, "sessions"));
        } else if (!strcmp(func, "lease_key")) {
            out = w31_compressio_220b6d_p1_lease_key(
                json_get_str(in, "session_id", ""), json_obj_get(in, "sessions"));
        } else if (!strcmp(func, "cooldown_active")) {
            out = w31_compressio_220b6d_p1_cooldown_active(
                json_get_str(in, "session_id", ""), json_obj_get(in, "row"),
                get_num(in, "now", 0.0));
        } else if (!strcmp(func, "stamp_ancestors")) {
            out = w31_compressio_220b6d_p1_stamp_ancestors(
                json_get_str(in, "session_id", ""), json_obj_get(in, "created"),
                json_obj_get(in, "steps"), get_int(in, "unset_ancestor", 0),
                get_int(in, "cap", W31_COMPRESSIO_220B6D_CHAIN_CAP));
        } else if (!strcmp(func, "child_row")) {
            out = w31_compressio_220b6d_p1_child_row(
                json_obj_get(in, "parent"), json_obj_get(in, "model_config"),
                json_get_str(in, "source", ""), opt_str(in, "model"),
                json_get_str(in, "parent_session_id", ""),
                json_get_str(in, "child_id", ""), json_get_str(in, "cwd", ""),
                json_get_str(in, "profile_name", ""),
                opt_str(in, "prompt_hash"),
                opt_str(in, "own_profile"),
                json_obj_get(in, "surface_parent"),
                json_obj_get(in, "surface_source"), get_num(in, "now", 0.0));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
