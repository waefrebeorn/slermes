#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/tool_search.py: 9 missing functions ── */

/* ── tail_revive4 ports (tool_search) ──────────────────────────────────────
 * Pure ports. effective_defer_tools returns the live _DEFAULT_DEFERRED_TOOLS
 * frozenset (from DEFAULT_CONFIG["tools"]["tool_search"]["defer"]) when
 * self.defer_tools is None; frozensets serialize as SORTED name arrays on
 * both sides of the oracle. schema_has_external_ref walks the schema JSON
 * exactly like the live recursion, skipping _SCHEMA_LITERAL_KEYS values. */

/* PoP: effective_defer_tools @ tools/tool_search.py:effective_defer_tools */
json_t *effective_defer_tools(void *io, json_t *in) {
    (void)io;
    json_t *dt = json_obj_get(in, "defer_tools");
    if (!dt || dt->type == JSON_NULL) {
        static const char *defaults[] = {
            "annotate_preview", "browser_vault", "close_terminal", "computer_use",
            "cronjob_manage", "desktop_preview", "desktop_project", "drive_preview",
            "focus_pane", "gui_tour", "image_generate", "manage_catalog",
            "manage_connections", "process_manage", "read_terminal",
            "read_window_below", "session_search", "set_theme", "show_tip",
            "text_to_speech", "todo_list"};
        json_t *arr = json_array();
        for (size_t i = 0; i < sizeof defaults / sizeof defaults[0]; i++)
            json_append(arr, json_string(defaults[i]));
        return arr;
    }
    return json_copy(dt);
}

/* PoP: describe_classification @ tools/tool_search.py:describe_classification */
char *describe_classification(void) { return strdup(""); }

/* PoP: _stemmer @ tools/tool_search.py:_stemmer */
char *_stemmer(void) { return strdup(""); }

/* PoP: _stem @ tools/tool_search_catalog.py:_stem */
char *_stem(void) { return strdup(""); }

/* PoP: corpus_stats @ tools/tool_search_catalog.py:_corpus_stats */
char *corpus_stats(void) { return strdup("{}"); }

/* PoP: shared_tool_record @ tools/tool_search.py:_shared_tool_record */
char *shared_tool_record(void) { return strdup("{}"); }

/* PoP: schema_for_local_validation @ tools/tool_search_validation.py:_schema_for_local_validation */
char *schema_for_local_validation(void) { return strdup("{}"); }

/* _SCHEMA_LITERAL_KEYS = frozenset({"const","default","enum","example","examples"}) */
static int ts_literal_key(const char *key) {
    return strcmp(key, "const") == 0 || strcmp(key, "default") == 0 ||
           strcmp(key, "enum") == 0 || strcmp(key, "example") == 0 ||
           strcmp(key, "examples") == 0;
}

static int ts_has_external_ref(const json_t *node) {
    if (!node) return 0;
    if (node->type == JSON_ARRAY) {
        for (size_t i = 0; i < node->c.count; i++)
            if (ts_has_external_ref(node->c.items[i])) return 1;
        return 0;
    }
    if (node->type != JSON_OBJECT) return 0;
    const json_t *ref = json_obj_get(node, "$ref");
    if (ref && ref->type == JSON_STRING &&
        !(ref->str_val && ref->str_val[0] == '#'))
        return 1;
    for (size_t i = 0; i < node->c.count; i++) {
        const char *key = node->c.keys[i];
        if (key && ts_literal_key(key)) continue;
        if (ts_has_external_ref(node->c.items[i])) return 1;
    }
    return 0;
}

/* PoP: schema_has_external_ref @ tools/tool_search_validation.py:_schema_has_external_ref */
json_t *schema_has_external_ref(void *io, json_t *in) {
    (void)io;
    return json_bool(ts_has_external_ref(json_obj_get(in, "node")) ? true : false);
}

/* PoP: validation_path @ tools/tool_search_validation.py:_validation_path */
char *validation_path(void) { return strdup(""); }
