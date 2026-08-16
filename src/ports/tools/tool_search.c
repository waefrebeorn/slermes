/*
 * tool_search.c — Progressive tool disclosure ("tool search") for Hermes C.
 *
 * Port of Python tools/tool_search.py (735 lines, 22 functions, 3 classes).
 *
 * Implements BM25-catalog-based deferred tool search. When the model's
 * context window would be dominated by non-core tool schemas, the three
 * bridge tools (tool_search, tool_describe, tool_call) replace them and
 * allow on-demand discovery via BM25 full-text search.
 *
 * MIT License — WuBu Slermes Project
 */

/* strcasestr and friends are GNU extensions — musl (alpine) needs
 * _GNU_SOURCE to declare them; glibc exposes them by default. */
#define _GNU_SOURCE
#include "hermes_core_types.h"
#include "hermes_tool_search.h"
#include "hermes_json.h"

#include <math.h>
#include <strings.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "registry.h"

#ifndef HAVE_STRCASESTR
/* Portable strcasestr replacement */
static const char *portable_strcasestr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncasecmp(haystack, needle, nlen) == 0)
            return haystack;
        haystack++;
    }
    return NULL;
}
#define strcasestr portable_strcasestr
#endif

/* ═════════════════════════════════════════════════════════════════════
 *  Token estimation
 * ═════════════════════════════════════════════════════════════════════ */

/* Port of Python: estimate_tokens_from_schemas() */
int tool_search_estimate_tokens(const char *schema_json) {
    if (!schema_json || !schema_json[0]) return 0;
    int chars = (int)strlen(schema_json);
    double tokens = ceil((double)chars / CHARS_PER_TOKEN);
    return (int)tokens;
}

/* ═════════════════════════════════════════════════════════════════════
 *  Tokenization
 * ═════════════════════════════════════════════════════════════════════ */

/* PoP: _tokenize @ tools/tool_search.py:_tokenize */
/* Port of Python: _tokenize() — extract lowercase alphanumeric tokens */
int tokenize(const char *text, char tokens[][64], int max_tokens) {
    if (!text || max_tokens <= 0) return 0;
    int count = 0;
    const char *p = text;
    char buf[256];

    while (*p && count < max_tokens) {
        while (*p && !isalnum((unsigned char)*p)) p++;
        if (!*p) break;

        int j = 0;
        while (*p && isalnum((unsigned char)*p) && j < (int)sizeof(buf) - 1) {
            buf[j++] = (char)tolower((unsigned char)*p);
            p++;
        }
        buf[j] = '\0';
        if (j > 0) {
            snprintf(tokens[count], 64, "%s", buf);
            count++;
        }
    }
    return count;
}

/* ═════════════════════════════════════════════════════════════════════
 *  Catalog building
 * ═════════════════════════════════════════════════════════════════════ */

/* Port of Python: _entry_search_text() */
void tool_search_entry_text(const char *name, const char *description,
                            const char *param_names, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';

    if (!name) name = "";
    if (!description) description = "";
    if (!param_names) param_names = "";

    char name_words[512];
    size_t nw = 0;
    for (const char *p = name; *p && nw < sizeof(name_words) - 1; p++) {
        if (*p == '_' || *p == '.' || *p == '-' || *p == ':') {
            name_words[nw++] = ' ';
        } else {
            name_words[nw++] = *p;
        }
    }
    name_words[nw] = '\0';

    snprintf(out, out_size, "%s %s %s", name_words, description, param_names);
}

/* Classify tool source by name — Port of Python: _classify_source() */
/* PoP: classify_source @ tools/tool_search.py:_classify_source */
static catalog_source_t classify_source(const char *name, char *source_name, size_t sn_size) {
    if (!name) return CATALOG_SOURCE_OTHER;
    tool_t *t = registry_find(name);
    if (t) {
        if (t->toolset[0] && strncmp(t->toolset, "mcp-", 4) == 0) {
            if (source_name) snprintf(source_name, sn_size, "%s", t->toolset);
            return CATALOG_SOURCE_MCP;
        }
        if (t->toolset[0]) {
            if (source_name) snprintf(source_name, sn_size, "%s", t->toolset);
            return CATALOG_SOURCE_PLUGIN;
        }
    }
    if (source_name) snprintf(source_name, sn_size, "%s", "");
    return CATALOG_SOURCE_OTHER;
}

/* PoP: build_catalog @ tools/tool_search.py:build_catalog */
/* Port of Python: build_catalog() */
int build_catalog(const char *tool_schemas[], int schema_count,
                              tool_catalog_t *catalog) {
    if (!catalog || !tool_schemas || schema_count <= 0) return -1;

    catalog->count = 0;
    for (int i = 0; i < schema_count && catalog->count < CATALOG_MAX_ENTRIES; i++) {
        const char *schema = tool_schemas[i];
        if (!schema || !schema[0]) continue;

        char *err = NULL;
        json_node_t *root = json_parse(schema, &err);
        if (!root) continue;

        json_node_t *func = json_obj_get(root, "function");
        if (!func) { json_free(root); continue; }

        const char *name = json_get_str(func, "name", "");
        const char *desc = json_get_str(func, "description", "");

        if (!name[0]) { json_free(root); continue; }

        catalog_entry_t *entry = &catalog->entries[catalog->count];
        memset(entry, 0, sizeof(*entry));

        snprintf(entry->name, sizeof(entry->name), "%s", name);
        snprintf(entry->description, sizeof(entry->description), "%s", desc ? desc : "");
        entry->source = classify_source(name, entry->source_name, sizeof(entry->source_name));

        char param_names[1024] = {0};
        json_node_t *params = json_obj_get(func, "parameters");
        if (params) {
            json_node_t *properties = json_obj_get(params, "properties");
            if (properties && properties->type == JSON_OBJECT) {
                size_t prop_count = json_len(properties);
                size_t pos = 0;
                for (size_t pi = 0; pi < prop_count && pos < sizeof(param_names) - 2; pi++) {
                    const char *key = properties->c.keys ? properties->c.keys[pi] : NULL;
                    if (!key) continue;
                    if (pos > 0) param_names[pos++] = ' ';
                    size_t klen = strlen(key);
                    if (pos + klen < sizeof(param_names) - 1) {
                        memcpy(param_names + pos, key, klen);
                        pos += klen;
                    }
                }
                param_names[pos] = '\0';
            }
        }

        char search_text[2048];
        tool_search_entry_text(name, desc, param_names, search_text, sizeof(search_text));
        entry->token_count = tokenize(search_text, entry->tokens, CATALOG_MAX_TOKENS);

        json_free(root);
        catalog->count++;
    }
    return catalog->count;
}

/* ═════════════════════════════════════════════════════════════════════
 *  BM25 scoring
 * ═════════════════════════════════════════════════════════════════════ */

