/*
 * status_report.c — faithful C11 port of hermes_cli/status_report.py.
 * Gate w11_statusrepo_0555a2.
 *
 * PoP: w11_statusrepo_0555a2_status_lines @ hermes_cli/status_report.py:status_lines
 */
#include "hermes_json.h"
#include "noc_w11_statusrepo_0555a2.h"
#include <stdio.h>
#include <string.h>

/* English labels shared by the CLI and TUI renderers. */
static const char *label_for(const char *key) {
    if (!key) return NULL;
    if (!strcmp(key, "session_id"))    return "Session ID";
    if (!strcmp(key, "path"))          return "Path";
    if (!strcmp(key, "title"))         return "Title";
    if (!strcmp(key, "model"))         return "Model";
    if (!strcmp(key, "created"))       return "Created";
    if (!strcmp(key, "last_activity")) return "Last Activity";
    if (!strcmp(key, "tokens"))        return "Tokens";
    if (!strcmp(key, "agent_running")) return "Agent Running";
    return NULL;
}

/* Python truthiness of a JSON value. */
static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

/* str() coercion of a JSON value (used by the f-string value interpolation). */
static void py_str(const json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return; }
    switch (v->type) {
        case JSON_STRING: snprintf(buf, n, "%s", v->str_val ? v->str_val : ""); break;
        case JSON_BOOL:   snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, n, "%lld", (long long)v->num_val);
            else
                snprintf(buf, n, "%g", v->num_val);
            break;
        default: buf[0] = '\0'; break;
    }
}

json_t *w11_statusrepo_0555a2_status_lines(const json_t *fields, const json_t *keys) {
    json_t *out = json_array();
    if (!keys || keys->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(keys); i++) {
        json_t *kn = json_get(keys, i);
        if (!kn || kn->type != JSON_STRING || !kn->str_val) continue;
        const char *key = kn->str_val;
        const char *label = label_for(key);
        if (!label) continue;                       /* STATUS_LABELS[key] would KeyError */
        json_t *value = json_obj_get(fields, key); /* fields[key] */
        if (!strcmp(key, "title") && !py_truthy(value)) continue;
        char valbuf[2048];
        if (!strcmp(key, "model")) {
            const char *m = json_get_str(fields, "model", "");
            if (!m || !*m) m = "(unknown)";
            const char *p = json_get_str(fields, "provider", "");
            if (!p || !*p) p = "unknown";
            snprintf(valbuf, sizeof(valbuf), "%s (%s)", m, p);
        } else if (!strcmp(key, "agent_running")) {
            snprintf(valbuf, sizeof(valbuf), "%s", py_truthy(value) ? "Yes" : "No");
        } else {
            py_str(value, valbuf, sizeof(valbuf));
        }
        char line[2304];
        snprintf(line, sizeof(line), "%s: %s", label, valbuf);
        json_append(out, json_string(line));
    }
    return out;
}
