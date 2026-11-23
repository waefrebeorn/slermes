/*
 * blueprint_plugins.c — C port of upstream Hermes cron/blueprint_plugins.py.
 *
 * PoP: blueprintpl_text @ cron/blueprint_plugins.py:_text
 * PoP: blueprintpl_strings @ cron/blueprint_plugins.py:_strings
 * PoP: blueprintpl_placeholders @ cron/blueprint_plugins.py:_placeholders
 *
 * Each function returns {"ok":true,"value":...} or
 * {"ok":false,"error":"ValueError","message":...} exactly as the oracle wraps
 * the live function's return/raise.  Registration-time collaborators
 * (AutomationBlueprint/fill_blueprint/parse_schedule) are outside these pure
 * validators and are not exercised by this gate.
 */
#include "noc_w8_blueprintpl_9d3e14.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *xstrdup(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}
static char *xfmt(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) return xstrdup("");
    char *b = (char *)malloc((size_t)n + 1);
    if (!b) return xstrdup("");
    va_start(ap, fmt); vsnprintf(b, (size_t)n + 1, fmt, ap); va_end(ap);
    return b;
}
static char *py_strip(const char *s) {
    if (!s) return xstrdup("");
    const char *a = s, *b = s + strlen(s);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    char *r = (char *)malloc((size_t)(b - a) + 1);
    memcpy(r, a, (size_t)(b - a)); r[b - a] = '\0';
    return r;
}
static json_t *verr(const char *message) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string("ValueError"));
    json_set(o, "message", json_string(message));
    return o;
}
static json_t *vok(json_t *v) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", v);
    return o;
}
static int all_nonempty_strings(json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (!v || v->type != JSON_STRING) return 0;
        char *s = py_strip(v->str_val);
        int ok = s[0] != '\0';
        free(s);
        if (!ok) return 0;
    }
    return 1;
}

json_t *blueprintpl_text(const char *name, json_t *value) {
    if (!value || value->type != JSON_STRING) {
        return verr(xfmt("%s must be a non-empty string", name));
    }
    char *s = py_strip(value->str_val);
    if (!s[0]) {
        free(s);
        return verr(xfmt("%s must be a non-empty string", name));
    }
    json_t *r = vok(json_string(s));
    free(s);
    return r;
}

json_t *blueprintpl_strings(const char *name, json_t *value) {
    if (!value || (value->type != JSON_ARRAY && value->type != JSON_OBJECT)) {
        return verr(xfmt("%s must be a list of strings", name));
    }
    json_t *arr = json_array();
    if (value->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(value); i++)
            json_append(arr, json_copy(json_get(value, i)));
    } else {
        for (size_t i = 0; i < json_object_size(value); i++)
            json_append(arr, json_string(json_object_get_key_at(value, i)));
    }
    if (!all_nonempty_strings(arr)) {
        json_free(arr);
        return verr(xfmt("%s must be a list of non-empty strings", name));
    }
    return vok(arr);
}

/* ^[A-Za-z_]\w{0,63}$ */
static int valid_slot_name(const char *s) {
    if (!s || !s[0]) return 0;
    unsigned char c = (unsigned char)s[0];
    if (!(isalpha(c) || c == '_')) return 0;
    size_t n = strlen(s);
    if (n > 64) return 0;
    for (size_t i = 1; i < n; i++) {
        unsigned char d = (unsigned char)s[i];
        if (!(isalnum(d) || d == '_')) return 0;
    }
    return 1;
}

json_t *blueprintpl_placeholders(const char *template_str, const char *what) {
    if (!template_str) template_str = "";
    if (!what) what = "";
    json_t *names = json_array();
    size_t n = strlen(template_str), i = 0;
    while (i < n) {
        if (template_str[i] == '{') {
            if (i + 1 < n && template_str[i + 1] == '{') { i += 2; continue; }
            size_t j = i + 1;
            int depth = 0, found = 0;
            for (; j < n; j++) {
                if (template_str[j] == '{' && j + 1 < n && template_str[j + 1] == '{') {
                    j++; continue;
                }
                if (template_str[j] == '}') {
                    if (j + 1 < n && template_str[j + 1] == '}' && depth == 0) { j++; continue; }
                    if (depth == 0) { found = 1; break; }
                    depth--; continue;
                }
                if (template_str[j] == '{') { depth++; continue; }
            }
            if (!found) {
                json_free(names);
                return verr("Single '{' encountered in format string");
            }
            size_t e = j;
            for (size_t k = i + 1; k < j; k++) {
                if (template_str[k] == '!' || template_str[k] == ':') { e = k; break; }
            }
            size_t len = (e > i + 1) ? (e - i - 1) : 0;
            char *name = (char *)malloc(len + 1);
            memcpy(name, template_str + i + 1, len);
            name[len] = '\0';
            if (!valid_slot_name(name)) {
                char *msg = xfmt("%s placeholder {%s} must be a plain slot name", what, name);
                free(name); json_free(names);
                json_t *r = verr(msg); free(msg);
                return r;
            }
            json_append(names, json_string(name));
            free(name);
            i = j + 1;
        } else if (template_str[i] == '}') {
            if (i + 1 < n && template_str[i + 1] == '}') { i += 2; continue; }
            json_free(names);
            return verr("Single '}' encountered in format string");
        } else {
            i++;
        }
    }
    return vok(names);
}
