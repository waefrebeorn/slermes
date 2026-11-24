/*
 * search.c -- real C11 port of the connector leg of tool search of upstream
 * Hermes tools/connectors/search.py.
 *
 * PoP: w21_search_eff38d_format_name @ tools/connectors/gateway/names.py:format_connector_name
 * PoP: w21_search_eff38d_vendor_candidates @ tools/connectors/gateway/names.py:vendor_slug_candidates
 * PoP: w21_search_eff38d_is_connector_name @ tools/connectors/gateway/names.py:is_connector_name
 * PoP: w21_search_eff38d_model_parameters @ tools/connectors/gateway/files.py:model_parameters
 * PoP: w21_search_eff38d_connections_in_scope @ tools/connectors/search.py:connections_in_scope
 * PoP: w21_search_eff38d_unavailable @ tools/connectors/search.py:connectors_unavailable
 * PoP: w21_search_eff38d_entries_by_group @ tools/connectors/search.py:connector_entries_by_group
 * PoP: w21_search_eff38d_remote_schemas @ tools/connectors/search.py:remote_schemas_for
 *
 * The bridge legs (connector_search / connector_describe) are the request's
 * scripted (failure, payload); the BM25 `_tokens` of a catalog entry stay
 * upstream with tools.tool_search_catalog._tokenize.
 */
#include "hermes_json.h"
#include "noc_w21_search_eff38d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* tools/connectors/gateway/bridge.py */
static const char SE_NOT_SET_UP[] = "not_set_up";
static const char SE_SIGN_IN_EXPIRED[] = "sign_in_expired";
static const char SE_UNREACHABLE[] = "unreachable";
static const char SE_NOT_SET_UP_MESSAGE[] =
    "Hosted connectors are not set up for this user yet. Call manage_connections to connect"
    " the app first; that sets them up.";
/* tools/connectors/gateway/names.py */
static const char SE_PREFIX[] = "connectors__";
static const char SE_MANAGE_NAME[] = "manage_connections";
/* tools/connectors/gateway/files.py */
static const char SE_FILE_INPUT_NOTE[] = "Pass a local file path.";

/* --- helpers ------------------------------------------------------------- */

static char *se_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *se_lower(const char *s) {
    size_t i, n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        char c = s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[n] = '\0';
    return out;
}

static char *se_upper(const char *s) {
    size_t i, n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        char c = s[i];
        out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }
    out[n] = '\0';
    return out;
}

/* str(value or "") for the scalar shapes a schema field carries. */
static char *se_str_or_empty(const json_t *v) {
    char buf[64];
    if (!v) return se_dup("");
    switch (v->type) {
    case JSON_NULL:   return se_dup("");
    case JSON_BOOL:   return se_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return se_dup(buf);
    case JSON_STRING: return se_dup(v->str_val ? v->str_val : "");
    case JSON_ARRAY:  return se_dup("[]");
    case JSON_OBJECT: return se_dup("{}");
    }
    return se_dup("");
}

static int se_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* --- names codec --------------------------------------------------------- */

char *w21_search_eff38d_format_name(const char *connector, const char *tool) {
    char *upper;
    const char *slug = tool ? tool : "";
    char *out;
    size_t need;
    if (!connector) connector = "";
    upper = se_upper(connector);
    if (!upper) return NULL;
    {
        size_t plen = strlen(upper) + 1;
        char *prefix = (char *)malloc(plen + 1);
        if (prefix) {
            snprintf(prefix, plen + 1, "%s_", upper);
            if (!strncmp(slug, prefix, plen)) slug += plen;   /* removeprefix */
            free(prefix);
        }
    }
    need = strlen(SE_PREFIX) + strlen(connector) + strlen(slug) + 8;
    out = (char *)malloc(need);
    if (out) snprintf(out, need, "%s%s__%s", SE_PREFIX, connector, slug);
    free(upper);
    return out;
}

