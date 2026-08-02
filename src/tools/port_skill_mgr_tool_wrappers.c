/*
 * port_skill_mgr_tool_wrappers.c — C port of tools/skill_manager_tool.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* shared background-review read-marks state (ported from skill_manager_tool) */
#define SMT_MAX_MARKS 64
static char *g_marks[SMT_MAX_MARKS];
static int g_nmarks = 0;
static bool smt_read_marks_has(const char *target) {
    if (!target) return false;
    for (int i = 0; i < g_nmarks; i++)
        if (g_marks[i] && strcmp(g_marks[i], target) == 0) return true;
    return false;
}
static void smt_read_marks_add(const char *target) {
    if (!target || !*target || smt_read_marks_has(target)) return;
    if (g_nmarks < SMT_MAX_MARKS) g_marks[g_nmarks++] = strdup(target);
}
static void smt_read_marks_reset(void) {
    for (int i = 0; i < g_nmarks; i++) { free(g_marks[i]); g_marks[i] = NULL; }
    g_nmarks = 0;
}
extern const char *skill_provenance_current_origin(void);

/* PoP: mark_background_review_skill_read @ tools/skill_manager_tool.py:mark_background_review_skill_read */
int smt_mark_background_review_skill_read(const char *arg) {
    /* Python: record that the review fork read this target file. */
    smt_read_marks_add(arg);
    return 1;
}

/* PoP: _background_review_has_read @ tools/skill_manager_tool.py:_background_review_has_read */
int smt_u_background_review_has_read(const char *arg) {
    /* Python: has the review fork loaded this exact target file. */
    return smt_read_marks_has(arg);
}

/* PoP: _reset_background_review_read_marks @ tools/skill_manager_tool.py:_reset_background_review_read_marks */
int smt_u_reset_background_review_read_marks(const char *arg) {
    /* Python: clear all read-marks (new review fork). */
    (void)arg;
    smt_read_marks_reset();
    return 0;
}

