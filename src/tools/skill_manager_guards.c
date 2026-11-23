/*
 * skill_manager_guards.c — C port of upstream Hermes tools/skill_manager_guards.py.
 *
 * PoP: w14_skillmanag_328d03_refusal @ tools/skill_manager_guards.py:_refusal
 * PoP: w14_skillmanag_328d03_pinned_guard @ tools/skill_manager_guards.py:_pinned_guard
 * PoP: w14_skillmanag_328d03_read_before_write_guard @ tools/skill_manager_guards.py:_background_review_read_before_write_guard
 * PoP: w14_skillmanag_328d03_curator_consolidation_delete_guard @ tools/skill_manager_guards.py:_curator_consolidation_delete_guard
 *
 * The provenance/origin lookups (ESSENTIAL_SKILLS, _is_pinned,
 * _is_background_review, _background_review_has_read) are fixture-injected
 * seams, exactly as the oracle stubs them on the live Python module.
 */
#include "noc_w14_skillmanag_328d03.h"
#include "hermes_json.h"

#include <stdio.h>
#include <string.h>

static struct json_t *refusal_obj(const char *message, struct json_t *extra) {
    struct json_t *out = json_object();
    if (!out) { json_free(extra); return NULL; }
    json_set(out, "success", json_bool(false));
    json_set(out, "error", json_string(message ? message : ""));
    if (json_is_object(extra)) {
        for (size_t i = 0; i < json_object_size(extra); i++) {
            const char *k = json_object_get_key_at(extra, i);
            struct json_t *v = json_object_get_at(extra, i);
            if (k) json_set(out, k, v ? json_copy(v) : json_null());
        }
    }
    json_free(extra);
    return out;
}

struct json_t *w14_skillmanag_328d03_refusal(const char *message,
                                             const struct json_t *extra) {
    return refusal_obj(message, extra ? json_copy(extra) : NULL);
}

static int in_string_array(const struct json_t *arr, const char *name) {
    if (!json_is_array(arr) || !name) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const struct json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, name)) return 1;
    }
    return 0;
}

struct json_t *w14_skillmanag_328d03_pinned_guard(const char *name,
                                                  const struct json_t *essential,
                                                  int pinned) {
    char buf[4096];
    const char *n = name ? name : "";

    if (in_string_array(essential, n)) {
        snprintf(buf, sizeof buf,
                 "Skill '%s' is essential to Hermes (the agent's own "
                 "operating manual referenced by the system prompt) and "
                 "cannot be deleted. Patches and edits are still allowed.", n);
        return json_string(buf);
    }
    if (pinned == 1) {
        snprintf(buf, sizeof buf,
                 "Skill '%s' is pinned and cannot be deleted by skill_manage. "
                 "Ask the user to run `hermes curator unpin %s` if they want to "
                 "delete it. Patches and edits are allowed on pinned skills; "
                 "only deletion is blocked.", n, n);
        return json_string(buf);
    }
    return json_null();
}

struct json_t *w14_skillmanag_328d03_read_before_write_guard(
    const char *name, const char *target, const char *action,
    const char *file_label, int is_background_review, int has_read) {
    char msg[8192];
    struct json_t *extra, *out;
    (void)target;  /* the live module's has_read() seam already resolved it */
    if (!is_background_review || has_read) return json_null();

    snprintf(msg, sizeof msg,
             "Refusing background curator %s for skill '%s': the current %s "
             "content has not been loaded in this review turn. Call "
             "skill_view(name) for SKILL.md, or skill_view(name, "
             "file_path=...) for a supporting file, then retry the write "
             "using the content just returned.",
             action ? action : "", name ? name : "", file_label ? file_label : "");
    extra = json_object();
    json_set(extra, "_read_before_write_required", json_bool(true));
    out = refusal_obj(msg, extra);
    return out;
}

struct json_t *w14_skillmanag_328d03_curator_consolidation_delete_guard(
    const char *name, const char *absorbed_into, int is_background_review) {
    char msg[8192];
    struct json_t *extra, *out;
    const char *n = name ? name : "";

    if (!is_background_review) return json_null();
    if (absorbed_into) {
        const char *p = absorbed_into;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p) return json_null();  /* non-blank umbrella supplied */
    }
    snprintf(msg, sizeof msg,
             "Refusing background curator delete of skill '%s': the "
             "consolidation pass may only archive a skill it has absorbed "
             "into an umbrella. Pass absorbed_into=<umbrella> (the umbrella "
             "must already exist) to record a verified consolidation. "
             "Pruning a skill with no forwarding target is not permitted "
             "here \xe2\x80\x94 the deterministic inactivity prune handles "
             "staleness archival separately. Keeping '%s' active.", n, n);
    extra = json_object();
    json_set(extra, "_fail_closed", json_bool(true));
    out = refusal_obj(msg, extra);
    return out;
}
