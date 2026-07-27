/*
 * port_agent_title_generator.c — faithful C11 port of the pure/plumbing
 * surface of agent/title_generator.py.
 *
 * Stacked on real plumbing (no façades):
 *   - config:   config_py_load_config_impl (hermes_cli/config.py port)
 *   - truthy:   is_truthy_value (utils.py port, lib-wide)
 *   - skills:   describe_skill_invocation (agent/skill_commands.py port)
 *   - store:    session_title_* (hermes_state.py title surface over libdb)
 *
 * generate_title / auto_title_session / maybe_auto_title drive the live LLM
 * + thread runtime and remain in the runtime TUs.
 */
#include "title_generator_helpers.h"
#include "port_config_py_helpers.h"
#include "truthy.h"
#include "skill_scaffolding.h"
#include "session_title.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* PoP: title_language @ agent/title_generator.py:_title_language */
void title_language(char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';   /* Python except-branch: return "" */

    json_t *cfg = config_py_load_config_impl(0);
    if (!cfg) return;
    json_t *lang = config_py_get_nested(cfg, "auxiliary.title_generation.language");
    const char *s = json_is_string(lang) ? json_string_value(lang) : NULL;
    if (s) {
        /* str(...).strip() */
        while (*s && isspace((unsigned char)*s)) s++;
        size_t len = strlen(s);
        while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
        if (len >= out_sz) len = out_sz - 1;
        memcpy(out, s, len);
        out[len] = '\0';
    }
    json_free(cfg);
}

/* PoP: auto_title_enabled @ agent/title_generator.py:_auto_title_enabled */
bool auto_title_enabled(void) {
    /* Python: load_config_readonly() -> (auxiliary or {}).title_generation
     * or {} -> is_truthy_value(enabled, default=True); any failure -> True. */
    json_t *cfg = config_py_load_config_impl(0);
    if (!cfg) return true;

    json_t *enabled = config_py_get_nested(cfg, "auxiliary.title_generation.enabled");
    bool result = true;
    if (enabled) {
        if (json_is_bool(enabled)) {
            result = json_is_true(enabled);
        } else if (json_is_null(enabled)) {
            result = true;   /* None -> default=True */
        } else if (json_is_string(enabled)) {
            result = is_truthy_value(json_string_value(enabled), true);
        } else if (json_is_number(enabled)) {
            result = json_number_value(enabled) != 0.0;   /* Python bool(value) */
        }
    }
    json_free(cfg);
    return result;
}

/* PoP: summarize_user_message @ agent/title_generator.py:_summarize_user_message */
char *summarize_user_message(const char *user_message) {
    if (!user_message || !*user_message) return strdup("");
    /* Reuse the canonical scaffolding parser so the titler sees
     * "/work — fix the title leak" instead of the expanded skill body. */
    char *described = describe_skill_invocation(user_message);
    if (described) return described;
    /* None -> not skill scaffolding: title the raw message. */
    return strdup(user_message);
}

/* PoP: persist_session_title @ agent/title_generator.py:_persist_session_title */
/* Predicate-guarded write with duplicate-collision recovery: the write goes
 * through the auto-if-empty predicate so a manual title set while generation
 * was in flight is never overwritten; on a uniqueness conflict, append a #N
 * suffix via session_title_next_in_lineage and retry once; a still-colliding
 * or unchanged dedup re-raises (returns the conflict). Returns the malloc'd
 * title actually persisted, or NULL when a concurrent manual title won the
 * race / the write failed (caller frees; *result gets the store outcome when
 * non-NULL). */
char *persist_session_title(db_t *session_db, const char *session_id,
                            const char *title,
                            session_title_result_t *result) {
    session_title_result_t rc =
        session_title_set_auto_if_empty(session_db, session_id, title);
    if (result) *result = rc;
    if (rc == SESSION_TITLE_OK) return strdup(title);
    if (rc == SESSION_TITLE_SKIPPED) return NULL;   /* manual title won */
    if (rc != SESSION_TITLE_CONFLICT) return NULL;  /* not found / invalid */

    /* ValueError branch: dedup via lineage suffix, then retry. */
    char *deduped = session_title_next_in_lineage(session_db, title);
    if (!deduped || strcmp(deduped, title) == 0) {
        free(deduped);
        return NULL;   /* Python re-raises */
    }
    rc = session_title_set_auto_if_empty(session_db, session_id, deduped);
    if (result) *result = rc;
    if (rc == SESSION_TITLE_OK) return deduped;
    free(deduped);
    return NULL;
}
