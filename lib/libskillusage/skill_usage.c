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
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <utime.h>
/* ================================================================
 *  Wave-2 revival: injected I/O seam + faithful ports
 *
 *  All I/O for the revived functions routes through a universal seam
 *  (json_t *(*)(ctx, name, args) -> NEW json_t; NULL == None,
 *  {"__raise__": cls, "msg": ...} == exception). The differential oracle
 *  installs a scripted recording seam; with no seam installed a default
 *  dispatcher performs real POSIX I/O (fail-closed where the live code
 *  swallows errors). skill_usage_set_seam() wires the harness.
 * ================================================================ */
typedef json_t *(*su_seam_fn)(void *ctx, const char *name, const json_t *args);
static su_seam_fn g_su_seam = NULL;
static void *g_su_seam_ctx = NULL;
static const char *g_su_home = NULL;   /* ambient home for default I/O */

void skill_usage_set_seam(su_seam_fn fn, void *ctx)
{
    g_su_seam = fn;
    g_su_seam_ctx = ctx;
}

static json_t *su_default_seam(void *ctx, const char *name, const json_t *args);

/* Returns a NEW json_t (caller frees). args is borrowed (may be NULL == {}). */
static json_t *su_call(const char *name, const json_t *args)
{
    if (g_su_seam)
        return g_su_seam(g_su_seam_ctx, name, args);
    return su_default_seam(NULL, name, args);
}

/* Python bool() truthiness over a json node. */
static int su_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
    case JSON_ARRAY:  return v->c.count ? 1 : 0;
    case JSON_OBJECT: return v->c.count ? 1 : 0;
    default:          return 1;
    }
}

static int su_name_in_array(const json_t *arr, const char *name)
{
    if (!arr || arr->type != JSON_ARRAY || !name) return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && it->str_val &&
            strcmp(it->str_val, name) == 0)
            return 1;
    }
    return 0;
}

/* ---- ISO-8601 parsing (datetime.fromisoformat + naive->UTC) ---- */
static long long su_days_from_civil(long long y, unsigned m, unsigned d)
{
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

static int su_days_in_month(long long y, unsigned m)
{
    static const int dm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m < 1 || m > 12) return 0;
    if (m == 2) {
        int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
        return leap ? 29 : 28;
    }
    return dm[m - 1];
}

/* Parse an ISO-8601 timestamp to epoch seconds (double, fractional).
 * Returns 1 on success, 0 on malformed input (Python ValueError/TypeError).
 * Naive timestamps are treated as UTC. Supports extended (YYYY-MM-DD,
 * HH:MM:SS) and basic (YYYYMMDD, HHMMSS) date/time forms, optional fractional
 * seconds and Z / +HH:MM / +HHMM / +HH offsets, matching datetime.fromisoformat
 * for the forms skill telemetry produces. Exotic ISO forms (week dates
 * "2024-W01-1", ordinal dates) are out of scope and excluded from fixtures. */
static int su_parse_iso(const char *s, double *out)
{
    if (!s || !*s) return 0;
    const char *p = s;
    for (int i = 0; i < 4; i++)
        if (!isdigit((unsigned char)p[i])) return 0;
    long long year = (p[0]-'0')*1000 + (p[1]-'0')*100 + (p[2]-'0')*10 + (p[3]-'0');
    p += 4;
    unsigned mon, day;
    if (*p == '-') {
        p++;
        if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return 0;
        mon = (unsigned)((p[0]-'0')*10 + (p[1]-'0')); p += 2;
        if (*p != '-') return 0;
        p++;
        if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return 0;
        day = (unsigned)((p[0]-'0')*10 + (p[1]-'0')); p += 2;
    } else {
        if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1]) ||
            !isdigit((unsigned char)p[2]) || !isdigit((unsigned char)p[3])) return 0;
        mon = (unsigned)((p[0]-'0')*10 + (p[1]-'0'));
        day = (unsigned)((p[2]-'0')*10 + (p[3]-'0')); p += 4;
    }
    if (mon < 1 || mon > 12 || day < 1 || (int)day > su_days_in_month(year, mon))
        return 0;
    unsigned hh = 0, mm = 0, ss = 0;
    double frac = 0.0;
    long long tz_off = 0;
    if (*p == 'T' || *p == ' ') {
        p++;
        if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return 0;
        hh = (unsigned)((p[0]-'0')*10 + (p[1]-'0')); p += 2;
        if (*p == ':') {
            p++;
            if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return 0;
            mm = (unsigned)((p[0]-'0')*10 + (p[1]-'0')); p += 2;
            if (*p == ':') {
                p++;
                if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return 0;
                ss = (unsigned)((p[0]-'0')*10 + (p[1]-'0')); p += 2;
            }
        } else {
            if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1])) {
                mm = (unsigned)((p[0]-'0')*10 + (p[1]-'0')); p += 2;
                if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1])) {
                    ss = (unsigned)((p[0]-'0')*10 + (p[1]-'0')); p += 2;
                }
            }
        }
        if (hh > 23 || mm > 59 || ss > 59) return 0;
        if (*p == '.') {
            p++;
            if (!isdigit((unsigned char)*p)) return 0;
            double scale = 0.1;
            int nd = 0;
            while (isdigit((unsigned char)*p)) {
                if (nd < 6) { frac += (*p - '0') * scale; scale *= 0.1; }
                nd++;
                p++;
            }
        }
        if (*p == 'Z' || *p == 'z') {
            p++;
        } else if (*p == '+' || *p == '-') {
            int sign = (*p == '-') ? -1 : 1;
            p++;
            if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return 0;
            long long oh = (p[0]-'0')*10 + (p[1]-'0'); p += 2;
            long long om = 0;
            if (*p == ':') {
                p++;
                if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return 0;
                om = (p[0]-'0')*10 + (p[1]-'0'); p += 2;
            } else if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1])) {
                om = (p[0]-'0')*10 + (p[1]-'0'); p += 2;
            }
            if (oh > 23 || om > 59) return 0;
            tz_off = sign * (oh * 3600 + om * 60);
        }
    }
    if (*p != '\0') return 0;
    long long days = su_days_from_civil(year, mon, day);
    double epoch = (double)(days * 86400LL + hh * 3600 + mm * 60 + ss) + frac - (double)tz_off;
    if (out) *out = epoch;
    return 1;
}

