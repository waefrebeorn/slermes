/*
 * plugins_cmd_update_all.c - C11 port of upstream
 * hermes_cli/plugins_cmd_update_all.py (gate w26_pluginscmd_167cdc).
 *
 * PoP: w26_pluginscmd_167cdc_plan_update_all @ hermes_cli/plugins_cmd_update_all.py:plan_update_all
 * PoP: w26_pluginscmd_167cdc_short @ hermes_cli/plugins_cmd_update_all.py:_short
 * PoP: w26_pluginscmd_167cdc_constants @ hermes_cli/plugins_cmd_update_all.py:<module constants>
 *
 * plan_update_all's only imports are policies over the row itself
 * (held_for_newer_hermes, held_with_ref): the feed probe, the install
 * directory and update_one all stay in the caller, and the rows plus the
 * install records arrive as values.
 *
 * held_for_newer_hermes is the one version-dependent branch.  It answers None
 * unless requires_hermes_error rejects the running Hermes, and the running
 * version this port is proven against reports "unknown", which every spec
 * satisfies -- so a row that names min_hermes is movable here, exactly as the
 * live function decides in that environment.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_pluginscmd_167cdc.h"

/* HELD_WITH_REF (plugins_auto_update): the live text, {name} placeholder and all. */
#define PC_HELD_WITH_REF \
    "held with --ref (installed off the catalog pin); `hermes plugins update {name}` moves it"
#define PC_HELD_PLACEHOLDER "{name}"

/* _UPDATE_KIND: the classes `hermes plugins update <name>` can drive. */
static const struct { const char *klass; const char *how; } PC_UPDATE_KIND[] = {
    {"catalog", "catalog re-pin"},
    {"git", "git pull"},
};
#define PC_UPDATE_KIND_N 2

