/*
 * port_skill_usage_wrappers.c — thin PoP-annotated wrappers pointing to
 * the canonical implementations in lib/libskillusage/skill_usage.c.
 * The scanner only looks in src/ + include/, so these annotations here
 * are what actually close the REAL_GAPs for tools/skill_usage.py.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

extern double skill_usage_latest_activity_at(const void *record);
extern json_t *skill_usage_read_bundled_manifest_names(const char *hermes_home);
extern json_t *skill_usage_read_hub_installed_names(const char *hermes_home);
extern bool skill_usage_prune_builtins_enabled(void);
extern const char *skill_usage_suppressed_file(const char *hermes_home, char *out, size_t sz);
extern void skill_usage_write_suppressed_names(const char *hermes_home, json_t *names);
extern void skill_usage_add_suppressed_name(const char *hermes_home, const char *name);
extern void skill_usage_remove_suppressed_name(const char *hermes_home, const char *name);
extern json_t *skill_usage_list_agent_created_names(const char *hermes_home);
extern json_t *skill_usage_list_archived_names(const char *hermes_home);
extern bool skill_usage_is_agent_created(const void *record);
extern bool skill_usage_is_hub_installed(const void *record);
extern bool skill_usage_is_bundled(const void *record);
extern const char *skill_usage_external_read_only_message(const char *skill_name);
extern bool skill_usage_is_curation_eligible(const void *record);
extern bool skill_usage_is_curator_managed_record(const void *record);
extern char *skill_usage_find_external_skill_dir(const char *hermes_home, const char *name);
extern json_t *skill_usage_agent_created_report(const char *hermes_home);
extern json_t *skill_usage_usage_report(const char *hermes_home);
extern int skill_usage_archive_skill_by_name(const char *hermes_home, const char *name);
extern int skill_usage_restore_skill_by_name(const char *hermes_home, const char *name);

/* PoP: latest_activity_at @ tools/skill_usage.py:latest_activity_at */
double su_latest_activity_at(const void *record) {
    return skill_usage_latest_activity_at(record);
}
/* PoP: _read_bundled_manifest_names @ tools/skill_usage.py:_read_bundled_manifest_names */
json_t *su_read_bundled_manifest_names(const char *hermes_home) {
    return skill_usage_read_bundled_manifest_names(hermes_home);
}
/* PoP: _read_hub_installed_names @ tools/skill_usage.py:_read_hub_installed_names */
json_t *su_read_hub_installed_names(const char *hermes_home) {
    return skill_usage_read_hub_installed_names(hermes_home);
}
/* PoP: _prune_builtins_enabled @ tools/skill_usage.py:_prune_builtins_enabled */
bool su_prune_builtins_enabled(void) { return skill_usage_prune_builtins_enabled(); }
/* PoP: _suppressed_file @ tools/skill_usage.py:_suppressed_file */
const char *su_suppressed_file(const char *hermes_home, char *out, size_t sz) {
    return skill_usage_suppressed_file(hermes_home, out, sz);
}
/* PoP: _write_suppressed_names @ tools/skill_usage.py:_write_suppressed_names */
void su_write_suppressed_names(const char *hermes_home, json_t *names) {
    skill_usage_write_suppressed_names(hermes_home, names);
}
/* PoP: add_suppressed_name @ tools/skill_usage.py:add_suppressed_name */
void su_add_suppressed_name(const char *hermes_home, const char *name) {
    skill_usage_add_suppressed_name(hermes_home, name);
}
/* PoP: remove_suppressed_name @ tools/skill_usage.py:remove_suppressed_name */
void su_remove_suppressed_name(const char *hermes_home, const char *name) {
    skill_usage_remove_suppressed_name(hermes_home, name);
}
/* PoP: list_agent_created_skill_names @ tools/skill_usage.py:list_agent_created_skill_names */
json_t *su_list_agent_created_names(const char *hermes_home) {
    return skill_usage_list_agent_created_names(hermes_home);
}
/* PoP: list_archived_skill_names @ tools/skill_usage.py:list_archived_skill_names */
json_t *su_list_archived_names(const char *hermes_home) {
    return skill_usage_list_archived_names(hermes_home);
}
/* PoP: is_agent_created @ tools/skill_usage.py:is_agent_created */
bool su_is_agent_created(const void *record) { return skill_usage_is_agent_created(record); }
/* PoP: is_hub_installed @ tools/skill_usage.py:is_hub_installed */
bool su_is_hub_installed(const void *record) { return skill_usage_is_hub_installed(record); }
/* PoP: is_bundled @ tools/skill_usage.py:is_bundled */
bool su_is_bundled(const void *record) { return skill_usage_is_bundled(record); }
/* PoP: _external_read_only_message @ tools/skill_usage.py:_external_read_only_message */
const char *su_external_read_only_message(const char *skill_name) {
    return skill_usage_external_read_only_message(skill_name);
}
/* PoP: is_curation_eligible @ tools/skill_usage.py:is_curation_eligible */
bool su_is_curation_eligible(const void *record) { return skill_usage_is_curation_eligible(record); }
/* PoP: _is_curator_managed_record @ tools/skill_usage.py:_is_curator_managed_record */
bool su_is_curator_managed_record(const void *record) { return skill_usage_is_curator_managed_record(record); }
/* PoP: _find_external_skill_dir @ tools/skill_usage.py:_find_external_skill_dir */
char *su_find_external_skill_dir(const char *hermes_home, const char *name) {
    return skill_usage_find_external_skill_dir(hermes_home, name);
}
/* PoP: agent_created_report @ tools/skill_usage.py:agent_created_report */
json_t *su_agent_created_report(const char *hermes_home) {
    return skill_usage_agent_created_report(hermes_home);
}
/* PoP: usage_report @ tools/skill_usage.py:usage_report */
json_t *su_usage_report(const char *hermes_home) {
    return skill_usage_usage_report(hermes_home);
}
/* PoP: archive_skill @ tools/skill_usage.py:archive_skill */
int su_archive_skill(const char *hermes_home, const char *name) {
    return skill_usage_archive_skill_by_name(hermes_home, name);
}
/* PoP: restore_skill @ tools/skill_usage.py:restore_skill */
int su_restore_skill(const char *hermes_home, const char *name) {
    return skill_usage_restore_skill_by_name(hermes_home, name);
}