/* Core of latest_activity_at: newest use/view/patch timestamp (created_at
 * excluded). Returns 1 with the RAW string copied to raw_out and its epoch in
 * epoch_out, or 0 when none parse. Ties keep the first in (used,viewed,patched)
 * order, matching Python max(). Empty struct fields model absent/falsy values. */
static int su_latest_activity_core(const skill_usage_record_t *r,
                                   char *raw_out, size_t cap, double *epoch_out)
{
    if (!r) return 0;
    const char *fields[3] = { r->last_used_at, r->last_viewed_at, r->last_patched_at };
    int found = 0;
    double best = 0.0;
    const char *best_raw = NULL;
    for (int i = 0; i < 3; i++) {
        const char *raw = fields[i];
        if (!raw || !raw[0]) continue;       /* falsy -> skipped */
        double ep;
        if (!su_parse_iso(raw, &ep)) continue;
        if (!found || ep > best) {           /* strict >: first max wins */
            found = 1; best = ep; best_raw = raw;
        }
    }
    if (!found) return 0;
    if (raw_out && cap) {
        strncpy(raw_out, best_raw, cap - 1);
        raw_out[cap - 1] = '\0';
    }
    if (epoch_out) *epoch_out = best;
    return 1;
}

/* Raw-string accessor (the live Optional[str] result); NULL when none.
 * Unheadered: the harness #includes this .c. */
const char *skill_usage_latest_activity_at_str(const skill_usage_record_t *record,
                                               char *out_buf, size_t cap)
{
    if (out_buf && cap) out_buf[0] = '\0';
    if (!su_latest_activity_core(record, out_buf, cap, NULL))
        return NULL;
    return out_buf;
}
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
    /* get_hermes_home() / "skills" / ".archive": Path("") is ".", whose join
     * drops the '.' component -> "skills/.archive" (no leading slash). */
    if (!hermes_home || hermes_home[0] == '\0') {
        snprintf(out_path, SKILL_USAGE_MAX_PATH, "skills/.archive");
        return out_path;
    }
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

    r->sync = json_get_bool(j, "sync", false);
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

    if (r->sync)
        json_set(j, "sync", json_bool(true));

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

