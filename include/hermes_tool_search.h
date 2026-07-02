#ifndef HERMES_TOOL_SEARCH_H
#define HERMES_TOOL_SEARCH_H

/*
 * tool_search.h — Progressive tool disclosure ("tool search") for Hermes C.
 *
 * Port of Python tools/tool_search.py (735 lines, 22 functions, 3 classes).
 *
 * When enabled, MCP and non-core plugin tools are replaced in the model-visible
 * tools array by three bridge tools — tool_search, tool_describe, tool_call —
 * and surfaced on demand. Core Hermes tools never defer.
 *
 * The BM25 catalog enables full-text search across deferred tool names,
 * descriptions, and parameter names. The threshold gate decides whether
 * tool search activates based on estimated token consumption.
 *
 * Design constraints (from the Python prototype):
 * - Core tools defined as "never deferred". Always-load means always-load.
 * - The threshold gate runs every assembly: when deferrable tools would consume
 *   less than threshold_pct of the model's context window (default 10%),
 *   tool search is a no-op and the tools array passes through unchanged.
 * - The catalog is stateless across turns and rebuilt from the current tool-defs
 *   list every time.
 * - Bridge tools route through dispatch exactly like a direct call.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* ─────────────────────────────────────────────────────────────────────
 *  Constants
 * ───────────────────────────────────────────────────────────────────── */

/* Bridge tool names — reserved, may not collide with user/plugin/MCP tools */
#define TOOL_SEARCH_NAME   "tool_search"
#define TOOL_DESCRIBE_NAME "tool_describe"
#define TOOL_CALL_NAME     "tool_call"

/* Token estimation: ~4 chars per token for English+JSON */
#define CHARS_PER_TOKEN 4.0

/* Default BM25 parameters */
#define BM25_K1 1.5
#define BM25_B  0.75

/* ─────────────────────────────────────────────────────────────────────
 *  Catalog entry
 * ───────────────────────────────────────────────────────────────────── */

#define CATALOG_MAX_TOKENS 256
#define CATALOG_MAX_ENTRIES 512

/** Source classification for a catalog entry. */
typedef enum {
    CATALOG_SOURCE_MCP,       /* MCP-provided tool */
    CATALOG_SOURCE_PLUGIN,    /* Plugin-provided tool */
    CATALOG_SOURCE_OTHER,     /* Unclassified */
} catalog_source_t;

/** One deferrable tool entry in the search catalog. */
typedef struct {
    char name[128];
    char description[512];
    /* Source info */
    catalog_source_t source;
    char source_name[64];
    /* Pre-tokenized search text for BM25 */
    char tokens[CATALOG_MAX_TOKENS][64];
    int token_count;
} catalog_entry_t;

/** The search catalog — built from the set of deferrable tool definitions. */
typedef struct {
    catalog_entry_t entries[CATALOG_MAX_ENTRIES];
    int count;
} tool_catalog_t;

/* ─────────────────────────────────────────────────────────────────────
 *  Search configuration
 * ───────────────────────────────────────────────────────────────────── */

typedef enum {
    TOOL_SEARCH_AUTO = 0,
    TOOL_SEARCH_ON,
    TOOL_SEARCH_OFF,
} tool_search_mode_t;

typedef struct {
    tool_search_mode_t enabled;      /* auto/on/off */
    double threshold_pct;            /* 0..100 */
    int search_default_limit;        /* default results per query */
    int max_search_limit;            /* max results per query */
} tool_search_config_t;

/* Default config: auto, 10% threshold, limit 5/20 */
#define TOOL_SEARCH_CONFIG_DEFAULT \
    { TOOL_SEARCH_AUTO, 10.0, 5, 20 }

/* ─────────────────────────────────────────────────────────────────────
 *  Assembly result
 * ───────────────────────────────────────────────────────────────────── */

typedef struct {
    bool activated;            /* Whether tool search was activated */
    int deferred_count;        /* Number of tools deferred */
    int deferred_tokens;       /* Estimated token cost of deferred schemas */
    int threshold_tokens;      /* Activation threshold in tokens */
} tool_search_assembly_t;

/* ─────────────────────────────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────────────────────────────── */

/* ── Token estimation ─────────────────────────────────────────────── */

/**
 * Estimate the token cost of a JSON-serialized tool schema string.
 * Port of Python: estimate_tokens_from_schemas().
 * Uses the chars/4 rule: ~4 chars per token for English+JSON.
 */
int tool_search_estimate_tokens(const char *schema_json);

/* ── Catalog building ─────────────────────────────────────────────── */

/**
 * Tokenize a text string into lowercase alphanumeric tokens.
 * Port of Python: _tokenize().
 * Returns token count (max CATALOG_MAX_TOKENS).
 */
int tool_search_tokenize(const char *text, char tokens[][64], int max_tokens);

