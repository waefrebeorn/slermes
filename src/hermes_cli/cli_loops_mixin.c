/*
 * cli_loops_mixin.c — C port of upstream Hermes hermes_cli/cli_loops_mixin.py.
 *
 * PoP: w13_cliloopsm_ec3523_preview @ hermes_cli/cli_loops_mixin.py:_preview
 * PoP: w13_cliloopsm_ec3523_parse_queue_index @ hermes_cli/cli_loops_mixin.py:_parse_queue_index
 * PoP: w13_cliloopsm_ec3523_last_assistant_response_text @ hermes_cli/cli_loops_mixin.py:CLILoopsMixin._last_assistant_response_text
 *
 * Pure string/token helpers: no process state, env or file I/O. Inputs arrive as
 * JSON seams and outputs are new json_t values owned by the caller.
 */
#include "noc_w13_cliloopsm_ec3523.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_t *w13_cliloopsm_ec3523_preview(const json_t *payload) {
    const char *s = (payload && payload->type == JSON_STRING) ? payload->str_val : "";
    if (!s) s = "";
    size_t n = strlen(s);
    char *buf = (char *)malloc(n + 4);
    if (!buf) return json_string("");
    memcpy(buf, s, n);
    if (n > 80) {
        memcpy(buf + 80, "...", 3);
        buf[83] = '\0';
    } else {
        buf[n] = '\0';
    }
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

json_t *w13_cliloopsm_ec3523_parse_queue_index(const json_t *token) {
    const char *s = (token && token->type == JSON_STRING) ? token->str_val : "";
    if (!s) s = "";
    size_t n = strlen(s);
    if (n == 0 || n > 4300) return json_null();
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return json_null();
    }
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if (!end || *end != '\0') return json_null();
    return json_number((double)v);
}

json_t *w13_cliloopsm_ec3523_last_assistant_response_text(const json_t *history) {
    if (!json_is_array(history)) return json_string("");
    long long count = (long long)json_len(history);
    for (long long i = count - 1; i >= 0; i--) {
        json_t *msg = json_get(history, (size_t)i);
        if (!json_is_object(msg)) continue;
        const char *role = json_get_str(msg, "role", "");
        if (!role || strcmp(role, "assistant") != 0) continue;
        json_t *content = json_obj_get(msg, "content");
        if (json_is_array(content)) {
            size_t parts = json_len(content);
            size_t cap = 64, used = 0;
            char *buf = (char *)malloc(cap);
            if (!buf) return json_string("");
            buf[0] = '\0';
            for (size_t p = 0; p < parts; p++) {
                json_t *part = json_get(content, p);
                if (!json_is_object(part)) continue;
                const char *ty = json_get_str(part, "type", "");
                if (!ty || (strcmp(ty, "text") != 0 && strcmp(ty, "output_text") != 0)) continue;
                const char *txt = json_get_str(part, "text", "");
                if (!txt || !*txt) continue; /* join only truthy parts */
                size_t tl = strlen(txt);
                if (used + tl + 2 > cap) {
                    while (used + tl + 2 > cap) cap *= 2;
                    char *nb = (char *)realloc(buf, cap);
                    if (!nb) { free(buf); return json_string(""); }
                    buf = nb;
                }
                if (used) buf[used++] = '\n';
                memcpy(buf + used, txt, tl);
                used += tl;
                buf[used] = '\0';
            }
            json_t *out = json_string(buf);
            free(buf);
            return out;
        }
        if (json_is_string(content) && content->str_val) return json_string(content->str_val);
        if (!content || json_is_null(content)) return json_string("");
        if (json_is_number(content)) {
            char num[64];
            snprintf(num, sizeof num, "%.17g", content->num_val);
            return json_string(num);
        }
        if (json_is_bool(content)) return json_string(content->bool_val ? "True" : "False");
        return json_string("");
    }
    return json_string("");
}