json_t *w21_search_eff38d_vendor_candidates(const char *connector, const char *tool) {
    char *upper = se_upper(connector ? connector : "");
    char *first;
    json_t *out = json_array();
    size_t need;
    if (!upper) return out;
    need = strlen(upper) + strlen(tool ? tool : "") + 4;
    first = (char *)malloc(need);
    if (first) snprintf(first, need, "%s_%s", upper, tool ? tool : "");
    json_append(out, json_string(first ? first : ""));
    json_append(out, json_string(tool ? tool : ""));
    free(first);
    free(upper);
    return out;
}

int w21_search_eff38d_is_connector_name(const json_t *name) {
    if (!name || name->type != JSON_STRING || !name->str_val) return 0;
    return !strncmp(name->str_val, SE_PREFIX, strlen(SE_PREFIX));
}

/* --- model_parameters ---------------------------------------------------- */

static json_t *se_model_schema(const json_t *node) {
    json_t *out;
    size_t i;

    if (!node) return json_null();
    if (node->type == JSON_ARRAY) {
        out = json_array();
        for (i = 0; i < node->c.count; i++)
            json_append(out, se_model_schema(node->c.items[i]));
        return out;
    }
    if (node->type != JSON_OBJECT) return json_copy(node);

    {
        json_t *uploadable = json_obj_get(node, "file_uploadable");
        if (uploadable && uploadable->type == JSON_BOOL && uploadable->bool_val) {
            char *description = se_str_or_empty(json_obj_get(node, "description"));
            char *joined;
            size_t need;
            size_t n;
            need = strlen(description) + strlen(SE_FILE_INPUT_NOTE) + 4;
            joined = (char *)malloc(need);
            if (joined) snprintf(joined, need, "%s %s", description, SE_FILE_INPUT_NOTE);
            n = joined ? strlen(joined) : 0;
            while (n && (joined[n - 1] == ' ' || joined[n - 1] == '\t' || joined[n - 1] == '\n' ||
                         joined[n - 1] == '\r')) joined[--n] = '\0';
            {
                char *trimmed = joined;
                while (trimmed && (*trimmed == ' ' || *trimmed == '\t' ||
                                   *trimmed == '\n' || *trimmed == '\r')) trimmed++;
                out = json_object();
                json_set(out, "type", json_string("string"));
                json_set(out, "description", json_string(trimmed ? trimmed : ""));
            }
            free(joined);
            free(description);
            return out;
        }
    }
    out = json_object();
    for (i = 0; i < node->c.count; i++) {
        const char *key = node->c.keys[i];
        if (!key) continue;
        json_set(out, key, se_model_schema(node->c.items[i]));
    }
    return out;
}

json_t *w21_search_eff38d_model_parameters(const json_t *input_schema) {
    if (!input_schema || input_schema->type != JSON_OBJECT) return json_object();
    return se_model_schema(input_schema);
}

/* --- scope --------------------------------------------------------------- */

/* `_fn(td).get("name")` for one tool def; -1 when the shape is not a mapping. */
static int se_fn_name_is(const json_t *td, const char *wanted, int *broken) {
    json_t *fn, *name;
    if (!td || td->type != JSON_OBJECT) { *broken = 1; return 0; }
    fn = json_obj_get(td, "function");
    if (!fn || fn->type == JSON_NULL) return 0;               /* `or {}` */
    if (fn->type != JSON_OBJECT) { *broken = 1; return 0; }
    name = json_obj_get(fn, "name");
    if (!name || name->type != JSON_STRING || !name->str_val) return 0;
    return !strcmp(name->str_val, wanted);
}

int w21_search_eff38d_connections_in_scope(const json_t *tool_defs) {
    size_t i;
    if (!tool_defs || tool_defs->type != JSON_ARRAY) return 0;
    for (i = 0; i < tool_defs->c.count; i++) {
        int broken = 0;
        if (se_fn_name_is(tool_defs->c.items[i], SE_MANAGE_NAME, &broken)) return 1;
    }
    return 0;
}