/**
 * Build search text for a tool entry (name + description + param names).
 * Port of Python: _entry_search_text().
 */
void tool_search_entry_text(const char *name, const char *description,
                            const char *param_names, char *out, size_t out_size);

/**
 * Build the deferred-tool catalog from an array of tool schema JSONs.
 * Port of Python: build_catalog().
 */
int tool_search_build_catalog(const char *tool_schemas[], int schema_count,
                              tool_catalog_t *catalog);

/* ── BM25 scoring ─────────────────────────────────────────────────── */

/**
 * Compute BM25 score for one query against one document.
 * Port of Python: _bm25_score().
 * Standard BM25 with k1=1.5, b=0.75.
 */
double tool_search_bm25_score(const char *query_tokens[], int query_count,
                              const catalog_entry_t *entry,
                              const int doc_lengths[], int n_docs,
                              const int doc_freq[], int vocab_size);

/**
 * Search the catalog for the top-`limit` entries matching `query`.
 * Port of Python: search_catalog().
 * Falls back to name-substring match when BM25 yields no hits.
 */
int tool_search_query(const tool_catalog_t *catalog, const char *query,
                      int limit, int *out_indices);

/* ── Threshold gate ───────────────────────────────────────────────── */

/**
 * Decide whether tool search should activate for the current assembly.
 * Port of Python: should_activate().
 */
bool tool_search_should_activate(const tool_search_config_t *config,
                                 int deferrable_tokens,
                                 int context_length);

/* ── Bridge tool schemas ──────────────────────────────────────────── */

/**
 * Build the schema JSON strings for the three bridge tools.
 * Port of Python: bridge_tool_schemas().
 * Returns 3 malloc'd JSON strings; caller must free each.
 * deferred_count: number of deferred tools (used in tool_search description).
 */
int tool_search_bridge_schemas(int deferred_count,
                               char **out_search_schema,
                               char **out_describe_schema,
                               char **out_call_schema);

/* ── Assembly ─────────────────────────────────────────────────────── */

/**
 * Run the full tool-search assembly decision.
 * Port of Python: assemble_tool_defs().
 *
 * tool_schemas: array of JSON schema strings for all registered tools.
 * schema_count: number of tool schemas.
 * context_length: model context window size, or 0 if unknown.
 * config: search config, or NULL for defaults.
 * out_result: populated with assembly decision.
 *
 * Returns: malloc'd JSON array of visible tool schemas (the assembled result),
 * or NULL on error. Caller must free.
 */
char *tool_search_assemble(const char *tool_schemas[], int schema_count,
                           int context_length,
                           const tool_search_config_t *config,
                           tool_search_assembly_t *out_result);

/* ── Bridge dispatch ─────────────────────────────────────────────── */

/**
 * Execute the tool_search bridge tool. Returns a malloc'd JSON result string.
 * Port of Python: dispatch_tool_search().
 */
char *tool_search_dispatch_search(const char *query_json,
                                  const tool_catalog_t *catalog,
                                  const tool_search_config_t *config);

/**
 * Execute the tool_describe bridge tool. Returns a malloc'd JSON result string.
 * Port of Python: dispatch_tool_describe().
 */
char *tool_search_dispatch_describe(const char *name,
                                    const tool_catalog_t *catalog);

/**
 * Parse a tool_call invocation into (underlying_name, args_json, error).
 * Port of Python: resolve_underlying_call().
 * Returns 0 on success, -1 on error (with error_msg set).
 */
int tool_search_resolve_call(const char *args_json,
                             char *out_name, size_t name_size,
                             char *out_args, size_t args_size,
                             char *error_msg, size_t error_size);

/**
 * Check if a tool name is a bridge tool.
 */
static inline bool tool_search_is_bridge(const char *name) {
    return (name && (strcmp(name, TOOL_SEARCH_NAME) == 0 ||
                     strcmp(name, TOOL_DESCRIBE_NAME) == 0 ||
                     strcmp(name, TOOL_CALL_NAME) == 0));
}

/**
 * Check if a tool name is deferrable (not a bridge tool, not a core tool).
 * Port of Python: is_deferrable_tool_name().
 * core_names: comma-separated list of core tool names, or NULL for default.
 */
bool tool_search_is_deferrable(const char *name, const char *core_names);

/**
 * Get the set of deferrable tool names from an array of schemas.
 * Port of Python: scoped_deferrable_names().
 * Returns 0 on success, negative on error.
 */
int tool_search_scoped_names(const char *tool_schemas[], int schema_count,
                             const char *core_names,
                             char out_names[][128], int *out_count, int max_names);

/**
 * Free the catalog (no-op for static catalog, useful if entries become dynamic).
 */
void tool_search_catalog_free(tool_catalog_t *catalog);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOL_SEARCH_H */
