/*
 * mcp_elicitation_form.c — C port of upstream Hermes tools/mcp_elicitation_form.py.
 *
 * PoP: w14_mcpelicita_0beae8_options @ tools/mcp_elicitation_form.py:_options
 * PoP: w14_mcpelicita_0beae8_field_kind @ tools/mcp_elicitation_form.py:_field_kind
 * PoP: w14_mcpelicita_0beae8_match_option @ tools/mcp_elicitation_form.py:_match_option
 * PoP: w14_mcpelicita_0beae8_coerce_boolean @ tools/mcp_elicitation_form.py:_coerce_boolean
 * PoP: w14_mcpelicita_0beae8_format_error @ tools/mcp_elicitation_form.py:_format_error
 * PoP: w14_mcpelicita_0beae8_coerce_string @ tools/mcp_elicitation_form.py:_coerce_string
 * PoP: w14_mcpelicita_0beae8_coerce_number @ tools/mcp_elicitation_form.py:_coerce_number
 * PoP: w14_mcpelicita_0beae8_coerce_multi @ tools/mcp_elicitation_form.py:_coerce_multi
 * PoP: w14_mcpelicita_0beae8_coerce_answer @ tools/mcp_elicitation_form.py:coerce_answer
 */
#include "noc_w14_mcpelicita_0beae8.h"
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    struct json_t *value;      /* borrowed pointer into spec */
    char label[1024];
} w14_opt;

/* Python str() for the JSON scalars used by form metadata. */
static void py_str(const struct json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return; }
    switch (v->type) {
        case JSON_STRING: snprintf(buf, n, "%s", v->str_val); break;
        case JSON_BOOL:   snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val &&
                v->num_val >= -9007199254740992.0 && v->num_val <= 9007199254740992.0)
                snprintf(buf, n, "%lld", (long long)v->num_val);
            else
                snprintf(buf, n, "%g", v->num_val);
            break;
        default: {
            char *s = json_dumps(v, 0);
            snprintf(buf, n, "%s", s ? s : "");
            free(s);
            break;
        }
    }
}