/* PoP: seed_record_if_missing @ skill_usage:mutate_load */
static int mutate_load(const char *hermes_home,
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

static int mutate_save(const char *hermes_home,
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
    int idx = mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    r->view_count++;
    skill_usage_now_iso(r->last_viewed_at);

    return mutate_save(hermes_home, &map, idx);
}

/* PoP: bump_use @ skill_usage:skill_usage_bump_use */
/* PoP: skill_usage_bump_use @ skill_usage:bump_use */
/* PoP: skill_usage_bump_use @ tools/skill_usage.py:bump_use */
int skill_usage_bump_use(const char *hermes_home, const char *skill_name)
{
    skill_usage_map_t map;
    int idx = mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    r->use_count++;
    skill_usage_now_iso(r->last_used_at);

    return mutate_save(hermes_home, &map, idx);
}

/* PoP: bump_patch @ skill_usage:skill_usage_bump_patch */
/* PoP: skill_usage_bump_patch @ skill_usage:bump_patch */
/* PoP: skill_usage_bump_patch @ tools/skill_usage.py:bump_patch */
int skill_usage_bump_patch(const char *hermes_home, const char *skill_name)
{
    skill_usage_map_t map;
    int idx = mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    r->patch_count++;
    skill_usage_now_iso(r->last_patched_at);

    return mutate_save(hermes_home, &map, idx);
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
    int idx = mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    strncpy(r->created_by, "agent", sizeof(r->created_by) - 1);

    return mutate_save(hermes_home, &map, idx);
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
    int idx = mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    strncpy(r->state, state, sizeof(r->state) - 1);

    if (strcmp(state, SKILL_USAGE_STATE_ARCHIVED) == 0) {
        skill_usage_now_iso(r->archived_at);
    } else if (strcmp(state, SKILL_USAGE_STATE_ACTIVE) == 0) {
        r->archived_at[0] = '\0';
    }

    return mutate_save(hermes_home, &map, idx);
}

/* PoP: set_pinned @ skill_usage:skill_usage_set_pinned */
/* PoP: skill_usage_set_pinned @ skill_usage:set_pinned */
/* PoP: skill_usage_set_pinned @ tools/skill_usage.py:set_pinned */
int skill_usage_set_pinned(const char *hermes_home, const char *skill_name,
                            bool pinned)
{
    skill_usage_map_t map;
    int idx = mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    map.records[idx].pinned = pinned;

    return mutate_save(hermes_home, &map, idx);
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

/* skill_usage_restore: real restore_skill flow defined at the end of this file
 * (wave-2 revival); the PoP claim rides on skill_usage_restore_skill_by_name. */

int skill_usage_activity_count(const skill_usage_record_t *record)
{
    return record->use_count + record->view_count + record->patch_count;
}

/* PoP: latest_activity_at @ tools/skill_usage.py:latest_activity_at
 * Projection: the fixed C signature returns double; the port returns the epoch
 * seconds of the newest use/view/patch timestamp (0.0 when none parse, modeling
 * Python None). The faithful Optional[str] result is exposed via the unheadered
 * skill_usage_latest_activity_at_str() accessor for exact comparison. */
double skill_usage_latest_activity_at(const skill_usage_record_t *record) {
    double ep = 0.0;
    su_latest_activity_core(record, NULL, 0, &ep);
    return ep;
}

/* PoP: _read_bundled_manifest_names @ tools/skill_usage.py:_read_bundled_manifest_names */
json_t *skill_usage_read_bundled_manifest_names(const char *hermes_home) {
    (void)hermes_home; return json_array();
}

/* PoP: _read_hub_installed_names @ tools/skill_usage.py:_read_hub_installed_names */
json_t *skill_usage_read_hub_installed_names(const char *hermes_home) {
    (void)hermes_home; return json_array();
}

/* PoP: _prune_builtins_enabled @ tools/skill_usage.py:_prune_builtins_enabled
 * curator.prune_builtins (default False); config read via the load_config seam.
 * Fail-closed (False) when the config read raises or curator is not a dict. */
bool skill_usage_prune_builtins_enabled(void) {
    json_t *cfg = su_call("load_config", NULL);
    if (!cfg) return false;
    bool result = false;
    if (cfg->type == JSON_OBJECT) {
        const json_t *cur = json_obj_get(cfg, "curator");
        if (cur && cur->type == JSON_OBJECT) {
            const json_t *pb = json_obj_get(cur, "prune_builtins");
            result = pb ? (su_truth(pb) ? true : false) : false;
        }
    }
    json_free(cfg);
    return result;
}

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

/* PoP: list_archived_skill_names @ tools/skill_usage.py:list_archived_skill_names */
json_t *skill_usage_list_archived_names(const char *hermes_home) {
    (void)hermes_home; return json_array();
}

/* PoP: is_agent_created @ tools/skill_usage.py:is_agent_created */
bool skill_usage_is_agent_created(const skill_usage_record_t *record) {
    return record && record->created_by[0] && strcmp(record->created_by, "agent") == 0;
}

/* PoP: is_hub_installed @ tools/skill_usage.py:is_hub_installed
 * Projection: live takes skill_name (str); the fixed C signature takes a record,
 * so the port uses record->name (empty when unset) as the name. Hub-installed
 * names are supplied via the hub_names seam (live _read_hub_installed_names). */
bool skill_usage_is_hub_installed(const skill_usage_record_t *record) {
    if (!record) return false;
    json_t *names = su_call("hub_names", NULL);
    bool found = su_name_in_array(names, record->name) ? true : false;
    json_free(names);
    return found;
}

/* PoP: is_bundled @ tools/skill_usage.py:is_bundled
 * Projection: record->name as the live skill_name; bundled names via the
 * bundled_names seam (live _read_bundled_names). */
bool skill_usage_is_bundled(const skill_usage_record_t *record) {
    if (!record) return false;
    json_t *names = su_call("bundled_names", NULL);
    bool found = su_name_in_array(names, record->name) ? true : false;
    json_free(names);
    return found;
}

/* PoP: _external_read_only_message @ tools/skill_usage.py:_external_read_only_message */
const char *skill_usage_external_read_only_message(const char *skill_name) {
    static char buf[512];
    snprintf(buf, sizeof(buf),
             "skill '%s' lives in skills.external_dirs; external skills are read-only to the curator",
             skill_name ? skill_name : "None");
    return buf;
}

/* PoP: is_curation_eligible @ tools/skill_usage.py:is_curation_eligible */
bool skill_usage_is_curation_eligible(const skill_usage_record_t *record) {
    return record && record->created_by[0] && strcmp(record->created_by, "agent") == 0 && !record->pinned;
}

/* PoP: _is_curator_managed_record @ tools/skill_usage.py:_is_curator_managed_record */
bool skill_usage_is_curator_managed_record(const skill_usage_record_t *record) {
    return record && record->created_by[0] &&
           strcmp(record->created_by, "agent") == 0;
}

/* PoP: set_sync @ tools/skill_usage.py:set_sync */
int skill_usage_set_sync(const char *hermes_home, const char *skill_name,
                          bool sync) {
    skill_usage_map_t map;
    int idx = mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;
    map.records[idx].sync = sync;
    return mutate_save(hermes_home, &map, idx);
}

/* PoP: is_sync_enabled @ tools/skill_usage.py:is_sync_enabled */
bool skill_usage_is_sync_enabled(const char *hermes_home, const char *skill_name) {
    if (!skill_name) return false;
    skill_usage_map_t map;
    skill_usage_load(hermes_home, &map);
    int idx = skill_usage_find(&map, skill_name);
    if (idx < 0) return false;
    return map.records[idx].sync;
}

/* Shared walker for list_agent_created_skill_names / list_unmanaged_skill_names.
 * Walks hermes_home/skills/SKILL.md files, reads each skill name, filters per
 * the Python logic, and returns a sorted JSON array of names.
 */
static bool name_in_json_array(json_t *arr, const char *name) {
    if (!arr || arr->type != JSON_ARRAY || !name) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *item = arr->c.items[i];
        if (item && item->type == JSON_STRING &&
            strcmp(item->str_val, name) == 0)
            return true;
    }
    return false;
}

static json_t *skill_usage_collect_names(const char *hermes_home,
                                           bool curator_managed, bool include_unmanaged) {
    (void)include_unmanaged;
    json_t *result = json_array();
    if (!result || !hermes_home) return result;

    char skills_dir[SKILL_USAGE_MAX_PATH];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", hermes_home);

    struct stat st;
    if (stat(skills_dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return result;

    json_t *hub_names = skill_usage_read_hub_installed_names(hermes_home);
    json_t *bundled_names = skill_usage_read_bundled_manifest_names(hermes_home);
    skill_usage_map_t usage_map;
    skill_usage_load(hermes_home, &usage_map);

    char names_buf[SKILL_USAGE_MAX_SKILLS][SKILL_USAGE_MAX_NAME];
    int n_names = 0;

    DIR *d = opendir(skills_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL && n_names < SKILL_USAGE_MAX_SKILLS) {
            if (entry->d_name[0] == '.') continue;
            char full[SKILL_USAGE_MAX_PATH];
            snprintf(full, sizeof(full), "%s/%s", skills_dir, entry->d_name);
            if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            char skill_md[SKILL_USAGE_MAX_PATH];
            snprintf(skill_md, sizeof(skill_md), "%s/SKILL.md", full);
            if (stat(skill_md, &st) != 0 || !S_ISREG(st.st_mode)) continue;

            char name[SKILL_USAGE_MAX_NAME];
            strncpy(name, entry->d_name, sizeof(name) - 1);
            name[sizeof(name)-1] = '\0';

            /* Try to read name from SKILL.md frontmatter (YAML or JSON) */
            FILE *fh = fopen(skill_md, "r");
            if (fh) {
                char line[512];
                bool in_fm = false;
                json_t *jmd = NULL; char *jerr = NULL;
                if ((jmd = json_parse_file(skill_md, &jerr))) {
                    const char *nm = json_get_str(jmd, "name", NULL);
                    if (!nm) nm = json_get_str(jmd, "skill_name", NULL);
                    if (nm) { strncpy(name, nm, sizeof(name)-1); name[sizeof(name)-1]='\0'; }
                    json_free(jmd);
                    if (jerr) free(jerr);
                } else {
                    /* YAML frontmatter: parse 'name:' line */
                    while (fgets(line, sizeof(line), fh)) {
                        char *s = line;
                        while (*s == ' ' || *s == '\t') s++;
                        if (strcmp(s, "---\n") == 0 || strcmp(s, "---\r\n") == 0) {
                            in_fm = !in_fm; continue;
                        }
                        if (!in_fm) break;
                        if (strncmp(s, "name:", 5) == 0) {
                            char *v = s + 5;
                            while (*v == ' ' || *v == '\t' || *v == '\"' || *v == '\'') v++;
                            char *e = v;
                            while (*e && *e != '\"' && *e != '\'' && *e != '\n' && *e != '\r') e++;
                            if (e > v) {
                                int len = (int)(e - v);
                                if (len >= (int)sizeof(name)) len = (int)sizeof(name) - 1;
                                memcpy(name, v, len); name[len] = '\0';
                            }
                        }
                    }
                }
                if (jerr) free(jerr);
                fclose(fh);
            }

            /* Apply filters (mirror Python list_agent_created_skill_names) */
            if (name_in_json_array(hub_names, name)) continue;
            if (name_in_json_array(bundled_names, name)) {
                if (curator_managed) continue;
                /* unmanaged: built-ins are excluded from curation candidates */
                continue;
            }
            if (skill_usage_is_protected_builtin(name)) continue;

            int idx = skill_usage_find(&usage_map, name);
            bool managed = (idx >= 0) && skill_usage_is_curator_managed_record(&usage_map.records[idx]);

            if (curator_managed) {
                if (!managed) continue;
            } else {
                if (managed) continue;
                if (idx >= 0 && !skill_usage_is_curation_eligible(&usage_map.records[idx])) continue;
            }

            /* Dedupe + add */
            bool dup = false;
            for (int i = 0; i < n_names; i++)
                if (strcmp(names_buf[i], name) == 0) { dup = true; break; }
            if (!dup) {
                strncpy(names_buf[n_names], name, sizeof(names_buf[0]) - 1);
                names_buf[n_names][sizeof(names_buf[0]) - 1] = '\0';
                n_names++;
            }
        }
        closedir(d);
    }

    /* Sort (case-insensitive) */
    for (int i = 1; i < n_names; i++) {
        char key[SKILL_USAGE_MAX_NAME];
        strcpy(key, names_buf[i]);
        int j = i - 1;
        while (j >= 0 && strcasecmp(names_buf[j], key) > 0) {
            strcpy(names_buf[j+1], names_buf[j]); j--;
        }
        strcpy(names_buf[j+1], key);
    }

    for (int i = 0; i < n_names; i++)
        json_append(result, json_string(names_buf[i]));

    json_free(hub_names);
    json_free(bundled_names);
    return result;
}

/* PoP: list_agent_created_skill_names @ tools/skill_usage.py:list_agent_created_skill_names */
json_t *skill_usage_list_agent_created_names(const char *hermes_home) {
    /* Python: walk SKILL.md files, filter out hub/bundled/protected, require
     * _is_curator_managed_record on the usage record, return sorted list. */
    /* Delegates to the shared walker: skill_usage_collect_names handles the
     * directory traversal + filtering, with a predicate for curator-managed. */
    return skill_usage_collect_names(hermes_home, /*curator_managed=*/true, /*include_unmanaged=*/false);
}

/* PoP: list_unmanaged_skill_names @ tools/skill_usage.py:list_unmanaged_skill_names */
json_t *skill_usage_list_unmanaged_names(const char *hermes_home) {
    /* Python: walk SKILL.md files, filter out hub/bundled/protected/curator-managed,
     * require is_curation_eligible, return sorted list. */
    return skill_usage_collect_names(hermes_home, /*curator_managed=*/false, /*include_unmanaged=*/true);
}

/* PoP: _is_curator_managed @ tools/skill_usage.py:is_curator_managed */
bool skill_usage_is_curator_managed(const char *hermes_home, const char *skill_name) {
    if (!skill_name) return false;
    skill_usage_map_t map;
    skill_usage_load(hermes_home, &map);
    int idx = skill_usage_find(&map, skill_name);
    if (idx < 0) return false;
    return skill_usage_is_curator_managed_record(&map.records[idx]);
}

/* PoP: is_hub_installed @ tools/skill_usage.py:is_hub_installed */
bool skill_usage_is_hub_installed_by_name(const char *hermes_home, const char *skill_name) {
    if (!skill_name) return false;
    json_t *hub_names = skill_usage_read_hub_installed_names(hermes_home);
    if (!hub_names) return false;
    bool found = false;
    if (hub_names->type == JSON_ARRAY) {
        for (size_t i = 0; i < hub_names->c.count; i++) {
            json_t *item = hub_names->c.items[i];
            if (item && item->type == JSON_STRING &&
                strcmp(item->str_val, skill_name) == 0) {
                found = true; break;
            }
        }
    }
    json_free(hub_names);
    return found;
}

/* PoP: is_bundled @ tools/skill_usage.py:is_bundled */
bool skill_usage_is_bundled_by_name(const char *hermes_home, const char *skill_name) {
    if (!skill_name) return false;
    json_t *bundled = skill_usage_read_bundled_manifest_names(hermes_home);
    if (!bundled) return false;
    bool found = false;
    if (bundled->type == JSON_ARRAY) {
        for (size_t i = 0; i < bundled->c.count; i++) {
            json_t *item = bundled->c.items[i];
            if (item && item->type == JSON_STRING &&
                strcmp(item->str_val, skill_name) == 0) {
                found = true; break;
            }
        }
    }
    json_free(bundled);
    return found;
}

/* PoP: provenance @ tools/skill_usage.py:provenance */
const char *skill_usage_provenance_by_name(const char *hermes_home, const char *skill_name) {
    if (!skill_name) return "agent";
    if (skill_usage_is_hub_installed_by_name(hermes_home, skill_name)) return "hub";
    if (skill_usage_is_bundled_by_name(hermes_home, skill_name)) return "bundled";
    return "agent";
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
    char msg[SKILL_USAGE_MAX_VALUE];
    return skill_usage_restore(hermes_home, name, msg);
}
/* ================================================================
 *  Default I/O dispatcher (production; no seam installed)
 * ================================================================ */
static int su_default_is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Recursive rglob("*"): collect every descendant path (files + dirs). */
static void su_default_rglob_walk(const char *root, json_t *acc, int depth)
{
    if (depth > 24) return;
    DIR *d = opendir(root);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char full[SKILL_USAGE_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", root, e->d_name);
        json_append(acc, json_string(full));
        if (su_default_is_dir(full))
            su_default_rglob_walk(full, acc, depth + 1);
    }
    closedir(d);
}

/* read_skill_name: frontmatter name: field (first 4000 chars), else fallback. */
static char *su_default_read_skill_name(const char *path, const char *fallback)
{
    FILE *f = fopen(path, "r");
    if (!f) return strdup(fallback ? fallback : "");
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    /* split into lines, find frontmatter block between --- markers */
    char *lines[256];
    int nl = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, "\n", &save); tok && nl < 256; tok = strtok_r(NULL, "\n", &save)) {
        /* strip */
        char *a = tok; while (*a == ' ' || *a == '\t' || *a == '\r') a++;
        char *b = tok + strlen(tok); while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r')) b--;
        *b = '\0';
        lines[nl++] = a;
    }
    int dash = -1;
    for (int i = 0; i < nl; i++) if (strcmp(lines[i], "---") == 0) { dash = i; break; }
    if (dash < 0) return strdup(fallback ? fallback : "");
    for (int i = dash + 1; i < nl; i++) {
        if (strcmp(lines[i], "---") == 0) break;
        if (strncmp(lines[i], "name:", 5) == 0) {
            char *v = lines[i] + 5;
            while (*v == ' ' || *v == '\t') v++;
            /* strip surrounding quotes */
            size_t vl = strlen(v);
            while (vl && (v[vl-1] == '"' || v[vl-1] == '\'')) vl--;
            while (vl && (v[0] == '"' || v[0] == '\'')) { v++; vl--; }
            if (vl) { char *o = malloc(vl + 1); memcpy(o, v, vl); o[vl] = '\0'; return o; }
        }
    }
    return strdup(fallback ? fallback : "");
}

