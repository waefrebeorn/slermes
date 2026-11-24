/*
 * tool_availability_notices.c — C home for upstream Hermes hermes_cli/tool_availability_notices.py.
 * STATUS: BEHAVIOR PORT — value->value rendering parity, gate w25_toolavaila_80adee.
 * PoP: hermes_cli_tool_availability_notices_pending @ hermes_cli/tool_availability_notices.py:<module>
 * PoP: w25_toolavaila_80adee_filter_enabled @ hermes_cli/tool_availability_notices.py:filter_to_enabled_toolsets
 * PoP: w25_toolavaila_80adee_warning_lines @ hermes_cli/tool_availability_notices.py:tool_availability_warning_lines
 * PoP: w25_toolavaila_80adee_terminal_line @ hermes_cli/tool_availability_notices.py:_terminal_line
 * PoP: w25_toolavaila_80adee_constants @ hermes_cli/tool_availability_notices.py:_MULTI_PROVIDER_NOTICES,_GENERIC_FOOTER
 *
 * The seam is the world outside the module: ``check_tool_availability()``
 * (which probes binaries and env vars) and ``toolsets.resolve_toolset`` (a
 * registry lookup that may raise) arrive here as the ``unavailable`` rows and
 * a name->tools map.  What is ported is exactly what those values decide: which
 * entries this session would load, and the lines the CLI prints for the rest.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_toolavaila_80adee.h"

/* Toolsets whose env_vars list is a multi-provider dump: one sentence instead. */
static const char *const TA_WEB_NOTICE =
    "[yellow]\xe2\x9a\xa0 Web search is off[/] \xe2\x80\x94 no search provider is set up yet "
    "(any one of Nous subscription, Exa, Tavily, Firecrawl, Brave, or free DuckDuckGo works). "
    "Run [bold]hermes setup tools[/] and set one up under \"Web Search & Scraping\".";

static const char *const TA_GENERIC_HEADER =
    "[yellow]\xe2\x9a\xa0\xef\xb8\x8f  Some tools disabled (missing API keys):[/]";

static const char *const TA_GENERIC_FOOTER = "[dim]   Run 'hermes setup tools' to configure[/]";

/* Python truthiness of a JSON value (absent and null are both falsy). */
static int ta_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static char *ta_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* Python str() over the scalars a toolset row can hold; NULL for containers
 * (whose repr no fixture asks for). */
static char *ta_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return ta_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return ta_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return ta_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return ta_dup(buf);
    default: return NULL;
    }
}

/* str(value or ""): the module's name normaliser. */
static char *ta_name(const json_t *v) {
    char *s;
    /* The lines interpolate the name with an f-string, so a null name renders as
     * the text "None" ("• None"), not as an empty slot. */
    if (!v || v->type == JSON_NULL) return ta_dup("None");
    if (!ta_truthy(v)) return ta_dup("");
    s = ta_pystr(v);
    return s ? s : ta_dup("");
}

typedef struct {
    char **v;
    size_t n;
    size_t cap;
} ta_strset;

static int ta_set_has(const ta_strset *s, const char *x) {
    size_t i;
    if (!x) return 0;
    for (i = 0; i < s->n; i++)
        if (strcmp(s->v[i], x) == 0) return 1;
    return 0;
}

static void ta_set_add(ta_strset *s, const char *x) {
    char *dup;
    if (!x) return;
    if (ta_set_has(s, x)) return;
    if (s->n == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 8;
        char **grown = (char **)realloc(s->v, cap * sizeof(char *));
        if (!grown) return;
        s->v = grown;
        s->cap = cap;
    }
    dup = ta_dup(x);
    if (!dup) return;
    s->v[s->n++] = dup;
}