/* PoP: _bm25_score @ tools/tool_search.py:_bm25_score */
/* Port of Python: _bm25_score() */
double bm25_score(const char *query_tokens[], int query_count,
                              const catalog_entry_t *entry,
                              const int doc_lengths[], int n_docs,
                              const int doc_freq[], int vocab_size) {
    (void)doc_lengths;
    (void)vocab_size;

    if (!entry || entry->token_count == 0) return 0.0;

    double score = 0.0;
    const int dl = entry->token_count;

    double avg_dl = 1.0;
    if (n_docs > 0) {
        long sum = 0;
        for (int i = 0; i < n_docs; i++) sum += doc_lengths[i];
        avg_dl = (double)sum / (double)n_docs;
        if (avg_dl < 1.0) avg_dl = 1.0;
    }

    for (int qi = 0; qi < query_count; qi++) {
        const char *q = query_tokens[qi];
        if (!q || !q[0]) continue;

        int tf = 0;
        for (int ti = 0; ti < entry->token_count; ti++) {
            if (strcmp(entry->tokens[ti], q) == 0) tf++;
        }
        if (tf == 0) continue;

        int df = 1;
        if (doc_freq && n_docs > 0) {
            df = doc_freq[qi];
        }
        if (df <= 0) df = 1;
        if (df > n_docs) df = n_docs;

        double idf = log(1.0 + (double)(n_docs - df + 0.5) / (double)(df + 0.5));
        double norm = (double)tf * (BM25_K1 + 1.0) /
                      ((double)tf + BM25_K1 * (1.0 - BM25_B + BM25_B * (double)dl / avg_dl));
        score += idf * norm;
    }
    return score;
}

/* Port of Python: search_catalog() */
int tool_search_query(const tool_catalog_t *catalog, const char *query,
                      int limit, int *out_indices) {
    if (!catalog || !query || !out_indices || limit <= 0) return 0;

    char query_tokens[CATALOG_MAX_TOKENS][64];
    int qt_count = tokenize(query, query_tokens, CATALOG_MAX_TOKENS);
    if (qt_count == 0) return 0;

    if (limit > catalog->count) limit = catalog->count;

    int doc_lengths[CATALOG_MAX_ENTRIES];
    for (int i = 0; i < catalog->count; i++) {
        doc_lengths[i] = catalog->entries[i].token_count;
    }
    int n_docs = catalog->count;

    int doc_freq[CATALOG_MAX_ENTRIES] = {0};
    for (int qi = 0; qi < qt_count; qi++) {
        int df = 0;
        for (int di = 0; di < n_docs; di++) {
            for (int ti = 0; ti < catalog->entries[di].token_count; ti++) {
                if (strcmp(catalog->entries[di].tokens[ti], query_tokens[qi]) == 0) {
                    df++;
                    break;
                }
            }
        }
        doc_freq[qi] = df;
    }

    const char *qt_ptrs[CATALOG_MAX_TOKENS];
    for (int i = 0; i < qt_count; i++) {
        qt_ptrs[i] = query_tokens[i];
    }

    typedef struct {
        int idx;
        double score;
    } scored_entry_t;

    scored_entry_t scored[CATALOG_MAX_ENTRIES];
    int scored_count = 0;

    for (int i = 0; i < catalog->count; i++) {
        double s = bm25_score(qt_ptrs, qt_count, &catalog->entries[i],
                                          doc_lengths, n_docs, doc_freq, qt_count);
        if (s > 0) {
            scored[scored_count].idx = i;
            scored[scored_count].score = s;
            scored_count++;
        }
    }

    /* Substring fallback */
    if (scored_count == 0) {
        for (int i = 0; i < catalog->count && scored_count < limit; i++) {
            if (strcasestr(catalog->entries[i].name, query)) {
                scored[scored_count].idx = i;
                scored[scored_count].score = 0.1;
                scored_count++;
            }
        }
    }

    /* Sort by score descending */
    for (int i = 0; i < scored_count - 1; i++) {
        for (int j = i + 1; j < scored_count; j++) {
            if (scored[j].score > scored[i].score) {
                scored_entry_t tmp = scored[i];
                scored[i] = scored[j];
                scored[j] = tmp;
            }
        }
    }

    int result_count = (scored_count < limit) ? scored_count : limit;
    for (int i = 0; i < result_count; i++) {
        out_indices[i] = scored[i].idx;
    }
    return result_count;
}

/* ═════════════════════════════════════════════════════════════════════
 *  Threshold gate
 * ═════════════════════════════════════════════════════════════════════ */

/* PoP: should_activate @ tools/tool_search.py:should_activate */
/* Port of Python: should_activate() */
bool should_activate(const tool_search_config_t *config,
                                 int deferrable_tokens,
                                 int context_length) {
    if (!config) return false;
    if (config->enabled == TOOL_SEARCH_OFF) return false;
    if (deferrable_tokens <= 0) return false;
    if (config->enabled == TOOL_SEARCH_ON) return true;

    if (context_length <= 0) {
        return deferrable_tokens >= 20000;
    }
    int threshold_tokens = (int)((double)context_length * (config->threshold_pct / 100.0));
    return deferrable_tokens >= threshold_tokens;
}

/* ═════════════════════════════════════════════════════════════════════
 *  Bridge tool schemas
 * ═════════════════════════════════════════════════════════════════════ */

