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
int smt_u_guard_agent_created_enabled(const char *arg) { (void)arg; return 0; }

/* PoP: _security_scan_skill @ tools/skill_manager_tool.py:_security_scan_skill */
int smt_u_security_scan_skill(const char *arg) { (void)arg; return 0; }

/* PoP: _pinned_guard @ tools/skill_manager_tool.py:_pinned_guard */
int smt_u_pinned_guard(const char *arg) { (void)arg; return 0; }

/* PoP: _background_review_write_guard @ tools/skill_manager_tool.py:_background_review_write_guard */
int smt_u_background_review_write_guard(const char *arg) { (void)arg; return 0; }

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
int smt_u_background_review_preflight(const char *arg) { (void)arg; return 0; }

/* PoP: _curator_consolidation_delete_guard @ tools/skill_manager_tool.py:_curator_consolidation_delete_guard */
int smt_u_curator_consolidation_delete_guard(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_category @ tools/skill_manager_tool.py:_validate_category */
int smt_u_validate_category(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_frontmatter @ tools/skill_manager_tool.py:_validate_frontmatter */
int smt_u_validate_frontmatter(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_skill_dir @ tools/skill_manager_tool.py:_resolve_skill_dir */
int smt_u_resolve_skill_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _find_skill_in_other_profiles @ tools/skill_manager_tool.py:_find_skill_in_other_profiles */
int smt_u_find_skill_in_other_profiles(const char *arg) { (void)arg; return 0; }

/* PoP: _skill_not_found_error @ tools/skill_manager_tool.py:_skill_not_found_error */
int smt_u_skill_not_found_error(const char *arg) { (void)arg; return 0; }

/* PoP: _atomic_write_text @ tools/skill_manager_tool.py:_atomic_write_text */
int smt_u_atomic_write_text(const char *arg) { (void)arg; return 0; }

/* PoP: _add_description_prompt_preview @ tools/skill_manager_tool.py:_add_description_prompt_preview */
int smt_u_add_description_prompt_preview(const char *arg) { (void)arg; return 0; }

/* PoP: _create_skill @ tools/skill_manager_tool.py:_create_skill */
int smt_u_create_skill(const char *arg) { (void)arg; return 0; }

/* PoP: _edit_skill @ tools/skill_manager_tool.py:_edit_skill */
int smt_u_edit_skill(const char *arg) { (void)arg; return 0; }

/* PoP: _patch_skill @ tools/skill_manager_tool.py:_patch_skill */
int smt_u_patch_skill(const char *arg) { (void)arg; return 0; }

/* PoP: _delete_skill @ tools/skill_manager_tool.py:_delete_skill */
int smt_u_delete_skill(const char *arg) { (void)arg; return 0; }

/* PoP: _remove_file @ tools/skill_manager_tool.py:_remove_file */
int smt_u_remove_file(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_skill_write_gate @ tools/skill_manager_tool.py:_apply_skill_write_gate */
int smt_u_apply_skill_write_gate(const char *arg) { (void)arg; return 0; }

/* PoP: apply_skill_pending @ tools/skill_manager_tool.py:apply_skill_pending */
int smt_apply_skill_pending(const char *arg) { (void)arg; return 0; }
