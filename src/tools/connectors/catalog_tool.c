/*
 * catalog_tool.c — C home for upstream Hermes tools/connectors/catalog_tool.py.
 * Gate w26_catalogtoo_5874af.
 *
 * PoP: w26_catalogtoo_5874af_check_args @ tools/connectors/catalog_tool.py:_check_args
 * PoP: w26_catalogtoo_5874af_dispatch @ tools/connectors/catalog_tool.py:manage_catalog
 * PoP: w26_catalogtoo_5874af_constants @ tools/connectors/catalog_tool.py:ACTIONS / ITEM_KINDS / _TOP_KEYS / _ITEM_KEYS / SEARCH_LIMIT
 *
 * The catalog resolver, the skills hub, the approval card and the installer are
 * seams (they open connections and block on a human).  What the model can get
 * wrong is entirely value-shaped -- an unknown parameter, a bad action, a
 * missing item field -- and that is what is ported: the argument validator, its
 * exact messages, and the surface gate that decides error / not-here / search /
 * install before any of those seams are touched.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_catalogtoo_5874af.h"

/* ACTIONS */
static const char *const CT_ACTIONS[] = {"search", "install"};
#define CT_ACTION_COUNT 2
/* ITEM_KINDS */
static const char *const CT_ITEM_KINDS[] = {"plugin", "skill"};
#define CT_ITEM_KIND_COUNT 2
/* _TOP_KEYS */
/* kept in Python sorted() order: the constants table exposes them sorted. */
static const char *const CT_TOP_KEYS[] = {"action", "items", "kind", "query", "reason"};
#define CT_TOP_KEY_COUNT 5
/* _ITEM_KEYS */
static const char *const CT_ITEM_KEYS[] = {"id", "kind"};
#define CT_ITEM_KEY_COUNT 2
/* SEARCH_LIMIT */
#define CT_SEARCH_LIMIT 10

#define CT_UNKNOWN_TAIL \
    ": manage_catalog takes action, kind, query, items and reason. " \
    "The host decides source, version, profile and settings."
#define CT_UNKNOWN_ITEM_TAIL \
    ": an item is {kind, id}. The host decides source, version, profile and settings."

static int ct_in(const char *value, const char *const *set, size_t n) {
    size_t i;
    if (!value) return 0;
    for (i = 0; i < n; i++)
        if (strcmp(value, set[i]) == 0) return 1;
    return 0;
}

/* "a, b" of the object's keys that are not in `set`, sorted (Python sorted()). */
static char *ct_unknown_keys(const json_t *obj, const char *const *set, size_t n) {
    size_t count = 0, i, k, used = 0, cap;
    const char **names;
    char *out;
    size_t need = 1;
    if (!obj || obj->type != JSON_OBJECT) return strdup("");
    cap = json_object_size(obj);
    if (cap == 0) return strdup("");
    names = (const char **)calloc(cap, sizeof(const char *));
    if (!names) return strdup("");
    for (i = 0; i < cap; i++) {
        const char *key = json_object_get_key_at(obj, i);
        if (!key || ct_in(key, set, n)) continue;
        k = count;
        while (k > 0 && strcmp(names[k - 1], key) > 0) { names[k] = names[k - 1]; k--; }
        names[k] = key;
        count++;
    }
    for (i = 0; i < count; i++) need += strlen(names[i]) + 2;
    out = (char *)malloc(need);
    if (out) {
        out[0] = '\0';
        for (i = 0; i < count; i++) {
            if (used) strcat(out, ", ");
            strcat(out, names[i]);
            used += strlen(names[i]) + 2;
        }
    }
    free(names);
    return out ? out : strdup("");
}

static int ct_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str(v or "").strip() for an item id. */
static char *ct_id_text(const json_t *v) {
    char *out;
    size_t n;
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("");
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        while (*s && ct_space(*s)) s++;
        n = strlen(s);
        while (n > 0 && ct_space(s[n - 1])) n--;
        out = (char *)malloc(n + 1);
        if (out) { memcpy(out, s, n); out[n] = '\0'; }
        return out ? out : strdup("");
    }
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == 0.0) return strdup("");
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return strdup(buf);
    }
    return strdup("");
}

