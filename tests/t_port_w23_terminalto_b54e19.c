/* JSON-lines harness for the w23_terminalto_b54e19 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_terminalto_b54e19.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* A count may arrive as a number or as a decimal string. */
static long long as_count(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_STRING && v->str_val) return strtoll(v->str_val, NULL, 10);
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return 0;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "prologue")) {
            const json_t *tok = json_obj_get(in, "timeout_token");
            const json_t *bg = json_obj_get(in, "background");
            char *s = w23_terminalto_b54e19_prologue(
                as_count(json_obj_get(in, "count")),
                (tok && tok->type == JSON_STRING) ? tok->str_val : w23_terminalto_b54e19_timeout_token(),
                bg ? (bg->type == JSON_BOOL ? (bg->bool_val ? 1 : 0) : 1) : 1,
                json_obj_get(in, "sudo_words"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "fill")) {
            const json_t *cmd = json_obj_get(in, "command");
            char *s = 0;
            if (cmd && cmd->type == JSON_STRING)
                s = w23_terminalto_b54e19_fill_timeout(cmd->str_val, json_obj_get(in, "timeout"));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "own_input")) {
            out = w23_terminalto_b54e19_own_input(json_obj_get(in, "words"));
        } else if (!strcmp(func, "supported")) {
            out = w23_terminalto_b54e19_supported_once(json_obj_get(in, "world"));
        } else if (!strcmp(func, "consts")) {
            out = json_object();
            json_set(out, "token", json_string(w23_terminalto_b54e19_timeout_token()));
            json_set(out, "prefix", json_string(w23_terminalto_b54e19_prefix()));
            json_set(out, "suffix", json_string(w23_terminalto_b54e19_suffix()));
            json_set(out, "probe", json_string(w23_terminalto_b54e19_capability_probe()));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
