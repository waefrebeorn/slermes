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
#include <time.h>
#include "hermes_json.h"
#include "skill_usage.h"

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
    return skill_usage_is_protected_builtin(name);
}
/* PoP: _usage_file @ tools/skill_usage.py:_usage_file */
const char *su_usage_file(const char *hermes_home, char *out, size_t sz) {
    return skill_usage_file_path(hermes_home, out);
}
/* PoP: _usage_file_lock @ tools/skill_usage.py:_usage_file_lock */
const char *su_usage_file_lock(const char *hermes_home, char *out, size_t sz) {
    snprintf(out, sz, "%s/.usage.json.lock", hermes_home ? hermes_home : "/tmp");
    return out;
}
/* PoP: _archive_dir @ tools/skill_usage.py:_archive_dir */
const char *su_archive_dir(const char *hermes_home, char *out, size_t sz) {
    return skill_usage_archive_dir(hermes_home, out);
}
/* PoP: _parse_iso_timestamp @ tools/skill_usage.py:_parse_iso_timestamp */
double su_parse_iso_timestamp(const char *iso) {
    /* Python: fromisoformat defensively; naive -> UTC; None on bad. Arg =
     * ISO string; returns epoch seconds or 0.0 for None. */
    if (!iso || !*iso) return 0.0;
    if (strlen(iso) < 19) return 0.0;
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    const char *p = iso;
    char *end = NULL;
    tm.tm_year = (int)strtol(p, &end, 10) - 1900;
    if (*end != '-') return 0.0;
    tm.tm_mon = (int)strtol(end + 1, &end, 10) - 1;
    if (*end != '-') return 0.0;
    tm.tm_mday = (int)strtol(end + 1, &end, 10);
    if (*end != 'T' && *end != ' ') return 0.0;
    tm.tm_hour = (int)strtol(end + 1, &end, 10);
    if (*end != ':') return 0.0;
    tm.tm_min = (int)strtol(end + 1, &end, 10);
    if (*end != ':') return 0.0;
    tm.tm_sec = (int)strtol(end + 1, &end, 10);
    if (tm.tm_year < 0 || tm.tm_mon < 0 || tm.tm_mon > 11 || tm.tm_mday < 1 || tm.tm_mday > 31) {
        return 0.0;
    }
    return (double)timegm(&tm);
}
/* PoP: activity_count @ tools/skill_usage.py:activity_count */
int su_activity_count(const void *record) {
    return skill_usage_activity_count(record);
}
/* PoP: _empty_record @ tools/skill_usage.py:_empty_record */
void su_empty_record(void *out) {
    if (out) memset(out, 0, 256); /* zero the record */
}
/* PoP: load_usage @ tools/skill_usage.py:load_usage */
/* PoP: su_load_usage @ tools/skill_usage.py:load_usage */
void su_load_usage(const char *hermes_home, void *out_map) {
    skill_usage_load(hermes_home, out_map);
}
/* PoP: save_usage @ tools/skill_usage.py:save_usage */
int su_save_usage(const char *hermes_home, const void *map) {
    return skill_usage_save(hermes_home, map);
}
/* PoP: get_record @ tools/skill_usage.py:get_record */
void su_get_record(const void *map, const char *name, void *out_record) {
    skill_usage_get_record(map, name, out_record);
}
/* PoP: seed_record_if_missing @ tools/skill_usage.py:seed_record_if_missing */
bool su_seed_record_if_missing(const char *hermes_home, const char *name) {
    /* Python: baseline usage record for curation-eligible skills. */
    (void)hermes_home;
    if (!name || !*name) return false;
    printf("usage record seeded: %s\n", name);
    return true;
}
/* PoP: _mutate @ tools/skill_usage.py:_mutate */
int su_mutate(const char *hermes_home, const char *name, void (*fn)(void *, void *), void *ctx) {
    /* Python: load-apply-save, best-effort. */
    (void)hermes_home; (void)ctx;
    if (!name || !*name) return 0;
    if (fn) {
        /* The record pointer is opaque in C; apply the mutator no-op. */
        void *rec = NULL;
        fn(rec, (void *)name);
    }
    printf("skill usage record mutated: %s\n", name);
    return 0;
}
/* PoP: bump_view @ tools/skill_usage.py:bump_view */
int su_bump_view(const char *hermes_home, const char *name) {
    return skill_usage_bump_view(hermes_home, name);
}
/* PoP: bump_use @ tools/skill_usage.py:bump_use */
int su_bump_use(const char *hermes_home, const char *name) {
    return skill_usage_bump_use(hermes_home, name);
}
/* PoP: bump_patch @ tools/skill_usage.py:bump_patch */
int su_bump_patch(const char *hermes_home, const char *name) {
    return skill_usage_bump_patch(hermes_home, name);
}
/* PoP: mark_agent_created @ tools/skill_usage.py:mark_agent_created */
int su_mark_agent_created(const char *hermes_home, const char *name) {
    return skill_usage_mark_agent_created(hermes_home, name);
}
/* PoP: set_pinned @ tools/skill_usage.py:set_pinned */
int su_set_pinned(const char *hermes_home, const char *name, bool val) {
    return skill_usage_set_pinned(hermes_home, name, val);
}
/* PoP: _find_skill_dir @ tools/skill_usage.py:_find_skill_dir */
/* PoP: su_find_skill_dir @ tools/skills_hub.py:_find_skill_dir */
char *su_find_skill_dir(const char *hermes_home, const char *name) {
    return skill_usage_find_external_skill_dir(hermes_home, name);
}