json_t *w26_catalogtoo_5874af_check_args(const json_t *args) {
    const json_t *action, *kind, *items;
    char *unknown;

    if (!args || args->type != JSON_OBJECT) return json_null();
    unknown = ct_unknown_keys(args, CT_TOP_KEYS, CT_TOP_KEY_COUNT);
    if (unknown && unknown[0]) {
        size_t need = strlen("unknown parameter(s) ") + strlen(unknown) + strlen(CT_UNKNOWN_TAIL) + 1;
        char *msg = (char *)malloc(need);
        json_t *out;
        if (msg) snprintf(msg, need, "unknown parameter(s) %s%s", unknown, CT_UNKNOWN_TAIL);
        out = json_string(msg ? msg : "");
        free(msg);
        free(unknown);
        return out;
    }
    free(unknown);

    action = json_obj_get(args, "action");
    if (!action || action->type != JSON_STRING
        || !ct_in(action->str_val, CT_ACTIONS, CT_ACTION_COUNT))
        return json_string("action must be one of search, install.");

    kind = json_obj_get(args, "kind");
    if (kind && kind->type != JSON_NULL
        && !(kind->type == JSON_STRING && ct_in(kind->str_val, CT_ITEM_KINDS, CT_ITEM_KIND_COUNT)))
        return json_string("kind must be one of plugin, skill.");

    if (strcmp(action->str_val, "search") == 0) return json_null();

    items = json_obj_get(args, "items");
    if (!items || items->type != JSON_ARRAY || json_len(items) == 0)
        return json_string("install needs 'items': "
                           "[{\"kind\": \"plugin\", \"id\": \"<catalog id>\"}, ...].");

    for (size_t i = 0; i < json_len(items); i++) {
        const json_t *item = json_get(items, i);
        const json_t *item_kind, *item_id;
        char *extra, *id_text;
        if (!item || item->type != JSON_OBJECT)
            return json_string("every item is an object {kind, id}.");
        extra = ct_unknown_keys(item, CT_ITEM_KEYS, CT_ITEM_KEY_COUNT);
        if (extra && extra[0]) {
            size_t need = strlen("unknown item field(s) ") + strlen(extra)
                          + strlen(CT_UNKNOWN_ITEM_TAIL) + 1;
            char *msg = (char *)malloc(need);
            json_t *out;
            if (msg) snprintf(msg, need, "unknown item field(s) %s%s", extra, CT_UNKNOWN_ITEM_TAIL);
            out = json_string(msg ? msg : "");
            free(msg);
            free(extra);
            return out;
        }
        free(extra);
        item_kind = json_obj_get(item, "kind");
        item_id = json_obj_get(item, "id");
        id_text = ct_id_text(item_id);
        if (!item_kind || item_kind->type != JSON_STRING
            || !ct_in(item_kind->str_val, CT_ITEM_KINDS, CT_ITEM_KIND_COUNT)
            || !id_text || !id_text[0]) {
            free(id_text);
            return json_string("every item needs kind (plugin or skill) and a non-empty id.");
        }
        free(id_text);
    }
    return json_null();
}

json_t *w26_catalogtoo_5874af_dispatch(const json_t *args, int card_surface, int has_callback) {
    json_t *error = w26_catalogtoo_5874af_check_args(args);
    json_t *out = json_object();
    if (error && error->type != JSON_NULL) {
        json_set(out, "route", json_string("error"));
        json_set(out, "error", error);
        return out;
    }
    json_free(error);
    if (!has_callback || !card_surface) {
        json_set(out, "route", json_string("not_here"));
        json_set(out, "error", json_string(CT_NOT_HERE));
        return out;
    }
    if (strcmp(json_get_str(args, "action", ""), "search") == 0)
        json_set(out, "route", json_string("search"));
    else
        json_set(out, "route", json_string("install"));
    return out;
}

json_t *w26_catalogtoo_5874af_constants(void) {
    json_t *o = json_object();
    json_t *actions = json_array(), *kinds = json_array(), *top = json_array(), *item = json_array();
    size_t i;
    for (i = 0; i < CT_ACTION_COUNT; i++) json_append(actions, json_string(CT_ACTIONS[i]));
    for (i = 0; i < CT_ITEM_KIND_COUNT; i++) json_append(kinds, json_string(CT_ITEM_KINDS[i]));
    for (i = 0; i < CT_TOP_KEY_COUNT; i++) json_append(top, json_string(CT_TOP_KEYS[i]));
    for (i = 0; i < CT_ITEM_KEY_COUNT; i++) json_append(item, json_string(CT_ITEM_KEYS[i]));
    json_set(o, "actions", actions);
    json_set(o, "item_kinds", kinds);
    json_set(o, "search_limit", json_number((double)CT_SEARCH_LIMIT));
    json_set(o, "top_keys", top);
    json_set(o, "item_keys", item);
    return o;
}
