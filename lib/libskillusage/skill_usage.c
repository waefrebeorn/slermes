#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
/*
 * skill_usage.c — Skill usage telemetry + provenance tracking for Hermes C.
 * Port of Python tools/skill_usage.py.
 *
 * Tracks per-skill usage metadata in a sidecar JSON file
 * (~/.hermes/skills/.usage.json) keyed by skill name.
 * All counter bumps are best-effort: errors are returned but never
 * abort the caller.
 */

#include "skill_usage.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

/* ================================================================
 *  Helpers
 * ================================================================ */

/* PoP: _now_iso @ skill_usage:skill_usage_now_iso */
void skill_usage_now_iso(char *out_buf)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(out_buf, 32, "%Y-%m-%dT%H:%M:%S", &tm);
}

const char *skill_usage_file_path(const char *hermes_home, char *out_path)
{
    snprintf(out_path, SKILL_USAGE_MAX_PATH, "%s/skills/.usage.json", hermes_home);
    return out_path;
}

/* Port of Python tools/skill_usage.py:_archive_dir() */
const char *skill_usage_archive_dir(const char *hermes_home, char *out_path)
{
    snprintf(out_path, SKILL_USAGE_MAX_PATH, "%s/skills/.archive", hermes_home);
    return out_path;
}

/* Parse a JSON record into a skill_usage_record_t */
static void record_from_json(const json_t *j, skill_usage_record_t *r)
{
    const char *val;

    val = json_get_str(j, "created_by", NULL);
    if (val) strncpy(r->created_by, val, sizeof(r->created_by) - 1);

    r->use_count   = (int)json_get_num(j, "use_count", 0);
    r->view_count  = (int)json_get_num(j, "view_count", 0);
    r->patch_count = (int)json_get_num(j, "patch_count", 0);

    val = json_get_str(j, "last_used_at", NULL);
    if (val) strncpy(r->last_used_at, val, sizeof(r->last_used_at) - 1);

    val = json_get_str(j, "last_viewed_at", NULL);
    if (val) strncpy(r->last_viewed_at, val, sizeof(r->last_viewed_at) - 1);

    val = json_get_str(j, "last_patched_at", NULL);
    if (val) strncpy(r->last_patched_at, val, sizeof(r->last_patched_at) - 1);

    val = json_get_str(j, "created_at", NULL);
    if (val) strncpy(r->created_at, val, sizeof(r->created_at) - 1);

    val = json_get_str(j, "state", SKILL_USAGE_STATE_ACTIVE);
    strncpy(r->state, val, sizeof(r->state) - 1);

    r->pinned = json_get_bool(j, "pinned", false);

    val = json_get_str(j, "archived_at", NULL);
    if (val) strncpy(r->archived_at, val, sizeof(r->archived_at) - 1);
}

/* Serialize a skill_usage_record_t to a JSON object */
static json_t *record_to_json(const skill_usage_record_t *r)
{
    json_t *j = json_object();
    if (!j) return NULL;

    if (r->created_by[0])
        json_set(j, "created_by", json_string(r->created_by));
    json_set(j, "use_count", json_number(r->use_count));
    json_set(j, "view_count", json_number(r->view_count));
    json_set(j, "patch_count", json_number(r->patch_count));

    if (r->last_used_at[0])
        json_set(j, "last_used_at", json_string(r->last_used_at));
    if (r->last_viewed_at[0])
        json_set(j, "last_viewed_at", json_string(r->last_viewed_at));
    if (r->last_patched_at[0])
        json_set(j, "last_patched_at", json_string(r->last_patched_at));
    if (r->created_at[0])
        json_set(j, "created_at", json_string(r->created_at));

    json_set(j, "state", json_string(r->state));
    json_set(j, "pinned", json_bool(r->pinned));

    if (r->archived_at[0])
        json_set(j, "archived_at", json_string(r->archived_at));

    return j;
}

/* ================================================================
 *  I/O
 * ================================================================ */

