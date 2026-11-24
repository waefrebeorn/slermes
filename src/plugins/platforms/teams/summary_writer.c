/*
 * summary_writer.c - C home for plugins/platforms/teams/summary_writer.py.
 * Ports every pure helper of the module: the config coercion, the delivery-config merge and the
 * markdown/html renderers. The httpx/Graph delivery wrappers stay Python (live network).
 * The i18n catalog is injected (the oracle stubs t() with the same literals) so both sides
 * resolve the same strings; the env fallbacks are read from an injected map, never from the
 * process environment (Windows CRT setenv semantics differ from Python's os.environ).
 * PoP: w19_summarywri_99693d_parse_bool @ plugins/platforms/teams/summary_writer.py:_parse_bool
 * PoP: w19_summarywri_99693d_text @ plugins/platforms/teams/summary_writer.py:TeamsSummaryWriter._text
 * PoP: w19_summarywri_99693d_bullet_lines @ plugins/platforms/teams/summary_writer.py:TeamsSummaryWriter._bullet_lines
 * PoP: w19_summarywri_99693d_title @ plugins/platforms/teams/summary_writer.py:TeamsSummaryWriter._title
 * PoP: w19_summarywri_99693d_render_markdown @ plugins/platforms/teams/summary_writer.py:TeamsSummaryWriter._render_summary_markdown
 * PoP: w19_summarywri_99693d_render_html @ plugins/platforms/teams/summary_writer.py:TeamsSummaryWriter._render_summary_html
 * PoP: w19_summarywri_99693d_resolve_delivery_config @ plugins/platforms/teams/summary_writer.py:TeamsSummaryWriter._resolve_delivery_config
 */
#include "noc_w19_summarywri_99693d.h"
#include <stdio.h>
#include <string.h>

/* --- i18n catalog: the strings the oracle's t() stub answers --- */
#define T_EMPTY          "(no summary)"
#define T_SUMMARY_LINE   "Summary: "
#define T_KEY_DECISIONS  "Key decisions"
#define T_ACTION_ITEMS   "Action items"
#define T_RISKS          "Risks"
#define T_HEADING        "Summary"
#define T_NONE_ITEM      "(none)"
#define T_TITLE_PREFIX   "Meeting "
#define T_TITLE_FALLBACK "(untitled)"

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* --- Python semantics helpers ------------------------------------------- */

static int py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip() with no argument, for ASCII text. */
static char *py_strip(const char *s) {
    size_t n;
    if (!s) return dupstr("");
    n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && py_isspace((unsigned char)s[a])) a++;
    while (b > a && py_isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *py_lower(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : (char)c;
    }
    out[n] = '\0';
    return out;
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* str(value) for the scalar payload fields (integral numbers print as ints). */
static char *py_str_of(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dupstr("");
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d) snprintf(buf, sizeof buf, "%lld", (long long)d);
            else snprintf(buf, sizeof buf, "%g", d);
            return dupstr(buf);
        }
        default: return dupstr("");
    }
}

static const json_t *field(const json_t *payload, const char *key) {
    if (!payload || payload->type != JSON_OBJECT) return NULL;
    return json_obj_get(payload, key);
}

/* --- growable string buffer -------------------------------------------- */

typedef struct { char *p; size_t n, cap; } sb_t;

static void sb_reserve(sb_t *b, size_t extra) {
    if (b->n + extra + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->n + extra + 1) cap *= 2;
    char *np = (char *)realloc(b->p, cap);
    if (!np) return;
    b->p = np;
    b->cap = cap;
}