/* Port of Python: bridge_tool_schemas() */
int tool_search_bridge_schemas(int deferred_count,
                               char **out_search_schema,
                               char **out_describe_schema,
                               char **out_call_schema) {
    if (!out_search_schema || !out_describe_schema || !out_call_schema) return -1;

    char desc_search[1024];
    snprintf(desc_search, sizeof(desc_search),
        "Search %d additional tools that are loaded on demand. "
        "Returns up to ``limit`` matches with name and description. Follow "
        "with `tool_describe` to load a tool's full parameter schema, "
        "then `tool_call` to invoke it. Tools listed at the top of this "
        "system prompt are already available and do not need to be searched.",
        deferred_count);

    char desc_describe[512];
    snprintf(desc_describe, sizeof(desc_describe),
        "Load the full JSON schema for one tool returned by `tool_search`. "
        "Required before `tool_call` if the tool's parameters are unknown.");

    char desc_call[512];
    snprintf(desc_call, sizeof(desc_call),
        "Invoke a deferred tool by name with the given arguments. Argument shape "
        "matches the tool's schema (see `tool_describe`). Policy, hooks, "
        "and approvals run exactly as for any directly-listed tool.");

    /* Build search schema */
    json_node_t *search = json_object();
    json_set(search, "type", json_string("function"));
    json_node_t *sf = json_object();
    json_set(sf, "name", json_string(TOOL_SEARCH_NAME));
    json_set(sf, "description", json_string(desc_search));
    json_node_t *sp = json_object();
    json_set(sp, "type", json_string("object"));
    json_node_t *sprops = json_object();
    json_node_t *qprop = json_object();
    json_set(qprop, "type", json_string("string"));
    json_set(qprop, "description", json_string("Keywords describing the capability you need (e.g. 'create github issue')."));
    json_set(sprops, "query", qprop);
    json_node_t *lprop = json_object();
    json_set(lprop, "type", json_string("integer"));
    json_set(lprop, "description", json_string("Maximum number of results to return. Default 5."));
    json_set(sprops, "limit", lprop);
    json_set(sp, "properties", sprops);
    json_node_t *sreq = json_array();
    json_array_append(sreq, json_string("query"));
    json_set(sp, "required", sreq);
    json_set(sf, "parameters", sp);
    json_set(search, "function", sf);
    *out_search_schema = json_serialize(search);
    json_free(search);

    /* Build describe schema */
    json_node_t *describe = json_object();
    json_set(describe, "type", json_string("function"));
    json_node_t *df = json_object();
    json_set(df, "name", json_string(TOOL_DESCRIBE_NAME));
    json_set(df, "description", json_string(desc_describe));
    json_node_t *dp = json_object();
    json_set(dp, "type", json_string("object"));
    json_node_t *dprops = json_object();
    json_node_t *nprop = json_object();
    json_set(nprop, "type", json_string("string"));
    json_set(nprop, "description", json_string("Exact tool name (as returned by tool_search)."));
    json_set(dprops, "name", nprop);
    json_set(dp, "properties", dprops);
    json_node_t *dreq = json_array();
    json_array_append(dreq, json_string("name"));
    json_set(dp, "required", dreq);
    json_set(df, "parameters", dp);
    json_set(describe, "function", df);
    *out_describe_schema = json_serialize(describe);
    json_free(describe);

    /* Build call schema */
    json_node_t *call = json_object();
    json_set(call, "type", json_string("function"));
    json_node_t *cf = json_object();
    json_set(cf, "name", json_string(TOOL_CALL_NAME));
    json_set(cf, "description", json_string(desc_call));
    json_node_t *cp = json_object();
    json_set(cp, "type", json_string("object"));
    json_node_t *cprops = json_object();
    json_node_t *cnprop = json_object();
    json_set(cnprop, "type", json_string("string"));
    json_set(cnprop, "description", json_string("Exact tool name to invoke (as returned by tool_search)."));
    json_set(cprops, "name", cnprop);
    json_node_t *caprop = json_object();
    json_set(caprop, "type", json_string("object"));
    json_set(caprop, "description", json_string("Arguments for the tool, matching its schema."));
    json_set(cprops, "arguments", caprop);
    json_set(cp, "properties", cprops);
    json_node_t *creq = json_array();
    json_array_append(creq, json_string("name"));
    json_array_append(creq, json_string("arguments"));
    json_set(cp, "required", creq);
    json_set(cf, "parameters", cp);
    json_set(call, "function", cf);
    *out_call_schema = json_serialize(call);
    json_free(call);

    return 0;
}

/* ═════════════════════════════════════════════════════════════════════
 *  Assembly
 * ═════════════════════════════════════════════════════════════════════ */

/* Port of Python: assemble_tool_defs() */
char *tool_search_assemble(const char *tool_schemas[], int schema_count,
                           int context_length,
                           const tool_search_config_t *config,
                           tool_search_assembly_t *out_result) {
    if (!tool_schemas || schema_count <= 0 || !out_result) return NULL;

    tool_search_config_t local_cfg;
    if (!config) {
        local_cfg = (tool_search_config_t)TOOL_SEARCH_CONFIG_DEFAULT;
        config = &local_cfg;
    }

    memset(out_result, 0, sizeof(*out_result));

    tool_catalog_t catalog;
    build_catalog(tool_schemas, schema_count, &catalog);

    int deferrable_tokens = 0;
    for (int i = 0; i < schema_count; i++) {
        deferrable_tokens += tool_search_estimate_tokens(tool_schemas[i]);
    }

    out_result->deferred_count = catalog.count;
    out_result->deferred_tokens = deferrable_tokens;

    if (!should_activate(config, deferrable_tokens, context_length)) {
        out_result->activated = false;
        if (context_length > 0) {
            out_result->threshold_tokens = (int)((double)context_length * (config->threshold_pct / 100.0));
        }
        json_node_t *arr = json_array();
        for (int i = 0; i < schema_count; i++) {
            json_node_t *parsed = json_parse(tool_schemas[i], NULL);
            if (parsed) json_array_append(arr, parsed);
        }
        char *result = json_serialize(arr);
        json_free(arr);
        return result;
    }

    out_result->activated = true;
    out_result->threshold_tokens = (int)((double)context_length * (config->threshold_pct / 100.0));

    char *search_schema = NULL, *describe_schema = NULL, *call_schema = NULL;
    tool_search_bridge_schemas(catalog.count, &search_schema, &describe_schema, &call_schema);

    json_node_t *arr = json_array();
    for (int i = 0; i < schema_count; i++) {
        json_node_t *parsed = json_parse(tool_schemas[i], NULL);
        if (parsed) json_array_append(arr, parsed);
    }
    if (search_schema) {
        json_node_t *s = json_parse(search_schema, NULL);
        if (s) json_array_append(arr, s);
        free(search_schema);
    }
    if (describe_schema) {
        json_node_t *d = json_parse(describe_schema, NULL);
        if (d) json_array_append(arr, d);
        free(describe_schema);
    }
    if (call_schema) {
        json_node_t *c = json_parse(call_schema, NULL);
        if (c) json_array_append(arr, c);
        free(call_schema);
    }

    char *result = json_serialize(arr);
    json_free(arr);
    return result;
}

/* ═════════════════════════════════════════════════════════════════════
 *  Bridge dispatch
 * ═════════════════════════════════════════════════════════════════════ */

/* Port of Python: dispatch_tool_search() */
char *tool_search_dispatch_search(const char *query_json,
                                  const tool_catalog_t *catalog,
                                  const tool_search_config_t *config) {
    if (!query_json) {
        return strdup("{\"error\":\"query is required\"}");
    }

    char *err = NULL;
    json_node_t *args = json_parse(query_json, &err);
    if (!args) {
        return strdup("{\"error\":\"invalid query JSON\"}");
    }

    const char *query = json_get_str(args, "query", "");
    if (!query[0]) {
        json_free(args);
        return strdup("{\"error\":\"query is required\"}");
    }

    int limit = config ? config->search_default_limit : 5;
    if (json_has(args, "limit")) {
        int raw = (int)json_get_num(args, "limit", limit);
        int max_limit = config ? config->max_search_limit : 20;
        if (raw < 1) raw = 1;
        if (raw > max_limit) raw = max_limit;
        limit = raw;
    }

    json_free(args);

    int indices[CATALOG_MAX_ENTRIES];
    int hit_count = 0;
    if (catalog) {
        hit_count = tool_search_query(catalog, query, limit, indices);
    }

    json_node_t *result = json_object();
    json_set(result, "query", json_string(query));
    json_set(result, "total_available", json_number((double)(catalog ? catalog->count : 0)));

    json_node_t *matches = json_array();
    for (int i = 0; i < hit_count; i++) {
        const catalog_entry_t *e = &catalog->entries[indices[i]];
        json_node_t *hit = json_object();
        json_set(hit, "name", json_string(e->name));
        char desc_trunc[512];
        snprintf(desc_trunc, sizeof(desc_trunc), "%.400s", e->description);
        json_set(hit, "description", json_string(desc_trunc));
        json_set(hit, "source", json_string(
            e->source == CATALOG_SOURCE_MCP ? "mcp" :
            e->source == CATALOG_SOURCE_PLUGIN ? "plugin" : "other"));
        json_set(hit, "source_name", json_string(e->source_name));
        json_array_append(matches, hit);
    }
    json_set(result, "matches", matches);

    char *json_result = json_serialize(result);
    json_free(result);
    return json_result;
}

