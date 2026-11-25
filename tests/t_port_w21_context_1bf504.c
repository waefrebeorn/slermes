/* JSON-lines harness for the w21_context_1bf504 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * Fixture paths may start with "$HOME" or "$SLERMES_HOME"; they are expanded
 * from the isolated environment this harness is run in, and the fixture's two
 * directories are created here so an "existing folder" case really exists
 * (the oracle builds the same two before it resolves anything).
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "noc_w21_context_1bf504.h"
#include "persistence/core/context.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* "$HOME/prof" -> the isolated temp home; anything else is used as given. */
static void expand_placeholders(const char *in, char *out, size_t cap) {
    const char *home = getenv("HOME");
    const char *rest = in;
    const char *sub = NULL;
    if (!home) home = "";
    if (!strncmp(in, "$SLERMES_HOME", 13)) {
        const char *sh = getenv("SLERMES_HOME");
        if (sh && sh[0]) home = sh;
        sub = in + 13;
    } else if (!strncmp(in, "$HOME", 5)) {
        sub = in + 5;
    }
    if (!sub) {
        snprintf(out, cap, "%s", rest);
        return;
    }
    snprintf(out, cap, "%s%s", home, sub);
}

static const char *arg_str(json_t *in, const char *key, char *buf, size_t cap) {
    json_t *v = json_obj_get(in, key);
    char *text;
    if (!v) { buf[0] = '\0'; return buf; }
    text = ctx1504_field_text(v);
    expand_placeholders(text ? text : "", buf, cap);
    free(text);
    return buf;
}

/* A field whose Python truthiness decides a ValueError: null, false, 0 and ""
 * are falsy, so they reach the port as the empty string it rejects. */
static const char *arg_scope_str(json_t *in, const char *key, char *buf, size_t cap) {
    json_t *v = json_obj_get(in, key);
    char *text;
    if (!v) { buf[0] = '\0'; return buf; }
    if (v->type == JSON_NULL || (v->type == JSON_BOOL && !v->bool_val) ||
        (v->type == JSON_NUMBER && v->num_val == 0.0) ||
        (v->type == JSON_STRING && (!v->str_val || !v->str_val[0]))) {
        buf[0] = '\0';
        return buf;
    }
    text = ctx1504_field_text(v);
    expand_placeholders(text ? text : "", buf, cap);
    free(text);
    return buf;
}

/* A label field: Python truthiness decides the ValueError, and a label is
 * never a path, so no placeholder expansion happens here. */
static const char *arg_name_str(json_t *in, const char *key, char *buf, size_t cap) {
    json_t *v = json_obj_get(in, key);
    char *text;
    if (!v) { buf[0] = '\0'; return buf; }
    if (v->type == JSON_NULL || (v->type == JSON_BOOL && !v->bool_val) ||
        (v->type == JSON_NUMBER && v->num_val == 0.0) ||
        (v->type == JSON_STRING && (!v->str_val || !v->str_val[0]))) {
        buf[0] = '\0';
        return buf;
    }
    text = ctx1504_field_text(v);
    snprintf(buf, cap, "%s", text ? text : "");
    free(text);
    return buf;
}

/* The two folders every path case is allowed to lean on. */
static void ensure_fixture_dirs(void) {
    const char *home = getenv("HOME");
    char path[4096];
    if (!home || !home[0]) return;
    snprintf(path, sizeof(path), "%s/prof", home);
    mkdir(path);
    snprintf(path, sizeof(path), "%s/prof/sub", home);
    mkdir(path);
}

int main(void) {
    char line[65536];
    char a[4096], b[4096], c[4096], d[4096];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    ensure_fixture_dirs();
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "expand_user")) {
            char *s = w21_context_1bf504_expand_user(arg_str(in, "path", a, sizeof(a)));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "canonical_dir")) {
            char *s = w21_context_1bf504_canonical_dir(arg_str(in, "path", a, sizeof(a)));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "canonical_home_key")) {
            char *s = w21_context_1bf504_canonical_home_key(arg_str(in, "path", a, sizeof(a)));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "scope")) {
            out = w21_context_1bf504_scope(arg_scope_str(in, "profile_dir", a, sizeof(a)),
                                           arg_name_str(in, "name", b, sizeof(b)));
        } else if (!strcmp(func, "scope_equal")) {
            out = w21_context_1bf504_scope_equal(arg_scope_str(in, "dir_a", a, sizeof(a)),
                                                 arg_name_str(in, "name_a", b, sizeof(b)),
                                                 arg_scope_str(in, "dir_b", c, sizeof(c)),
                                                 arg_name_str(in, "name_b", d, sizeof(d)));
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
