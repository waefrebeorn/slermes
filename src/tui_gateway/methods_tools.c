/*
 * methods_tools.c — C port of upstream Hermes tui_gateway/methods_tools.py.
 *
 * PoP: w13_methodstoo_ab6609_str_arg @ tui_gateway/methods_tools.py:_str_arg
 * PoP: w13_methodstoo_ab6609_joined_output @ tui_gateway/methods_tools.py:_joined_output
 * PoP: w13_methodstoo_ab6609_is_snapshot_restore @ tui_gateway/methods_tools.py:_is_snapshot_restore
 */
#include "noc_w13_methodstoo_ab6609.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *trim_copy(const char *s, char *buf, size_t n) {
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t len = (size_t)(e - s);
    if (len >= n) len = n - 1;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

static int py_bool(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

static const char *py_str(const struct json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return buf; }
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_BOOL) { snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); return buf; }
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (x == (double)(long long)x) snprintf(buf, n, "%lld", (long long)x);
        else snprintf(buf, n, "%g", x);
        return buf;
    }
    snprintf(buf, n, "");
    return buf;
}

struct json_t *w13_methodstoo_ab6609_str_arg(const struct json_t *params, const char *key) {
    char raw[8192], out[8192];
    const struct json_t *v = params ? json_obj_get(params, key) : NULL;
    if (!py_bool(v)) return json_string("");
    const char *s = trim_copy(py_str(v, raw, sizeof raw), out, sizeof out);
    return json_string(s);
}

struct json_t *w13_methodstoo_ab6609_joined_output(const struct json_t *stdout_v,
                                                   const struct json_t *stderr_v) {
    char joined[16384];
    size_t n = 0;
    joined[0] = '\0';
    const struct json_t *parts[2] = {stdout_v, stderr_v};
    for (int i = 0; i < 2; i++) {
        if (!py_bool(parts[i])) continue;
        char tmp[8192];
        const char *s = py_str(parts[i], tmp, sizeof tmp);
        size_t len = strlen(s);
        if (n + len + 2 > sizeof joined) break;
        if (n) { joined[n++] = '\n'; joined[n] = '\0'; }
        memcpy(joined + n, s, len);
        n += len;
        joined[n] = '\0';
    }
    char out[16384];
    return json_string(trim_copy(joined, out, sizeof out));
}

int w13_methodstoo_ab6609_is_snapshot_restore(const struct json_t *arg) {
    if (!py_bool(arg)) return 0;
    const char *s = (arg->type == JSON_STRING) ? arg->str_val : "";
    while (*s && isspace((unsigned char)*s)) s++;
    char tok[64];
    size_t n = 0;
    while (*s && !isspace((unsigned char)*s) && n + 1 < sizeof tok) {
        tok[n++] = (char)tolower((unsigned char)*s);
        s++;
    }
    tok[n] = '\0';
    return (!strcmp(tok, "restore") || !strcmp(tok, "rewind")) ? 1 : 0;
}