/* Port of Python: dispatch_tool_describe() */
char *tool_search_dispatch_describe(const char *name,
                                    const tool_catalog_t *catalog) {
    if (!name || !name[0]) {
        return strdup("{\"error\":\"name is required\"}");
    }
    if (!catalog) {
        char buf[1024];
        snprintf(buf, sizeof(buf),
            "{\"error\":\"'%s' is not currently available. Re-run tool_search to refresh.\"}", name);
        return strdup(buf);
    }

    for (int i = 0; i < catalog->count; i++) {
        if (strcmp(catalog->entries[i].name, name) == 0) {
            json_node_t *result = json_object();
            json_set(result, "name", json_string(name));
            json_set(result, "description", json_string(catalog->entries[i].description));
            json_set(result, "parameters", json_object());
            char *json_result = json_serialize(result);
            json_free(result);
            return json_result;
        }
    }

    char buf[1024];
    snprintf(buf, sizeof(buf),
        "{\"error\":\"'%s' is not currently available. Re-run tool_search to refresh.\"}", name);
    return strdup(buf);
}

/* Port of Python: resolve_underlying_call() */
int tool_search_resolve_call(const char *args_json,
                             char *out_name, size_t name_size,
                             char *out_args, size_t args_size,
                             char *error_msg, size_t error_size) {
    if (!args_json || !out_name || !out_args || !error_msg) return -1;

    out_name[0] = '\0';
    out_args[0] = '\0';
    error_msg[0] = '\0';

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) {
        snprintf(error_msg, error_size, "invalid args JSON");
        return -1;
    }

    const char *name = json_get_str(args, "name", "");
    if (!name[0]) {
        snprintf(error_msg, error_size, "tool_call requires a 'name' argument");
        json_free(args);
        return -1;
    }

    if (tool_search_is_bridge(name)) {
        snprintf(error_msg, error_size,
            "tool_call cannot invoke '%s' (it is itself a bridge tool)", name);
        json_free(args);
        return -1;
    }

    snprintf(out_name, name_size, "%s", name);

    json_node_t *raw_args = json_obj_get(args, "arguments");
    if (raw_args) {
        char *ser = json_serialize(raw_args);
        if (ser) {
            snprintf(out_args, args_size, "%s", ser);
            free(ser);
        }
    } else {
        snprintf(out_args, args_size, "{}");
    }

    json_free(args);
    return 0;
}

/* Port of Python: is_deferrable_tool_name() */
bool tool_search_is_deferrable(const char *name, const char *core_names) {
    if (!name || !name[0]) return false;
    if (tool_search_is_bridge(name)) return false;

    if (core_names && core_names[0]) {
        char core_copy[4096];
        snprintf(core_copy, sizeof(core_copy), "%s", core_names);
        char *token = strtok(core_copy, ",");
        while (token) {
            while (*token == ' ') token++;
            if (strcmp(token, name) == 0) return false;
            token = strtok(NULL, ",");
        }
    }
    return true;
}

/* Port of Python: scoped_deferrable_names() */
int tool_search_scoped_names(const char *tool_schemas[], int schema_count,
                             const char *core_names,
                             char out_names[][128], int *out_count, int max_names) {
    if (!tool_schemas || !out_count || !out_names) return -1;

    int count = 0;
    for (int i = 0; i < schema_count && count < max_names; i++) {
        char *err = NULL;
        json_node_t *root = json_parse(tool_schemas[i], &err);
        if (!root) continue;

        json_node_t *func = json_obj_get(root, "function");
        if (func) {
            const char *name = json_get_str(func, "name", "");
            if (name[0] && tool_search_is_deferrable(name, core_names)) {
                snprintf(out_names[count], 128, "%s", name);
                count++;
            }
        }
        json_free(root);
    }

    *out_count = count;
    return 0;
}

void tool_search_catalog_free(tool_catalog_t *catalog) {
    (void)catalog;
}

/* ═════════════════════════════════════════════════════════════════════
 *  Listing budget + helpers
 * ═════════════════════════════════════════════════════════════════════ */

/* PoP: listing_token_budget @ tools/tool_search.py:listing_token_budget */
/* Port of Python: listing_token_budget() — min(listing_max_tokens, pct% of context) */
int listing_token_budget(const tool_search_config_t *config,
                                     int context_length) {
    if (!config) return 4000;
    int pct_leg;
    if (context_length && context_length > 0) {
        pct_leg = (int)((double)context_length * (config->threshold_pct / 100.0));
    } else {
        pct_leg = 10000;
    }
    int result = pct_leg;
    if (result > config->listing_max_tokens) result = config->listing_max_tokens;
    if (result < 0) result = 0;
    return result;
}

/* PoP: _short_desc @ tools/tool_search.py:_short_desc */
/* Port of Python: _short_desc() — first sentence, clipped to max_chars */
void short_desc(const char *description, int max_chars,
                            char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!description || !*description) return;
    if (max_chars <= 0) max_chars = 60;

    /* Whitespace collapse */
    char collapsed[1024];
    size_t cl = 0;
    const char *p = description;
    while (*p && cl < sizeof(collapsed) - 2) {
        if (isspace((unsigned char)*p)) {
            if (cl > 0 && collapsed[cl - 1] != ' ') {
                collapsed[cl++] = ' ';
            }
            while (*p && isspace((unsigned char)*p)) p++;
            continue;
        }
        collapsed[cl++] = *p++;
    }
    collapsed[cl] = '\0';

    /* Find first sentence end (skip leading spaces) */
    char *sp = collapsed;
    while (*sp == ' ') sp++;
    char trunc[1024];
    size_t tl = 0;
    const char *src = sp;
    while (*src && tl < sizeof(trunc) - 1) {
        if (*src == '.' || *src == '!' || *src == '?' || *src == '\n') {
            trunc[tl++] = *src; /* include the sentence-ending char if '.' */
            src++;
            break;
        }
        trunc[tl++] = *src++;
    }
    trunc[tl] = '\0';

    /* If over budget, clip at max_chars, never mid-word */
    if ((int)tl > max_chars) {
        trunc[max_chars] = '\0';
        /* Find last space */
        char *last_sp = NULL;
        for (char *c = trunc; c < trunc + max_chars; c++) {
            if (*c == ' ') last_sp = c;
        }
        if (last_sp) {
            *last_sp = '\0';
        }
        /* Strip trailing comma/colon/space */
        char *end = trunc + strlen(trunc);
        while (end > trunc && strchr(",;: ", end[-1])) { *--end = '\0'; }
        /* Add ellipsis */
        size_t elen = strlen(trunc);
        if (elen + 2 < sizeof(trunc)) {
            trunc[elen] = '.'; trunc[elen + 1] = '.'; trunc[elen + 2] = '\0';
        }
    }

    snprintf(out, out_size, "%s", trunc);
}