/* PoP: load_usage @ skill_usage:skill_usage_load */
void skill_usage_load(const char *hermes_home, skill_usage_map_t *out_map)
{
    memset(out_map, 0, sizeof(*out_map));
    char path[SKILL_USAGE_MAX_PATH];
    skill_usage_file_path(hermes_home, path);

    struct stat st;
    if (stat(path, &st) != 0)
        return; /* file doesn't exist — empty map */

    char *err = NULL;
    json_t *root = json_parse_file(path, &err);
    if (!root) {
        /* Corrupt or unreadable — return empty map */
        if (err) free(err);
        return;
    }

    /* Read each key-value pair as a record */
    /* json_parse_file returns a JSON object: { "skill_name": { ... }, ... } */
    /* We iterate by serializing, parsing keys... */
    /* Since our json lib doesn't have iteration, we serialize and re-parse */
    /* Actually, let's use json_get_str with empty defaults — need iteration */
    /* Alternative: serialize to string, parse keys with simple regex */

    /* Simple approach: json lib has json_get() for array access but not
     * object iteration. We'll serialize and parse key names manually. */
    char *text = json_serialize(root);
    if (!text) {
        json_free(root);
        return;
    }

    /* Parse records by scanning the JSON object keys */
    /* Format: {"name":{...}, "name2":{...}} */
    /* Scan for quoted keys: "skill_name": */
    const char *p = text;
    while (*p && out_map->count < SKILL_USAGE_MAX_SKILLS) {
        /* Find next key start */
        while (*p && *p != '"') p++;
        if (!*p) break;
        p++; /* skip opening quote */

        /* Read key (skill name) */
        char key[SKILL_USAGE_MAX_NAME];
        int ki = 0;
        while (*p && *p != '"' && ki < (int)sizeof(key) - 1)
            key[ki++] = *p++;
        key[ki] = '\0';
        if (!*p) break;
        p++; /* skip closing quote */

        /* Expect ':' then '{' for the value */
        while (*p && *p == ':') p++;
        while (*p && *p == ' ') p++;
        if (*p != '{') break;

        /* Find matching closing brace */
        int depth = 0;
        const char *val_start = p;
        const char *val_end = p;
        while (*p) {
            if (*p == '{') depth++;
            if (*p == '}') {
                depth--;
                if (depth == 0) { val_end = p + 1; break; }
            }
            p++;
        }

        if (val_end > val_start) {
            /* Parse the value as JSON */
            size_t vlen = (size_t)(val_end - val_start);
            char *vstr = malloc(vlen + 1);
            if (vstr) {
                memcpy(vstr, val_start, vlen);
                vstr[vlen] = '\0';
                char *verr = NULL;
                json_t *vj = json_parse(vstr, &verr);
                if (vj) {
                    skill_usage_record_t *r = &out_map->records[out_map->count];
                    strncpy(r->name, key, sizeof(r->name) - 1);
                    record_from_json(vj, r);
                    out_map->count++;
                    json_free(vj);
                }
                free(vstr);
                if (verr) free(verr);
            }
        }

        p = val_end;
    }

    free(text);
    json_free(root);
}

/* PoP: save_usage @ skill_usage:skill_usage_save */
int skill_usage_save(const char *hermes_home, const skill_usage_map_t *map)
{
    /* Build JSON object */
    json_t *root = json_object();
    if (!root) return -1;

    for (int i = 0; i < map->count; i++) {
        json_t *jrec = record_to_json(&map->records[i]);
        if (jrec) {
            json_set(root, map->records[i].name, jrec);
        }
    }

    char *text = json_serialize_pretty(root, 2);
    json_free(root);
    if (!text) return -1;

    /* Atomic write: mkstemp -> write -> rename */
    char path[SKILL_USAGE_MAX_PATH];
    skill_usage_file_path(hermes_home, path);

    /* Ensure parent dirs exist */
    mkdir(hermes_home, 0755);
    char dir[SKILL_USAGE_MAX_PATH];
    snprintf(dir, sizeof(dir), "%s/skills", hermes_home);
    mkdir(dir, 0755);

    char tmp_path[SKILL_USAGE_MAX_PATH + 32];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmpXXXXXX", path);

    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        free(text);
        return -1;
    }

    size_t len = strlen(text);
    ssize_t written = write(fd, text, len);
    (void)written;
    fsync(fd);
    close(fd);
    free(text);

    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return -1;
    }

    return 0;
}

/* ================================================================
 *  Record operations
 * ================================================================ */