/* --- connectors_unavailable ---------------------------------------------- */

json_t *w21_search_eff38d_unavailable(const json_t *req) {
    const char *failure = json_get_str(req, "failure", "");
    const char *verb = json_get_str(req, "verb", "");
    json_t *names = json_obj_get(req, "names");
    json_t *last_failure = json_obj_get(req, "last_failure");
    json_t *out = json_object();

    if (!strcmp(failure, SE_NOT_SET_UP)) {
        const char *tail =
            " Hosted apps (Gmail, Calendar, Notion and more) are then searchable."
            " If the request is better served by the tools above, use those instead.";
        char *hint;
        size_t need = strlen(SE_NOT_SET_UP_MESSAGE) + strlen(tail) + 1;
        hint = (char *)malloc(need);
        if (hint) snprintf(hint, need, "%s%s", SE_NOT_SET_UP_MESSAGE, tail);
        if (se_truthy(last_failure)) {
            char *last = se_str_or_empty(last_failure);
            char *prefix = " The last attempt failed, so tell the user before retrying: ";
            size_t join_need = strlen(hint ? hint : "") + strlen(prefix) + strlen(last) + 4;
            char *joined = (char *)malloc(join_need);
            if (joined) snprintf(joined, join_need, "%s%s%s", hint ? hint : "", prefix, last);
            free(hint);
            free(last);
            hint = joined;
        }
        json_set(out, "status", json_string(SE_NOT_SET_UP));
        json_set(out, "hint", json_string(hint ? hint : ""));
        if (se_truthy(names)) json_set(out, "names", json_copy(names));
        free(hint);
        return out;
    }
    {
        const char *head = "Hosted connector tools could not be ";
        const char *tail = " right now. Do not conclude the app is missing.";
        const char *extra = !strcmp(failure, SE_SIGN_IN_EXPIRED)
                                ? " The user must sign in to Nous again." : "";
        size_t need = strlen(head) + strlen(verb) + strlen(tail) + strlen(extra) + 4;
        char *hint = (char *)malloc(need);
        if (hint) snprintf(hint, need, "%s%s%s%s", head, verb, tail, extra);
        json_set(out, "status", json_string("unavailable"));
        json_set(out, "reason", json_string(failure));
        json_set(out, "hint", json_string(hint ? hint : ""));
        if (se_truthy(names)) json_set(out, "names", json_copy(names));
        free(hint);
        return out;
    }
}

/* --- catalog entry ------------------------------------------------------- */

static json_t *se_entry(const char *name, const char *connector, const char *slug,
                        const json_t *schema) {
    char *description = se_str_or_empty(json_obj_get(schema, "description"));
    json_t *parameters = w21_search_eff38d_model_parameters(json_obj_get(schema, "input_schema"));
    json_t *fn = json_object();
    json_t *tool_def = json_object();
    json_t *entry = json_object();

    json_set(fn, "name", json_string(name));
    json_set(fn, "description", json_string(description));
    json_set(fn, "parameters", parameters);
    json_set(tool_def, "type", json_string("function"));
    json_set(tool_def, "function", fn);

    json_set(entry, "name", json_string(name));
    json_set(entry, "description", json_string(description));
    json_set(entry, "schema", tool_def);
    json_set(entry, "source", json_string("connectors"));
    json_set(entry, "source_name", json_string(connector));
    free(description);
    (void)slug;
    return entry;
}

/* --- connector_entries_by_group ------------------------------------------ */

typedef struct {
    char *name;
    char *slug;
    json_t *entry;
} se_pick;

static void se_picks_free(se_pick *picks, size_t count) {
    size_t i;
    for (i = 0; i < count; i++) {
        free(picks[i].name);
        free(picks[i].slug);
        json_free(picks[i].entry);
    }
    free(picks);
}

