#define _POSIX_C_SOURCE 200809L
/* memory_tool_pure.c - Faithful port of the pure, deterministic helpers in
 * tools/memory_tool.py (the bounded curated memory store).
 *
 * Ports the side-effect-free surface:
 *
 *   get_memory_dir              -> memory_tool_get_memory_dir (returns malloc'd path)
 *
 *   _drift_error                -> memory_tool_drift_error (malloc'd JSON dict)
 *
 *   _read_failed_error          -> memory_tool_read_failed_error (malloc'd JSON dict)
 *   MemoryStore consolidation counter:
 *     reset_consolidation_failures -> memory_tool_pure_store_reset_consolidation_failures
 *     _consolidation_failure        -> memory_tool_pure_store_consolidation_failure
 *
 * _scan_memory_content is intentionally NOT ported here (it delegates to
 * tools.threat_patterns.first_threat_message - a separate module to port).
 * Callers receive freshly malloc'd strings (free with free()) or NULL.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "truthy.h"
#include "port_config_py_helpers.h"
#include "port_models_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MEMORY_MAX_CONSOLIDATION_FAILURES_PER_TURN 3

/* --- get_memory_dir ----------------------------------------------- */
/* PoP: memory_tool_get_memory_dir @ tools/memory_tool.py:get_memory_dir */
char *memory_tool_get_memory_dir(void) {
    char home[4096];
    hermes_home_dir(home, sizeof(home));
    size_t l = strlen(home);
    while (l > 0 && (home[l - 1] == '/' || home[l - 1] == ' ')) home[--l] = '\0';
    char *out = (char *)malloc(l + 16);
    snprintf(out, l + 16, "%s/memories", home);
    return out;
}

/* --- _drift_error ------------------------------------------------- */
/* PoP: memory_tool_drift_error @ tools/memory_tool.py:_drift_error */
char *memory_tool_drift_error(const char *path_name, const char *bak_path) {
    char *out = (char *)malloc(2048);
    const char *pname = path_name ? path_name : "";
    const char *bak = bak_path ? bak_path : "";
    int n = snprintf(out, 2048,
        "{\"success\":false,\"error\":\"Refusing to write %s: file on disk has "
        "content that wouldn't round-trip through the memory tool (likely added "
        "by the patch tool, a shell append, a manual edit, or a concurrent "
        "session). A snapshot was saved to %s. Resolve the drift first — either "
        "rewrite the file as a clean §-delimited list of entries, or move the "
        "extra content out — then retry. This guard exists to prevent silent "
        "data loss (issue #26045).\",\"drift_backup\":\"%s\",\"remediation\":\"Open "
        "the .bak file, integrate the missing entries into the memory tool one at "
        "a time via memory(action=add, content=...), then remove or rewrite the "
        "original file to a clean state.\"}",
        pname, bak, bak);
    (void)n;
    return out;
}

/* --- _read_failed_error ------------------------------------------- */
/* PoP: memory_tool_read_failed_error @ tools/memory_tool.py:_read_failed_error */
char *memory_tool_read_failed_error(const char *path_name) {
    char *out = (char *)malloc(1024);
    const char *pname = path_name ? path_name : "";
    snprintf(out, 1024,
        "{\"success\":false,\"error\":\"Refusing to write %s: the file exists on "
        "disk but could not be read right now (temporarily locked by another "
        "program, a permission change, invalid/corrupt text encoding, or a "
        "filesystem error). Treating an unreadable file as empty and saving would "
        "wipe existing memory, so the write is refused. Nothing was changed — "
        "retry in a moment.\"}",
        pname);
    return out;
}

/* --- MemoryStore consolidation counter (opaque) ------------------- */
typedef struct memory_tool_pure_store {
    int consolidation_failures;
} memory_tool_pure_store_t;
/* PoP: memory_tool_pure_store_reset_consolidation_failures @ tools/memory_tool.py:reset_consolidation_failures */

void memory_tool_pure_store_reset_consolidation_failures(memory_tool_pure_store_t *s) {
    if (s) s->consolidation_failures = 0;
}

/* Returns a malloc'd JSON dict. On under-cap it mirrors `response` (the
 * caller's JSON) unchanged; past cap returns a terminal result.  */
/* PoP: memory_tool_pure_store_consolidation_failure @ tools/memory_tool.py:_consolidation_failure */

char *memory_tool_pure_store_consolidation_failure(memory_tool_pure_store_t *s, const char *response_json) {
    if (!s) return strdup(response_json ? response_json : "{}");
    s->consolidation_failures += 1;
    if (s->consolidation_failures <= MEMORY_MAX_CONSOLIDATION_FAILURES_PER_TURN) {
        return strdup(response_json ? response_json : "{}");
    }
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"success\":false,\"done\":true,\"error\":\"Memory consolidation failed %d "
        "times this turn. Stop retrying memory calls — leave memory unchanged for "
        "now and continue with your reply to the user. The fact can be saved in a "
        "later turn.\"}",
        s->consolidation_failures);
    return strdup(buf);
}

/* ── Built-in memory config resolution (v678 parity batch) ─────────── */

/* PoP: memory_tool_pure_get_builtin_memory_config @ tools/memory_tool.py:get_builtin_memory_config */
json_t *memory_tool_pure_get_builtin_memory_config(const json_t *config) {
    /* Normalized built-in memory config mapping: the "memory" section when
     * it is an object, else an empty mapping (missing flags resolve to the
     * enabled defaults at flag-read time). config == NULL loads the live
     * read-only config; a failed load returns {}. */
    if (!config) {
        json_t *live = config_py_load_config_readonly();
        if (!live) return json_object();
        const json_t *section = json_object_get(live, "memory");
        json_t *out = (section && json_is_object(section))
            ? json_copy(section) : json_object();
        json_free(live);
        return out;
    }
    if (!json_is_object(config)) return json_object();
    const json_t *section = json_object_get(config, "memory");
    if (section && json_is_object(section)) return json_copy(section);
    return json_object();
}

/* PoP: memory_tool_pure_get_builtin_memory_store_flags @ tools/memory_tool.py:get_builtin_memory_store_flags */
void memory_tool_pure_get_builtin_memory_store_flags(const json_t *config,
                                                     bool *memory_enabled,
                                                     bool *user_profile_enabled) {
    /* (memory_enabled, user_profile_enabled) from resolved config; both
     * default true via the shared truthy set. */
    json_t *section = memory_tool_pure_get_builtin_memory_config(config);
    bool me = is_truthy_value(json_string_value(json_object_get(section, "memory_enabled")), true);
    bool upe = is_truthy_value(json_string_value(json_object_get(section, "user_profile_enabled")), true);
    json_free(section);
    if (memory_enabled) *memory_enabled = me;
    if (user_profile_enabled) *user_profile_enabled = upe;
}