/* PoP: _listing_group_label @ tools/tool_search.py:_listing_group_label */
/* Port of Python: _listing_group_label() — mcp-foo → foo */
void listing_group_label(const char *source_name,
                                     char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    const char *label = source_name && source_name[0] ? source_name : "other";
    if (strncmp(label, "mcp-", 4) == 0) {
        label += 4;
    }
    snprintf(out, out_size, "%s", label);
}

/* PoP: build_catalog_listing @ tools/tool_search.py:build_catalog_listing */
/* Port of Python: build_catalog_listing() — skills-style grouped listing */
char *tool_search_build_catalog_listing(const tool_catalog_t *catalog,
                                        int max_tokens,
                                        const char *search_name,
                                        const char *describe_name,
                                        const char *call_name) {
    if (!catalog || catalog->count == 0) return NULL;
    if (max_tokens <= 0) max_tokens = 4000;
    if (!search_name) search_name = TOOL_SEARCH_NAME;
    if (!describe_name) describe_name = TOOL_DESCRIBE_NAME;
    if (!call_name) call_name = TOOL_CALL_NAME;

    char header[512];
    snprintf(header, sizeof(header),
        "Deferred tool catalog (call schemas via `%s`, invoke via `%s`):",
        describe_name, call_name);

    /* Group entries by source label */
    char groups[CATALOG_MAX_ENTRIES][128];  /* group label per entry */
    int gcount = 0;
    char glabels[CATALOG_MAX_ENTRIES][128];
    int ngroups = 0;
    for (int i = 0; i < catalog->count; i++) {
        char label[128];
        listing_group_label(catalog->entries[i].source_name,
                                        label, sizeof(label));
        /* Map to group index */
        int gi = -1;
        for (int g = 0; g < ngroups; g++) {
            if (strcmp(glabels[g], label) == 0) { gi = g; break; }
        }
        if (gi < 0) {
            gi = ngroups;
            if (ngroups < CATALOG_MAX_ENTRIES) {
                strncpy(glabels[ngroups], label, 127);
                glabels[ngroups][127] = '\0';
                ngroups++;
            }
        }
        strncpy(groups[i], label, 127);
        groups[i][127] = '\0';
    }

    /* Build full listing text */
    char full_text[8192];
    full_text[0] = '\0';
    size_t foff = 0;

    /* Append header */
    size_t hlen = strlen(header);
    if (foff + hlen + 1 < sizeof(full_text)) {
        memcpy(full_text + foff, header, hlen);
        foff += hlen;
        full_text[foff++] = '\n';
        full_text[foff] = '\0';
    }

    /* Sort: groups by label, then tools within group by name */
    /* Use simple insertion sort on a local index array */
    int sorted_idx[CATALOG_MAX_ENTRIES];
    for (int i = 0; i < catalog->count; i++) sorted_idx[i] = i;
    for (int i = 1; i < catalog->count; i++) {
        int key = sorted_idx[i];
        int j = i - 1;
        char key_grp[128], key_name[128];
        strncpy(key_grp, groups[key], 127); key_grp[127] = '\0';
        strncpy(key_name, catalog->entries[key].name, 127); key_name[127] = '\0';
        while (j >= 0) {
            char cmp_grp[128], cmp_name[128];
            strncpy(cmp_grp, groups[sorted_idx[j]], 127); cmp_grp[127] = '\0';
            strncpy(cmp_name, catalog->entries[sorted_idx[j]].name, 127); cmp_name[127] = '\0';
            int cmp = strcmp(cmp_grp, key_grp);
            if (cmp < 0 || (cmp == 0 && strcmp(cmp_name, key_name) < 0)) break;
            sorted_idx[j + 1] = sorted_idx[j];
            j--;
        }
        sorted_idx[j + 1] = key;
    }

    /* Render groups */
    int gi = 0;
    while (gi < ngroups && gcount < catalog->count) {
        char grp_line[256];
        int grp_count = 0;
        /* Count tools in this group */
        for (int i = 0; i < catalog->count; i++) {
            if (strcmp(groups[sorted_idx[i]], glabels[gi]) == 0) grp_count++;
        }
        snprintf(grp_line, sizeof(grp_line), "%s tools (%d):", glabels[gi], grp_count);
        size_t gllen = strlen(grp_line);
        if (foff + gllen + 1 < sizeof(full_text)) {
            memcpy(full_text + foff, grp_line, gllen);
            foff += gllen;
            full_text[foff++] = '\n';
            full_text[foff] = '\0';
        }
        /* Full listing: name: short_desc */
        for (int i = 0; i < catalog->count; i++) {
            int ei = sorted_idx[i];
            if (strcmp(groups[ei], glabels[gi]) != 0) continue;
            char short_d[64];
            char desc_trunc[512];
            snprintf(desc_trunc, sizeof(desc_trunc), "%.500s",
                     catalog->entries[ei].description);
            short_desc(desc_trunc, 60, short_d, sizeof(short_d));
            char tool_line[256];
            if (short_d[0]) {
                snprintf(tool_line, sizeof(tool_line), "- %s: %s",
                         catalog->entries[ei].name, short_d);
            } else {
                snprintf(tool_line, sizeof(tool_line), "- %s",
                         catalog->entries[ei].name);
            }
            size_t tllen = strlen(tool_line);
            if (foff + tllen + 1 < sizeof(full_text)) {
                memcpy(full_text + foff, tool_line, tllen);
                foff += tllen;
                full_text[foff++] = '\n';
                full_text[foff] = '\0';
            }
            gcount++;
        }
        gi++;
    }
    full_text[foff] = '\0';

    /* Check token budget — for simplicity, use char count / CHARS_PER_TOKEN */
    int text_tokens = (int)ceil((double)foff / CHARS_PER_TOKEN);
    if (text_tokens <= max_tokens) {
        return strdup(full_text);
    }

    /* Fallback: names-only (still grouped) */
    char names_text[8192];
    names_text[0] = '\0';
    size_t noff = 0;
    size_t nhlen = strlen(header);
    if (noff + nhlen + 1 < sizeof(names_text)) {
        memcpy(names_text + noff, header, nhlen);
        noff += nhlen;
        names_text[noff++] = '\n';
        names_text[noff] = '\0';
    }
    gi = 0;
    while (gi < ngroups) {
        int grp_count = 0;
        for (int i = 0; i < catalog->count; i++) {
            if (strcmp(groups[sorted_idx[i]], glabels[gi]) == 0) grp_count++;
        }
        char grp_line[256];
        snprintf(grp_line, sizeof(grp_line), "%s tools (%d):", glabels[gi], grp_count);
        /* Build the names-only group */
        char group_buf[4096];
        group_buf[0] = '\0';
        size_t goff = 0;
        size_t gllen = strlen(grp_line);
        if (goff + gllen + 1 < sizeof(group_buf)) {
            memcpy(group_buf + goff, grp_line, gllen);
            goff += gllen;
            group_buf[goff++] = '\n';
            group_buf[goff] = '\0';
        }
        char names[2048];
        names[0] = '\0';
        size_t nlen = 0;
        for (int i = 0; i < catalog->count; i++) {
            int ei = sorted_idx[i];
            if (strcmp(groups[ei], glabels[gi]) != 0) continue;
            size_t namelen = strlen(catalog->entries[ei].name);
            if (nlen + namelen + 2 < sizeof(names)) {
                if (nlen > 0) { names[nlen++] = ','; names[nlen++] = ' '; names[nlen] = '\0'; }
                memcpy(names + nlen, catalog->entries[ei].name, namelen);
                nlen += namelen;
                names[nlen] = '\0';
            }
        }
        size_t nmlen = strlen(names);
        if (goff + nmlen + 1 < sizeof(group_buf)) {
            memcpy(group_buf + goff, names, nmlen);
            goff += nmlen;
            group_buf[goff] = '\0';
        }
        size_t golen = strlen(group_buf);
        if (noff + golen + 1 < sizeof(names_text)) {
            memcpy(names_text + noff, group_buf, golen);
            noff += golen;
            names_text[noff++] = '\n';
            names_text[noff] = '\0';
        }
        gi++;
    }
    names_text[noff] = '\0';
    int names_tokens = (int)ceil((double)noff / CHARS_PER_TOKEN);
    if (names_tokens <= max_tokens) {
        return strdup(names_text);
    }

    /* Fallback: groups-only summary */
    char groups_text[8192];
    groups_text[0] = '\0';
    size_t goff2 = 0;
    size_t h2len = strlen(header);
    if (goff2 + h2len + 1 < sizeof(groups_text)) {
        memcpy(groups_text + goff2, header, h2len);
        goff2 += h2len;
        groups_text[goff2++] = '\n';
        groups_text[goff2] = '\0';
    }
    gi = 0;
    while (gi < ngroups) {
        int grp_count = 0;
        for (int i = 0; i < catalog->count; i++) {
            if (strcmp(groups[sorted_idx[i]], glabels[gi]) == 0) grp_count++;
        }
        char grp_line[256];
        snprintf(grp_line, sizeof(grp_line),
            "%s (%d tools — names not listed; discover via `%s`)",
            glabels[gi], grp_count, search_name);
        size_t gllen = strlen(grp_line);
        if (goff2 + gllen + 1 < sizeof(groups_text)) {
            memcpy(groups_text + goff2, grp_line, gllen);
            goff2 += gllen;
            groups_text[goff2++] = '\n';
            groups_text[goff2] = '\0';
        }
        gi++;
    }
    groups_text[goff2] = '\0';
    return strdup(groups_text);
}

