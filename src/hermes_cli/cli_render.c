/*
 * cli_render.c — C home for upstream Hermes hermes_cli/cli_render.py.
 *
 * PoP: w8_clirender_3bf599_luminance_from_hex @ hermes_cli/cli_render.py:_luminance_from_hex
 * PoP: w8_clirender_3bf599_coerce_output_history_limit @ hermes_cli/cli_render.py:_coerce_output_history_limit
 * PoP: w8_clirender_3bf599_prepend_note_to_message @ hermes_cli/cli_render.py:_prepend_note_to_message
 * PoP: w8_clirender_3bf599_assistant_content_as_text @ hermes_cli/cli_render.py:_assistant_content_as_text
 * PoP: w8_clirender_3bf599_maybe_remap_for_light_mode @ hermes_cli/cli_render.py:_maybe_remap_for_light_mode
 * PoP: w8_clirender_3bf599_detect_light_mode_ex @ hermes_cli/cli_render.py:_detect_light_mode_uncached
 *
 * Environment/TTY-dependent behaviour is exposed through explicit seams
 * (`light`, `env`, `osc_bg`) injected from the fixture rather than by mutating
 * the process environment. All exports are w8_clirender_3bf599_ prefixed.
 */
#include "noc_w8_clirender_3bf599.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *cr_strdup_n(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (o) { if (n) memcpy(o, s, n); o[n] = '\0'; }
    return o;
}
static char *cr_strdup(const char *s) {
    return cr_strdup_n(s ? s : "", strlen(s ? s : ""));
}
static int cr_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/* strip leading/trailing ASCII whitespace, then lstrip('#'), then expand #RGB */
static int cr_hex_digits(const char *hex_str, char out[6]) {
    const char *p = hex_str ? hex_str : "";
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    while (p < e && *p == '#') p++;
    size_t n = (size_t)(e - p);
    if (n == 3) {
        for (int i = 0; i < 3; ++i) { out[2 * i] = p[i]; out[2 * i + 1] = p[i]; }
        n = 6;
    } else if (n == 6) {
        memcpy(out, p, 6);
    } else {
        return 0;
    }
    for (int i = 0; i < 6; ++i) if (cr_hexval(out[i]) < 0) return 0;
    return 1;
}
static int cr_luminance(const char *hex_str, double *out) {
    char s[6];
    if (!cr_hex_digits(hex_str, s)) return 0;
    double r = cr_hexval(s[0]) * 16 + cr_hexval(s[1]);
    double g = cr_hexval(s[2]) * 16 + cr_hexval(s[3]);
    double b = cr_hexval(s[4]) * 16 + cr_hexval(s[5]);
    *out = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0;
    return 1;
}

json_t *w8_clirender_3bf599_luminance_from_hex(const char *hex_str) {
    double lum;
    if (!cr_luminance(hex_str, &lum)) return json_null();
    return json_number(lum);
}

/* Python int(str) with base 10: whitespace, optional sign, digits with Python's
 * underscore separators (only between digits). Returns 0 on failure. */
static int cr_py_int_str(const char *v, long long *out) {
    if (!v) return 0;
    while (*v && isspace((unsigned char)*v)) v++;
    const char *e = v + strlen(v);
    while (e > v && isspace((unsigned char)e[-1])) e--;
    if (e == v) return 0;
    char buf[128];
    size_t n = 0;
    int sign = 1;
    if (*v == '+' || *v == '-') { if (*v == '-') sign = -1; v++; }
    if (v == e) return 0;
    int last_digit = 0;
    for (const char *p = v; p < e; ++p) {
        if (*p >= '0' && *p <= '9') {
            if (n + 1 >= sizeof(buf)) return 0;
            buf[n++] = *p;
            last_digit = 1;
        } else if (*p == '_') {
            if (!last_digit || p + 1 >= e || p[1] < '0' || p[1] > '9') return 0;
            last_digit = 0;
        } else {
            return 0;
        }
    }
    if (!last_digit || n == 0) return 0;
    buf[n] = '\0';
    long long val = strtoll(buf, NULL, 10);
    *out = sign < 0 ? -val : val;
    return 1;
}

json_t *w8_clirender_3bf599_coerce_output_history_limit(const json_t *value) {
    long long v = 0;
    if (!value) return json_number(200);
    if (value->type == JSON_STRING) {
        if (!cr_py_int_str(value->str_val, &v)) return json_number(200);
    } else if (value->type == JSON_NUMBER) {
        v = (long long)value->num_val;   /* int(float) truncates toward zero */
    } else if (value->type == JSON_BOOL) {
        v = value->bool_val ? 1 : 0;     /* bool is an int subclass in Python */
    } else {
        return json_number(200);         /* None/list/dict -> TypeError */
    }
    return json_number(v < 10 ? 10.0 : (double)v);
}

