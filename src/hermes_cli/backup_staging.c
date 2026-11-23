/*
 * backup_staging.c — C port of upstream Hermes hermes_cli/backup_staging.py.
 *
 * PoP: w12_backupstag_5f272c_marker @ hermes_cli/backup_staging.py:_marker
 * PoP: w12_backupstag_5f272c_is_backup_name @ hermes_cli/backup_staging.py:_NAME.fullmatch
 * PoP: w12_backupstag_5f272c_is_legacy_name @ hermes_cli/backup_staging.py:_LEGACY_NAME.fullmatch
 * PoP: w12_backupstag_5f272c_stage_entries @ hermes_cli/backup_staging.py:_stage_entries
 * PoP: w12_backupstag_5f272c_is_backup_staging @ hermes_cli/backup_staging.py:is_backup_staging
 *
 * Filesystem facts (lstat plain-ness, lease bytes, directory entries) are
 * injected from the fixture; the C side never touches the real filesystem.
 */
#include "noc_w12_backupstag_5f272c.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_PREFIX ".hermes-backup-stage-"
#define MARK_PREFIX "hermes-backup-stage-v1:"
#define LEASE_NAME  "owner.lock"
#define NAME_HEX_LEN 32

static const char *const PAYLOADS[] = {
    "archive.partial", "snapshot.db", "snapshot.db-journal",
    "snapshot.db-wal", "snapshot.db-shm", LEASE_NAME, NULL
};

static int is_hex_lower(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

/* _NAME.fullmatch: \.hermes-backup-stage-[0-9a-f]{32}\Z */
static int name_matches(const char *s) {
    if (!s) return 0;
    const char *p = strstr(s, NAME_PREFIX);
    if (p != s) return 0;
    p += strlen(NAME_PREFIX);
    for (int i = 0; i < NAME_HEX_LEN; i++, p++)
        if (!*p || !is_hex_lower(*p)) return 0;
    return *p == '\0';
}

static int all_digits(const char *s, size_t n) {
    if (n == 0) return 0;
    for (size_t i = 0; i < n; i++)
        if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

/* _LEGACY_NAME.fullmatch: (?:tmp.+\.db|\..+\.zip\.\d+-\d+\.partial)\Z */
static int legacy_matches(const char *s) {
    if (!s) return 0;
    size_t n = strlen(s);
    /* Alternative 1: tmp .+ \.db   ('.' does not match '\n') */
    if (n > 6 && !strncmp(s, "tmp", 3) && !strcmp(s + n - 3, ".db")) {
        int ok = 1;
        for (size_t i = 3; i + 3 < n; i++)
            if (s[i] == '\n') { ok = 0; break; }
        if (ok) return 1;
    }
    /* Alternative 2: \. .+ \.zip\. \d+ - \d+ \.partial */
    if (n > 2 && s[0] == '.') {
        const char *tail = ".partial";
        size_t tl = strlen(tail);
        if (n > tl && !strcmp(s + n - tl, tail)) {
            const char *scan = s + 1;
            while ((scan = strstr(scan, ".zip.")) != NULL) {
                size_t gap = (size_t)(scan - (s + 1));
                const char *a = scan + 5;
                const char *dash = strchr(a, '-');
                if (gap >= 1 && dash && dash > a) {
                    const char *b = dash + 1;
                    const char *dot = strstr(b, ".partial");
                    if (dot == s + n - tl) {
                        if (all_digits(a, (size_t)(dash - a)) && all_digits(b, (size_t)(dot - b)))
                            return 1;
                    }
                }
                scan++;
            }
        }
    }
    return 0;
}

static int allowed_payload(const char *name) {
    if (!name) return 0;
    for (int i = 0; PAYLOADS[i]; i++)
        if (!strcmp(name, PAYLOADS[i])) return 1;
    return 0;
}

static const char *jstr(const json_t *o, const char *k, const char *d) {
    return (o && o->type == JSON_OBJECT) ? json_get_str(o, k, d) : d;
}
static int jbool(const json_t *o, const char *k, int d) {
    return (o && o->type == JSON_OBJECT) ? (json_get_bool(o, k, d != 0) ? 1 : 0) : d;
}

/* Mirrors _stage_entries' name/allow-list half; plain-ness is injected. */
static int stage_entries_ok(const json_t *entries) {
    if (!entries || entries->type != JSON_ARRAY) return 1;  /* empty iterdir */
    for (size_t i = 0; i < json_len(entries); i++) {
        json_t *e = json_get(entries, i);
        if (!allowed_payload(jstr(e, "name", ""))) return 0;
        if (!jbool(e, "plain", 1)) return 0;
    }
    return 1;
}

struct json_t *w12_backupstag_5f272c_marker(const char *name) {
    const char *nm = name ? name : "";
    size_t cap = strlen(MARK_PREFIX) + strlen(nm) + 4;
    char *buf = (char *)malloc(cap);
    if (!buf) return json_null();
    int len = snprintf(buf, cap, "%s%s\n", MARK_PREFIX, nm);
    if (len < 0 || (size_t)len >= cap) { free(buf); return json_null(); }
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

struct json_t *w12_backupstag_5f272c_is_backup_name(const char *name) {
    return json_bool(name_matches(name) ? true : false);
}

struct json_t *w12_backupstag_5f272c_is_legacy_name(const char *name) {
    return json_bool(legacy_matches(name) ? true : false);
}

struct json_t *w12_backupstag_5f272c_stage_entries(const struct json_t *entries) {
    if (!stage_entries_ok(entries)) return json_null();
    json_t *arr = json_array();
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(entries); i++)
            json_append(arr, json_string(jstr(json_get(entries, i), "name", "")));
    }
    return arr;
}

struct json_t *w12_backupstag_5f272c_is_backup_staging(const struct json_t *spec) {
    char name[256];
    snprintf(name, sizeof(name), "%s", jstr(spec, "name", ""));
    if (!name_matches(name)) return json_bool(false);
    if (!jbool(spec, "stage_plain", 1) || !jbool(spec, "lease_plain", 1)) return json_bool(false);

    /* lease.seek(1); lease.read(128) == _marker(stage) */
    char expected[320];
    int elen = snprintf(expected, sizeof(expected), "%s%s\n", MARK_PREFIX, name);
    if (elen < 0 || (size_t)elen >= sizeof(expected)) return json_bool(false);
    const char *lease = jstr(spec, "lease", "");
    size_t ll = strlen(lease);
    if (ll > 128 || ll != (size_t)elen || strncmp(lease, expected, (size_t)elen) != 0)
        return json_bool(false);

    if (!stage_entries_ok(spec ? json_obj_get(spec, "entries") : NULL)) return json_bool(false);
    return json_bool(true);
}
