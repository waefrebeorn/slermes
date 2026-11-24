/*
 * update_cmd_config.c — C port of upstream Hermes hermes_cli/update_cmd_config.py.
 *
 * PoP: w16_updatecmd_9c2684_print_items @ hermes_cli/update_cmd_config.py:_print_items
 * PoP: w16_updatecmd_9c2684_ask_configure_new_options @ hermes_cli/update_cmd_config.py:_ask_configure_new_options
 *
 * The two Python helpers are side-effecting (print/input). Their observable
 * results are exposed as pure seams: print_items returns the exact lines that
 * Python would print, and ask_configure_new_options takes the interactive
 * answers as injected fixture values instead of touching stdin/stdout.
 */
#include "noc_w16_updatecmd_9c2684.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}

/* Python str() for the scalar kinds the fixtures use. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d && d > -9e15 && d < 9e15)
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else
            snprintf(buf, sizeof(buf), "%.17g", d);
        return dupstr(buf);
    }
    return dupstr("");
}

static char *strip_dup(const char *s) {
    if (!s) return dupstr("");
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, a, n); p[n] = '\0'; }
    return p;
}

static char *lower_strip_dup(const char *s) {
    char *p = strip_dup(s);
    for (char *q = p; q && *q; q++) *q = (char)tolower((unsigned char)*q);
    return p;
}

static char *fmt_line(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return dupstr(""); }
    char *b = (char *)malloc((size_t)n + 1);
    if (!b) { va_end(ap2); return dupstr(""); }
    vsnprintf(b, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return b;
}

struct json_t *w16_updatecmd_9c2684_print_items(const json_t *items,
                                                const char *label,
                                                const char *key,
                                                const char *fallback_key) {
    struct json_t *out = json_array();
    if (!json_is_array(items) || json_len(items) == 0) return out;  /* Python: if not items: return */
    if (!label) label = "";
    if (!key) key = "";
    char *line = fmt_line("  %s:", label);
    json_append(out, json_string(line));
    free(line);

    size_t n = json_len(items);
    size_t shown = n < 8 ? n : 8;
    for (size_t i = 0; i < shown; i++) {
        const json_t *it = json_get(items, i);
        char *name = NULL, *desc = NULL;
        if (json_is_object(it)) {
            const json_t *nv = json_obj_get(it, key);
            if (!json_truthy(nv) && fallback_key) {
                const json_t *fv = json_obj_get(it, fallback_key);
                if (json_truthy(fv)) nv = fv;
            }
            name = json_truthy(nv) ? py_str(nv) : dupstr("?");
            const json_t *dv = json_obj_get(it, "description");
            if (json_truthy(dv)) {
                char *raw = py_str(dv);
                desc = strip_dup(raw);
                free(raw);
            } else {
                desc = dupstr("");
            }
        } else {
            name = py_str(it);
            desc = dupstr("");
        }
        line = desc[0]
            ? fmt_line("      \xe2\x80\xa2 %s \xe2\x80\x94 %s", name, desc)
            : fmt_line("      \xe2\x80\xa2 %s", name);
        json_append(out, json_string(line));
        free(line);
        free(name);
        free(desc);
    }
    if (n > 8) {
        line = fmt_line("      \xe2\x80\xa6 and %d more", (int)(n - 8));
        json_append(out, json_string(line));
        free(line);
    }
    return out;
}

struct json_t *w16_updatecmd_9c2684_ask_configure_new_options(
    bool assume_yes, bool gateway_mode, const char *gateway_reply,
    const char *input_reply, bool is_interactive) {
    if (assume_yes) return json_string("y");
    if (gateway_mode) {
        char *a = lower_strip_dup(gateway_reply);
        struct json_t *r = json_string(a);
        free(a);
        return r;
    }
    if (!is_interactive) return json_string("auto");
    if (!input_reply || !strcmp(input_reply, "__UDE__")) return json_string("n");
    char *a = lower_strip_dup(input_reply);
    struct json_t *r = json_string(a);
    free(a);
    return r;
}