static void ta_set_free(ta_strset *s) {
    size_t i;
    for (i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    s->v = NULL;
    s->n = s->cap = 0;
}

/* ``for t in seq``: an array's items, a string's characters, an object's keys. */
static void ta_iter_str(const json_t *seq, ta_strset *out) {
    size_t i;
    if (!seq) return;
    if (seq->type == JSON_ARRAY) {
        for (i = 0; i < json_len(seq); i++) {
            char *s = ta_pystr(json_get(seq, i));
            ta_set_add(out, s);
            free(s);
        }
    } else if (seq->type == JSON_STRING) {
        const char *s = seq->str_val ? seq->str_val : "";
        char one[2];
        one[1] = '\0';
        for (; *s; s++) { one[0] = *s; ta_set_add(out, one); }
    } else if (seq->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(seq); i++) ta_set_add(out, json_object_get_key_at(seq, i));
    }
}

/* ", ".join(values) with Python's str() over each element. */
static char *ta_join(const json_t *seq, const char *sep) {
    size_t i, total = 1;
    char *buf, *p;
    if (!seq || seq->type != JSON_ARRAY || json_len(seq) == 0) return ta_dup("");
    for (i = 0; i < json_len(seq); i++) {
        char *s = ta_pystr(json_get(seq, i));
        total += strlen(s ? s : "") + (i ? strlen(sep) : 0);
        free(s);
    }
    buf = (char *)malloc(total);
    if (!buf) return NULL;
    p = buf;
    for (i = 0; i < json_len(seq); i++) {
        char *s = ta_pystr(json_get(seq, i));
        if (i) { size_t k = strlen(sep); memcpy(p, sep, k); p += k; }
        if (s) { size_t k = strlen(s); memcpy(p, s, k); p += k; free(s); }
    }
    *p = '\0';
    return buf;
}

json_t *w25_toolavaila_80adee_filter_enabled(const json_t *request) {
    const json_t *unavail = json_obj_get(request, "unavailable");
    const json_t *enabled = json_obj_get(request, "enabled");
    const json_t *resolve = json_obj_get(request, "resolve");
    ta_strset names = {0}, tools = {0};
    json_t *out;
    size_t i;

    if (!unavail || unavail->type != JSON_ARRAY) return json_array();

    if (enabled && enabled->type == JSON_ARRAY) {
        for (i = 0; i < json_len(enabled); i++) {
            char *s = ta_pystr(json_get(enabled, i));
            if (s && s[0]) ta_set_add(&names, s);
            free(s);
        }
    }
    /* An empty selection means "everything", so nothing is filtered. */
    if (names.n == 0) {
        ta_set_free(&names);
        return json_copy(unavail);
    }
    for (i = 0; i < names.n; i++) {
        const json_t *expanded = NULL;
        int known = 0;
        size_t k;
        if (resolve && resolve->type == JSON_OBJECT) {
            for (k = 0; k < json_object_size(resolve); k++) {
                const char *key = json_object_get_key_at(resolve, k);
                if (key && strcmp(key, names.v[i]) == 0) { known = 1; expanded = json_object_get_at(resolve, k); break; }
            }
        }
        /* A selection name resolve() does not know raises live; skipped here. */
        if (!known) continue;
        if (ta_truthy(expanded)) ta_iter_str(expanded, &tools);
    }

    out = json_array();
    for (i = 0; i < json_len(unavail); i++) {
        const json_t *item = json_get(unavail, i);
        char *nm;
        int keep = 0;
        if (!item || item->type != JSON_OBJECT) continue;
        nm = ta_name(json_obj_get(item, "name"));
        if (nm && ta_set_has(&names, nm)) keep = 1;
        free(nm);
        if (!keep) {
            const json_t *item_tools = json_obj_get(item, "tools");
            if (ta_truthy(item_tools)) {
                ta_strset seen = {0};
                size_t k;
                ta_iter_str(item_tools, &seen);
                for (k = 0; k < seen.n; k++)
                    if (ta_set_has(&tools, seen.v[k])) { keep = 1; break; }
                ta_set_free(&seen);
            }
        }
        if (keep) json_append(out, json_copy(item));
    }
    ta_set_free(&names);
    ta_set_free(&tools);
    return out;
}