json_t *w21_search_eff38d_entries_by_group(const json_t *req) {
    json_t *queries = json_obj_get(req, "queries");
    json_t *leg = json_obj_get(req, "leg");
    size_t qcount = (queries && queries->type == JSON_ARRAY) ? queries->c.count : 0;
    json_t *per_query = json_array();
    json_t *out = json_object();
    const char *leg_failure = leg ? json_get_str(leg, "failure", "") : "";
    json_t *payload;
    json_t *schemas;
    json_t *groups;
    size_t position;

    for (position = 0; position < qcount; position++) json_append(per_query, json_array());

    /* `if leg.failure: return per_query, leg.failure` */
    if (leg_failure && leg_failure[0]) {
        json_set(out, "per_query", per_query);
        json_set(out, "failure", json_string(leg_failure));
        return out;
    }
    payload = leg ? json_obj_get(leg, "payload") : NULL;
    if (!se_truthy(payload)) {
        payload = NULL;                                   /* `leg.payload or {}` */
    }
    schemas = payload ? json_obj_get(payload, "schemas") : NULL;
    groups = payload ? json_obj_get(payload, "results") : NULL;
    if (!schemas || schemas->type != JSON_OBJECT || !groups || groups->type != JSON_ARRAY) {
        json_set(out, "per_query", per_query);
        json_set(out, "failure",
                 se_truthy(payload) ? json_string(SE_UNREACHABLE) : json_null());
        return out;
    }

    for (position = 0; position < groups->c.count && position < qcount; position++) {
        json_t *group = groups->c.items[position];
        json_t *echoed;
        json_t *slugs;
        se_pick *picks = NULL;
        size_t pick_count = 0;
        size_t i;
        const char *query = (queries->c.items[position] &&
                             queries->c.items[position]->type == JSON_STRING)
                                ? queries->c.items[position]->str_val : NULL;
        json_t *bucket;

        if (!group || group->type != JSON_OBJECT) continue;
        echoed = json_obj_get(group, "use_case");
        if (echoed && echoed->type == JSON_STRING && echoed->str_val && echoed->str_val[0] &&
            (query == NULL || strcmp(echoed->str_val, query)))
            continue;
        slugs = json_obj_get(group, "tools");
        if (!slugs || slugs->type != JSON_ARRAY) slugs = NULL;

        for (i = 0; slugs && i < slugs->c.count; i++) {
            json_t *slug_node = slugs->c.items[i];
            json_t *schema;
            char *slug, *connector_lower, *name;
            size_t j;
            int prior_index = -1;

            if (!slug_node) continue;
            if (slug_node->type == JSON_OBJECT || slug_node->type == JSON_ARRAY) {
                /* `schemas.get(<unhashable>)` raises: the whole leg is unreachable */
                size_t k;
                se_picks_free(picks, pick_count);
                for (k = 0; k < qcount; k++) json_array_set(per_query, k, json_array());
                json_set(out, "per_query", per_query);
                json_set(out, "failure", json_string(SE_UNREACHABLE));
                return out;
            }
            if (slug_node->type != JSON_STRING) continue;        /* cannot match a key */
            schema = json_obj_get(schemas, slug_node->str_val);
            if (!schema || schema->type != JSON_OBJECT) continue;
            if (!se_truthy(json_obj_get(schema, "connector"))) continue;

            slug = se_dup(slug_node->str_val);
            connector_lower = se_lower(se_str_or_empty(json_obj_get(schema, "connector")));
            name = w21_search_eff38d_format_name(connector_lower, slug);
            for (j = 0; j < pick_count; j++) {
                if (picks[j].name && name && !strcmp(picks[j].name, name)) { prior_index = (int)j; break; }
            }
            if (prior_index >= 0) {
                if (strcmp(picks[prior_index].slug, slug)) {
                    /* collision: the composed name must reach this very slug */
                    const char *rest = NULL;
                    const char *first = name ? strstr(name, SE_PREFIX) : NULL;
                    const char *second;
                    char *reaches;
                    size_t need;
                    if (first) {
                        second = strstr(first + strlen(SE_PREFIX), "__");
                        rest = second ? second + 2 : "";
                    } else {
                        rest = "";
                    }
                    need = strlen(connector_lower) + strlen(rest) + 4;
                    reaches = (char *)malloc(need);
                    if (reaches) {
                        char *upper_connector = se_upper(connector_lower);
                        snprintf(reaches, need, "%s_%s", upper_connector ? upper_connector : "",
                                 rest);
                        free(upper_connector);
                    }
                    {
                        int take = reaches && !strcmp(slug, reaches);
                        free(reaches);
                        if (!take) { free(slug); free(connector_lower); free(name); continue; }
                    }
                    json_free(picks[prior_index].entry);
                    picks[prior_index].entry =
                        se_entry(name, se_str_or_empty(json_obj_get(schema, "connector")),
                                 slug, schema);
                    free(picks[prior_index].slug);
                    picks[prior_index].slug = se_dup(slug);
                    free(slug);
                    free(connector_lower);
                    free(name);
                    continue;
                }
                free(slug);
                free(connector_lower);
                free(name);
                continue;
            }
            {
                se_pick *grown = (se_pick *)realloc(picks, (pick_count + 1) * sizeof(se_pick));
                if (!grown) { free(slug); free(connector_lower); free(name); continue; }
                picks = grown;
                picks[pick_count].name = name;
                picks[pick_count].slug = slug;
                picks[pick_count].entry =
                    se_entry(name, se_str_or_empty(json_obj_get(schema, "connector")),
                             slug, schema);
                pick_count++;
                free(connector_lower);
            }
        }

        bucket = json_array();
        for (i = 0; i < pick_count; i++) json_append(bucket, json_copy(picks[i].entry));
        json_array_set(per_query, position, bucket);
        se_picks_free(picks, pick_count);
    }

    json_set(out, "per_query", per_query);
    json_set(out, "failure", json_null());
    return out;
}