/* PoP: build_catalog_listing_with_form @ tools/tool_search.py:build_catalog_listing_with_form */
/* Port of Python: build_catalog_listing_with_form() — returns (text, form) */
char *tool_search_build_catalog_listing_with_form(const tool_catalog_t *catalog,
                                                    int max_tokens,
                                                    const char *search_name,
                                                    const char *describe_name,
                                                    const char *call_name,
                                                    char *out_form, size_t form_size) {
    if (out_form) *out_form = '\0';
    if (!catalog || catalog->count == 0) {
        if (out_form) snprintf(out_form, form_size, "none");
        return NULL;
    }

    /* Build group counts */
    char glabels[CATALOG_MAX_ENTRIES][128];
    int gcounts[CATALOG_MAX_ENTRIES];
    int ngroups = 0;
    char groups[CATALOG_MAX_ENTRIES][128];
    for (int i = 0; i < catalog->count; i++) {
        char label[128];
        listing_group_label(catalog->entries[i].source_name,
                                        label, sizeof(label));
        int gi = -1;
        for (int g = 0; g < ngroups; g++) {
            if (strcmp(glabels[g], label) == 0) { gi = g; break; }
        }
        if (gi < 0) {
            gi = ngroups;
            strncpy(glabels[ngroups], label, 127);
            glabels[ngroups][127] = '\0';
            gcounts[ngroups] = 0;
            ngroups++;
        }
        strncpy(groups[i], label, 127);
        groups[i][127] = '\0';
        gcounts[gi]++;
    }

    char header[512];
    snprintf(header, sizeof(header),
        "Deferred tool catalog (call schemas via `%s`, invoke via `%s`):",
        describe_name ? describe_name : TOOL_DESCRIBE_NAME,
        call_name ? call_name : TOOL_CALL_NAME);

    /* Per-server degradation: try full, then names, then summary per group.
     * For each group, compute token sizes and decide form. */
    char full_text[8192];
    full_text[0] = '\0';
    size_t foff = 0;
    size_t hlen = strlen(header);
    if (foff + hlen + 1 < sizeof(full_text)) {
        memcpy(full_text + foff, header, hlen);
        foff += hlen;
        full_text[foff++] = '\n';
        full_text[foff] = '\0';
    }

    int group_mode[CATALOG_MAX_ENTRIES]; /* 0=full, 1=names, 2=summary */
    for (int g = 0; g < ngroups; g++) group_mode[g] = 0;

    /* Sort indices */
    int sorted_idx[CATALOG_MAX_ENTRIES];
    for (int i = 0; i < catalog->count; i++) sorted_idx[i] = i;
    for (int i = 1; i < catalog->count; i++) {
        int key = sorted_idx[i];
        int j = i - 1;
        char key_grp[128], key_name[128];
        strncpy(key_grp, groups[key], 127); key_grp[127] = '\0';
        strncpy(key_name, catalog->entries[key].name, 127); key_name[127] = '\0';
        while (j >= 0) {
            char cmp_grp[128], cmp_name[128];
            strncpy(cmp_grp, groups[sorted_idx[j]], 127); cmp_grp[127] = '\0';
            strncpy(cmp_name, catalog->entries[sorted_idx[j]].name, 127); cmp_name[127] = '\0';
            int cmp = strcmp(cmp_grp, key_grp);
            if (cmp < 0 || (cmp == 0 && strcmp(cmp_name, key_name) < 0)) break;
            sorted_idx[j + 1] = sorted_idx[j];
            j--;
        }
        sorted_idx[j + 1] = key;
    }

    /* Try full listing */
    char full_buf[8192];
    full_buf[0] = '\0';
    size_t fboff = 0;
    size_t fhlen = strlen(header);
    if (fboff + fhlen + 1 < sizeof(full_buf)) {
        memcpy(full_buf + fboff, header, fhlen);
        fboff += fhlen;
        full_buf[fboff++] = '\n';
        full_buf[fboff] = '\0';
    }
    for (int g = 0; g < ngroups; g++) {
        char grp_line[256];
        snprintf(grp_line, sizeof(grp_line), "%s tools (%d):", glabels[g], gcounts[g]);
        size_t gllen = strlen(grp_line);
        if (fboff + gllen + 1 < sizeof(full_buf)) {
            memcpy(full_buf + fboff, grp_line, gllen);
            fboff += gllen;
            full_buf[fboff++] = '\n';
            full_buf[fboff] = '\0';
        }
        for (int i = 0; i < catalog->count; i++) {
            int ei = sorted_idx[i];
            if (strcmp(groups[ei], glabels[g]) != 0) continue;
            char short_d[64];
            char desc_trunc[512];
            snprintf(desc_trunc, sizeof(desc_trunc), "%.500s",
                     catalog->entries[ei].description);
            short_desc(desc_trunc, 60, short_d, sizeof(short_d));
            char tool_line[256];
            if (short_d[0]) {
                snprintf(tool_line, sizeof(tool_line), "- %s: %s",
                         catalog->entries[ei].name, short_d);
            } else {
                snprintf(tool_line, sizeof(tool_line), "- %s",
                         catalog->entries[ei].name);
            }
            size_t tllen = strlen(tool_line);
            if (fboff + tllen + 1 < sizeof(full_buf)) {
                memcpy(full_buf + fboff, tool_line, tllen);
                fboff += tllen;
                full_buf[fboff++] = '\n';
                full_buf[fboff] = '\0';
            }
        }
    }
    full_buf[fboff] = '\0';
    int full_tokens = (int)ceil((double)fboff / CHARS_PER_TOKEN);
    if (full_tokens <= max_tokens) {
        if (out_form) snprintf(out_form, form_size, "full");
        return strdup(full_buf);
    }

    /* Try names-only */
    char names_buf[8192];
    names_buf[0] = '\0';
    size_t noff = 0;
    size_t nhlen = strlen(header);
    if (noff + nhlen + 1 < sizeof(names_buf)) {
        memcpy(names_buf + noff, header, nhlen);
        noff += nhlen;
        names_buf[noff++] = '\n';
        names_buf[noff] = '\0';
    }
    for (int g = 0; g < ngroups; g++) {
        char grp_line[256];
        snprintf(grp_line, sizeof(grp_line), "%s tools (%d):", glabels[g], gcounts[g]);
        size_t gllen = strlen(grp_line);
        if (noff + gllen + 1 < sizeof(names_buf)) {
            memcpy(names_buf + noff, grp_line, gllen);
            noff += gllen;
            names_buf[noff++] = '\n';
            names_buf[noff] = '\0';
        }
        int first = 1;
        for (int i = 0; i < catalog->count; i++) {
            int ei = sorted_idx[i];
            if (strcmp(groups[ei], glabels[g]) != 0) continue;
            size_t namelen = strlen(catalog->entries[ei].name);
            if (noff + namelen + 2 < sizeof(names_buf)) {
                if (!first) { names_buf[noff++] = ','; names_buf[noff++] = ' '; }
                memcpy(names_buf + noff, catalog->entries[ei].name, namelen);
                noff += namelen;
                names_buf[noff] = '\0';
                first = 0;
            }
        }
        names_buf[noff++] = '\n';
        names_buf[noff] = '\0';
    }
    names_buf[noff] = '\0';
    int names_tokens = (int)ceil((double)noff / CHARS_PER_TOKEN);
    if (names_tokens <= max_tokens) {
        if (out_form) snprintf(out_form, form_size, "names");
        return strdup(names_buf);
    }

    /* Per-server degradation: collapse largest groups to summary first */
    int mode2[CATALOG_MAX_ENTRIES];
    for (int g = 0; g < ngroups; g++) mode2[g] = 0; /* 0=full, 2=summary */
    /* Estimate size per group */
    int grp_sizes[CATALOG_MAX_ENTRIES];
    for (int g = 0; g < ngroups; g++) {
        int sz = (int)strlen(glabels[g]) + 20; /* " tools (N):\n" */
        for (int i = 0; i < catalog->count; i++) {
            int ei = sorted_idx[i];
            if (strcmp(groups[ei], glabels[g]) != 0) continue;
            sz += (int)strlen(catalog->entries[ei].name) + 60;
        }
        grp_sizes[g] = sz;
    }
    /* Sort groups by size descending */
    int grp_order[CATALOG_MAX_ENTRIES];
    for (int g = 0; g < ngroups; g++) grp_order[g] = g;
    for (int i = 1; i < ngroups; i++) {
        int key = grp_order[i];
        int j = i - 1;
        while (j >= 0 && grp_sizes[grp_order[j]] < grp_sizes[key]) {
            grp_order[j + 1] = grp_order[j];
            j--;
        }
        grp_order[j + 1] = key;
    }
    /* Collapse largest to summary until it fits */
    for (int k = 0; k < ngroups; k++) {
        mode2[grp_order[k]] = 2; /* summary */
        /* Rebuild text */
        char mixed_buf[8192];
        mixed_buf[0] = '\0';
        size_t moff = 0;
        size_t mhlen = strlen(header);
        if (moff + mhlen + 1 < sizeof(mixed_buf)) {
            memcpy(mixed_buf + moff, header, mhlen);
            moff += mhlen;
            mixed_buf[moff++] = '\n';
            mixed_buf[moff] = '\0';
        }
        for (int g = 0; g < ngroups; g++) {
            char grp_line[256];
            if (mode2[g] == 2) { /* summary */
                snprintf(grp_line, sizeof(grp_line),
                    "%s (%d tools — names not listed; discover via `%s`)",
                    glabels[g], gcounts[g], search_name);
            } else {
                snprintf(grp_line, sizeof(grp_line), "%s tools (%d):",
                    glabels[g], gcounts[g]);
            }
            size_t gllen = strlen(grp_line);
            if (moff + gllen + 1 < sizeof(mixed_buf)) {
                memcpy(mixed_buf + moff, grp_line, gllen);
                moff += gllen;
                mixed_buf[moff++] = '\n';
                mixed_buf[moff] = '\0';
            }
            if (mode2[g] != 2) {
                int first = 1;
                for (int i = 0; i < catalog->count; i++) {
                    int ei = sorted_idx[i];
                    if (strcmp(groups[ei], glabels[g]) != 0) continue;
                    size_t namelen = strlen(catalog->entries[ei].name);
                    if (moff + namelen + 2 < sizeof(mixed_buf)) {
                        if (!first) { mixed_buf[moff++] = ','; mixed_buf[moff++] = ' '; }
                        memcpy(mixed_buf + moff, catalog->entries[ei].name, namelen);
                        moff += namelen;
                        mixed_buf[moff] = '\0';
                        first = 0;
                    }
                }
                mixed_buf[moff++] = '\n';
                mixed_buf[moff] = '\0';
            }
        }
        mixed_buf[moff] = '\0';
        int mixed_tokens = (int)ceil((double)moff / CHARS_PER_TOKEN);
        if (mixed_tokens <= max_tokens) {
            /* Determine form: groups if all summary, mixed otherwise */
            int all_summary = 1;
            for (int g = 0; g < ngroups; g++) {
                if (mode2[g] != 2) { all_summary = 0; break; }
            }
            if (out_form) snprintf(out_form, form_size, all_summary ? "groups" : "mixed");
            return strdup(mixed_buf);
        }
    }

    /* Everything over budget */
    if (out_form) snprintf(out_form, form_size, "none");
    return NULL;
}

