/*
 * port_memory_manager_helpers.c
 *
 * Pure, portable helpers ported from agent/memory_manager.py. These are the
 * module-level string/dict shaping helpers that do NOT touch providers, the
 * tool registry, or any IO:
 *   - normalize_tool_schema   (unwrap OpenAI "function" tool entry; return
 *                              None when name is missing/non-string)
 *   - sanitize_context        (strip <memory-context> fences, internal context
 *                              blocks, and [System note: ...] markers)
 *
 * The provider/agent-coupled functions (MemoryManager class, inject_*,
 * _strip_skill_scaffolding) are NOT ported here. build_memory_context_block
 * is already ported in src/tools/memory.c.
 *
 * Module prefix used by the scanner for agent/memory_manager.py is
 * "memory_manager_".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "hermes_json.h"

/* Forward decls. */
static int mm_istag_at(const char *s, const char *tag, size_t *consumed, int *is_closing);
static const char *mm_find_closing_fence(const char *s, size_t L);

/* --- normalize_tool_schema --------------------------------------------- */
/* PoP: normalize_tool_schema @ agent/memory_manager.py:normalize_tool_schema */
/*
 * Parse a tool-schema JSON string. If it is a bare function schema (dict with
 * a string "name") or an already-wrapped {"type":"function","function":{...}}
 * with a resolvable name, return 1 and set *out_obj to the bare function
 * node (borrowed from *out_root). Otherwise return 0. Caller frees *out_root
 * with json_free after done with *out_obj.
 */
int memory_manager_normalize_tool_schema(const char *schema_json,
                                         json_t **out_root,
                                         json_t **out_obj)
{
    *out_root = NULL;
    *out_obj = NULL;
    if (!schema_json) return 0;
    json_t *root = json_parse(schema_json, NULL);
    if (!root || root->type != JSON_OBJECT) {
        if (root) json_free(root);
        return 0;
    }
    json_t *cur = root;
    json_t *ty = json_object_get(cur, "type");
    json_t *fn = json_object_get(cur, "function");
    if (ty && ty->type == JSON_STRING && json_string_value(ty) &&
        strcmp(json_string_value(ty), "function") == 0 &&
        fn && fn->type == JSON_OBJECT) {
        cur = fn;
    }
    json_t *name = json_object_get(cur, "name");
    if (!name || name->type != JSON_STRING || !json_string_value(name) ||
        !json_string_value(name)[0]) {
        json_free(root);
        return 0;
    }
    *out_root = root;
    *out_obj = cur;
    return 1;
}

/* --- sanitize_context --------------------------------------------------- */
/* PoP: sanitize_context @ agent/memory_manager.py:sanitize_context */
/*
 * Strip fence tags, injected context blocks, and [System note: ...] markers.
 * Three removals (matching the Python regexes):
 *   1. <memory-context>...</memory-context> (case-insensitive, first close)
 *   2. [System note: ...] (case-insensitive, up to first ']')
 *   3. any standalone </?memory-context> fence tag
 * Result is malloc'd (caller frees).
 */
static int mm_istag_at(const char *s, const char *tag, size_t *consumed, int *is_closing)
{
    size_t i = 0;
    if (s[i] != '<') return 0;
    i++;
    int closing = 0;
    if (s[i] == '/') { closing = 1; i++; }
    size_t t = 0;
    while (tag[t]) {
        char sc = s[i];
        if (sc != tag[t] && sc != (char)toupper((unsigned char)tag[t]) &&
            sc != (char)tolower((unsigned char)tag[t])) {
            return 0;
        }
        i++; t++;
    }
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (s[i] != '>') return 0;
    i++;
    *consumed = i;
    *is_closing = closing;
    return 1;
}

/* Locate the first closing </memory-context> fence at/after s. */
static const char *mm_find_closing_fence(const char *s, size_t L)
{
    for (size_t k = 0; k < L; k++) {
        size_t cc = 0; int closing = 0;
        if (mm_istag_at(s + k, "memory-context", &cc, &closing) && closing) {
            return s + k;
        }
    }
    return NULL;
}

char *memory_manager_sanitize_context(const char *text)
{
    if (!text) return strdup("");
    size_t L = strlen(text);
    char *out = malloc(L + 1);
    size_t o = 0, i = 0;
    while (i < L) {
        size_t c1 = 0; int closing = 0;
        /* 1. opening <memory-context> ... </memory-context> block */
        if (mm_istag_at(text + i, "memory-context", &c1, &closing) && !closing) {
            const char *close = mm_find_closing_fence(text + i + c1, L - (i + c1));
            if (close) {
                /* advance past the closing fence tag */
                size_t cc = 0; int cclose = 0;
                mm_istag_at(close, "memory-context", &cc, &cclose);
                i = (size_t)(close - text) + cc;
                continue;
            }
            /* no matching close: fall through to standalone-tag handling below */
        }
        /* 2. [System note: ...] marker */
        if (strncasecmp(text + i, "[System note:", 13) == 0) {
            size_t j = i + 13;
            while (j < L && text[j] != ']') j++;
            if (j < L) { i = j + 1; continue; }
        }
        /* 3. standalone fence tag */
        if (mm_istag_at(text + i, "memory-context", &c1, &closing)) {
            i += c1;
            continue;
        }
        out[o++] = text[i++];
    }
    out[o] = '\0';
    return out;
}