/* --- remote_schemas_for -------------------------------------------------- */

json_t *w21_search_eff38d_remote_schemas(const json_t *req) {
    json_t *names = json_obj_get(req, "names");
    json_t *tool_defs = json_obj_get(req, "tool_defs");
    json_t *leg = json_obj_get(req, "leg");
    json_t *out = json_object();
    json_t *connector_names = json_array();
    size_t i;
    int any = 0;
    const char *leg_failure = leg ? json_get_str(leg, "failure", "") : "";

    for (i = 0; names && names->type == JSON_ARRAY && i < names->c.count; i++) {
        if (w21_search_eff38d_is_connector_name(names->c.items[i])) {
            json_append(connector_names, json_copy(names->c.items[i]));
            any = 1;
        }
    }
    if (!any || !w21_search_eff38d_connections_in_scope(tool_defs)) {
        json_free(connector_names);
        json_set(out, "tools", json_object());
        json_set(out, "failure", json_null());
        return out;
    }
    json_free(connector_names);

    if (leg_failure && leg_failure[0]) {
        json_set(out, "tools", json_object());
        json_set(out, "failure", json_string(leg_failure));
        return out;
    }
    {
        json_t *payload = leg ? json_obj_get(leg, "payload") : NULL;
        json_t *tools = NULL;
        if (payload && payload->type == JSON_OBJECT) tools = json_obj_get(payload, "tools");
        if (tools && tools->type == JSON_OBJECT) {
            json_set(out, "tools", json_copy(tools));
            json_set(out, "failure", json_null());
            return out;
        }
        json_set(out, "tools", json_object());
        /* `leg.payload.get("tools")` on a non-mapping payload raises -> unreachable */
        json_set(out, "failure",
                 (payload && payload->type != JSON_OBJECT)
                     ? json_string(SE_UNREACHABLE)
                     : (se_truthy(payload) ? json_string(SE_UNREACHABLE) : json_null()));
        return out;
    }
}