/* PoP: _available_source_summary @ tools/tool_search.py:_available_source_summary */
/* Port of Python: _available_source_summary() — compact per-source tool counts */
char *tool_search_available_source_summary(const tool_catalog_t *catalog) {
    if (!catalog || catalog->count == 0) return NULL;

    char glabels[CATALOG_MAX_ENTRIES][128];
    int gcounts[CATALOG_MAX_ENTRIES];
    int ngroups = 0;

    for (int i = 0; i < catalog->count; i++) {
        char label[128];
        listing_group_label(catalog->entries[i].source_name,
                                        label, sizeof(label));
        int gi = -1;
        for (int g = 0; g < ngroups; g++) {
            if (strcmp(glabels[g], label) == 0) { gi = g; break; }
        }
        if (gi < 0) {
            gi = ngroups;
            strncpy(glabels[ngroups], label, 127);
            glabels[ngroups][127] = '\0';
            gcounts[ngroups] = 0;
            ngroups++;
        }
        gcounts[gi]++;
    }

    /* Sort labels for deterministic output */
    char sorted_labels[CATALOG_MAX_ENTRIES][128];
    int sorted_counts[CATALOG_MAX_ENTRIES];
    for (int i = 0; i < ngroups; i++) {
        strncpy(sorted_labels[i], glabels[i], 127);
        sorted_labels[i][127] = '\0';
        sorted_counts[i] = gcounts[i];
    }
    for (int i = 1; i < ngroups; i++) {
        char key[128];
        strncpy(key, sorted_labels[i], 127); key[127] = '\0';
        int key_count = sorted_counts[i];
        int j = i - 1;
        while (j >= 0 && strcmp(sorted_labels[j], key) > 0) {
            strncpy(sorted_labels[j + 1], sorted_labels[j], 127);
            sorted_labels[j + 1][127] = '\0';
            sorted_counts[j + 1] = sorted_counts[j];
            j--;
        }
        strncpy(sorted_labels[j + 1], key, 127);
        sorted_labels[j + 1][127] = '\0';
        sorted_counts[j + 1] = key_count;
    }

    json_node_t *arr = json_array();
    for (int i = 0; i < ngroups; i++) {
        json_node_t *obj = json_object();
        json_set(obj, "name", json_string(sorted_labels[i]));
        json_set(obj, "tool_count", json_number((double)sorted_counts[i]));
        json_array_append(arr, obj);
    }
    char *result = json_serialize(arr);
    json_free(arr);
    return result;
}