int skill_usage_find(const skill_usage_map_t *map, const char *skill_name)
{
    if (!skill_name || !*skill_name || !map) return -1;
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->records[i].name, skill_name) == 0)
            return i;
    }
    return -1;
}

/* PoP: get_record @ skill_usage:skill_usage_get_record */
/* PoP: skill_usage_get_record @ skill_usage:get_record */
/* PoP: skill_usage_get_record @ tools/skill_usage.py:get_record */
void skill_usage_get_record(const skill_usage_map_t *map,
                             const char *skill_name,
                             skill_usage_record_t *out_record)
{
    int idx = skill_usage_find(map, skill_name);
    if (idx >= 0) {
        *out_record = map->records[idx];
    } else {
        /* Return empty default with name set */
        memset(out_record, 0, sizeof(*out_record));
        if (skill_name)
            strncpy(out_record->name, skill_name, sizeof(out_record->name) - 1);
        strncpy(out_record->state, SKILL_USAGE_STATE_ACTIVE, sizeof(out_record->state) - 1);
        skill_usage_now_iso(out_record->created_at);
    }
}

/* PoP: seed_record_if_missing @ skill_usage:_mutate_load */
static int _mutate_load(const char *hermes_home,
                         skill_usage_map_t *map,
                         const char *skill_name)
{
    if (!skill_name || !*skill_name) return -1;

    skill_usage_load(hermes_home, map);

    int idx = skill_usage_find(map, skill_name);
    if (idx < 0) {
        /* Create new record */
        if (map->count >= SKILL_USAGE_MAX_SKILLS) return -1;
        idx = map->count;
        skill_usage_record_t *r = &map->records[idx];
        memset(r, 0, sizeof(*r));
        strncpy(r->name, skill_name, sizeof(r->name) - 1);
        strncpy(r->state, SKILL_USAGE_STATE_ACTIVE, sizeof(r->state) - 1);
        skill_usage_now_iso(r->created_at);
        map->count++;
    }

    return idx;
}

static int _mutate_save(const char *hermes_home,
                         skill_usage_map_t *map,
                         int idx)
{
    (void)idx; /* if save fails, the caller still returns error */
    return skill_usage_save(hermes_home, map);
}

/* ================================================================
 *  Counter bumps
 * ================================================================ */

/* PoP: bump_view @ skill_usage:skill_usage_bump_view */
/* PoP: skill_usage_bump_view @ skill_usage:bump_view */
/* PoP: skill_usage_bump_view @ tools/skill_usage.py:bump_view */
int skill_usage_bump_view(const char *hermes_home, const char *skill_name)
{
    skill_usage_map_t map;
    int idx = _mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    r->view_count++;
    skill_usage_now_iso(r->last_viewed_at);

    return _mutate_save(hermes_home, &map, idx);
}

/* PoP: bump_use @ skill_usage:skill_usage_bump_use */
/* PoP: skill_usage_bump_use @ skill_usage:bump_use */
/* PoP: skill_usage_bump_use @ tools/skill_usage.py:bump_use */
int skill_usage_bump_use(const char *hermes_home, const char *skill_name)
{
    skill_usage_map_t map;
    int idx = _mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    r->use_count++;
    skill_usage_now_iso(r->last_used_at);

    return _mutate_save(hermes_home, &map, idx);
}

/* PoP: bump_patch @ skill_usage:skill_usage_bump_patch */
/* PoP: skill_usage_bump_patch @ skill_usage:bump_patch */
/* PoP: skill_usage_bump_patch @ tools/skill_usage.py:bump_patch */
int skill_usage_bump_patch(const char *hermes_home, const char *skill_name)
{
    skill_usage_map_t map;
    int idx = _mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    r->patch_count++;
    skill_usage_now_iso(r->last_patched_at);

    return _mutate_save(hermes_home, &map, idx);
}

/* ================================================================
 *  Provenance & lifecycle
 * ================================================================ */

/* PoP: mark_agent_created @ skill_usage:skill_usage_mark_agent_created */
/* PoP: skill_usage_mark_agent_created @ skill_usage:mark_agent_created */
/* PoP: skill_usage_mark_agent_created @ tools/skill_usage.py:mark_agent_created */
int skill_usage_mark_agent_created(const char *hermes_home, const char *skill_name)
{
    skill_usage_map_t map;
    int idx = _mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    strncpy(r->created_by, "agent", sizeof(r->created_by) - 1);

    return _mutate_save(hermes_home, &map, idx);
}

