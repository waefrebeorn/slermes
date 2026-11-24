/*
 * _backup.c - C11 port of upstream persistence/sqlite/admin/_backup.py
 * (gates w32_backup_1d0d4e_p1, w32_backup_1d0d4e_p2).
 *
 * PoP: w32_backup_1d0d4e_p1_emergency_stamp @ persistence/sqlite/admin/_backup.py:BackupAdmin.preflight_backup
 * PoP: w32_backup_1d0d4e_p1_emergency_paths @ persistence/sqlite/admin/_backup.py:BackupAdmin.preflight_backup
 * PoP: w32_backup_1d0d4e_p1_prune_survivors @ persistence/sqlite/admin/_backup.py:BackupAdmin.preflight_backup
 * PoP: w32_backup_1d0d4e_p1_reclaim_staging @ persistence/sqlite/admin/_backup.py:_reclaim_abandoned_staging
 * PoP: w32_backup_1d0d4e_p1_step_plan @ persistence/sqlite/admin/_backup.py:_stepped_rollback_journal_copy
 * PoP: w32_backup_1d0d4e_p1_hold_budget @ persistence/sqlite/admin/_backup.py:_stepped_rollback_journal_copy
 * PoP: w32_backup_1d0d4e_p2_constants @ persistence/sqlite/admin/_backup.py:<module constants>
 * PoP: w32_backup_1d0d4e_p2_ledger_probe @ persistence/sqlite/admin/_backup.py:_LEDGER_PROBE
 * PoP: w32_backup_1d0d4e_p2_epoch_sql @ persistence/sqlite/admin/_backup.py:_restore_epoch_source
 * PoP: w32_backup_1d0d4e_p2_uses_wal @ persistence/sqlite/admin/_backup.py:_uses_wal
 * PoP: w32_backup_1d0d4e_p2_verify_manifest @ persistence/sqlite/admin/_backup.py:verify_sqlite_integrity
 * PoP: w32_backup_1d0d4e_p2_epoch_decision @ persistence/sqlite/admin/_backup.py:_restore_epoch_source
 *
 * Every sqlite/filesystem/clock seam stays in the caller: the snapshot itself
 * (sqlite3.backup, the staging file, the /proc walk) is fed here as values.
 * What is ported is what those values decide - the emergency snapshot's names
 * and timestamp, which staged backups survive pruning/reclaim, the stepped-copy
 * page budget and its refusal, the exact SQL the restore stages, the header
 * predicate, and the {valid, message, size} manifest of a verification.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "noc_w32_backup_1d0d4e_p1.h"
#include "noc_w32_backup_1d0d4e_p2.h"

/* ------------------------------------------------------------------ */
/* shared helpers                                                      */
/* ------------------------------------------------------------------ */

static int bp_starts(const char *s, const char *pre) {
    size_t n;
    if (!s || !pre) return 0;
    n = strlen(pre);
    return strncmp(s, pre, n) == 0;
}

static int bp_ends(const char *s, const char *suf) {
    size_t a, b;
    if (!s || !suf) return 0;
    a = strlen(s);
    b = strlen(suf);
    return a >= b && strcmp(s + a - b, suf) == 0;
}

static const char *bp_str(const json_t *obj, const char *key, const char *def) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return def;
}

/* Path(home) / name, as the upstream pathlib joins lay them out. */
static void bp_join(char *out, size_t cap, const char *home, const char *name) {
    size_t n;
    if (!home || !home[0]) { snprintf(out, cap, "%s", name); return; }
    n = strlen(home);
    if (home[n - 1] == '/' || home[n - 1] == '\\')
        snprintf(out, cap, "%s%s", home, name);
    else
        snprintf(out, cap, "%s/%s", home, name);
}

/* A directory listing has unique names; glob can never yield a duplicate. */
static int bp_seen(const char *const *arr, size_t n, const char *s) {
    size_t i;
    for (i = 0; i < n; i++)
        if (!strcmp(arr[i], s)) return 1;
    return 0;
}

static int bp_name_cmp_desc(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(y, x);
}