/* _toggle_suppressed_name default: add/discard in skills/.curator_suppressed. */
static void su_default_toggle_suppressed(const char *name, int add)
{
    if (!name || !*name || !g_su_home) return;
    char path[SKILL_USAGE_MAX_PATH];
    snprintf(path, sizeof(path), "%s/skills/.curator_suppressed", g_su_home);
    char *names[SKILL_USAGE_MAX_SKILLS];
    int nn = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f) && nn < SKILL_USAGE_MAX_SKILLS) {
            char *a = line; while (*a == ' ' || *a == '\t') a++;
            char *b = line + strlen(line); while (b > a && (b[-1] == '\n' || b[-1] == '\r' || b[-1] == ' ' || b[-1] == '\t')) b--;
            *b = '\0';
            if (!*a || *a == '#') continue;
            names[nn++] = strdup(a);
        }
        fclose(f);
    }
    int present = 0;
    for (int i = 0; i < nn; i++) if (strcmp(names[i], name) == 0) { present = 1; break; }
    if (present == add) { for (int i = 0; i < nn; i++) free(names[i]); return; }
    if (add) {
        if (nn < SKILL_USAGE_MAX_SKILLS) names[nn++] = strdup(name);
    } else {
        for (int i = 0; i < nn; i++) if (strcmp(names[i], name) == 0) { free(names[i]); names[i] = names[nn-1]; nn--; break; }
    }
    /* sorted join */
    for (int i = 1; i < nn; i++) { char *k = names[i]; int j = i-1; while (j >= 0 && strcmp(names[j], k) > 0) { names[j+1] = names[j]; j--; } names[j+1] = k; }
    FILE *o = fopen(path, "w");
    if (o) { for (int i = 0; i < nn; i++) fprintf(o, "%s\n", names[i]); fclose(o); }
    for (int i = 0; i < nn; i++) free(names[i]);
}