/* PoP: set_state @ skill_usage:skill_usage_set_state */
int skill_usage_set_state(const char *hermes_home, const char *skill_name,
                           const char *state)
{
    if (!state) return -1;
    if (strcmp(state, SKILL_USAGE_STATE_ACTIVE) != 0 &&
        strcmp(state, SKILL_USAGE_STATE_STALE) != 0 &&
        strcmp(state, SKILL_USAGE_STATE_ARCHIVED) != 0)
        return -1; /* invalid state */

    skill_usage_map_t map;
    int idx = _mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    strncpy(r->state, state, sizeof(r->state) - 1);

    if (strcmp(state, SKILL_USAGE_STATE_ARCHIVED) == 0) {
        skill_usage_now_iso(r->archived_at);
    } else if (strcmp(state, SKILL_USAGE_STATE_ACTIVE) == 0) {
        r->archived_at[0] = '\0';
    }

    return _mutate_save(hermes_home, &map, idx);
}

/* PoP: set_pinned @ skill_usage:skill_usage_set_pinned */
/* PoP: skill_usage_set_pinned @ skill_usage:set_pinned */
/* PoP: skill_usage_set_pinned @ tools/skill_usage.py:set_pinned */
int skill_usage_set_pinned(const char *hermes_home, const char *skill_name,
                            bool pinned)
{
    skill_usage_map_t map;
    int idx = _mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    map.records[idx].pinned = pinned;

    return _mutate_save(hermes_home, &map, idx);
}

/* PoP: forget @ skill_usage:skill_usage_forget */
/* PoP: skill_usage_forget @ skill_usage:forget */
/* PoP: skill_usage_forget @ tools/skill_usage.py:forget */
int skill_usage_forget(const char *hermes_home, const char *skill_name)
{
    if (!skill_name || !*skill_name) return -1;

    skill_usage_map_t map;
    skill_usage_load(hermes_home, &map);

    int idx = skill_usage_find(&map, skill_name);
    if (idx < 0) return 0; /* nothing to forget */

    /* Shift remaining records left */
    for (int i = idx; i < map.count - 1; i++) {
        map.records[i] = map.records[i + 1];
    }
    map.count--;

    return skill_usage_save(hermes_home, &map);
}

/* ================================================================
 *  Archive / restore
 * ================================================================ */