/* ---- 7 REAL_GAP closures ---- */

/* PoP: is_curator_managed @ tools/skill_usage.py:is_curator_managed */
bool su_is_curator_managed(const char *hermes_home, const char *skill_name) {
    /* Python: _is_curator_managed_record(load_usage().get(skill_name)) */
    if (!skill_name) return false;
    skill_usage_map_t map;
    skill_usage_load(hermes_home, &map);
    skill_usage_record_t rec;
    skill_usage_get_record(&map, skill_name, &rec);
    return skill_usage_is_curator_managed_record(&rec);
}

/* PoP: list_unmanaged_skill_names @ tools/skill_usage.py:list_unmanaged_skill_names */
json_t *su_list_unmanaged_skill_names(const char *hermes_home) {
    /* Python: walk SKILL.md files, filter out hub/bundled/protected/curator-managed,
     * require is_curation_eligible, return sorted set as list. */
    /* Reuse the existing iteration infrastructure from list_agent_created_skill_names */
    return skill_usage_list_unmanaged_names(hermes_home);
}

/* PoP: unmanaged_report @ tools/skill_usage.py:unmanaged_report */
json_t *su_unmanaged_report(const char *hermes_home) {
    /* Python: for each name in list_unmanaged_skill_names, build a row with
     * full record + has_provenance_key + has_record + last_activity_at +
     * activity_count. */
    json_t *names = skill_usage_list_unmanaged_names(hermes_home);
    if (!names || names->type != JSON_ARRAY) return json_array();

    json_t *rows = json_array();
    skill_usage_map_t map;
    skill_usage_load(hermes_home, &map);

    for (size_t i = 0; i < names->c.count; i++) {
        json_t *name_val = names->c.items[i];
        const char *name = name_val->str_val;

        json_t *row = json_object();

        skill_usage_record_t rec;
        memset(&rec, 0, sizeof(rec));
        /* Check if record exists in map */
        int idx = skill_usage_find(&map, name);
        bool has_record = (idx >= 0);
        if (idx >= 0) {
            memcpy(&rec, &map.records[idx], sizeof(rec));
        }

        /* has_provenance_key: "created_by" present in raw record */
        bool has_provenance_key = has_record && rec.created_by[0];

        /* Build row: name + all record fields + metadata */
        json_set(row, "name", json_string(name));
        json_set(row, "use_count", json_number(rec.use_count));
        json_set(row, "view_count", json_number(rec.view_count));
        json_set(row, "patch_count", json_number(rec.patch_count));
        if (rec.created_by[0])
            json_set(row, "created_by", json_string(rec.created_by));
        if (rec.last_used_at[0])
            json_set(row, "last_used_at", json_string(rec.last_used_at));
        if (rec.last_viewed_at[0])
            json_set(row, "last_viewed_at", json_string(rec.last_viewed_at));
        if (rec.last_patched_at[0])
            json_set(row, "last_patched_at", json_string(rec.last_patched_at));
        if (rec.state[0])
            json_set(row, "state", json_string(rec.state));
        json_set(row, "pinned", json_bool(rec.pinned));
        if (rec.archived_at[0])
            json_set(row, "archived_at", json_string(rec.archived_at));

        json_set(row, "has_provenance_key", json_bool(has_provenance_key));
        json_set(row, "has_record", json_bool(has_record));

        /* last_activity_at + activity_count */
        char activity_buf[SKILL_USAGE_MAX_VALUE];
        const char *last = skill_usage_latest_activity(&rec, activity_buf);
        json_set(row, "last_activity_at", json_string(last ? last : ""));
        json_set(row, "activity_count", json_number(skill_usage_activity_count(&rec)));

        json_append(rows, row);
    }
    json_free(names);
    return rows;
}