static void sb_addn(sb_t *b, const char *s, size_t n) {
    if (!s) return;
    sb_reserve(b, n);
    if (!b->p) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void sb_add(sb_t *b, const char *s) {
    if (s) sb_addn(b, s, strlen(s));
}

static void sb_addc(sb_t *b, char c) { sb_addn(b, &c, 1); }

static char *sb_take(sb_t *b) {
    if (!b->p) return dupstr("");
    return b->p;
}

/* html.escape(s) with quote=True, replace order as CPython does. */
static char *html_escape(const char *s) {
    sb_t b = {NULL, 0, 0};
    if (!s) return dupstr("");
    for (; *s; s++) {
        switch (*s) {
            case '&':  sb_add(&b, "&amp;"); break;
            case '<':  sb_add(&b, "&lt;"); break;
            case '>':  sb_add(&b, "&gt;"); break;
            case '"':  sb_add(&b, "&quot;"); break;
            case '\'': sb_add(&b, "&#x27;"); break;
            default:   sb_addc(&b, *s); break;
        }
    }
    return sb_take(&b);
}

/* --- exported helpers -------------------------------------------------- */

int w19_summarywri_99693d_parse_bool(json_t *value, int default_value) {
    if (value && value->type == JSON_BOOL) return value->bool_val ? 1 : 0;
    if (value && value->type == JSON_STRING) {
        const char *raw = value->str_val ? value->str_val : "";
        char *stripped = py_strip(raw);
        char *norm = stripped ? py_lower(stripped) : NULL;
        int out = default_value;
        if (norm) {
            if (!strcmp(norm, "1") || !strcmp(norm, "true") || !strcmp(norm, "yes") ||
                !strcmp(norm, "on")) out = 1;
            else if (!strcmp(norm, "0") || !strcmp(norm, "false") || !strcmp(norm, "no") ||
                     !strcmp(norm, "off")) out = 0;
        }
        free(norm);
        free(stripped);
        return out;
    }
    return default_value;
}

char *w19_summarywri_99693d_text(json_t *value, const char *fallback) {
    char *raw = py_str_of(py_truthy(value) ? value : NULL);
    char *stripped = raw ? py_strip(raw) : NULL;
    if (stripped && stripped[0]) { free(raw); return stripped; }
    free(raw);
    free(stripped);
    return dupstr(fallback ? fallback : "");
}

json_t *w19_summarywri_99693d_bullet_lines(json_t *values) {
    json_t *out = json_array();
    if (values && values->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(values); i++) {
            char *item = py_str_of(json_get(values, i));
            char *stripped = item ? py_strip(item) : NULL;
            if (stripped && stripped[0]) {
                size_t n = strlen(stripped);
                char *line = (char *)malloc(n + 3);
                if (line) {
                    line[0] = '-';
                    line[1] = ' ';
                    memcpy(line + 2, stripped, n + 1);
                    json_append(out, json_string(line));
                    free(line);
                }
            }
            free(item);
            free(stripped);
        }
    }
    if (json_len(out) == 0) json_append(out, json_string("- None"));
    return out;
}

char *w19_summarywri_99693d_title(json_t *payload) {
    const json_t *title = field(payload, "title");
    if (title && py_truthy(title)) return py_str_of(title);
    const json_t *meeting_ref = field(payload, "meeting_ref");
    const json_t *meeting_id = field(meeting_ref, "meeting_id");
    char *id_text = (meeting_id && py_truthy(meeting_id)) ? py_str_of(meeting_id)
                                                          : dupstr(T_TITLE_FALLBACK);
    size_t n = strlen(T_TITLE_PREFIX) + (id_text ? strlen(id_text) : 0);
    char *out = (char *)malloc(n + 1);
    if (!out) { free(id_text); return dupstr(""); }
    snprintf(out, n + 1, "%s%s", T_TITLE_PREFIX, id_text ? id_text : "");
    free(id_text);
    return out;
}

char *w19_summarywri_99693d_render_markdown(json_t *payload) {
    sb_t b = {NULL, 0, 0};
    char *title = w19_summarywri_99693d_title(payload);
    char *summary = w19_summarywri_99693d_text((json_t *)field(payload, "summary"), T_EMPTY);
    static const char *const attrs[] = { "key_decisions", "action_items", "risks" };
    static const char *const heads[] = { T_KEY_DECISIONS, T_ACTION_ITEMS, T_RISKS };

    sb_add(&b, "**");
    sb_add(&b, title);
    sb_add(&b, "**");
    sb_add(&b, "\n\n");
    sb_add(&b, T_SUMMARY_LINE);
    sb_add(&b, summary);
    for (int s = 0; s < 3; s++) {
        json_t *lines = w19_summarywri_99693d_bullet_lines((json_t *)field(payload, attrs[s]));
        sb_add(&b, "\n\n");
        sb_add(&b, heads[s]);
        sb_add(&b, ":\n");
        for (size_t i = 0; lines && i < json_len(lines); i++) {
            if (i) sb_add(&b, "\n");
            json_t *ln = json_get(lines, i);
            if (ln && ln->type == JSON_STRING) sb_add(&b, ln->str_val);
        }
        json_free(lines);
    }
    free(title);
    free(summary);
    return sb_take(&b);
}