static int bp_str_cmp_asc(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Decimal digits with the thousands separators Python's ``{:,}`` emits. */
static void bp_group(char *out, size_t cap, long long value) {
    char raw[32];
    size_t len, i, o = 0;
    snprintf(raw, sizeof raw, "%lld", value);
    len = strlen(raw);
    for (i = 0; i < len; i++) {
        size_t left = len - i;
        if (i > 0 && left % 3 == 0 && o + 1 < cap) out[o++] = ',';
        if (o + 1 < cap) out[o++] = raw[i];
    }
    out[o < cap ? o : cap - 1] = '\0';
}

/* ---------------------------------------------------------------- */
/* gate w32_backup_1d0d4e_p1 - emergency layout, retention, budget   */
/* ---------------------------------------------------------------- */

/* days -> civil date (Howard Hinnant's civil_from_days). */
static void bp_civil_from_days(long long z, int *year, unsigned *month, unsigned *day) {
    long long era, yoe, y, doy, mp;
    unsigned doe;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (unsigned)(z - era * 146097);
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *day = (unsigned)(doy - (153 * mp + 2) / 5 + 1);
    *month = (unsigned)(mp < 10 ? mp + 3 : mp - 9);
    *year = (int)(y + (*month <= 2));
}

/* datetime.now(UTC).strftime("%Y-%m-%dT%H-%M-%S-%fZ") for a UTC epoch. */
json_t *w32_backup_1d0d4e_p1_emergency_stamp(const json_t *req) {
    double epoch = json_get_num(req, "epoch", 0.0);
    long long secs = (long long)floor(epoch);
    double frac = epoch - (double)secs;
    long long micros = (long long)floor(frac * 1000000.0 + 0.5);
    long long days, sod;
    int year;
    unsigned month, day;
    char buf[48];

    if (micros >= 1000000LL) { micros -= 1000000LL; secs += 1; }
    days = secs / 86400;
    if (secs % 86400 != 0 && secs < 0) days -= 1;
    sod = secs - days * 86400;
    bp_civil_from_days(days, &year, &month, &day);
    snprintf(buf, sizeof buf, "%04d-%02u-%02uT%02u-%02u-%02u-%06lldZ",
             year, month, day, (unsigned)(sod / 3600),
             (unsigned)((sod % 3600) / 60), (unsigned)(sod % 60), micros);
    return json_string(buf);
}

/* The three names preflight_backup derives, plus the restore temp names. */
json_t *w32_backup_1d0d4e_p1_emergency_paths(const json_t *req) {
    const char *home = bp_str(req, "home", "");
    const char *stamp = bp_str(req, "stamp", "");
    const char *db_name = bp_str(req, "db_name", "state.db");
    const char *prefix = "state.db.pre-update-emergency-";
    long long pid = (long long)json_get_num(req, "pid", 0.0);
    char leaf[512], staged_leaf[512], journal_leaf[520], tmp_leaf[600];
    char destination[4096], staged[4096], journal[4200], tmp[4200];
    char sidecar[4200];
    json_t *out = json_object();
    json_t *sidecars = json_array();
    static const char *const suffixes[3] = { "-wal", "-shm", "-journal" };
    size_t i;

    snprintf(leaf, sizeof leaf, "%s%s-%lld.bak", prefix, stamp, pid);
    snprintf(staged_leaf, sizeof staged_leaf, "%s%s-%lld.partial", prefix, stamp, pid);
    snprintf(journal_leaf, sizeof journal_leaf, "%s-journal", staged_leaf);
    snprintf(tmp_leaf, sizeof tmp_leaf, ".%s.snap_restore", db_name);
    bp_join(destination, sizeof destination, home, leaf);
    bp_join(staged, sizeof staged, home, staged_leaf);
    bp_join(journal, sizeof journal, home, journal_leaf);
    bp_join(tmp, sizeof tmp, home, tmp_leaf);

    json_set(out, "destination", json_string(destination));
    json_set(out, "staged", json_string(staged));
    json_set(out, "journal", json_string(journal));
    json_set(out, "tmp_restore", json_string(tmp));
    for (i = 0; i < 3; i++) {
        char name[600];
        snprintf(name, sizeof name, "%s%s", db_name, suffixes[i]);
        bp_join(sidecar, sizeof sidecar, home, name);
        json_append(sidecars, json_string(sidecar));
    }
    json_set(out, "sidecars", sidecars);
    return out;
}

/* sorted(home.glob(prefix + "*.bak"), reverse=True): first two kept, rest pruned. */
json_t *w32_backup_1d0d4e_p1_prune_survivors(const json_t *req) {
    const char *prefix = "state.db.pre-update-emergency-";
    const json_t *names = req ? json_obj_get(req, "names") : NULL;
    size_t n = names ? json_len(names) : 0, i, kept = 0;
    const char **matched = NULL;
    json_t *ordered = json_array(), *pruned = json_array(), *out = json_object();

    if (n) matched = (const char **)calloc(n, sizeof(*matched));
    if (n && !matched) { json_free(ordered); json_free(pruned); json_free(out); return json_object(); }
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(names, i);
        const char *s;
        if (!v || v->type != JSON_STRING || !v->str_val) continue;
        s = v->str_val;
        if (!bp_starts(s, prefix) || !bp_ends(s, ".bak")) continue;
        if (strlen(s) < strlen(prefix) + 4) continue;
        if (strchr(s, '/')) continue;
        if (bp_seen(matched, kept, s)) continue;
        matched[kept++] = s;
    }
    if (kept > 1) qsort(matched, kept, sizeof(*matched), bp_name_cmp_desc);
    for (i = 0; i < kept; i++) {
        json_append(ordered, json_string(matched[i]));
        if (i >= 2) json_append(pruned, json_string(matched[i]));
    }
    free(matched);
    json_set(out, "ordered", ordered);
    json_set(out, "prune", pruned);
    return out;
}

