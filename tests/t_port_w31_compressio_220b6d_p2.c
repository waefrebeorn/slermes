/* JSON-lines harness for the w31_compressio_220b6d_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_compressio_220b6d_p2.h"

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
        if (!strcmp(func, "chain_step_sql")) {
            out = w31_compressio_220b6d_p2_chain_step_sql(
                json_get_str(in, "jx_branched", ""),
                json_get_str(in, "jx_delegate", ""),
                json_get_str(in, "reset_child", ""),
                json_get_str(in, "last_active", ""));
        } else if (!strcmp(func, "unset_ancestor_sql")) {
            out = w31_compressio_220b6d_p2_unset_ancestor_sql(
                json_get_str(in, "unset_sql", ""));
        } else if (!strcmp(func, "unstamped_edge_sql")) {
            out = w31_compressio_220b6d_p2_unstamped_edge_sql(
                json_get_str(in, "unset_parent", ""),
                json_get_str(in, "unset_child", ""),
                json_get_str(in, "non_continuation", ""));
        } else if (!strcmp(func, "record_cooldown")) {
            out = w31_compressio_220b6d_p2_record_cooldown(
                json_get_str(in, "session_id", ""),
                json_obj_get(in, "cooldown_until"), json_obj_get(in, "error"));
        } else if (!strcmp(func, "clear_cooldown")) {
            out = w31_compressio_220b6d_p2_clear_cooldown(
                json_get_str(in, "session_id", ""));
        } else if (!strcmp(func, "finalize_orphans")) {
            out = w31_compressio_220b6d_p2_finalize_orphans(get_num(in, "now", 0.0));
        } else if (!strcmp(func, "read_number")) {
            out = w31_compressio_220b6d_p2_read_number(
                json_obj_get(in, "value"), json_get_str(in, "kind", "int"),
                json_obj_get(in, "zero"));
        } else if (!strcmp(func, "streak_write")) {
            out = w31_compressio_220b6d_p2_streak_write(
                json_get_str(in, "column", ""), json_get_str(in, "session_id", ""),
                json_obj_get(in, "streak"));
        } else if (!strcmp(func, "deadline_write")) {
            out = w31_compressio_220b6d_p2_deadline_write(
                json_get_str(in, "session_id", ""), json_obj_get(in, "deadline"));
        } else if (!strcmp(func, "overload_bump")) {
            out = w31_compressio_220b6d_p2_overload_bump(
                json_get_str(in, "session_id", ""), json_obj_get(in, "row"));
        } else if (!strcmp(func, "parent_gate")) {
            out = w31_compressio_220b6d_p2_parent_gate(
                get_int(in, "parent_exists", 0), get_int(in, "ended_at_set", 0),
                get_int(in, "automatic_reason", 0));
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