/* Python truthiness of a JSON value. */
static int pc_truthy(const json_t *v) {
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

/* `value is True` -- a bool, not 1 and not "true". */
static int pc_is_true(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}

/* `value is None` -- absent or an explicit null. */
static int pc_is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

/* str(value) for the shapes a reason/flag can hold. */
static char *pc_text(const json_t *v) {
    char buf[64];
    char *dumped;
    if (!v) return strdup("None");
    switch (v->type) {
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return strdup(buf);
    case JSON_NULL:   return strdup("None");
    default:
        dumped = json_dumps(v, 0);
        return strdup(dumped ? dumped : "");
    }
}

/* records: the install record for `name`, or NULL. */
static const json_t *pc_install(const json_t *installs, const char *name) {
    if (!installs || installs->type != JSON_OBJECT || !name) return NULL;
    return json_obj_get(installs, name);
}

/* plugins_auto_update.held_with_ref: pinned with an empty catalog.pin. */
static int pc_held_with_ref(const json_t *record) {
    const json_t *block;
    if (!record || record->type != JSON_OBJECT) return 0;
    block = json_obj_get(record, "catalog");
    if (!block || block->type != JSON_OBJECT) return 0;
    if (!pc_is_true(json_obj_get(record, "pinned"))) return 0;
    return !pc_truthy(json_obj_get(block, "pin"));
}

/* The class name of a row, as the f-string renders it. */
static char *pc_klass(const json_t *row) {
    return pc_text(json_obj_get(row, "klass"));
}

static int pc_kind_supported(const char *klass) {
    int i;
    for (i = 0; i < PC_UPDATE_KIND_N; i++)
        if (!strcmp(klass, PC_UPDATE_KIND[i].klass)) return 1;
    return 0;
}

json_t *w26_pluginscmd_167cdc_plan_update_all(const json_t *req) {
    const json_t *results = req ? json_obj_get(req, "results") : NULL;
    const json_t *installs = req ? json_obj_get(req, "installs") : NULL;
    json_t *movable = json_array();
    json_t *skipped = json_array();
    double current = 0.0;
    json_t *out;
    size_t i;

    if (!results || results->type != JSON_ARRAY) {
        out = json_object();
        json_set(out, "movable", movable);
        json_set(out, "skipped", skipped);
        json_set(out, "current", json_number(0.0));
        return out;
    }

    for (i = 0; i < json_len(results); i++) {
        const json_t *row = json_get(results, i);
        const json_t *needs, *available, *min_hermes, *reason;
        const char *name;
        char *klass = NULL;
        char *why = NULL;

        if (!row || row->type != JSON_OBJECT) continue;
        name = json_get_str(row, "name", "");
        needs = json_obj_get(row, "needs_fixing");
        available = json_obj_get(row, "update_available");
        min_hermes = json_obj_get(row, "min_hermes");
        reason = json_obj_get(row, "reason");
        klass = pc_klass(row);

        /* The live chain is if/elif over `update_available is True`, then a
         * reason test; `decided` carries the elif. */
        {
            int is_true = pc_is_true(available);
            int decided = 0;
            if (!decided && pc_truthy(needs)) {
                char *detail = pc_text(needs);
                size_t need = strlen("needs fixing: ") + strlen(detail) + 1;
                why = (char *)malloc(need);
                if (why) snprintf(why, need, "needs fixing: %s", detail);
                free(detail);
                decided = 1;
            }
            if (!decided && is_true && pc_truthy(min_hermes)) {
                /* held_for_newer_hermes is None here: the running version
                 * reports "unknown", which every version spec satisfies, so
                 * this branch skips nothing and the row keeps falling. */
            }
            if (!decided && is_true && pc_held_with_ref(pc_install(installs, name))) {
                const char *slot = strstr(PC_HELD_WITH_REF, PC_HELD_PLACEHOLDER);
                size_t head = slot ? (size_t)(slot - PC_HELD_WITH_REF) : strlen(PC_HELD_WITH_REF);
                const char *tail = slot ? slot + strlen(PC_HELD_PLACEHOLDER) : "";
                size_t need = head + strlen(name) + strlen(tail) + 1;
                why = (char *)malloc(need);
                if (why) {
                    memcpy(why, PC_HELD_WITH_REF, head);
                    why[head] = '\0';
                    strcat(why, name);
                    strcat(why, tail);
                }
                decided = 1;
            }
            if (!decided && is_true && pc_kind_supported(klass)) {
                json_append(movable, json_string(name));
                decided = 1;
            }
            if (!decided && is_true) {
                size_t need = strlen(klass) + strlen(" distribution; Hermes does not upgrade it") + 1;
                why = (char *)malloc(need);
                if (why) snprintf(why, need, "%s distribution; Hermes does not upgrade it", klass);
                decided = 1;
            }
            if (!decided && pc_is_none(available)) {
                why = pc_truthy(reason) ? pc_text(reason) : strdup("update status unknown");
                decided = 1;
            }
            if (!decided && pc_truthy(reason)) {
                why = pc_text(reason);
                decided = 1;
            }
            if (!decided) current += 1.0;
        }

        if (why) {
            json_t *pair = json_array();
            json_append(pair, json_string(name));
            json_append(pair, json_string(why));
            json_append(skipped, pair);
            free(why);
        }
        free(klass);
    }

    out = json_object();
    json_set(out, "movable", movable);
    json_set(out, "skipped", skipped);
    json_set(out, "current", json_number(current));
    return out;
}

/* Python str.strip() over ASCII whitespace. */
static char *pc_strip(const char *s) {
    const char *end;
    size_t n;
    char *out;
    if (!s) return strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    n = (size_t)(end - s);
    out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

json_t *w26_pluginscmd_167cdc_short(const json_t *req) {
    char *stripped = pc_strip(req ? json_get_str(req, "reason", "") : "");
    char *out_text;
    size_t len;
    json_t *out;

    if (!stripped[0]) {
        free(stripped);
        return json_string("");
    }
    /* first = stripped.splitlines()[0] */
    {
        size_t stop = strcspn(stripped, "\r\n");
        stripped[stop] = '\0';
    }
    len = strlen(stripped);
    if (len <= 200) {
        out = json_string(stripped);
        free(stripped);
        return out;
    }
    out_text = (char *)malloc(201);
    if (!out_text) { free(stripped); return json_string(""); }
    memcpy(out_text, stripped, 197);
    memcpy(out_text + 197, "...", 4);
    out = json_string(out_text);
    free(out_text);
    free(stripped);
    return out;
}

json_t *w26_pluginscmd_167cdc_constants(void) {
    json_t *o = json_object();
    json_t *kinds = json_object();
    int i;
    for (i = 0; i < PC_UPDATE_KIND_N; i++)
        json_set(kinds, PC_UPDATE_KIND[i].klass, json_string(PC_UPDATE_KIND[i].how));
    json_set(o, "update_kind", kinds);
    json_set(o, "held_with_ref", json_string(PC_HELD_WITH_REF));
    return o;
}