/* _reclaim_abandoned_staging: unlink prefix*.partial* older than 2 hours. */
json_t *w32_backup_1d0d4e_p1_reclaim_staging(const json_t *req) {
    const char *prefix = "state.db.pre-update-emergency-";
    const json_t *names = req ? json_obj_get(req, "names") : NULL;
    const json_t *mtimes = req ? json_obj_get(req, "mtimes") : NULL;
    double now = json_get_num(req, "now", 0.0);
    double horizon = now - 7200.0;
    size_t n = names ? json_len(names) : 0, m = mtimes ? json_len(mtimes) : 0;
    size_t i, removed_n = 0, kept_n = 0;
    const char **removed, **survivors;
    json_t *out = json_object(), *ra = json_array(), *sa = json_array();

    removed = (const char **)calloc(n + 1, sizeof(*removed));
    survivors = (const char **)calloc(n + 1, sizeof(*survivors));
    if (!removed || !survivors) {
        free(removed); free(survivors);
        json_free(ra); json_free(sa); json_free(out);
        return json_object();
    }
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(names, i);
        const char *s;
        int stale;
        if (!v || v->type != JSON_STRING || !v->str_val) continue;
        s = v->str_val;
        if (bp_seen(survivors, kept_n, s) || bp_seen(removed, removed_n, s)) continue;
        if (!bp_starts(s, prefix) || !strstr(s + strlen(prefix), ".partial")) { survivors[kept_n++] = s; continue; }
        if (strchr(s, '/')) { survivors[kept_n++] = s; continue; }
        if (i >= m) { survivors[kept_n++] = s; continue; }
        {
            const json_t *mt = json_get(mtimes, (size_t)i);
            double mt_val = (mt && mt->type == JSON_NUMBER) ? mt->num_val : now;
            stale = mt_val < horizon;
        }
        if (stale) removed[removed_n++] = s;
        else survivors[kept_n++] = s;
    }
    qsort(removed, removed_n, sizeof(*removed), bp_str_cmp_asc);
    qsort(survivors, kept_n, sizeof(*survivors), bp_str_cmp_asc);
    for (i = 0; i < removed_n; i++) json_append(ra, json_string(removed[i]));
    for (i = 0; i < kept_n; i++) json_append(sa, json_string(survivors[i]));
    free(removed);
    free(survivors);
    json_set(out, "removed", ra);
    json_set(out, "survivors", sa);
    return out;
}

/* rate.pages_per_second(), then max(256, int(pps * 0.1)) as the next step. */
json_t *w32_backup_1d0d4e_p1_step_plan(const json_t *req) {
    double pages = json_get_num(req, "rate_pages", 0.0);
    double seconds = json_get_num(req, "rate_seconds", 0.0);
    double denom = seconds > 1e-6 ? seconds : 1e-6;
    double pps = pages / denom;
    long long next = (long long)(pps * 0.1);
    json_t *out = json_object();
    if (next < 256) next = 256;
    json_set(out, "pages_per_second", json_number(pps));
    json_set(out, "next_pages", json_number((double)next));
    return out;
}