static int py_truthy(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

static const char *trim_copy(const char *s, char *buf, size_t n) {
    size_t len;
    while (*s && isspace((unsigned char)*s)) s++;
    len = strlen(s);
    while (len && isspace((unsigned char)s[len - 1])) len--;
    if (len >= n) len = n - 1;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

static void fold_copy(const char *s, char *buf, size_t n) {
    size_t len = strlen(s);
    if (len >= n) len = n - 1;
    for (size_t i = 0; i < len; i++) buf[i] = (char)tolower((unsigned char)s[i]);
    buf[len] = '\0';
}

static void opt_pair(struct json_t *arr, const struct json_t *value, const char *label) {
    struct json_t *pair = json_array();
    json_append(pair, json_copy(value));
    json_append(pair, json_string(label));
    json_append(arr, pair);
}

/* Faithful _options(): enum/enumNames or titled oneOf/anyOf const lists. */
static size_t build_options(const struct json_t *spec, w14_opt *opts, size_t cap) {
    const struct json_t *values, *names, *titled;
    size_t n = 0;
    if (!json_is_object(spec)) return 0;

    values = json_obj_get(spec, "enum");
    if (json_is_array(values)) {
        names = json_obj_get(spec, "enumNames");
        for (size_t i = 0; i < json_len(values) && n < cap; i++) {
            const struct json_t *v = json_get(values, i);
            if (!v) continue;
            if (json_is_array(names) && i < json_len(names))
                py_str(json_get(names, i), opts[n].label, sizeof opts[n].label);
            else
                py_str(v, opts[n].label, sizeof opts[n].label);
            opts[n].value = v;
            n++;
        }
        return n;
    }

    titled = json_obj_get(spec, "oneOf");
    if (!py_truthy(titled)) titled = json_obj_get(spec, "anyOf");
    if (!py_truthy(titled) || !json_is_array(titled)) return 0;
    for (size_t i = 0; i < json_len(titled) && n < cap; i++) {
        const struct json_t *c = json_get(titled, i);
        const struct json_t *cv, *title;
        if (!json_is_object(c)) continue;
        cv = json_obj_get(c, "const");
        if (!cv) continue;
        title = json_obj_get(c, "title");
        if (py_truthy(title))
            py_str(title, opts[n].label, sizeof opts[n].label);
        else
            py_str(cv, opts[n].label, sizeof opts[n].label);
        opts[n].value = cv;
        n++;
    }
    return n;
}

static const char *kind_of(const struct json_t *spec, char *buf, size_t n) {
    const char *t = json_get_str(spec, "type", "");
    w14_opt opts[256];
    size_t count;
    if (!strcmp(t, "array")) return "multi";
    count = build_options(spec, opts, 256);
    if (count) return "enum";
    if (!strcmp(t, "number") || !strcmp(t, "integer") || !strcmp(t, "boolean")) {
        snprintf(buf, n, "%s", t);
        return buf;
    }
    return "string";
}

static const struct json_t *options_source(const struct json_t *spec) {
    char kbuf[64];
    const char *k = kind_of(spec, kbuf, sizeof kbuf);
    if (!strcmp(k, "multi")) {
        const struct json_t *items = json_obj_get(spec, "items");
        return json_is_object(items) ? items : NULL;
    }
    return spec;
}

struct json_t *w14_mcpelicita_0beae8_options(const struct json_t *spec) {
    w14_opt opts[256];
    size_t n = build_options(options_source(spec), opts, 256);
    struct json_t *arr = json_array();
    for (size_t i = 0; i < n; i++) opt_pair(arr, opts[i].value, opts[i].label);
    return arr;
}

struct json_t *w14_mcpelicita_0beae8_field_kind(const struct json_t *spec) {
    char kbuf[64];
    const char *kind = kind_of(spec, kbuf, sizeof kbuf);
    struct json_t *out = json_array();
    json_append(out, json_string(kind));
    json_append(out, w14_mcpelicita_0beae8_options(spec));
    return out;
}

/* Returns 1 and copies the matching value when wanted matches an option. */
static int match_one(w14_opt *opts, size_t n, const char *wanted,
                     struct json_t **value) {
    char vstr[1024], wfold[1024], bfold[1024];
    struct json_t *first = NULL;
    int hits = 0;
    for (size_t i = 0; i < n; i++) {
        py_str(opts[i].value, vstr, sizeof vstr);
        if (!strcmp(wanted, opts[i].label) || !strcmp(wanted, vstr)) {
            *value = opts[i].value;
            return 1;
        }
    }
    fold_copy(wanted, wfold, sizeof wfold);
    for (size_t i = 0; i < n; i++) {
        py_str(opts[i].value, vstr, sizeof vstr);
        fold_copy(opts[i].label, bfold, sizeof bfold);
        if (!strcmp(wfold, bfold)) { first = opts[i].value; hits++; }
        fold_copy(vstr, bfold, sizeof bfold);
        if (!strcmp(wfold, bfold) && first != opts[i].value) {
            first = opts[i].value; hits++;
        }
    }
    if (hits == 1) { *value = first; return 1; }
    return 0;
}

struct json_t *w14_mcpelicita_0beae8_match_option(const struct json_t *spec,
                                                  const char *text) {
    w14_opt opts[256];
    size_t n = build_options(options_source(spec), opts, 256);
    char wanted[4096];
    struct json_t *value = NULL;
    int ok;
    struct json_t *out = json_array();
    trim_copy(text ? text : "", wanted, sizeof wanted);
    ok = match_one(opts, n, wanted, &value);
    json_append(out, json_bool(ok ? true : false));
    json_append(out, ok && value ? json_copy(value) : json_null());
    return out;
}

struct json_t *w14_mcpelicita_0beae8_coerce_boolean(const char *text) {
    static const char *yes[] = {"yes", "y", "true", "t", "1", "on", NULL};
    static const char *no[]  = {"no", "n", "false", "f", "0", "off", NULL};
    char folded[4096], msg[8192];
    struct json_t *out = json_array();
    fold_copy(text ? text : "", folded, sizeof folded);
    for (int i = 0; yes[i]; i++) if (!strcmp(folded, yes[i])) {
        json_append(out, json_bool(true));
        json_append(out, json_string(""));
        return out;
    }
    for (int i = 0; no[i]; i++) if (!strcmp(folded, no[i])) {
        json_append(out, json_bool(false));
        json_append(out, json_string(""));
        return out;
    }
    snprintf(msg, sizeof msg, "'%s' is not yes or no", text ? text : "");
    json_append(out, json_null());
    json_append(out, json_string(msg));
    return out;
}

static int check_email(const char *s) {
    const char *at = strchr(s, '@'), *dot, *p;
    if (!at || at == s || !at[1]) return 0;
    for (p = s; p < at; p++) if (isspace((unsigned char)*p)) return 0;
    dot = NULL;
    for (p = at + 1; *p; p++) {
        if (isspace((unsigned char)*p) || *p == '@') return 0;
        if (*p == '.') dot = p;
    }
    if (!dot || dot == at + 1 || !dot[1]) return 0;
    return 1;
}

static int check_uri(const char *s) {
    const char *p;
    if (!isalpha((unsigned char)s[0])) return 0;
    p = s + 1;
    while (isalnum((unsigned char)*p) || *p == '+' || *p == '.' || *p == '-') p++;
    if (*p != ':') return 0;
    p++;
    if (!*p) return 0;
    for (; *p; p++) if (isspace((unsigned char)*p)) return 0;
    return 1;
}

static int all_digits(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) if (!isdigit((unsigned char)s[i])) return 0;
    return 1;
}

static int valid_ymd(int y, int m, int d) {
    static const int mdays[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    int leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    if (m < 1 || m > 12 || d < 1) return 0;
    return d <= mdays[m] + (m == 2 && leap ? 1 : 0);
}

static int check_date_prefix(const char *s) {
    if (s[4] != '-' || s[7] != '-') return 0;
    if (!all_digits(s, 4) || !all_digits(s + 5, 2) || !all_digits(s + 8, 2)) return 0;
    return valid_ymd(atoi(s), atoi(s + 5), atoi(s + 8));
}

static int check_date(const char *s) {
    return strlen(s) == 10 && check_date_prefix(s);
}

static int check_datetime(const char *s) {
    const char *p;
    int hh, mm, ss;
    if (strlen(s) < 20 || !check_date_prefix(s)) return 0;
    if (s[10] != 'T' && s[10] != 't' && s[10] != ' ') return 0;
    if (s[13] != ':' || s[16] != ':') return 0;
    if (!all_digits(s + 11, 2) || !all_digits(s + 14, 2) || !all_digits(s + 17, 2)) return 0;
    hh = atoi(s + 11); mm = atoi(s + 14); ss = atoi(s + 17);
    if (hh > 23 || mm > 59 || ss > 59) return 0;
    p = s + 19;
    if (*p == '.') {
        p++;
        if (!isdigit((unsigned char)*p)) return 0;
        while (isdigit((unsigned char)*p)) p++;
    }
    if (*p == 'Z' || *p == 'z') { p++; return *p == '\0'; }
    if (*p == '+' || *p == '-') {
        p++;
        if (!all_digits(p, 2) || p[2] != ':') return 0;
        if (atoi(p) > 23 || !all_digits(p + 3, 2) || atoi(p + 3) > 59) return 0;
        p += 5;
        return *p == '\0';
    }
    return 0;
}

static char fmt_err_buf[8192];

const char *w14_mcpelicita_0beae8_format_error(const char *fmt, const char *text) {
    int valid = 0;
    const char *t = text ? text : "";
    if (!fmt || !*fmt) return "";
    if (!strcmp(fmt, "email")) valid = check_email(t);
    else if (!strcmp(fmt, "uri")) valid = check_uri(t);
    else if (!strcmp(fmt, "date")) valid = check_date(t);
    else if (!strcmp(fmt, "date-time")) valid = check_datetime(t);
    else return "";  /* no check registered -> "" */
    if (valid) return "";
    snprintf(fmt_err_buf, sizeof fmt_err_buf, "'%s' is not a valid %s", t, fmt);
    return fmt_err_buf;
}

/* [value|null, error] helper */
static struct json_t *coerce_pair(struct json_t *value, const char *err) {
    struct json_t *out = json_array();
    json_append(out, value ? value : json_null());
    json_append(out, json_string(err ? err : ""));
    return out;
}

struct json_t *w14_mcpelicita_0beae8_coerce_string(const struct json_t *spec,
                                                   const char *text) {
    char msg[8192], specval[512], fmt[512];
    const struct json_t *v;
    const char *err;
    int minlen, maxlen;
    v = json_obj_get(spec, "minLength");
    if (v) {
        py_str(v, specval, sizeof specval);
        minlen = atoi(specval);
        if ((int)strlen(text) < minlen) {
            snprintf(msg, sizeof msg, "must be at least %s characters", specval);
            return coerce_pair(NULL, msg);
        }
    }
    v = json_obj_get(spec, "maxLength");
    if (v) {
        py_str(v, specval, sizeof specval);
        maxlen = atoi(specval);
        if ((int)strlen(text) > maxlen) {
            snprintf(msg, sizeof msg, "must be at most %s characters", specval);
            return coerce_pair(NULL, msg);
        }
    }
    v = json_obj_get(spec, "format");
    if (py_truthy(v)) py_str(v, fmt, sizeof fmt); else fmt[0] = '\0';
    err = w14_mcpelicita_0beae8_format_error(fmt, text);
    if (*err) return coerce_pair(NULL, err);
    return coerce_pair(json_string(text), "");
}

struct json_t *w14_mcpelicita_0beae8_coerce_number(const struct json_t *spec,
                                                   const char *text) {
    char kbuf[64], msg[8192], tbuf[4096], specval[512];
    const char *kind = kind_of(spec, kbuf, sizeof kbuf);
    const char *trimmed = trim_copy(text ? text : "", tbuf, sizeof tbuf);
    char *end;
    double v;
    const struct json_t *lim;

    v = strtod(trimmed, &end);
    while (*end && isspace((unsigned char)*end)) end++;
    if (end == trimmed || *end || !isfinite(v)) {
        snprintf(msg, sizeof msg, "'%s' is not a number", text ? text : "");
        return coerce_pair(NULL, msg);
    }
    if (!strcmp(kind, "integer") && floor(v) != v) {
        snprintf(msg, sizeof msg, "'%s' is not a whole number", text ? text : "");
        return coerce_pair(NULL, msg);
    }
    lim = json_obj_get(spec, "minimum");
    if (lim && lim->type == JSON_NUMBER && v < lim->num_val) {
        py_str(lim, specval, sizeof specval);
        snprintf(msg, sizeof msg, "must be at least %s", specval);
        return coerce_pair(NULL, msg);
    }
    lim = json_obj_get(spec, "maximum");
    if (lim && lim->type == JSON_NUMBER && v > lim->num_val) {
        py_str(lim, specval, sizeof specval);
        snprintf(msg, sizeof msg, "must be at most %s", specval);
        return coerce_pair(NULL, msg);
    }
    return coerce_pair(json_number(v), "");
}

static char multi_err_buf[8192];

struct json_t *w14_mcpelicita_0beae8_coerce_multi(const struct json_t *spec,
                                                  const struct json_t *raw) {
    w14_opt opts[256];
    size_t n = build_options(options_source(spec), opts, 256);
    struct json_t *values = json_array();
    struct json_t *parsed = NULL;
    const struct json_t *items = raw;
    struct json_t *result;
    char item_buf[1024];
    struct json_t *matched = NULL;
    int minitems = 0, maxitems = -1;
    const struct json_t *lim;
    int items_owned = 0;

    if (raw && raw->type == JSON_STRING) {
        parsed = json_parse(raw->str_val, NULL);
        if (parsed && parsed->type == JSON_ARRAY) {
            items = parsed;
        } else {
            /* [p for p in raw.split(",") if p.strip()] */
            const char *p = raw->str_val;
            items = json_array();
            items_owned = 1;
            while (*p) {
                const char *end = strchr(p, ',');
                size_t len = end ? (size_t)(end - p) : strlen(p);
                char seg[1024];
                size_t n2 = len >= sizeof seg ? sizeof seg - 1 : len;
                memcpy(seg, p, n2); seg[n2] = '\0';
                if (*trim_copy(seg, item_buf, sizeof item_buf))
                    json_append(items, json_string(item_buf));
                if (!end) break;
                p = end + 1;
            }
        }
    }

    if (items && items->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(items); i++) {
            const struct json_t *it = json_get(items, i);
            const char *txt;
            if (!it) continue;
            if (it->type == JSON_STRING) txt = it->str_val;
            else { py_str(it, item_buf, sizeof item_buf); txt = item_buf; }
            if (!match_one(opts, n, txt, &matched)) {
                char labels[8192] = "";
                size_t used = 0;
                for (size_t j = 0; j < n; j++) {
                    int w = snprintf(labels + used, sizeof labels - used, "%s%s",
                                     j ? ", " : "", opts[j].label);
                    if (w <= 0 || (size_t)w >= sizeof labels - used) break;
                    used += (size_t)w;
                }
                snprintf(multi_err_buf, sizeof multi_err_buf,
                         "'%s' is not one of: %s", txt, labels);
                json_free(values);
                json_free(parsed);
                if (items_owned) json_free((struct json_t *)items);
                return coerce_pair(NULL, multi_err_buf);
            }
            json_append(values, json_copy(matched));
        }
    }

    lim = json_obj_get(spec, "minItems");
    if (lim) {
        char b[64]; py_str(lim, b, sizeof b); minitems = atoi(b);
    }
    lim = json_obj_get(spec, "maxItems");
    if (lim) {
        char b[64]; py_str(lim, b, sizeof b); maxitems = atoi(b);
    }
    if ((int)json_len(values) < minitems) {
        char b[64]; py_str(json_obj_get(spec, "minItems"), b, sizeof b);
        snprintf(multi_err_buf, sizeof multi_err_buf, "pick at least %s", b);
        json_free(values); json_free(parsed);
        if (items_owned) json_free((struct json_t *)items);
        return coerce_pair(NULL, multi_err_buf);
    }
    if (maxitems >= 0 && (int)json_len(values) > maxitems) {
        char b[64]; py_str(json_obj_get(spec, "maxItems"), b, sizeof b);
        snprintf(multi_err_buf, sizeof multi_err_buf, "pick at most %s", b);
        json_free(values); json_free(parsed);
        if (items_owned) json_free((struct json_t *)items);
        return coerce_pair(NULL, multi_err_buf);
    }
    json_free(parsed);
    if (items_owned) json_free((struct json_t *)items);
    result = coerce_pair(values, "");
    return result;
}

static struct json_t *coerce_enum(const struct json_t *spec, const char *text) {
    w14_opt opts[256];
    size_t n = build_options(options_source(spec), opts, 256);
    struct json_t *value = NULL;
    char labels[8192] = "", msg[8192];
    size_t used = 0;
    if (match_one(opts, n, text, &value)) return coerce_pair(json_copy(value), "");
    for (size_t j = 0; j < n; j++) {
        int w = snprintf(labels + used, sizeof labels - used, "%s%s",
                         j ? ", " : "", opts[j].label);
        if (w <= 0 || (size_t)w >= sizeof labels - used) break;
        used += (size_t)w;
    }
    snprintf(msg, sizeof msg, "'%s' is not one of: %s", text, labels);
    return coerce_pair(NULL, msg);
}

struct json_t *w14_mcpelicita_0beae8_coerce_answer(const struct json_t *spec,
                                                   const struct json_t *raw,
                                                   int required) {
    char kbuf[64], rbuf[4096], msg[512], tbuf[4096];
    const char *kind = kind_of(spec, kbuf, sizeof kbuf);
    const struct json_t *def;
    struct json_t *out = json_array(), *pair;
    int blank = 0;

    if (!raw || raw->type == JSON_NULL) blank = 1;
    else if (raw->type == JSON_STRING) blank = !*trim_copy(raw->str_val, rbuf, sizeof rbuf);
    else if (raw->type == JSON_ARRAY) blank = json_len(raw) == 0;

    if (blank) {
        def = json_obj_get(spec, "default");
        if (def && def->type != JSON_NULL) {
            json_append(out, json_bool(true));
            json_append(out, json_copy(def));
            json_append(out, json_string(""));
            return out;
        }
        json_append(out, json_bool(false));
        json_append(out, json_null());
        json_append(out, json_string(required ? "this field is required" : ""));
        return out;
    }

    if (!strcmp(kind, "multi")) {
        pair = w14_mcpelicita_0beae8_coerce_multi(spec, raw);
    } else {
        const char *txt;
        if (raw->type == JSON_STRING) txt = trim_copy(raw->str_val, rbuf, sizeof rbuf);
        else { py_str(raw, tbuf, sizeof tbuf); txt = trim_copy(tbuf, msg, sizeof msg); }
        if (!strcmp(kind, "boolean")) pair = w14_mcpelicita_0beae8_coerce_boolean(txt);
        else if (!strcmp(kind, "number") || !strcmp(kind, "integer"))
            pair = w14_mcpelicita_0beae8_coerce_number(spec, txt);
        else if (!strcmp(kind, "enum")) pair = coerce_enum(spec, txt);
        else pair = w14_mcpelicita_0beae8_coerce_string(spec, txt);
    }

    {
        const char *err = (json_get(pair, 1) && json_get(pair, 1)->type == JSON_STRING)
                          ? json_get(pair, 1)->str_val : "";
        json_append(out, json_bool(*err ? false : true));
        json_append(out, json_copy(json_get(pair, 0)));
        json_append(out, json_string(err));
    }
    json_free(pair);
    return out;
}

