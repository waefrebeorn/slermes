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
int tool_search_tokenize(const char *text, char tokens[][64], int max_tokens) {
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
/* PoP: _classify_source @ tools/tool_search.py:_classify_source */
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
int tool_search_build_catalog(const char *tool_schemas[], int schema_count,
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
        entry->token_count = tool_search_tokenize(search_text, entry->tokens, CATALOG_MAX_TOKENS);

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
double tool_search_bm25_score(const char *query_tokens[], int query_count,
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
    int qt_count = tool_search_tokenize(query, query_tokens, CATALOG_MAX_TOKENS);
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
        double s = tool_search_bm25_score(qt_ptrs, qt_count, &catalog->entries[i],
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
bool tool_search_should_activate(const tool_search_config_t *config,
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
    tool_search_build_catalog(tool_schemas, schema_count, &catalog);

    int deferrable_tokens = 0;
    for (int i = 0; i < schema_count; i++) {
        deferrable_tokens += tool_search_estimate_tokens(tool_schemas[i]);
    }

    out_result->deferred_count = catalog.count;
    out_result->deferred_tokens = deferrable_tokens;

    if (!tool_search_should_activate(config, deferrable_tokens, context_length)) {
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