/* The stepped-copy refusal: expected_hold = total / max(pps, 1.0). */
json_t *w32_backup_1d0d4e_p1_hold_budget(const json_t *req) {
    double pages = json_get_num(req, "rate_pages", 0.0);
    double seconds = json_get_num(req, "rate_seconds", 0.0);
    double total = json_get_num(req, "total", 0.0);
    double denom = seconds > 1e-6 ? seconds : 1e-6;
    double pps = pages / denom;
    double expected = total / (pps > 1.0 ? pps : 1.0);
    int refuse = expected > 0.25;
    json_t *out = json_object();
    json_set(out, "expected_hold", json_number(expected));
    json_set(out, "refuse", json_bool(refuse));
    if (refuse) {
        char msg[512];
        snprintf(msg, sizeof msg,
                 "state.db uses a rollback journal and is written continuously; a consistent "
                 "snapshot would block those writes for about %.1fs, so it was not taken. "
                 "Retry when no agent turn is running.", expected);
        json_set(out, "message", json_string(msg));
    } else {
        json_set(out, "message", json_null());
    }
    return out;
}

/* ---------------------------------------------------------------- */
/* gate w32_backup_1d0d4e_p2 - statements, predicates, manifest      */
/* ---------------------------------------------------------------- */

json_t *w32_backup_1d0d4e_p2_constants(void) {
    json_t *out = json_object();
    json_set(out, "sqlite_header_hex", json_string("53514c69746520666f726d6174203300"));
    json_set(out, "integrity_max_bytes", json_number(2147483648.0));
    json_set(out, "abandoned_staging_seconds", json_number(7200.0));
    json_set(out, "rollback_step_seconds", json_number(0.1));
    json_set(out, "rollback_first_step_pages", json_number(256.0));
    json_set(out, "rollback_busy_sleep_seconds", json_number(0.01));
    json_set(out, "rollback_max_restarts", json_number(20.0));
    json_set(out, "rollback_lock_hold_seconds", json_number(0.25));
    return out;
}

json_t *w32_backup_1d0d4e_p2_ledger_probe(void) {
    json_t *out = json_object();
    json_set(out, "probe", json_string(
        "SELECT EXISTS(SELECT 1 FROM session_admissions) "
        "OR EXISTS(SELECT 1 FROM worker_executions)"));
    return out;
}

/* The two statements _restore_epoch_source runs on its staged copy, verbatim. */
json_t *w32_backup_1d0d4e_p2_epoch_sql(void) {
    json_t *out = json_object();
    json_set(out, "epoch_ddl", json_string(
        "CREATE TABLE IF NOT EXISTS runtime_epoch (\n"
        "                singleton INTEGER PRIMARY KEY CHECK (singleton = 1),\n"
        "                epoch INTEGER NOT NULL CHECK (epoch > 0), instance_id TEXT NOT NULL)"));
    json_set(out, "epoch_upsert", json_string(
        "INSERT INTO runtime_epoch VALUES(1,?, 'offline-restore')\n"
        "                ON CONFLICT(singleton) DO UPDATE SET epoch=excluded.epoch,\n"
        "                instance_id=excluded.instance_id"));
    return out;
}

/* _uses_wal: 19 readable bytes and file-format write version 2. */
json_t *w32_backup_1d0d4e_p2_uses_wal(const json_t *req) {
    const char *hex = bp_str(req, "header_hex", "");
    unsigned char bytes[256];
    size_t n = 0;
    int wal;
    json_t *out = json_object();

    while (hex[0] && hex[1] && n < sizeof bytes) {
        unsigned int byte = 0;
        if (sscanf(hex, "%2x", &byte) != 1) break;
        bytes[n++] = (unsigned char)byte;
        hex += 2;
    }
    wal = (n == 19 && bytes[18] == 2);
    json_set(out, "wal", json_bool(wal));
    return out;
}