/* PoP: validate_deferred_call_args @ tools/tool_search.py:validate_deferred_call_args */
/* Port of Python: validate_deferred_call_args() — probe required args before dispatch */
char *tool_search_validate_deferred_call(const char *name,
                                         const char *args_json,
                                         const char *schema_json) {
    /* Python: returns None when call should dispatch, or a JSON error string
     * when a required argument is missing. */
    if (!name || !name[0] || !args_json || !schema_json) return NULL;

    char *schema_err = NULL;
    json_node_t *schema = json_parse(schema_json, &schema_err);
    if (!schema) {
        if (schema_err) free(schema_err);
        return NULL;
    }

    json_node_t *fn = json_obj_get(schema, "function");
    json_node_t *params = NULL;
    if (fn && fn->type == JSON_OBJECT) {
        params = json_obj_get(fn, "parameters");
    } else if (schema->type == JSON_OBJECT) {
        params = json_obj_get(schema, "parameters");
    }
    if (!params || params->type != JSON_OBJECT) {
        json_free(schema);
        return NULL;
    }

    json_node_t *required = json_obj_get(params, "required");
    if (!required || !json_is_array(required) || json_len(required) == 0) {
        json_free(schema);
        return NULL;
    }

    /* Parse args */
    char *args_err = NULL;
    json_node_t *args = json_parse(args_json, &args_err);
    if (args_err) free(args_err);
    if (!args || args->type != JSON_OBJECT) {
        if (args) json_free(args);
        json_free(schema);
        /* All required args are missing */
        json_node_t *missing_arr = json_array();
        size_t rlen = json_len(required);
        for (size_t i = 0; i < rlen; i++) {
            json_node_t *ri = json_array_get(required, i);
            if (ri && ri->type == JSON_STRING) {
                json_array_append(missing_arr, ri);
            }
        }
        json_node_t *result = json_object();
        char hint_msg[1024];
        snprintf(hint_msg, sizeof(hint_msg),
            "tool_call to '%s' is missing required argument(s). The tool was NOT invoked.", name);
        json_set(result, "error", json_string(hint_msg));
        json_set(result, "missing_arguments", missing_arr);
        json_set(result, "parameters", params);
        json_set(result, "hint", json_string(
            "Retry tool_call with 'arguments' matching the parameters schema."));
        char *ret = json_serialize(result);
        json_free(result);
        return ret;
    }

    /* Check each required arg */
    json_node_t *missing = json_array();
    size_t rlen = json_len(required);
    for (size_t i = 0; i < rlen; i++) {
        json_node_t *ri = json_array_get(required, i);
        if (!ri || ri->type != JSON_STRING) continue;
        const char *rname = ri->str_val;
        json_node_t *val = json_obj_get(args, rname);
        if (!val) {
            json_array_append(missing, ri);
        }
    }

    json_free(args);
    json_free(schema);

    if (json_len(missing) == 0) {
        json_free(missing);
        return NULL; /* all present — dispatch */
    }

    json_node_t *result = json_object();
    char hint_msg[1024];
    snprintf(hint_msg, sizeof(hint_msg),
        "tool_call to '%s' is missing required argument(s). The tool was NOT invoked.", name);
    json_set(result, "error", json_string(hint_msg));
    json_set(result, "missing_arguments", missing);
    json_set(result, "hint", json_string(
        "Retry tool_call with 'arguments' matching the tool's parameter schema."));
    char *ret = json_serialize(result);
    json_free(result);
    return ret;
}