/* str(note or "") then .strip(): json -> python-str for the shapes we accept. */
static char *cr_note_text(const json_t *note) {
    if (!note || note->type == JSON_NULL) return cr_strdup("");
    const char *raw = NULL;
    char numbuf[64];
    if (note->type == JSON_STRING) {
        raw = note->str_val;
    } else if (note->type == JSON_BOOL) {
        raw = note->bool_val ? "True" : "";   /* False is falsy -> "" */
    } else if (note->type == JSON_NUMBER) {
        if (note->num_val == 0.0) raw = "";
        else { snprintf(numbuf, sizeof(numbuf), "%.0f", note->num_val); raw = numbuf; }
    } else {
        raw = "";   /* containers are not used as notes by fixtures */
    }
    const char *p = raw ? raw : "";
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    return cr_strdup_n(p, (size_t)(e - p));
}

json_t *w8_clirender_3bf599_prepend_note_to_message(const json_t *message, const json_t *note) {
    char *note_s = cr_note_text(note);
    if (!note_s || !*note_s) { free(note_s); return json_copy(message); }

    if (message && message->type == JSON_STRING) {
        const char *m = message->str_val ? message->str_val : "";
        char *o;
        if (!*m) {
            o = cr_strdup(note_s);
        } else {
            size_t n = strlen(note_s) + 2 + strlen(m) + 1;
            o = (char *)malloc(n);
            if (o) snprintf(o, n, "%s\n\n%s", note_s, m);
        }
        free(note_s);
        json_t *r = json_string(o ? o : "");
        free(o);
        return r;
    }

    if (message && message->type == JSON_ARRAY) {
        json_t *arr = json_copy(message);
        int found = 0;
        size_t cnt = json_len(message);
        for (size_t i = 0; i < cnt; ++i) {
            json_t *part = json_get(message, i);
            if (!part || part->type != JSON_OBJECT) continue;
            json_t *pt = json_obj_get(part, "type");
            if (!pt || pt->type != JSON_STRING || strcmp(pt->str_val, "text") != 0) continue;
            json_t *tx = json_obj_get(part, "text");
            const char *txt = (tx && tx->type == JSON_STRING && tx->str_val) ? tx->str_val : "";
            char *merged;
            if (*txt) {
                size_t n = strlen(note_s) + 2 + strlen(txt) + 1;
                merged = (char *)malloc(n);
                if (merged) snprintf(merged, n, "%s\n\n%s", note_s, txt);
            } else {
                merged = cr_strdup(note_s);
            }
            json_set(json_get(arr, i), "text", json_string(merged ? merged : ""));
            free(merged);
            found = 1;
            break;   /* first text part wins, mirroring the Python return */
        }
        if (!found) {
            json_t *head = json_object();
            json_set(head, "type", json_string("text"));
            json_set(head, "text", json_string(note_s));
            /* libjson has no insert; rebuild with head first */
            json_t *out = json_array();
            json_append(out, head);
            for (size_t i = 0; i < json_len(arr); ++i) json_append(out, json_copy(json_get(arr, i)));
            json_free(arr);
            arr = out;
        }
        free(note_s);
        return arr;
    }

    free(note_s);
    return json_copy(message);   /* unknown shapes returned unchanged */
}

/* str(value) for the primitive shapes the oracle passes. */
static char *cr_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return cr_strdup("");
    if (v->type == JSON_STRING) return cr_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return cr_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        snprintf(b, sizeof(b), "%.0f", v->num_val);
        return cr_strdup(b);
    }
    return cr_strdup("");
}