/* _terminal_line(backend, reason). */
static char *ta_terminal_text(const char *backend, const char *reason) {
    const char *fmt =
        "[yellow]\xe2\x9a\xa0 Terminal tool disabled:[/] the '%s' backend is not usable%s. "
        "Run [bold]hermes doctor[/] for details, or [bold]hermes setup terminal[/] to pick another backend.";
    size_t need = strlen(fmt) + strlen(backend ? backend : "") + strlen(reason ? reason : "") + 8;
    char *detail = NULL, *buf;
    if (reason && reason[0]) {
        detail = (char *)malloc(strlen(reason) + 4);
        if (detail) snprintf(detail, strlen(reason) + 4, " (%s)", reason);
    }
    buf = (char *)malloc(need + (detail ? strlen(detail) : 0));
    if (!buf) { free(detail); return NULL; }
    snprintf(buf, need + (detail ? strlen(detail) : 0), fmt, backend ? backend : "", detail ? detail : "");
    free(detail);
    return buf;
}

static const char *ta_str_or_null(const json_t *obj, const char *key) {
    const json_t *v = json_obj_get(obj, key);
    return v && v->type == JSON_STRING ? v->str_val : NULL;
}

json_t *w25_toolavaila_80adee_terminal_line(const json_t *request) {
    const char *backend = ta_str_or_null(request, "backend");
    const char *reason = ta_str_or_null(request, "reason");
    char *line = ta_terminal_text(backend ? backend : "local", reason);
    json_t *out = json_string(line ? line : "");
    free(line);
    return out;
}

json_t *w25_toolavaila_80adee_warning_lines(const json_t *request) {
    const json_t *unavail = json_obj_get(request, "unavailable");
    const char *reason = ta_str_or_null(request, "terminal_reason");
    const char *backend = ta_str_or_null(request, "terminal_backend");
    json_t *out = json_array(), *generic = json_array();
    size_t i;

    if (backend == NULL) backend = "local";
    if (unavail && unavail->type == JSON_ARRAY) {
        for (i = 0; i < json_len(unavail); i++) {
            const json_t *item = json_get(unavail, i);
            char *nm;
            if (!item || item->type != JSON_OBJECT) continue;
            nm = ta_name(json_obj_get(item, "name"));
            if (nm && strcmp(nm, "terminal") == 0) {
                char *line = ta_terminal_text(backend, reason);
                json_append(out, json_string(line ? line : ""));
                free(line);
            } else if (nm && strcmp(nm, "web") == 0) {
                json_append(out, json_string(TA_WEB_NOTICE));
            } else if (ta_truthy(json_obj_get(item, "env_vars"))) {
                json_append(generic, json_copy(item));
            }
            free(nm);
        }
    }
    if (json_len(generic) > 0) {
        json_append(out, json_string(TA_GENERIC_HEADER));
        for (i = 0; i < json_len(generic); i++) {
            const json_t *item = json_get(generic, i);
            char *nm = ta_name(json_obj_get(item, "name"));
            char *keys = ta_join(json_obj_get(item, "env_vars"), ", ");
            size_t need = strlen(nm) + strlen(keys ? keys : "") + 64;
            char *line = (char *)malloc(need);
            if (line) {
                snprintf(line, need, "   [dim]\xe2\x80\xa2 %s[/] [dim italic](%s)[/]", nm, keys ? keys : "");
                json_append(out, json_string(line));
                free(line);
            }
            free(nm);
            free(keys);
        }
        json_append(out, json_string(TA_GENERIC_FOOTER));
    }
    json_free(generic);
    return out;
}

json_t *w25_toolavaila_80adee_constants(void) {
    json_t *out = json_object();
    json_t *keys = json_array();
    json_append(keys, json_string("web"));
    json_set(out, "generic_footer", json_string(TA_GENERIC_FOOTER));
    json_set(out, "multi_provider_keys", keys);
    json_set(out, "web_notice", json_string(TA_WEB_NOTICE));
    return out;
}
