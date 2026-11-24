/*
 * plugins_auto_update.c — C home for upstream Hermes hermes_cli/plugins_auto_update.py.
 *
 * PoP: pluginsaut_held_with_ref @ hermes_cli/plugins_auto_update.py:held_with_ref
 * PoP: pluginsaut_summary_lines @ hermes_cli/plugins_auto_update.py:summary_lines
 * PoP: pluginsaut_slash_action @ hermes_cli/plugins_auto_update.py:plugins_slash_action
 */
#include "noc_w18_pluginsaut_e30b1a.h"

#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (out) { if (n) memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *dup_lower(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (out) { for (size_t i = 0; i < n; ++i) out[i] = (char)tolower((unsigned char)s[i]); out[n] = '\0'; }
    return out;
}
static int falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) return !v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;
    if (v->type == JSON_STRING) return !v->str_val || !*v->str_val;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count == 0;
    return 0;
}
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

int pluginsaut_held_with_ref(const struct json_t *record) {
    if (!record || record->type != JSON_OBJECT) return 0;
    json_t *block = json_obj_get(record, "catalog");
    if (!block || block->type != JSON_OBJECT) return 0;
    json_t *pinned = json_obj_get(record, "pinned");
    if (!pinned || pinned->type != JSON_BOOL || !pinned->bool_val) return 0; /* is True */
    return falsy(json_obj_get(block, "pin")) ? 1 : 0;                        /* not pin */
}

static const char *mark_for(const char *status) {
    if (!strcmp(status, "updated"))      return "✓ updated";
    if (!strcmp(status, "current"))      return "= already current";
    if (!strcmp(status, "failed"))       return "✗ failed";
    if (!strcmp(status, "needs_review")) return "! needs review";
    return NULL;
}

struct json_t *pluginsaut_summary_lines(const struct json_t *outcome) {
    json_t *lines = json_array();
    const json_t *results = outcome ? json_obj_get(outcome, "results") : NULL;
    const json_t *skipped = outcome ? json_obj_get(outcome, "skipped") : NULL;
    size_t nr = (results && results->type == JSON_ARRAY) ? json_len(results) : 0;
    int any_review = 0, any_updated = 0;
    char buf[4096];
    for (size_t i = 0; i < nr; ++i) {
        json_t *r = json_get(results, i);
        if (!r || r->type != JSON_OBJECT) continue;
        const char *status = jstr(json_obj_get(r, "status"));
        const char *name = jstr(json_obj_get(r, "name"));
        const char *detail = jstr(json_obj_get(r, "detail"));
        const char *mark = mark_for(status);
        if (!mark) continue;
        if (!strcmp(status, "needs_review")) any_review = 1;
        if (!strcmp(status, "updated")) any_updated = 1;
        snprintf(buf, sizeof(buf), "  %s  %s%s%s%s", mark, name,
                 detail && *detail ? "  (" : "", detail && *detail ? detail : "",
                 detail && *detail ? ")" : "");
        json_append(lines, json_string(buf));
    }
    size_t ns = (skipped && skipped->type == JSON_ARRAY) ? json_len(skipped) : 0;
    for (size_t i = 0; i < ns; ++i) {
        json_t *s = json_get(skipped, i);
        if (!s || s->type != JSON_OBJECT) continue;
        snprintf(buf, sizeof(buf), "  - skipped %s: %s",
                 jstr(json_obj_get(s, "name")), jstr(json_obj_get(s, "reason")));
        json_append(lines, json_string(buf));
    }
    if (nr == 0) json_append(lines, json_string("  No plugin updates to apply."));
    if (any_review)
        json_append(lines, json_string(
            "  Review those with `hermes plugins update <name>` in a terminal."));
    if (any_updated)
        json_append(lines, json_string(
            "  Updated plugins were reloaded: MCP servers and skills are live now, Python tools load in new sessions."));
    return lines;
}

struct json_t *pluginsaut_slash_action(const struct json_t *parts,
                                       const struct json_t *outcome,
                                       const struct json_t *state) {
    json_t *out = json_array();
    size_t np = (parts && parts->type == JSON_ARRAY) ? json_len(parts) : 0;
    if (np == 0) return out;
    const char *first = jstr(json_get(parts, 0));
    if (!strcmp(first, "update")) {
        if (np > 1) {
            json_append(out, json_string(
                "Usage: /plugins update (updates every plugin that has an update; "
                "update one with `hermes plugins update <name>` in a terminal)"));
        } else {
            json_append(out, json_string("Plugin updates:"));
            json_t *lines = pluginsaut_summary_lines(outcome);
            for (size_t i = 0; i < json_len(lines); ++i)
                json_append(out, json_copy(json_get(lines, i)));
            json_free(lines);
        }
        return out;
    }
    char *arg = np > 1 ? dup_lower(jstr(json_get(parts, 1))) : dupstr("");
    if (strcmp(arg, "") && strcmp(arg, "on") && strcmp(arg, "off")) {
        json_append(out, json_string("Usage: /plugins auto-update [on|off]"));
        free(arg);
        return out;
    }
    free(arg);
    int auto_apply = state && json_obj_get(state, "auto_apply")
                     && json_obj_get(state, "auto_apply")->type == JSON_BOOL
                     && json_obj_get(state, "auto_apply")->bool_val;
    char hours[64];
    snprintf(hours, sizeof(hours), "%g", state ? json_get_num(state, "check_hours", 0.0) : 0.0);
    char line[512];
    snprintf(line, sizeof(line),
             "Plugin auto-update: %s (gateway check every %sh; updates load in new sessions)",
             auto_apply ? "on" : "off", hours);
    json_append(out, json_string(line));
    return out;
}