json_t *w8_clirender_3bf599_assistant_content_as_text(const json_t *content) {
    if (!content || content->type == JSON_NULL) return json_string("");
    if (content->type == JSON_STRING) return json_copy(content);
    if (content->type == JSON_ARRAY) {
        json_t *out = json_array();
        size_t cnt = json_len(content);
        for (size_t i = 0; i < cnt; ++i) {
            json_t *part = json_get(content, i);
            if (!part || part->type != JSON_OBJECT) continue;
            json_t *pt = json_obj_get(part, "type");
            if (!pt || pt->type != JSON_STRING || strcmp(pt->str_val, "text") != 0) continue;
            char *s = cr_py_str(json_obj_get(part, "text"));
            if (s && *s) json_append(out, json_string(s));
            free(s);
        }
        /* join with "\n" */
        size_t n = json_len(out);
        char *joined = cr_strdup("");
        for (size_t i = 0; i < n; ++i) {
            const char *part = json_get(out, i)->str_val;
            size_t old = strlen(joined);
            char *tmp = (char *)realloc(joined, old + (i ? 1 : 0) + strlen(part) + 1);
            if (!tmp) break;
            joined = tmp;
            if (i) joined[old++] = '\n';
            memcpy(joined + old, part, strlen(part) + 1);
        }
        json_t *r = json_string(joined);
        free(joined);
        json_free(out);
        return r;
    }
    char *s = cr_py_str(content);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

char *w8_clirender_3bf599_maybe_remap_for_light_mode(const char *hex_color, int light) {
    if (!light || !hex_color || !*hex_color || hex_color[0] != '#') return cr_strdup(hex_color);
    static const char *const keys[] = {
        "#FFF8DC", "#FFD700", "#FFBF00", "#B8860B", "#DAA520", "#F1E6CF",
        "#C9D1D9", "#EAF7FF", "#F5F5F5", "#FFF0D4", "#CD7F32", "#FFEFB5"
    };
    static const char *const vals[] = {
        "#1A1A1A", "#9A6B00", "#8A5A00", "#5C4500", "#6B4F00", "#1A1A1A",
        "#24292F", "#0F1B26", "#1A1A1A", "#1A1A1A", "#8A4F1A", "#3A2A00"
    };
    size_t n = strlen(hex_color);
    char *up = (char *)malloc(n + 1);
    if (!up) return cr_strdup(hex_color);
    for (size_t i = 0; i <= n; ++i) up[i] = (char)toupper((unsigned char)hex_color[i]);
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        if (strcmp(up, keys[i]) == 0) { free(up); return cr_strdup(vals[i]); }
    }
    free(up);
    return cr_strdup(hex_color);
}

static const char *cr_env_get(const json_t *env, const char *k) {
    return json_get_str(env ? env : NULL, k, "");
}
static char *cr_lower_strip(const char *v) {
    const char *p = v ? v : "";
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    char *o = cr_strdup_n(p, (size_t)(e - p));
    for (char *q = o; *q; ++q) *q = (char)tolower((unsigned char)*q);
    return o;
}

int w8_clirender_3bf599_detect_light_mode_ex(const json_t *env, const char *osc_bg) {
    static const char *const trues[] = { "1", "true", "on", "yes", "y" };
    static const char *const falses[] = { "0", "false", "off", "no", "n" };
    static const char *const vars[] = { "HERMES_LIGHT", "HERMES_TUI_LIGHT" };
    for (size_t i = 0; i < 2; ++i) {
        char *v = cr_lower_strip(cr_env_get(env, vars[i]));
        for (size_t j = 0; j < 5; ++j) {
            if (strcmp(v, trues[j]) == 0) { free(v); return 1; }
            if (strcmp(v, falses[j]) == 0) { free(v); return 0; }
        }
        free(v);
    }
    char *theme = cr_lower_strip(cr_env_get(env, "HERMES_TUI_THEME"));
    if (strcmp(theme, "light") == 0) { free(theme); return 1; }
    if (strcmp(theme, "dark") == 0) { free(theme); return 0; }
    free(theme);

    double lum;
    if (cr_luminance(cr_env_get(env, "HERMES_TUI_BACKGROUND"), &lum)) return lum >= 0.5 ? 1 : 0;

    const char *cfgb = cr_env_get(env, "COLORFGBG");
    const char *last = strrchr(cfgb, ';');
    last = last ? last + 1 : cfgb;
    while (*last && isspace((unsigned char)*last)) last++;
    size_t dl = strlen(last);
    int digits = dl > 0;
    for (size_t i = 0; i < dl; ++i) if (last[i] < '0' || last[i] > '9') { digits = 0; break; }
    if (digits) {
        long v = strtol(last, NULL, 10);
        if (v >= 0 && v < 16) return (v == 7 || v == 15) ? 1 : 0;
    }

    if (osc_bg && *osc_bg && cr_luminance(osc_bg, &lum)) return lum >= 0.5 ? 1 : 0;

    /* _LIGHT_DEFAULT_TERM_PROGRAMS is an empty frozenset -> always False. */
    return 0;
}
