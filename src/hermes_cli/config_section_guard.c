/*
 * config_section_guard.c -- real C11 port of the `hermes config set` section
 * guard of upstream Hermes hermes_cli/config_section_guard.py.
 *
 * PoP: w21_configsect_61e00a_guard @ hermes_cli/config_section_guard.py:_guard_section_overwrite
 *
 * The live function's whole observable contract is (returned key, stdout,
 * stderr, exit code), so the port reproduces all four. The only seam is the
 * CLI sink `hermes_cli.config._exit_invalid` (print to stderr, sys.exit(1));
 * the port folds it into the result instead of terminating the process.
 */
#include "hermes_json.h"
#include "noc_w21_configsect_61e00a.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char buf[8192];
    size_t len;
} csg_buf;

static void csg_init(csg_buf *b) {
    b->len = 0;
    b->buf[0] = '\0';
}

static void csg_add(csg_buf *b, const char *s) {
    size_t n;
    if (!s) return;
    n = strlen(s);
    if (b->len + n + 1 >= sizeof(b->buf)) n = sizeof(b->buf) - b->len - 1;
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void csg_addf(csg_buf *b, const char *fmt, ...) {
    char tmp[4096];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof(tmp)) tmp[sizeof(tmp) - 1] = '\0';
    csg_add(b, tmp);
}

static int csg_is_py_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* --- Python repr ------------------------------------------------------------ */

static void csg_repr_str(csg_buf *b, const char *s) {
    char quote = '\'';
    const char *p;
    if (!s) s = "";
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    csg_addf(b, "%c", quote);
    for (p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == (unsigned char)quote || c == '\\') {
            csg_addf(b, "\\%c", c);
        } else if (c == '\n') {
            csg_add(b, "\\n");
        } else if (c == '\r') {
            csg_add(b, "\\r");
        } else if (c == '\t') {
            csg_add(b, "\\t");
        } else if (c < 0x20 || c == 0x7f) {
            csg_addf(b, "\\x%02x", (unsigned)c);
        } else {
            csg_addf(b, "%c", c);
        }
    }
    csg_addf(b, "%c", quote);
}

static void csg_repr(csg_buf *b, const json_t *v) {
    size_t i;
    if (!v || v->type == JSON_NULL) { csg_add(b, "None"); return; }
    switch (v->type) {
    case JSON_BOOL:
        csg_add(b, v->bool_val ? "True" : "False");
        return;
    case JSON_NUMBER:
        if (csg_is_py_int(v)) csg_addf(b, "%lld", (long long)v->num_val);
        else csg_addf(b, "%.17g", v->num_val);
        return;
    case JSON_STRING:
        csg_repr_str(b, v->str_val);
        return;
    case JSON_ARRAY:
        csg_add(b, "[");
        for (i = 0; i < v->c.count; i++) {
            if (i) csg_add(b, ", ");
            csg_repr(b, v->c.items[i]);
        }
        csg_add(b, "]");
        return;
    case JSON_OBJECT:
        csg_add(b, "{");
        for (i = 0; i < v->c.count; i++) {
            if (i) csg_add(b, ", ");
            csg_repr_str(b, v->c.keys[i]);
            csg_add(b, ": ");
            csg_repr(b, v->c.items[i]);
        }
        csg_add(b, "}");
        return;
    }
}

/* --- result shapers --------------------------------------------------------- */

static json_t *csg_result(const char *status, const char *key, int code,
                          const csg_buf *out, const csg_buf *err) {
    json_t *res = json_object();
    json_set(res, "status", json_string(status));
    if (!strcmp(status, "ok")) json_set(res, "key", json_string(key ? key : ""));
    else json_set(res, "code", json_number((double)code));
    json_set(res, "stdout", json_string(out ? out->buf : ""));
    json_set(res, "stderr", json_string(err ? err->buf : ""));
    return res;
}

static const char *const CSG_MODEL_REFUSAL_TAIL =
    "the bare 'model' shorthand takes a model id, and a container value has no "
    "slot there.\n"
    "  Set the section's keys individually instead:\n"
    "    hermes config set model.provider <provider>\n"
    "    hermes config set model.default <model-id>\n"
    "  Or replace the whole section deliberately:\n"
    "    hermes config set --force model '{provider: <provider>, default: <model-id>}'";

json_t *w21_configsect_61e00a_guard(const char *key, const json_t *value,
                                    const json_t *user_config, int force) {
    csg_buf out, err;
    const json_t *existing;
    int value_is_map, value_is_list;
    const char *kind;

    csg_init(&out);
    csg_init(&err);
    if (!key) key = "";

    existing = json_obj_get(user_config, key);
    value_is_map = value && value->type == JSON_OBJECT;
    value_is_list = value && value->type == JSON_ARRAY;
    kind = value_is_map ? "mapping" : "list";

    /* Containers under the model-id shorthand have no reader slot (#131435). */
    if (!strcmp(key, "model") && !force &&
        (value_is_list || (value_is_map && existing && existing->type == JSON_OBJECT))) {
        csg_add(&err, "\xe2\x9c\x97 Cannot set 'model' to a ");
        csg_add(&err, kind);
        csg_add(&err, " \xe2\x80\x94 ");
        csg_add(&err, CSG_MODEL_REFUSAL_TAIL);
        csg_add(&err, "\n");
        return csg_result("exit", NULL, 1, &out, &err);
    }
    if (strchr(key, '.') || !(existing && existing->type == JSON_OBJECT)) {
        return csg_result("ok", key, 0, &out, &err);
    }
    if (!strcmp(key, "model")) {
        if (force) {
            csg_add(&out, "\xe2\x9a\xa0 Replacing entire 'model' section with ");
            if (value_is_map || value_is_list) {
                csg_add(&out, "the given ");
                csg_add(&out, kind);
            } else {
                csg_add(&out, "a scalar");
            }
            csg_addf(&out, " (discarding %llu existing sub-key(s))\n",
                     (unsigned long long)existing->c.count);
            return csg_result("ok", key, 0, &out, &err);
        }
        csg_addf(&out, "\xe2\x9c\x93 Redirecting bare 'model' to 'model.default' "
                       "(preserving %llu existing model sub-key(s))\n",
                 (unsigned long long)existing->c.count);
        return csg_result("ok", "model.default", 0, &out, &err);
    }
    if (force) return csg_result("ok", key, 0, &out, &err);

    csg_addf(&err, "\xe2\x9c\x97 Cannot set '%s' to a scalar \xe2\x80\x94 '%s' is a "
                   "configuration section with %llu sub-key(s).",
             key, key, (unsigned long long)existing->c.count);
    if (existing->c.count) {
        size_t i, shown = existing->c.count > 8 ? 8 : existing->c.count;
        csg_add(&err, "\n  Sub-keys: ");
        for (i = 0; i < shown; i++) {
            if (i) csg_add(&err, ", ");
            csg_add(&err, existing->c.keys[i] ? existing->c.keys[i] : "");
        }
        if (existing->c.count > 8) {
            csg_addf(&err, "\n  ... and %llu more", (unsigned long long)(existing->c.count - 8));
        }
    }
    csg_add(&err, "\n  Use a dotted path to set a specific leaf key:");
    csg_addf(&err, "\n    hermes config set %s.<sub-key> <value>", key);
    csg_add(&err, "\n  Or use --force to replace the entire section:");
    csg_addf(&err, "\n    hermes config set --force %s ", key);
    csg_repr(&err, value);
    csg_add(&err, "\n");
    return csg_result("exit", NULL, 1, &out, &err);
}