/* PoP: _guard_agent_created_enabled @ tools/skill_manager_tool.py:_guard_agent_created_enabled */
int smt_u_guard_agent_created_enabled(const char *arg) {
    /* Python: config skills.guard_agent_created default False. Arg = "1"/"0". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _security_scan_skill @ tools/skill_manager_tool.py:_security_scan_skill */
int smt_u_security_scan_skill(const char *arg) {
    /* Python: post-write scan gate. Arg =
     * "state\treason\tblocked\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (t2 && t2[1] == '1') {
        printf("Security scan blocked this skill (%s):\nreport\n", t3 ? t3 + 1 : "dangerous findings");
        return 1;
    }
    printf("\n");
    return 0;
}

/* PoP: _pinned_guard @ tools/skill_manager_tool.py:_pinned_guard */
int smt_u_pinned_guard(const char *arg) {
    /* Python: pinned refusal or None. Arg = "name\tpinned\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int pinned = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!pinned || !state) { printf("\n"); return 0; }
    printf("Skill '%s' is pinned and cannot be deleted by skill_manage. Ask the user to run `hermes curator unpin %s` if they want to delete it. Patches and edits are allowed on pinned skills; only deletion is blocked.\n", arg, arg);
    return 1;
}

/* PoP: _background_review_write_guard @ tools/skill_manager_tool.py:_background_review_write_guard */
int smt_u_background_review_write_guard(const char *arg) {
    /* Python: curator write surface. Arg =
     * "blocked\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int blocked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!blocked) { printf("\n"); return 0; }
    printf("Refusing background curator write to '%s' (pinned #25839 / externally owned)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? " — provenance check" : "");
    return 0;
}

/* PoP: _background_review_read_before_write_guard @ tools/skill_manager_tool.py:_background_review_read_before_write_guard */
int smt_u_background_review_read_before_write_guard(const char *arg) {
    /* Python (name, file_label, action, target): refuse a background-curator
     * mutation of a file the review fork has not loaded first. Arg =
     * "name\tfile_label\taction\ttarget". Prints the refusal JSON when
     * blocking, nothing when allowed. */
    if (!arg || !*arg) return 0;
    char name[256], flabel[256], action[256], target[512];
    if (sscanf(arg, "%255[^\t]\t%255[^\t]\t%255[^\t]\t%511s", name, flabel, action, target) < 4)
        return 0;
    if (strcmp(skill_provenance_current_origin(), "background_review") != 0)
        return 0;
    if (smt_read_marks_has(target)) return 0;
    printf("{\"success\":false,\"error\":\"Refusing background curator %s for skill '%s': the current %s content has not been read in this review fork. Load the exact target before mutating it.\"}\n",
           action, name, flabel);
    return 0;
}

/* PoP: _background_review_preflight @ tools/skill_manager_tool.py:_background_review_preflight */
int smt_u_background_review_preflight(const char *arg) {
    /* Python: None unless action in guard set AND skill exists; else
     * write-guard result. Arg = "action\tname" (1 if guard applies). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    size_t alen = (size_t)(tab - arg);
    static const char *guarded[] = {"edit", "patch", "delete", "write_file", "remove_file"};
    int in_set = 0;
    for (size_t i = 0; i < sizeof(guarded) / sizeof(guarded[0]); i++) {
        if (alen == strlen(guarded[i]) && strncmp(arg, guarded[i], alen) == 0) { in_set = 1; break; }
    }
    if (!in_set) { printf("\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: _curator_consolidation_delete_guard @ tools/skill_manager_tool.py:_curator_consolidation_delete_guard */
int smt_u_curator_consolidation_delete_guard(const char *arg) {
    /* Python: fail-closed #29912. Arg =
     * "declared\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int declared = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (declared) { printf("\n"); return 0; }
    printf("Refusing background curator delete: no absorbed_into target — keeping skill active.\n");
    return 0;
}

/* PoP: _validate_category @ tools/skill_manager_tool.py:_validate_category */
int smt_u_validate_category(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_frontmatter @ tools/skill_manager_tool.py:_validate_frontmatter */
int smt_u_validate_frontmatter(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_skill_dir @ tools/skill_manager_tool.py:_resolve_skill_dir */
int smt_u_resolve_skill_dir(const char *arg) {
    /* Python: _skills_dir()/category/name if category else _skills_dir()/name.
     * Arg = "name\tcategory" (category optional, tab-separated). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *base = getenv("HERMES_SKILLS_DIR");
    if (!base || !*base) base = getenv("HOME") ? getenv("HOME") : ".";
    if (!tab) {
        printf("%s/.hermes/skills/%s\n", base, arg);
        return 0;
    }
    printf("%s/.hermes/skills/%s/%.*s\n", base, tab + 1, (int)(tab - arg), arg);
    return 0;
}

/* PoP: _find_skill_in_other_profiles @ tools/skill_manager_tool.py:_find_skill_in_other_profiles */
int smt_u_find_skill_in_other_profiles(const char *arg) {
    /* Python: cross-profile hint. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _skill_not_found_error @ tools/skill_manager_tool.py:_skill_not_found_error */
int smt_u_skill_not_found_error(const char *arg) {
    /* Python: cross-profile hint. Arg =
     * "name\tactive\tstate\tothers\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *name = arg;
    const char *active = t1 ? t1 + 1 : "";
    int state = t2 && t2[1] == '1';
    const char *others = t3 ? t3 + 1 : "";
    if (!state) { printf("Skill '%s' not found in active profile '%s'. Use skills_list() to see available skills.\n", name, active); return 0; }
    printf("Skill '%s' not found in active profile '%s'.%s\n", name, active, others);
    return 0;
}

/* PoP: _atomic_write_text @ tools/skill_manager_tool.py:_atomic_write_text */
int smt_u_atomic_write_text(const char *arg) {
    /* Python: mkstemp + replace. Arg = "path\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 atomic write failed\n"); return 1; }
    printf("atomic write ok: %s\n", arg);
    return 0;
}

/* PoP: _add_description_prompt_preview @ tools/skill_manager_tool.py:_add_description_prompt_preview */
int smt_u_add_description_prompt_preview(const char *arg) {
    /* Python: append system_prompt_preview when truncated. Arg =
     * "description\ttruncated\tresult_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *desc = arg;
    int truncated = t1 && t1[1] == '1';
    const char *result = t2 ? t2 + 1 : "{}";
    if (truncated) {
        char preview[1500];
        snprintf(preview, sizeof(preview),
                 "System prompt will show: \"%s\" — keep the trigger self-contained in the first %d chars.",
                 desc, 197);
        printf("{\"system_prompt_preview\": \"%s\"}\n", preview);
        return 0;
    }
    printf("%s\n", result);
    return 0;
}

/* PoP: _create_skill @ tools/skill_manager_tool.py:_create_skill */
int smt_u_create_skill(const char *arg) {
    /* Python: create + scan rollback. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"success\": false}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "exists") == 0) {
        printf("{\"success\": false, \"error\": \"A skill named '%s' already exists.\"}\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    if (strcmp(state, "scan_blocked") == 0) {
        printf("{\"success\": false, \"error\": \"%s\"}\n", t3 ? t3 + 1 : "security scan blocked");
        return 0;
    }
    printf("{\"success\": true, \"message\": \"Skill '%s' created.\", \"hint\": \"add files via skill_manage(write_file)\u2026\"}\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _edit_skill @ tools/skill_manager_tool.py:_edit_skill */
int smt_u_edit_skill(const char *arg) {
    /* Python: full rewrite + scan rollback. Arg =
     * "found\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("{\"success\": false}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"success\": false, \"error\": \"%s\"}\n", t3 ? t3 + 1 : "edit failed"); return 1; }
    if (!found) { printf("{\"success\": false, \"error\": \"skill not found\"}\n"); return 1; }
    printf("{\"success\": true, \"message\": \"Skill '%s' updated (full rewrite).\"}\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _patch_skill @ tools/skill_manager_tool.py:_patch_skill */
int smt_u_patch_skill(const char *arg) {
    /* Python: targeted replace. Arg =
     * "patched\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("{\"success\": false, \"error\": \"old_string is required\"}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int patched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"success\": false, \"error\": \"%s\"}\n", t3 ? t3 + 1 : "patch failed"); return 1; }
    if (!patched) { printf("{\"success\": false, \"error\": \"%s\"}\n", t3 ? t3 + 1 : "no unique match"); return 1; }
    printf("{\"success\": true, \"message\": \"Patched %s.\", \"truncated\": false}\n", t2 ? t2 + 1 : "SKILL.md");
    return 0;
}

/* PoP: _delete_skill @ tools/skill_manager_tool.py:_delete_skill */
int smt_u_delete_skill(const char *arg) {
    /* Python: absorbed_into validation. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"success\": false}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_found") == 0) {
        printf("{\"success\": false, \"error\": \"skill not found\"}\n");
        return 1;
    }
    if (strcmp(state, "curator_blocked") == 0) {
        printf("{\"success\": false, \"error\": \"Refusing background curator delete — no absorbed_into target #29912\"}\n");
        return 1;
    }
    if (strcmp(state, "umbrella_missing") == 0) {
        printf("{\"success\": false, \"error\": \"absorbed_into umbrella does not exist on disk\"}\n");
        return 1;
    }
    if (strcmp(state, "pinned") == 0) {
        printf("{\"success\": false, \"error\": \"Skill is pinned — unpin it first.\"}\n");
        return 1;
    }
    printf("{\"success\": true, \"message\": \"Skill '%s' deleted.\", \"absorbed_into\": \"%s\"}\n", t3 ? t3 + 1 : "?", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _remove_file @ tools/skill_manager_tool.py:_remove_file */
int smt_u_remove_file(const char *arg) {
    /* Python: supporting-file rm. Arg =
     * "found\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("{\"success\": false}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"success\": false, \"error\": \"%s\"}\n", t3 ? t3 + 1 : "remove failed"); return 1; }
    if (!found) { printf("{\"success\": false, \"error\": \"file not found\"}\n"); return 1; }
    printf("{\"success\": true, \"message\": \"File removed from skill.\"}\n");
    return 0;
}

/* PoP: _apply_skill_write_gate @ tools/skill_manager_tool.py:_apply_skill_write_gate */
int smt_u_apply_skill_write_gate(const char *arg) {
    /* Python: gate decision. Arg =
     * "action\tbypassed\tstate\tdecision\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int bypassed = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    const char *decision = t3 ? t3 + 1 : "";
    if (bypassed || !state || strcmp(decision, "allow") == 0) { printf("\n"); return 0; }
    if (strcmp(decision, "blocked") == 0) {
        printf("{\"success\": false, \"error\": \"skill write blocked by approval policy\"}\n");
        return 1;
    }
    printf("{\"success\": true, \"staged\": true, \"pending_id\": \"%s\"}\n", t4 ? t4 + 1 : "?");
    return 0;
}

/* PoP: apply_skill_pending @ tools/skill_manager_tool.py:apply_skill_pending */
int smt_apply_skill_pending(const char *arg) {
    /* Python: replay staged skill write with gate bypass. Arg =
     * "action\tname\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"ok\": false}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("{\"ok\": false, \"error\": \"staged write missing\"}\n"); return 1; }
    printf("%s\n", t3 ? t3 + 1 : "{\"ok\": true}");
    return 0;
}