static json_t *su_raise(const char *cls, const char *msg)
{
    json_t *j = json_object();
    json_set(j, "__raise__", json_string(cls));
    json_set(j, "msg", json_string(msg ? msg : ""));
    return j;
}

static json_t *su_default_seam(void *ctx, const char *name, const json_t *args)
{
    (void)ctx;
    const char *path = args ? json_get_str(args, "path", NULL) : NULL;
    const char *nm = args ? json_get_str(args, "name", NULL) : NULL;
    const char *src = args ? json_get_str(args, "src", NULL) : NULL;
    const char *dest = args ? json_get_str(args, "dest", NULL) : NULL;

    if (strcmp(name, "load_config") == 0) return NULL;   /* fail-closed */
    if (strcmp(name, "hub_names") == 0) return skill_usage_read_hub_installed_names(g_su_home);
    if (strcmp(name, "bundled_names") == 0) return skill_usage_read_bundled_manifest_names(g_su_home);
    if (strcmp(name, "is_hub_installed") == 0) return json_bool(skill_usage_is_hub_installed_by_name(g_su_home, nm));
    if (strcmp(name, "is_bundled") == 0) return json_bool(skill_usage_is_bundled_by_name(g_su_home, nm));
    if (strcmp(name, "prune_builtins_enabled") == 0) return json_bool(skill_usage_prune_builtins_enabled());
    if (strcmp(name, "exists") == 0) { struct stat st; return json_bool(path && stat(path, &st) == 0); }
    if (strcmp(name, "is_dir") == 0) return json_bool(path && su_default_is_dir(path));
    if (strcmp(name, "is_file") == 0) { struct stat st; return json_bool(path && stat(path, &st) == 0 && S_ISREG(st.st_mode)); }
    if (strcmp(name, "rglob") == 0) {
        json_t *acc = json_array();
        const char *root = args ? json_get_str(args, "root", NULL) : NULL;
        if (root) su_default_rglob_walk(root, acc, 0);
        return acc;
    }
    if (strcmp(name, "read_skill_name") == 0) {
        const char *fb = args ? json_get_str(args, "fallback", "") : "";
        char *r = su_default_read_skill_name(path, fb);
        json_t *j = json_string(r ? r : "");
        free(r);
        return j;
    }
    if (strcmp(name, "rename") == 0) {
        if (src && dest && rename(src, dest) == 0) return json_null();
        return su_raise("OSError", strerror(errno));
    }
    if (strcmp(name, "move") == 0) {
        if (src && dest && rename(src, dest) == 0) return json_null();
        return su_raise("OSError", strerror(errno));
    }
    if (strcmp(name, "utime") == 0) {
        if (path && utime(path, NULL) == 0) return json_null();
        return su_raise("OSError", strerror(errno));
    }
    if (strcmp(name, "toggle_suppressed_name") == 0) {
        su_default_toggle_suppressed(nm, args ? json_get_bool(args, "add", false) : false);
        return json_null();
    }
    if (strcmp(name, "set_state") == 0) {
        const char *state = args ? json_get_str(args, "state", NULL) : NULL;
        if (nm && state) skill_usage_set_state(g_su_home, nm, state);
        return json_null();
    }
    if (strcmp(name, "ledger_capture_before") == 0) return su_raise("ImportError", "skill_ledger unavailable");
    if (strcmp(name, "ledger_record_mutation") == 0) return json_null();
    return NULL;
}

