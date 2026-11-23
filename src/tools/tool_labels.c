/*
 * tool_labels.c — C port of upstream Hermes tools/tool_labels.py.
 *
 * PoP: toollabels_app_title @ tools/tool_labels.py:app_title
 * PoP: toollabels_action_phrase @ tools/tool_labels.py:_action_phrase
 * PoP: toollabels_label_for_tool_name @ tools/tool_labels.py:label_for_tool_name
 * PoP: toollabels_bridge_labels @ tools/tool_labels.py:_bridge_labels
 *
 * parse_connector_name (tools/connectors/gateway/names.py) and MCP_TOOL_NAME_PREFIX
 * are reproduced here; verb/emoji/preview come from the injected stubs.
 */
#include "noc_toollabels.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *CONNECTOR_EMOJI = "\xF0\x9F\x94\x97";   /* 🔗 */
static const char *MCP_EMOJI       = "\xF0\x9F\x94\x8C";   /* 🔌 */
static const char *SEARCH_EMOJI    = "\xF0\x9F\x94\x8E";   /* 🔎 */
static const char *DESCRIBE_EMOJI  = "\xF0\x9F\x93\x96";   /* 📖 */
static const char *DEFAULT_TOOL_EMOJI = "\xE2\x9A\xA1";    /* ⚡ */
static const char *MIDDOT = "\xC2\xB7";                     /* · */

static char *trim_dup(const char *s) {
    if (!s) return strdup("");
    const char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - p);
    char *out = (char *)malloc(n + 1);
    memcpy(out, p, n); out[n] = '\0';
    return out;
}
static char *lower_dup(const char *s) {
    char *o = strdup(s ? s : "");
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *upper_dup(const char *s) {
    char *o = strdup(s ? s : "");
    for (char *p = o; *p; p++) *p = (char)toupper((unsigned char)*p);
    return o;
}
static int truthy(const json_t *j) {
    if (!j) return 0;
    switch (j->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return j->bool_val ? 1 : 0;
        case JSON_NUMBER: return j->num_val != 0.0;
        case JSON_STRING: return j->str_val && j->str_val[0];
        case JSON_ARRAY: case JSON_OBJECT: return j->c.count > 0;
    }
    return 0;
}
static char *pystr(const json_t *j) {
    if (!j) return strdup("");
    switch (j->type) {
        case JSON_STRING: return strdup(j->str_val ? j->str_val : "");
        case JSON_BOOL: return strdup(j->bool_val ? "True" : "False");
        case JSON_NULL: return strdup("None");
        case JSON_NUMBER: {
            char b[64]; double v = j->num_val;
            if (v == (double)(long long)v) snprintf(b, sizeof b, "%lld", (long long)v);
            else snprintf(b, sizeof b, "%g", v);
            return strdup(b);
        }
        default: return strdup("");
    }
}

static const struct { const char *k, *v; } APP_TITLES[] = {
    {"discord","Discord"},{"figma","Figma"},{"github","GitHub"},{"gmail","Gmail"},
    {"googlecalendar","Google Calendar"},{"googledocs","Google Docs"},
    {"googledrive","Google Drive"},{"jira","Jira"},{"linear","Linear"},
    {"notion","Notion"},{"outlook","Outlook"},{"slack","Slack"},
    {"stripe_mcp","Stripe"},{"todoist","Todoist"},
};

char *toollabels_app_title(const char *slug) {
    char *key = trim_dup(slug ? slug : "");
    char *orig = strdup(key);
    char *lkey = lower_dup(key);
    for (size_t i = 0; i < sizeof(APP_TITLES)/sizeof(APP_TITLES[0]); i++)
        if (!strcmp(lkey, APP_TITLES[i].k)) { free(lkey); free(orig); return strdup(APP_TITLES[i].v); }
    free(lkey);
    for (char *p = key; *p; p++) if (*p == '-' || *p == '_') *p = ' ';
    size_t cap = strlen(key) + 16, len = 0;
    char *out = (char *)malloc(cap ? cap : 1);
    out[0] = '\0';
    char *p = key;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t wlen = (size_t)(p - start);
        if (len + wlen + 2 > cap) { cap = (len + wlen + 2) * 2; out = (char *)realloc(out, cap); }
        if (len) out[len++] = ' ';
        out[len++] = (char)toupper((unsigned char)start[0]);
        memcpy(out + len, start + 1, wlen - 1); len += wlen - 1;
    }
    out[len] = '\0';
    free(key);
    if (!len) { free(out); return orig; }
    free(orig);
    return out;
}

char *toollabels_action_phrase(const char *tool, const char *app_slug) {
    char *text = trim_dup(tool ? tool : "");
    const char *slug = app_slug ? app_slug : "";
    if (*slug) {
        char *s = strdup(slug);
        for (char *p = s; *p; p++) if (*p == '-') *p = '_';
        char *su = upper_dup(s);
        char *prefix = (char *)malloc(strlen(su) + 2);
        strcpy(prefix, su); strcat(prefix, "_");
        free(su); free(s);
        char *utext = upper_dup(text);
        if (strncmp(utext, prefix, strlen(prefix)) == 0) {
            size_t n = strlen(prefix);
            memmove(text, text + n, strlen(text + n) + 1);
        }
        free(utext); free(prefix);
    }
    for (char *p = text; *p; p++) if (*p == '-' || *p == '_') *p = ' ';
    size_t cap = strlen(text) + 2, len = 0;
    char *out = (char *)malloc(cap);
    out[0] = '\0';
    char *p = text;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (len) out[len++] = ' ';
        while (*p && !isspace((unsigned char)*p)) out[len++] = (char)tolower((unsigned char)*p++);
    }
    out[len] = '\0';
    free(text);
    return out;
}