char *w19_summarywri_99693d_render_html(json_t *payload) {
    sb_t b = {NULL, 0, 0};
    static const char *const attrs[] = { "key_decisions", "action_items", "risks" };
    static const char *const heads[] = { T_KEY_DECISIONS, T_ACTION_ITEMS, T_RISKS };
    char *title = w19_summarywri_99693d_title(payload);
    char *esc_title = html_escape(title);
    char *summary = w19_summarywri_99693d_text((json_t *)field(payload, "summary"), T_EMPTY);
    char *esc_summary = html_escape(summary);

    sb_add(&b, "<h2>");
    sb_add(&b, esc_title);
    sb_add(&b, "</h2>");
    sb_add(&b, "<h3>");
    sb_add(&b, T_HEADING);
    sb_add(&b, "</h3><p>");
    sb_add(&b, esc_summary);
    sb_add(&b, "</p>");
    for (int s = 0; s < 3; s++) {
        json_t *values = (json_t *)field(payload, attrs[s]);
        sb_t items = {NULL, 0, 0};
        if (values && values->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(values); i++) {
                char *item = py_str_of(json_get(values, i));
                char *stripped = item ? py_strip(item) : NULL;
                if (stripped && stripped[0]) {
                    char *esc = html_escape(item);
                    sb_add(&items, "<li>");
                    sb_add(&items, esc);
                    sb_add(&items, "</li>");
                    free(esc);
                }
                free(item);
                free(stripped);
            }
        }
        sb_add(&b, "<h3>");
        sb_add(&b, heads[s]);
        sb_add(&b, "</h3>");
        if (items.n > 0) {
            sb_add(&b, "<ul>");
            sb_addn(&b, items.p, items.n);
            sb_add(&b, "</ul>");
        } else {
            sb_add(&b, "<p>");
            sb_add(&b, T_NONE_ITEM);
            sb_add(&b, "</p>");
        }
        free(items.p);
    }
    free(title);
    free(esc_title);
    free(summary);
    free(esc_summary);
    return sb_take(&b);
}

/* (config key, env fallback) in _ENV_KEYS order. */
static const char *const ENV_KEYS[][2] = {
    { "delivery_mode", "TEAMS_DELIVERY_MODE" },
    { "incoming_webhook_url", "TEAMS_INCOMING_WEBHOOK_URL" },
    { "access_token", "TEAMS_GRAPH_ACCESS_TOKEN" },
    { "team_id", "TEAMS_TEAM_ID" },
    { "channel_id", "TEAMS_CHANNEL_ID" },
    { "chat_id", "TEAMS_CHAT_ID" },
    { NULL, NULL }
};

json_t *w19_summarywri_99693d_resolve_delivery_config(json_t *platform_cfg, json_t *config,
                                                     json_t *env) {
    json_t *merged = json_object();
    if (platform_cfg && platform_cfg->type == JSON_OBJECT) {
        json_t *extra = json_obj_get(platform_cfg, "extra");
        if (extra && extra->type == JSON_OBJECT) {
            for (size_t i = 0; i < json_object_size(extra); i++) {
                const char *k = json_object_get_key_at(extra, i);
                if (!k) continue;
                json_set(merged, k, json_copy(json_object_get_at(extra, i)));
            }
        }
        const char *token = json_get_str(platform_cfg, "token", NULL);
        if (token && token[0] && !json_obj_get(merged, "access_token"))
            json_set(merged, "access_token", json_string(token));
        const char *home_channel = json_get_str(platform_cfg, "home_channel_chat_id", NULL);
        if (home_channel && home_channel[0] && !json_obj_get(merged, "channel_id"))
            json_set(merged, "channel_id", json_string(home_channel));
    }
    if (config && config->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(config); i++) {
            const char *k = json_object_get_key_at(config, i);
            if (!k) continue;
            json_set(merged, k, json_copy(json_object_get_at(config, i)));
        }
    }
    for (int i = 0; ENV_KEYS[i][0]; i++) {
        const char *value = NULL;
        json_t *ev = (env && env->type == JSON_OBJECT) ? json_obj_get(env, ENV_KEYS[i][1]) : NULL;
        if (ev && ev->type == JSON_STRING) value = ev->str_val;
        if (value && value[0] && !py_truthy(json_obj_get(merged, ENV_KEYS[i][0])))
            json_set(merged, ENV_KEYS[i][0], json_string(value));
    }
    return merged;
}