/* verify_sqlite_integrity's {valid, message, size}, from the observed file state. */
json_t *w32_backup_1d0d4e_p2_verify_manifest(const json_t *req) {
    const char *path = bp_str(req, "path", "");
    const json_t *file = req ? json_obj_get(req, "file") : NULL;
    const char *kind = bp_str(file, "kind", "missing");
    const char *header_hex = bp_str(file, "header_hex", "");
    long long size = (long long)json_get_num(file, "size", 0.0);
    long long max_bytes = (long long)json_get_num(req, "max_bytes", 2147483648.0);
    int check_header = json_get_bool(req, "check_header", 1);
    int run_pragma = json_get_bool(req, "run_pragma", 1);
    int exists = strcmp(kind, "missing") != 0;
    char buf[8192];
    unsigned char head[64];
    size_t head_n = 0;
    json_t *out = json_object();

    if (!exists) {
        json_set(out, "valid", json_bool(0));
        snprintf(buf, sizeof buf, "not found: %s", path);
        json_set(out, "message", json_string(buf));
        json_set(out, "size", json_null());
        return out;
    }
    if (size < 100) {
        char digits[32];
        snprintf(digits, sizeof digits, "%lld", size);
        snprintf(buf, sizeof buf, "too small (%s bytes) to be a valid SQLite database", digits);
        json_set(out, "valid", json_bool(0));
        json_set(out, "message", json_string(buf));
        json_set(out, "size", json_number((double)size));
        return out;
    }
    if (check_header) {
        const char *h = header_hex;
        while (h[0] && h[1] && head_n < sizeof head) {
            unsigned int byte = 0;
            if (sscanf(h, "%2x", &byte) != 1) break;
            head[head_n++] = (unsigned char)byte;
            h += 2;
        }
        {
            static const unsigned char magic[16] = {
                'S','Q','L','i','t','e',' ','f','o','r','m','a','t',' ','3',0x00
            };
            int ok = head_n >= 16 && memcmp(head, magic, 16) == 0;
            if (!ok) {
                char hexbuf[64];
                size_t i, lim = head_n < 16 ? head_n : 16;
                for (i = 0; i < lim; i++) snprintf(hexbuf + i * 2, 3, "%02x", head[i]);
                hexbuf[lim * 2] = '\0';
                snprintf(buf, sizeof buf, "missing SQLite header magic (got '%s')", hexbuf);
                json_set(out, "valid", json_bool(0));
                json_set(out, "message", json_string(buf));
                json_set(out, "size", json_number((double)size));
                return out;
            }
        }
    }
    if (max_bytes > 0 && size > max_bytes) {
        char sz[48], mb[48];
        bp_group(sz, sizeof sz, size);
        bp_group(mb, sizeof mb, max_bytes);
        snprintf(buf, sizeof buf,
                 "size %s bytes exceeds max_bytes %s; skipped PRAGMA integrity_check "
                 "(header + schema probe passed)", sz, mb);
        json_set(out, "valid", json_bool(1));
        json_set(out, "message", json_string(buf));
        json_set(out, "size", json_number((double)size));
        return out;
    }
    if (run_pragma) {
        const json_t *rows = file ? json_obj_get(file, "rows") : NULL;
        size_t rn = rows ? json_len(rows) : 0;
        int ok = rn == 1;
        if (ok) {
            const json_t *only = json_get(rows, 0);
            ok = only && only->type == JSON_STRING && only->str_val && !strcmp(only->str_val, "ok");
        }
        if (ok) {
            json_set(out, "valid", json_bool(1));
            json_set(out, "message", json_string("integrity check passed"));
        } else {
            char joined[4096];
            size_t i, lim = rn < 5 ? rn : 5, off = 0;
            joined[0] = '\0';
            for (i = 0; i < lim; i++) {
                const json_t *row = json_get(rows, (size_t)i);
                const char *text = (row && row->type == JSON_STRING && row->str_val) ? row->str_val : "";
                off += (size_t)snprintf(joined + off, sizeof joined - off, "%s%s",
                                        i ? "; " : "", text);
                if (off >= sizeof joined) { off = sizeof joined - 1; break; }
            }
            snprintf(buf, sizeof buf, "integrity check failed: %s", joined);
            json_set(out, "valid", json_bool(0));
            json_set(out, "message", json_string(buf));
        }
        json_set(out, "size", json_number((double)size));
        return out;
    }
    json_set(out, "valid", json_bool(1));
    json_set(out, "message", json_string("header check passed"));
    json_set(out, "size", json_number((double)size));
    return out;
}

/* _restore_epoch_source: only a state.db destination needs an epoch floor staged. */
json_t *w32_backup_1d0d4e_p2_epoch_decision(const json_t *req) {
    const char *dst_name = bp_str(req, "dst_name", "");
    const char *resolved = bp_str(req, "dst_resolved_name", "");
    long long floor = (long long)json_get_num(req, "floor", 0.0);
    long long src_epoch = (long long)json_get_num(req, "src_epoch", 0.0);
    int is_state = !strcmp(dst_name, "state.db") || !strcmp(resolved, "state.db");
    int stage = is_state && floor > src_epoch;
    json_t *out = json_object();

    json_set(out, "stage", json_bool(stage));
    json_set(out, "action", json_string(stage ? "stage" : "passthrough"));
    json_set(out, "work_prefix", json_string(".restore-epoch-"));
    json_set(out, "prepared", json_string("state.db"));
    return out;
}