/* name.split("__", 2) with prefix=="connectors" and non-empty connector/tool. */
static int parse_connector(const char *name, char **conn, char **tool) {
    if (!name) return 0;
    const char *p1 = strstr(name, "__");
    if (!p1) return 0;
    const char *p2 = strstr(p1 + 2, "__");
    if (!p2) return 0;
    size_t npre = (size_t)(p1 - name);
    size_t ncon = (size_t)(p2 - (p1 + 2));
    if (npre != strlen("connectors") || strncmp(name, "connectors", npre)) return 0;
    if (ncon == 0 || p2[2] == '\0') return 0;
    *conn = (char *)malloc(ncon + 1);
    memcpy(*conn, p1 + 2, ncon); (*conn)[ncon] = '\0';
    *tool = strdup(p2 + 2);
    return 1;
}

/* ToolLabel.as_payload() */
static json_t *make_label(const char *kind, const char *app, const char *action,
                          const char *emoji, const char *name, const char *preview) {
    json_t *o = json_object();
    json_set(o, "kind", json_string(kind));
    json_set(o, "app", json_string(app ? app : ""));
    json_set(o, "action", json_string(action ? action : ""));
    json_set(o, "emoji", json_string(emoji ? emoji : ""));
    size_t tlen = (action && *action) ? strlen(app) + strlen(MIDDOT) + 2 + strlen(action) + 1 : strlen(app) + 1;
    char *text = (char *)malloc(tlen);
    if (action && *action) snprintf(text, tlen, "%s %s %s", app, MIDDOT, action);
    else snprintf(text, tlen, "%s", app);
    json_set(o, "text", json_string(text));
    free(text);
    json_set(o, "name", json_string(name ? name : ""));
    json_set(o, "preview", json_string(preview ? preview : ""));
    return o;
}

/* _arg_preview: empty/non-dict arguments -> ""; else injected preview or "". */
static char *arg_preview(const json_t *arguments, const char *preview) {
    if (!json_is_object(arguments) || json_object_size(arguments) == 0) return strdup("");
    return strdup(preview ? preview : "");
}

json_t *toollabels_label_for_tool_name(const char *name, const json_t *arguments,
                                       const char *verb, const char *emoji, const char *preview) {
    char *trimmed = trim_dup(name ? name : "");
    if (!*trimmed) { free(trimmed); return json_null(); }
    char *conn = NULL, *tool = NULL;
    if (parse_connector(name, &conn, &tool)) {
        char *action = toollabels_action_phrase(tool, conn);
        const char *alias = json_is_object(arguments) ? json_get_str(arguments, "connector_alias", "") : "";
        char *at = trim_dup(alias);
        if (*at) {
            size_t n = strlen(action) + 3 + strlen(at) + 1;
            char *na = (char *)malloc(n);
            snprintf(na, n, "%s (%s)", action, at);
            free(action); action = na;
        }
        free(at);
        char *app = toollabels_app_title(conn);
        char *pv = arg_preview(arguments, preview);
        json_t *o = make_label("connector", app, action, CONNECTOR_EMOJI, name, pv);
        free(app); free(action); free(pv); free(conn); free(tool); free(trimmed);
        return o;
    }
    if (!strncmp(name, "mcp__", 5)) {
        const char *rest = name + 5;
        const char *sep = strstr(rest, "__");
        if (sep && sep[2]) {
            char *server = (char *)malloc((size_t)(sep - rest) + 1);
            memcpy(server, rest, (size_t)(sep - rest)); server[sep - rest] = '\0';
            char *mt = strdup(sep + 2);
            char *app = toollabels_app_title(server);
            char *action = toollabels_action_phrase(mt, "");
            char *pv = arg_preview(arguments, preview);
            json_t *o = make_label("mcp", app, action, MCP_EMOJI, name, pv);
            free(app); free(action); free(pv); free(server); free(mt); free(trimmed);
            return o;
        }
    }
    char *app_fallback = toollabels_app_title(name);
    const char *app = (verb && *verb) ? verb : app_fallback;
    const char *em = (emoji && *emoji) ? emoji : DEFAULT_TOOL_EMOJI;
    char *pv = arg_preview(arguments, preview);
    json_t *o = make_label("tool", app, "", em, name, pv);
    free(app_fallback); free(pv); free(trimmed);
    return o;
}

json_t *toollabels_bridge_labels(const char *function_name, const json_t *args) {
    json_t *arr = json_array();
    if (!strcmp(function_name, "tool_search")) {
        char *first = strdup("");
        json_t *qj = json_is_object(args) ? json_obj_get(args, "queries") : NULL;
        if (qj && qj->type == JSON_STRING) {
            char *s = pystr(qj), *t = trim_dup(s);
            if (*t) { free(first); first = s; } else free(s);
            free(t);
        } else if (qj && qj->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(qj); i++) {
                json_t *q = json_get(qj, i);
                if (!truthy(q)) continue;
                char *s = pystr(q), *t = trim_dup(s);
                if (*t) { free(first); first = s; free(t); break; }
                free(s); free(t);
            }
        }
        json_append(arr, make_label("tool", "Searching tools", first, SEARCH_EMOJI, function_name, ""));
        free(first);
    } else {
        json_t *nj = json_is_object(args) ? json_obj_get(args, "names") : NULL;
        size_t count = 0;
        if (nj && nj->type == JSON_STRING) count = 1;
        else if (nj && nj->type == JSON_ARRAY) count = json_len(nj);
        char action[64];
        if (count == 1) snprintf(action, sizeof action, "1 tool");
        else snprintf(action, sizeof action, "%zu tools", count);
        json_append(arr, make_label("tool", "Reading tool details", action, DESCRIBE_EMOJI, function_name, ""));
    }
    return arr;
}