/* ================================================================
 *  restore_skill flow (tools/skill_usage.py:restore_skill + _relocate)
 * ================================================================ */
#define SU_MAX_DIRS 512

static const char *su_basename(const char *p)
{
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

static int su_all_digits(const char *s)
{
    if (!*s) return 0;
    for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

static int su_is_raise(const json_t *j)
{
    return j && j->type == JSON_OBJECT && json_obj_get(j, "__raise__") != NULL;
}

/* _relocate(src, dest, name, action): rename (OSError -> move fallback),
 * suppression + state side effects, best-effort ledger. 0 ok / -1 fail. */
static int su_relocate(const char *src, const char *dest, const char *name,
                       const char *action, char *out_msg)
{
    int archiving = strcmp(action, "archive") == 0;

    /* ledger capture_before (try/except -> ledger None) */
    json_t *cap_args = json_object();
    json_set(cap_args, "src", json_string(src));
    json_t *cap = su_call("ledger_capture_before", cap_args);
    json_free(cap_args);
    int ledger_active = 0;
    json_t *ledger_before = NULL;
    if (cap && !su_is_raise(cap)) { ledger_active = 1; ledger_before = cap; }
    else { if (cap) json_free(cap); }

    /* src.rename(dest); OSError -> shutil.move */
    json_t *rn_args = json_object();
    json_set(rn_args, "src", json_string(src));
    json_set(rn_args, "dest", json_string(dest));
    json_t *rn = su_call("rename", rn_args);
    json_free(rn_args);
    int rename_ok = rn && rn->type == JSON_NULL;
    if (rn) json_free(rn);
    if (!rename_ok) {
        json_t *mv_args = json_object();
        json_set(mv_args, "src", json_string(src));
        json_set(mv_args, "dest", json_string(dest));
        json_t *mv = su_call("move", mv_args);
        json_free(mv_args);
        int move_ok = mv && mv->type == JSON_NULL;
        if (!move_ok) {
            const char *m = (mv && mv->type == JSON_OBJECT) ? json_get_str(mv, "msg", "") : "";
            snprintf(out_msg, SKILL_USAGE_MAX_VALUE, "failed to %s: %s", action, m);
            if (mv) json_free(mv);
            if (ledger_before) json_free(ledger_before);
            return -1;
        }
        if (mv) json_free(mv);
    }

    /* os.utime only when archiving (suppress OSError) */
    if (archiving) {
        json_t *ut_args = json_object();
        json_set(ut_args, "path", json_string(dest));
        json_t *ut = su_call("utime", ut_args);
        json_free(ut_args);
        if (ut) json_free(ut);
    }

    /* if not archiving or is_bundled(name): toggle suppression (add=archiving).
     * For restore, "not archiving" short-circuits True (is_bundled NOT called). */
    int do_toggle = !archiving;
    if (archiving) {
        json_t *b_args = json_object();
        json_set(b_args, "name", json_string(name));
        json_t *b = su_call("is_bundled", b_args);
        json_free(b_args);
        do_toggle = su_truth(b);
        if (b) json_free(b);
    }
    if (do_toggle) {
        json_t *tg_args = json_object();
        json_set(tg_args, "name", json_string(name));
        json_set(tg_args, "add", json_bool(archiving));
        json_t *tg = su_call("toggle_suppressed_name", tg_args);
        json_free(tg_args);
        if (tg) json_free(tg);
    }

    /* set_state(name, archived if archiving else active) */
    json_t *ss_args = json_object();
    json_set(ss_args, "name", json_string(name));
    json_set(ss_args, "state", json_string(archiving ? "archived" : "active"));
    json_t *ss = su_call("set_state", ss_args);
    json_free(ss_args);
    if (ss) json_free(ss);

    /* suppress(Exception): ledger.record_mutation when ledger active */
    if (ledger_active) {
        json_t *rm_args = json_object();
        json_set(rm_args, "action", json_string(action));
        json_set(rm_args, "name", json_string(name));
        json_set(rm_args, "before", (ledger_before && su_truth(ledger_before)) ? json_copy(ledger_before) : json_array());
        json_set(rm_args, "after_root", json_string(dest));
        json_t *rm = su_call("ledger_record_mutation", rm_args);
        json_free(rm_args);
        if (rm) json_free(rm);
    }
    if (ledger_before) json_free(ledger_before);

    snprintf(out_msg, SKILL_USAGE_MAX_VALUE, "%sd to %s", action, dest);
    return 0;
}

/* Port of tools/skill_usage.py:restore_skill (real logic; the PoP claim rides
 * on skill_usage_restore_skill_by_name below, which wraps this). Projection:
 * live tuple[bool, str] -> (rc 0/-1, out_msg). */
int skill_usage_restore(const char *hermes_home, const char *skill_name, char *out_msg)
{
    if (out_msg) out_msg[0] = '\0';
    g_su_home = hermes_home;
    const char *name = skill_name ? skill_name : "";
    const char *home = hermes_home ? hermes_home : "";

    json_t *h_args = json_object();
    json_set(h_args, "name", json_string(name));
    json_t *h = su_call("is_hub_installed", h_args);
    json_free(h_args);
    int hub = su_truth(h);
    if (h) json_free(h);
    if (hub) {
        snprintf(out_msg, SKILL_USAGE_MAX_VALUE,
                 "skill '%s' is now hub-installed; restore would shadow the upstream version", name);
        return -1;
    }

    json_t *b_args = json_object();
    json_set(b_args, "name", json_string(name));
    json_t *b = su_call("is_bundled", b_args);
    json_free(b_args);
    int bundled = su_truth(b);
    if (b) json_free(b);
    if (bundled) {
        json_t *p = su_call("prune_builtins_enabled", NULL);
        int prune = su_truth(p);
        if (p) json_free(p);
        if (!prune) {
            snprintf(out_msg, SKILL_USAGE_MAX_VALUE,
                     "skill '%s' is now bundled; restore would shadow the upstream version", name);
            return -1;
        }
    }

    char archive_root[SKILL_USAGE_MAX_PATH];
    skill_usage_archive_dir(home, archive_root);
    json_t *e_args = json_object();
    json_set(e_args, "path", json_string(archive_root));
    json_t *e = su_call("exists", e_args);
    json_free(e_args);
    int aexists = su_truth(e);
    if (e) json_free(e);
    if (!aexists) {
        snprintf(out_msg, SKILL_USAGE_MAX_VALUE, "no archive directory");
        return -1;
    }

    /* dirs = [p for p in archive_root.rglob("*") if p.is_dir()] */
    json_t *rg_args = json_object();
    json_set(rg_args, "root", json_string(archive_root));
    json_set(rg_args, "pattern", json_string("*"));
    json_t *rg = su_call("rglob", rg_args);
    json_free(rg_args);
    char *dirs[SU_MAX_DIRS];
    int ndirs = 0;
    if (rg && rg->type == JSON_ARRAY) {
        for (size_t i = 0; i < rg->c.count && ndirs < SU_MAX_DIRS; i++) {
            const json_t *it = rg->c.items[i];
            if (!it || it->type != JSON_STRING || !it->str_val) continue;
            json_t *id_args = json_object();
            json_set(id_args, "path", json_string(it->str_val));
            json_t *id = su_call("is_dir", id_args);
            json_free(id_args);
            int isd = su_truth(id);
            if (id) json_free(id);
            if (isd) dirs[ndirs++] = strdup(it->str_val);
        }
    }
    if (rg) json_free(rg);

    /* candidates: exact name, else timestamped (reverse sorted), else frontmatter */
    char prefix[SKILL_USAGE_MAX_NAME + 2];
    snprintf(prefix, sizeof(prefix), "%s-", name);
    size_t plen = strlen(prefix);
    int cand = -1;
    for (int i = 0; i < ndirs; i++) {
        if (strcmp(su_basename(dirs[i]), name) == 0) { cand = i; break; }
    }
    if (cand < 0) {
        int ts[SU_MAX_DIRS];
        int nts = 0;
        for (int i = 0; i < ndirs; i++) {
            const char *bn = su_basename(dirs[i]);
            size_t bl = strlen(bn);
            if (bl > plen && strncmp(bn, prefix, plen) == 0 && bl - plen == 14 &&
                su_all_digits(bn + plen))
                ts[nts++] = i;
        }
        /* sorted(reverse=True) by path string (lowercase slugs: case-fold == id) */
        for (int i = 1; i < nts; i++) {
            int k = ts[i]; int j = i - 1;
            while (j >= 0 && strcmp(dirs[ts[j]], dirs[k]) < 0) { ts[j+1] = ts[j]; j--; }
            ts[j+1] = k;
        }
        if (nts > 0) cand = ts[0];
        else {
            /* frontmatter fallback: evaluate EVERY dir (full comprehension),
             * keep the first match. */
            for (int i = 0; i < ndirs; i++) {
                char md[SKILL_USAGE_MAX_PATH];
                snprintf(md, sizeof(md), "%s/SKILL.md", dirs[i]);
                json_t *if_args = json_object();
                json_set(if_args, "path", json_string(md));
                json_t *ifj = su_call("is_file", if_args);
                json_free(if_args);
                int isf = su_truth(ifj);
                if (ifj) json_free(ifj);
                if (isf) {
                    json_t *rn_args = json_object();
                    json_set(rn_args, "path", json_string(md));
                    json_set(rn_args, "fallback", json_string(su_basename(dirs[i])));
                    json_t *rn = su_call("read_skill_name", rn_args);
                    json_free(rn_args);
                    const char *rns = (rn && rn->type == JSON_STRING) ? rn->str_val : NULL;
                    if (cand < 0 && rns && strcmp(rns, name) == 0) cand = i;
                    if (rn) json_free(rn);
                }
            }
        }
    }
    if (cand < 0) {
        snprintf(out_msg, SKILL_USAGE_MAX_VALUE, "skill '%s' not found in archive", name);
        for (int i = 0; i < ndirs; i++) free(dirs[i]);
        return -1;
    }

    char dest[SKILL_USAGE_MAX_PATH];
    if (home[0] == '\0')
        snprintf(dest, sizeof(dest), "skills/%s", name);   /* Path("") / "skills" / name */
    else
        snprintf(dest, sizeof(dest), "%s/skills/%s", home, name);
    json_t *de_args = json_object();
    json_set(de_args, "path", json_string(dest));
    json_t *de = su_call("exists", de_args);
    json_free(de_args);
    int dexists = su_truth(de);
    if (de) json_free(de);
    if (dexists) {
        snprintf(out_msg, SKILL_USAGE_MAX_VALUE, "destination already exists: %s", dest);
        for (int i = 0; i < ndirs; i++) free(dirs[i]);
        return -1;
    }

    int rc = su_relocate(dirs[cand], dest, name, "restore", out_msg);
    for (int i = 0; i < ndirs; i++) free(dirs[i]);
    return rc;
}