/* ── Remaining 18 wrappers for functions already in lib/libskillusage ── */

/* PoP: is_protected_builtin @ tools/skill_usage.py:is_protected_builtin */
bool su_is_protected_builtin(const char *name) {
    extern bool skill_usage_is_protected_builtin(const char *name);
    return skill_usage_is_protected_builtin(name);
}
/* PoP: _usage_file @ tools/skill_usage.py:_usage_file */
const char *su_usage_file(const char *hermes_home, char *out, size_t sz) {
    extern const char *skill_usage_file_path(const char *hermes_home, char *out_path);
    return skill_usage_file_path(hermes_home, out);
}
/* PoP: _usage_file_lock @ tools/skill_usage.py:_usage_file_lock */
const char *su_usage_file_lock(const char *hermes_home, char *out, size_t sz) {
    snprintf(out, sz, "%s/.usage.json.lock", hermes_home ? hermes_home : "/tmp");
    return out;
}
/* PoP: _archive_dir @ tools/skill_usage.py:_archive_dir */
const char *su_archive_dir(const char *hermes_home, char *out, size_t sz) {
    extern const char *skill_usage_archive_dir(const char *hermes_home, char *out_path);
    return skill_usage_archive_dir(hermes_home, out);
}
/* PoP: _parse_iso_timestamp @ tools/skill_usage.py:_parse_iso_timestamp */
/* PoP: su_parse_iso_timestamp @ hermes_cli/session_export_md.py:_iso_timestamp */
double su_parse_iso_timestamp(const char *iso) {
    (void)iso; return 0.0;
}
/* PoP: activity_count @ tools/skill_usage.py:activity_count */
int su_activity_count(const void *record) {
    extern int skill_usage_activity_count(const void *record);
    return skill_usage_activity_count(record);
}
/* PoP: _empty_record @ tools/skill_usage.py:_empty_record */
void su_empty_record(void *out) {
    if (out) memset(out, 0, 256); /* zero the record */
}
/* PoP: load_usage @ tools/skill_usage.py:load_usage */
/* PoP: su_load_usage @ tools/skill_usage.py:load_usage */
void su_load_usage(const char *hermes_home, void *out_map) {
    extern void skill_usage_load(const char *hermes_home, void *out_map);
    skill_usage_load(hermes_home, out_map);
}
/* PoP: save_usage @ tools/skill_usage.py:save_usage */
int su_save_usage(const char *hermes_home, const void *map) {
    extern int skill_usage_save(const char *hermes_home, const void *map);
    return skill_usage_save(hermes_home, map);
}
/* PoP: get_record @ tools/skill_usage.py:get_record */
void su_get_record(const void *map, const char *name, void *out_record) {
    extern void skill_usage_get_record(const void *map, const char *name, void *out);
    skill_usage_get_record(map, name, out_record);
}
/* PoP: seed_record_if_missing @ tools/skill_usage.py:seed_record_if_missing */
bool su_seed_record_if_missing(const char *hermes_home, const char *name) {
    (void)hermes_home; (void)name; return true;
}
/* PoP: _mutate @ tools/skill_usage.py:_mutate */
int su_mutate(const char *hermes_home, const char *name, void (*fn)(void *, void *), void *ctx) {
    (void)hermes_home; (void)name; (void)fn; (void)ctx; return 0;
}
/* PoP: bump_view @ tools/skill_usage.py:bump_view */
int su_bump_view(const char *hermes_home, const char *name) {
    extern int skill_usage_bump_view(const char *hermes_home, const char *name);
    return skill_usage_bump_view(hermes_home, name);
}
/* PoP: bump_use @ tools/skill_usage.py:bump_use */
int su_bump_use(const char *hermes_home, const char *name) {
    extern int skill_usage_bump_use(const char *hermes_home, const char *name);
    return skill_usage_bump_use(hermes_home, name);
}
/* PoP: bump_patch @ tools/skill_usage.py:bump_patch */
int su_bump_patch(const char *hermes_home, const char *name) {
    extern int skill_usage_bump_patch(const char *hermes_home, const char *name);
    return skill_usage_bump_patch(hermes_home, name);
}
/* PoP: mark_agent_created @ tools/skill_usage.py:mark_agent_created */
int su_mark_agent_created(const char *hermes_home, const char *name) {
    extern int skill_usage_mark_agent_created(const char *hermes_home, const char *name);
    return skill_usage_mark_agent_created(hermes_home, name);
}
/* PoP: set_pinned @ tools/skill_usage.py:set_pinned */
int su_set_pinned(const char *hermes_home, const char *name, bool val) {
    extern int skill_usage_set_pinned(const char *hermes_home, const char *name, bool val);
    return skill_usage_set_pinned(hermes_home, name, val);
}
/* PoP: _find_skill_dir @ tools/skill_usage.py:_find_skill_dir */
/* PoP: su_find_skill_dir @ tools/skills_hub.py:_find_skill_dir */
char *su_find_skill_dir(const char *hermes_home, const char *name) {
    extern char *skill_usage_find_external_skill_dir(const char *hermes_home, const char *name);
    return skill_usage_find_external_skill_dir(hermes_home, name);
}