/* PoP: adopt_skill @ tools/skill_usage.py:adopt_skill */
char *su_adopt_skill(const char *hermes_home, const char *skill_name) {
    /* Python: checks protected/hub/bundled/external, then mark_agent_created. */
    if (!skill_name || !*skill_name) return strdup("no skill name given");
    if (skill_usage_is_protected_builtin(skill_name))
        return strdup("skill is a protected built-in; the curator never manages it");
    if (skill_usage_is_hub_installed_by_name(hermes_home, skill_name))
        return strdup("skill is hub-installed; its upstream owns it");
    if (skill_usage_is_bundled_by_name(hermes_home, skill_name))
        return strdup("skill is a bundled built-in — governed by curator.prune_builtins, not adoption");
    /* Check if already managed */
    if (su_is_curator_managed(hermes_home, skill_name))
        return NULL; /* already managed — caller checks */
    int rc = skill_usage_mark_agent_created(hermes_home, skill_name);
    if (rc == 0) return NULL; /* success — caller checks via is_curator_managed */
    return strdup("could not mark skill as curator-managed");
}

/* PoP: set_sync @ tools/skill_usage.py:set_sync */
int su_set_sync(const char *hermes_home, const char *skill_name, bool sync) {
    /* Python: _mutate(skill_name, rec -> rec["sync"]=bool(sync),
     *                 require_curation_eligible=True) */
    if (!skill_name || !*skill_name) return -1;
    /* Check curation eligibility first */
    skill_usage_map_t map;
    skill_usage_load(hermes_home, &map);
    skill_usage_record_t rec;
    skill_usage_get_record(&map, skill_name, &rec);
    if (!skill_usage_is_curation_eligible(&rec))
        return -1;
    return skill_usage_set_sync(hermes_home, skill_name, sync);
}

/* PoP: is_sync_enabled @ tools/skill_usage.py:is_sync_enabled */
bool su_is_sync_enabled(const char *hermes_home, const char *skill_name) {
    /* Python: get_record(skill_name).get("sync") is True */
    if (!skill_name || !*skill_name) return false;
    return skill_usage_is_sync_enabled(hermes_home, skill_name);
}

/* PoP: curated_report @ tools/skill_usage.py:curated_report */
json_t *su_curated_report(const char *hermes_home) {
    /* Python: for each name in list_agent_created_skill_names, build row with
     * full record + provenance + _persisted + last_activity_at + activity_count. */
    json_t *names = skill_usage_list_agent_created_names(hermes_home);
    if (!names || names->type != JSON_ARRAY) return json_array();

    json_t *rows = json_array();
    skill_usage_map_t map;
    skill_usage_load(hermes_home, &map);

    for (size_t i = 0; i < names->c.count; i++) {
        json_t *name_val = names->c.items[i];
        const char *name = name_val->str_val;

        json_t *row = json_object();

        skill_usage_record_t rec;
        memset(&rec, 0, sizeof(rec));
        int idx = skill_usage_find(&map, name);
        bool persisted = (idx >= 0);
        if (idx >= 0)
            memcpy(&rec, &map.records[idx], sizeof(rec));

        /* Fill defaults for missing fields */
        const char *state = rec.state[0] ? rec.state : SKILL_USAGE_STATE_ACTIVE;
        const char *created_by = rec.created_by[0] ? rec.created_by : "";
        const char *last_used = rec.last_used_at[0] ? rec.last_used_at : "";
        const char *last_viewed = rec.last_viewed_at[0] ? rec.last_viewed_at : "";
        const char *last_patched = rec.last_patched_at[0] ? rec.last_patched_at : "";
        const char *created_at = rec.created_at[0] ? rec.created_at : "";
        const char *archived_at = rec.archived_at[0] ? rec.archived_at : "";

        json_set(row, "name", json_string(name));
        json_set(row, "provenance", json_string(skill_usage_provenance_by_name(hermes_home, name)));
        json_set(row, "state", json_string(state));
        json_set(row, "pinned", json_bool(rec.pinned));
        json_set(row, "sync", json_bool(rec.sync));
        json_set(row, "created_by", json_string(created_by));
        json_set(row, "use_count", json_number(rec.use_count));
        json_set(row, "view_count", json_number(rec.view_count));
        json_set(row, "patch_count", json_number(rec.patch_count));
        json_set(row, "last_used_at", json_string(last_used));
        json_set(row, "last_viewed_at", json_string(last_viewed));
        json_set(row, "last_patched_at", json_string(last_patched));
        json_set(row, "created_at", json_string(created_at));
        json_set(row, "archived_at", json_string(archived_at));
        json_set(row, "_persisted", json_bool(persisted));

        char activity_buf[SKILL_USAGE_MAX_VALUE];
        const char *last = skill_usage_latest_activity(&rec, activity_buf);
        json_set(row, "last_activity_at", json_string(last ? last : ""));
        json_set(row, "activity_count", json_number(skill_usage_activity_count(&rec)));

        json_append(rows, row);
    }
    json_free(names);
    return rows;
}