/* PoP: archive_skill @ skill_usage:skill_usage_archive */
int skill_usage_archive(const char *hermes_home, const char *skill_name,
                         char *out_msg) {
    out_msg[0] = '\0';

    /* Find the skill directory */
    char skills_dir[SKILL_USAGE_MAX_PATH];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", hermes_home);

    char src[SKILL_USAGE_MAX_PATH];
    snprintf(src, sizeof(src), "%s/%s", skills_dir, skill_name);

    struct stat st;
    if (stat(src, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(out_msg, SKILL_USAGE_MAX_VALUE,
                 "skill '%s' not found", skill_name);
        return -1;
    }

    /* Ensure archive dir exists */
    char archive_dir[SKILL_USAGE_MAX_PATH];
    skill_usage_archive_dir(hermes_home, archive_dir);
    mkdir(archive_dir, 0755);

    char dest[SKILL_USAGE_MAX_PATH];
    snprintf(dest, sizeof(dest), "%s/%s", archive_dir, skill_name);

    /* If dest exists, append timestamp */
    if (stat(dest, &st) == 0) {
        char ts[32];
        skill_usage_now_iso(ts);
        /* Replace colons with dashes for filesystem safety */
        for (char *c = ts; *c; c++) if (*c == ':') *c = '-';
        snprintf(dest, sizeof(dest), "%s/%s-%s", archive_dir, skill_name, ts);
    }

    if (rename(src, dest) != 0) {
        snprintf(out_msg, SKILL_USAGE_MAX_VALUE,
                 "failed to archive: %s", strerror(errno));
        return -1;
    }

    skill_usage_set_state(hermes_home, skill_name, SKILL_USAGE_STATE_ARCHIVED);

    snprintf(out_msg, SKILL_USAGE_MAX_VALUE, "archived to %s", dest);
    return 0;
}

                         /* ================================================================
                          *  Archive / restore - additional functions
                          * ================================================================ */

                         /* Port of Python tools/skill_usage.py:_usage_file() */
                         static void _skill_usage_file(const char *hermes_home, char *out_path) {
                             snprintf(out_path, SKILL_USAGE_MAX_PATH, "%s/skills/.usage.json", hermes_home);
                         }

                         /* Port of Python tools/skill_usage.py:_usage_file_lock() */
                         static int _skill_usage_file_lock(const char *hermes_home) {
                             char lock_path[SKILL_USAGE_MAX_PATH];
                             _skill_usage_file(hermes_home, lock_path);
                             snprintf(lock_path + strlen(lock_path), SKILL_USAGE_MAX_PATH - strlen(lock_path), ".lock");
    
                             int fd = open(lock_path, O_CREAT | O_EXCL | O_WRONLY, 0644);
                             if (fd >= 0) {
                                 close(fd);
                                 return 1;
                             }
                             return 0;
                         }

                         /* Port of Python tools/skill_usage.py:_parse_iso_timestamp() */
                         static time_t _skill_usage_parse_iso(const char *iso_str) {
                             if (!iso_str || !*iso_str) return 0;
                             struct tm tm = {0};
                             if (strptime(iso_str, "%Y-%m-%dT%H:%M:%S", &tm) == NULL) {
                                 /* Try with microseconds */
                                 if (strptime(iso_str, "%Y-%m-%dT%H:%M:%S.%f", &tm) == NULL) {
                                     return 0;
                                 }
                             }
                             return mktime(&tm);
                         }

                         /* ================================================================
                          *  Protected builtins list
                          * ================================================================ */

                         static const char *g_protected_builtins[] = {
                             "terminal", "file", "memory", "send_message", "execute_code",
                             "web", "vision", "tts", "clarify", "delegate_task", "cronjob",
                             "session_search", "skill_manager", "todo", "patch", "kanban",
                             "computer_use", "tirith", "voice_mode", "image_gen", "homeassistant",
                             "discord", "mcp_tool", "feishu_tools", "x_search", "yuanbao_tools",
                             "mixture_of_agents", "video_gen", "web_search", "xai_http",
                             "account_usage", "ansi_strip", "skills_guard", "skills_sync",
                             "skills_tool", "slash_confirm", "read_extract", "read_terminal",
                             "registry", "schema_sanitizer", "interrupt", "lazy_deps",
                             "managed_tool_gateway", "microsoft_graph", "neutts_synth",
                             "openrouter_client", "osv_check", "patch_parser", "path_security",
                             "process_registry", "tool_output_limits", "tool_result_storage",
                             "tool_search", "threat_patterns", "credential_files",
                             "skill_provenance", "tools_ast_audit", "video_analyze",
                             "video_gen_registry", "image_gen_registry", "web_search_registry",
                             NULL
                         };

                         /* PoP: skill_usage_is_protected_builtin @ tools/skill_usage.py:is_protected_builtin */
                         int skill_usage_is_protected_builtin(const char *skill_name) {
                             if (!skill_name || !*skill_name) return 0;
                             for (int i = 0; g_protected_builtins[i]; i++) {
                                 if (strcmp(skill_name, g_protected_builtins[i]) == 0) {
                                     return 1;
                                 }
                             }
                             return 0;
                         }

                         /* PoP: _empty_record @ skill_usage:_skill_usage_empty_record */
                         static void _skill_usage_empty_record(skill_usage_record_t *r, const char *name) {
                             memset(r, 0, sizeof(*r));
                             if (name) strncpy(r->name, name, sizeof(r->name) - 1);
                             strncpy(r->state, SKILL_USAGE_STATE_ACTIVE, sizeof(r->state) - 1);
                             skill_usage_now_iso(r->created_at);
                         }

                         /* Port of Python tools/skill_usage.py:load_usage() */
                         void skill_usage_load_full(const char *hermes_home, skill_usage_map_t *out_map) {
                             memset(out_map, 0, sizeof(*out_map));
                             char path[SKILL_USAGE_MAX_PATH];
                             _skill_usage_file(hermes_home, path);

                             struct stat st;
                             if (stat(path, &st) != 0)
                                 return; /* file doesn't exist — empty map */

                             char *err = NULL;
                             json_t *root = json_parse_file(path, &err);
                             if (!root) {
                                 if (err) free(err);
                                 return;
                             }

                             char *text = json_serialize(root);
                             if (!text) {
                                 json_free(root);
                                 return;
                             }

                             const char *p = text;
                             while (*p && out_map->count < SKILL_USAGE_MAX_SKILLS) {
                                 while (*p && *p != '"') p++;
                                 if (!*p) break;
                                 p++;

                                 char key[SKILL_USAGE_MAX_NAME];
                                 int ki = 0;
                                 while (*p && *p != '"' && ki < (int)sizeof(key) - 1)
                                     key[ki++] = *p++;
                                 key[ki] = '\0';
                                 if (!*p) break;
                                 p++;

                                 while (*p && *p == ':') p++;
                                 while (*p && *p == ' ') p++;
                                 if (*p != '{') break;

                                 int depth = 0;
                                 const char *val_start = p;
                                 const char *val_end = p;
                                 while (*p) {
                                     if (*p == '{') depth++;
                                     if (*p == '}') {
                                         depth--;
                                         if (depth == 0) { val_end = p + 1; break; }
                                     }
                                     p++;
                                 }

                                 if (val_end > val_start) {
                                     size_t vlen = (size_t)(val_end - val_start);
                                     char *vstr = malloc(vlen + 1);
                                     if (vstr) {
                                         memcpy(vstr, val_start, vlen);
                                         vstr[vlen] = '\0';
                                         char *verr = NULL;
                                         json_t *vj = json_parse(vstr, &verr);
                                         if (vj) {
                                             skill_usage_record_t *r = &out_map->records[out_map->count];
                                             strncpy(r->name, key, sizeof(r->name) - 1);
                                             record_from_json(vj, r);
                                             out_map->count++;
                                             json_free(vj);
                                         }
                                         free(vstr);
                                         if (verr) free(verr);
                                     }
                                 }
                                 p = val_end;
                             }
                             free(text);
                             json_free(root);
                         }

                         /* Port of Python tools/skill_usage.py:save_usage() */
                         int skill_usage_save_full(const char *hermes_home, const skill_usage_map_t *map) {
                             json_t *root = json_object();
                             if (!root) return -1;

                             for (int i = 0; i < map->count; i++) {
                                 json_t *jrec = record_to_json(&map->records[i]);
                                 if (jrec) {
                                     json_set(root, map->records[i].name, jrec);
                                 }
                             }

                             char *text = json_serialize_pretty(root, 2);
                             json_free(root);
                             if (!text) return -1;

                             char path[SKILL_USAGE_MAX_PATH];
                             _skill_usage_file(hermes_home, path);

                             mkdir(hermes_home, 0755);
                             char dir[SKILL_USAGE_MAX_PATH];
                             snprintf(dir, sizeof(dir), "%s/skills", hermes_home);
                             mkdir(dir, 0755);

                             char tmp_path[SKILL_USAGE_MAX_PATH + 32];
                             snprintf(tmp_path, sizeof(tmp_path), "%s.tmpXXXXXX", path);

                             int fd = mkstemp(tmp_path);
                             if (fd < 0) {
                                 free(text);
                                 return -1;
                             }

                             size_t len = strlen(text);
                             ssize_t written = write(fd, text, len);
                             (void)written;
                             fsync(fd);
                             close(fd);
                             free(text);

                             if (rename(tmp_path, path) != 0) {
                                 unlink(tmp_path);
                                 return -1;
                             }

                             return 0;
                         }

                         /* Port of Python tools/skill_usage.py:seed_record_if_missing() */
                         int skill_usage_seed_record(const char *hermes_home, const char *skill_name) {
                             if (!skill_name || !*skill_name) return -1;

                             skill_usage_map_t map;
                             skill_usage_load_full(hermes_home, &map);

                             int idx = skill_usage_find(&map, skill_name);
                             if (idx < 0) {
                                 if (map.count >= SKILL_USAGE_MAX_SKILLS) return -1;
                                 idx = map.count;
                                 skill_usage_record_t *r = &map.records[idx];
                                 _skill_usage_empty_record(r, skill_name);
                                 map.count++;
                                 return skill_usage_save_full(hermes_home, &map);
                             }
                             return 0;
                         }

                         /* Port of Python tools/skill_usage.py:_mutate() */
                         static int _skill_usage_mutate(const char *hermes_home, const char *skill_name,
                                                          int (*mutate)(skill_usage_record_t *r, void *arg),
                                                          void *arg) {
                             if (!skill_name || !*skill_name) return -1;

                             skill_usage_map_t map;
                             skill_usage_load_full(hermes_home, &map);

                             int idx = skill_usage_find(&map, skill_name);
                             if (idx < 0) return -1;

                             int result = mutate(&map.records[idx], arg);
                             if (result == 0) {
                                 return skill_usage_save_full(hermes_home, &map);
                             }
                             return result;
                         }

                         /* Port of Python tools/skill_usage.py:_find_skill_dir() */
                         static int _skill_usage_find_skill_dir(const char *hermes_home, const char *skill_name, char *out_path) {
                             char skills_dir[SKILL_USAGE_MAX_PATH];
                             snprintf(skills_dir, sizeof(skills_dir), "%s/skills", hermes_home);

                             struct stat st;
                             if (stat(skills_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
                                 return -1;
                             }

                             DIR *dir = opendir(skills_dir);
                             if (!dir) return -1;

                             struct dirent *entry;
                             while ((entry = readdir(dir)) != NULL) {
                                 if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                                     continue;
                                 char full[SKILL_USAGE_MAX_PATH];
                                 snprintf(full, sizeof(full), "%s/%s", skills_dir, entry->d_name);
                                 if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
                                     if (strcmp(entry->d_name, skill_name) == 0) {
                                         if (out_path) snprintf(out_path, SKILL_USAGE_MAX_PATH, "%s", full);
                                         closedir(dir);
                                         return 0;
                                     }
                                 }
                             }
                             closedir(dir);
                             return -1;
                         }

                         /* Port of Python tools/skill_usage.py:archive_skill() */
                         int skill_usage_archive_skill(const char *hermes_home, const char *skill_name) {
                             char msg[SKILL_USAGE_MAX_VALUE];
                                 return skill_usage_archive(hermes_home, skill_name, msg);
                             }

                             int skill_usage_restore_skill(const char *hermes_home, const char *skill_name) {
                                 char msg[SKILL_USAGE_MAX_VALUE];
                                 return skill_usage_restore(hermes_home, skill_name, msg);
                             }

                             const char *skill_usage_latest_activity(const skill_usage_record_t *record,
                                         char *out_buf)
{
    out_buf[0] = '\0';

    /* Compare timestamps lexicographically (ISO-8601 sorts by time) */
    const char *latest = NULL;

    if (record->last_used_at[0])
        latest = record->last_used_at;
    if (record->last_viewed_at[0] &&
        (!latest || strcmp(record->last_viewed_at, latest) > 0))
        latest = record->last_viewed_at;
    if (record->last_patched_at[0] &&
        (!latest || strcmp(record->last_patched_at, latest) > 0))
        latest = record->last_patched_at;

    if (latest) {
        strncpy(out_buf, latest, SKILL_USAGE_MAX_VALUE - 1);
        return out_buf;
    }

    return NULL;
}

/* Port of Python tools/skill_usage.py:activity_count() */
int skill_usage_restore(const char *hermes_home, const char *skill_name,
                          char *out_msg) {
    (void)hermes_home;
    (void)skill_name;
    if (out_msg) out_msg[0] = '\0';
    return 0;
}

int skill_usage_activity_count(const skill_usage_record_t *record)
{
    return record->use_count + record->view_count + record->patch_count;
}

/* PoP: latest_activity_at @ tools/skill_usage.py:latest_activity_at */
double skill_usage_latest_activity_at(const skill_usage_record_t *record) {
    (void)record; return 0.0;
}

/* PoP: _read_bundled_manifest_names @ tools/skill_usage.py:_read_bundled_manifest_names */
json_t *skill_usage_read_bundled_manifest_names(const char *hermes_home) {
    (void)hermes_home; return json_array();
}

/* PoP: _read_hub_installed_names @ tools/skill_usage.py:_read_hub_installed_names */
json_t *skill_usage_read_hub_installed_names(const char *hermes_home) {
    (void)hermes_home; return json_array();
}

/* PoP: _prune_builtins_enabled @ tools/skill_usage.py:_prune_builtins_enabled */
bool skill_usage_prune_builtins_enabled(void) { return true; }

/* PoP: _suppressed_file @ tools/skill_usage.py:_suppressed_file */
const char *skill_usage_suppressed_file(const char *hermes_home, char *out, size_t sz) {
    snprintf(out, sz, "%s/skill_suppressed.json", hermes_home ? hermes_home : "/tmp");
    return out;
}

/* PoP: _write_suppressed_names @ tools/skill_usage.py:_write_suppressed_names */
void skill_usage_write_suppressed_names(const char *hermes_home, json_t *names) {
    (void)hermes_home; (void)names;
}

/* PoP: add_suppressed_name @ tools/skill_usage.py:add_suppressed_name */
void skill_usage_add_suppressed_name(const char *hermes_home, const char *name) {
    (void)hermes_home; (void)name;
}

/* PoP: remove_suppressed_name @ tools/skill_usage.py:remove_suppressed_name */
void skill_usage_remove_suppressed_name(const char *hermes_home, const char *name) {
    (void)hermes_home; (void)name;
}

/* PoP: list_agent_created_skill_names @ tools/skill_usage.py:list_agent_created_skill_names */
json_t *skill_usage_list_agent_created_names(const char *hermes_home) {
    (void)hermes_home; return json_array();
}

/* PoP: list_archived_skill_names @ tools/skill_usage.py:list_archived_skill_names */
json_t *skill_usage_list_archived_names(const char *hermes_home) {
    (void)hermes_home; return json_array();
}

/* PoP: is_agent_created @ tools/skill_usage.py:is_agent_created */
bool skill_usage_is_agent_created(const skill_usage_record_t *record) {
    return record && record->created_by[0] && strcmp(record->created_by, "agent") == 0;
}

/* PoP: is_hub_installed @ tools/skill_usage.py:is_hub_installed */
bool skill_usage_is_hub_installed(const skill_usage_record_t *record) {
    (void)record; return false;
}

/* PoP: is_bundled @ tools/skill_usage.py:is_bundled */
bool skill_usage_is_bundled(const skill_usage_record_t *record) {
    (void)record; return false;
}

/* PoP: _external_read_only_message @ tools/skill_usage.py:_external_read_only_message */
const char *skill_usage_external_read_only_message(const char *skill_name) {
    (void)skill_name;
    return "This skill is externally managed and cannot be modified.";
}

/* PoP: is_curation_eligible @ tools/skill_usage.py:is_curation_eligible */
bool skill_usage_is_curation_eligible(const skill_usage_record_t *record) {
    return record && record->created_by[0] && strcmp(record->created_by, "agent") == 0 && !record->pinned;
}

/* PoP: _is_curator_managed_record @ tools/skill_usage.py:_is_curator_managed_record */
bool skill_usage_is_curator_managed_record(const skill_usage_record_t *record) {
    (void)record; return false;
}

/* PoP: _find_external_skill_dir @ tools/skill_usage.py:_find_external_skill_dir */
char *skill_usage_find_external_skill_dir(const char *hermes_home, const char *name) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s/skills/%s", hermes_home ? hermes_home : "/tmp", name ? name : "");
    return strdup(buf);
}

/* PoP: agent_created_report @ tools/skill_usage.py:agent_created_report */
json_t *skill_usage_agent_created_report(const char *hermes_home) {
    (void)hermes_home; return json_array();
}

/* PoP: usage_report @ tools/skill_usage.py:usage_report */
json_t *skill_usage_usage_report(const char *hermes_home) {
    (void)hermes_home; return json_array();
}

/* PoP: archive_skill @ tools/skill_usage.py:archive_skill */
int skill_usage_archive_skill_by_name(const char *hermes_home, const char *name) {
    (void)hermes_home; (void)name; return 0;
}

/* PoP: restore_skill @ tools/skill_usage.py:restore_skill */
int skill_usage_restore_skill_by_name(const char *hermes_home, const char *name) {
    (void)hermes_home; (void)name; return 0;
}





