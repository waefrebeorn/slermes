/*
 * session_info_reads.c — C port of upstream Hermes gateway/session_info_reads.py.
 *
 * PoP: w13_sessioninf_c15e44_owner_lines @ gateway/session_info_reads.py:owner_lines
 * PoP: w13_sessioninf_c15e44_run_info_read @ gateway/session_info_reads.py:run_info_read
 *
 * The owner-loop offload and the info-line builders are scripted in the oracle;
 * this port reproduces the live module's join/strip and read dispatch.
 */
#include "noc_w13_sessioninf_c15e44.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* '\n'.join(lines).strip('\n') — strips ONLY newline characters, both ends. */
static json_t *build_exec(const json_t *lines) {
    size_t n = json_is_array(lines) ? json_len(lines) : 0;
    size_t total = 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = jstr(json_get(lines, i));
        total += (s ? strlen(s) : 0) + 1;
    }
    char *buf = (char *)malloc(total + 1);
    if (!buf) return json_null();
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = jstr(json_get(lines, i));
        if (s) { size_t k = strlen(s); memcpy(buf + m, s, k); m += k; }
        buf[m++] = '\n';
    }
    buf[m] = '\0';
    size_t a = 0, b = m;
    while (a < b && buf[a] == '\n') a++;
    while (b > a && buf[b - 1] == '\n') b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) { free(buf); return json_null(); }
    if (b > a) memcpy(out, buf + a, b - a);
    out[b - a] = '\0'; /* NUL at the NEW, shortened length */
    free(buf);
    json_t *reply = json_object();
    json_set(reply, "type", json_string("exec"));
    json_set(reply, "output", json_string(out));
    free(out);
    return reply;
}

json_t *w13_sessioninf_c15e44_owner_lines(const struct json_t *lines) {
    return build_exec(lines);
}

json_t *w13_sessioninf_c15e44_run_info_read(const struct json_t *name,
                                             const struct json_t *toolsets,
                                             const struct json_t *policy) {
    const char *which = jstr(name);
    json_t *lines = json_array();
    if (which && !strcmp(which, "toolsets")) {
        int has_policy = policy && policy->type == JSON_BOOL && policy->bool_val;
        if (has_policy && json_is_array(toolsets)) {
            for (size_t i = 0; i < json_len(toolsets); i++) {
                const char *t = jstr(json_get(toolsets, i));
                size_t k = t ? strlen(t) : 0;
                char *buf = (char *)malloc(k + sizeof("toolset:"));
                if (!buf) continue;
                memcpy(buf, "toolset:", 8);
                if (k) memcpy(buf + 8, t, k);
                buf[8 + k] = '\0';
                json_append(lines, json_string(buf));
                free(buf);
            }
        }
    } else if (which && !strcmp(which, "platforms")) {
        json_append(lines, json_string("platform:telegram"));
    } else {
        json_free(lines);
        json_t *err = json_object();
        json_set(err, "error", json_string("KeyError"));
        return err;
    }
    json_t *reply = build_exec(lines);
    json_free(lines);
    return reply;
}

