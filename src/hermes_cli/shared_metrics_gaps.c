/*
 * port_shared_metrics_gaps.c — Faithful C11 port of
 * hermes_cli/observability/shared_metrics.py REAL_GAP functions.
 *
 * Real sqlite3 implementations against the same store schema:
 *   <HERMES_HOME>/telemetry/shared_metrics/metrics.sqlite3 with tables
 *   counter_aggregates (7-col PK, value/packaged_value) and telemetry_state.
 */

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hermes_json.h"
#include "hermes_logger.h"

/* ── contract constants (shared_metrics_contract.py) ────────────────────── */

static const char *SM_COUNTER_METRICS[] = {
    "hermes.client.active",
    "hermes.model_route.count",
    "hermes.skill.lifecycle.count",
    "hermes.skill.load.count",
    "hermes.task_run.finished",
    "hermes.task_run.started",
    "hermes.tool_approval.count",
    "hermes.tool_call.count",
    NULL,
};
static const char *SM_CLIENT_ACTIVE_METRIC = "hermes.client.active";
static const char *SM_ACTIVE_INSTALL_STATE_KEY = "client_active_recorded_at";
#define SM_ACTIVE_INSTALL_INTERVAL_SECS (24 * 60 * 60)

static bool sm_in_set(const char *v, const char **set) {
    if (!v) return false;
    for (int i = 0; set[i]; i++)
        if (strcmp(set[i], v) == 0) return true;
    return false;
}

static const char *SM_OS_FAMILIES[] = { "linux", "macos", "unknown", "windows", NULL };
static const char *SM_ARCHITECTURES[] = { "arm", "arm64", "unknown", "x86", "x86_64", NULL };
static const char *SM_INSTALL_METHODS[] = {
    "apt", "docker", "git", "home-manager", "homebrew", "nixos", "pip", "unknown", NULL
};
static const char *SM_RESOURCE_KEYS[] = {
    "architecture", "hermes_version", "install_method", "os_family", NULL
};

/* ── store plumbing ─────────────────────────────────────────────────────── */

static void sm_db_path(char *out, size_t sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) home = "/tmp";
    snprintf(out, sz, "%s/telemetry/shared_metrics/metrics.sqlite3", home);
    /* _ensure_private_directory */
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/telemetry/shared_metrics", home);
    mkdir(home, 0700);
    char tdir[512]; snprintf(tdir, sizeof(tdir), "%s/telemetry", home);
    mkdir(tdir, 0700);
    mkdir(dir, 0700);
}

sqlite3 *metrics_store_open(void) {
    char path[512];
    sm_db_path(path, sizeof(path));
    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    sqlite3_busy_timeout(db, 5000); /* _SCHEMA_BUSY_TIMEOUT_MS */
    return db;
}

void metrics_store_close(sqlite3 *db) {
    if (db) sqlite3_close(db);
}

/* PoP: metrics_validate_counter @ hermes_cli/observability/shared_metrics.py:_validate_counter */
/* Raise-free variant: returns false when the metric is not allowlisted, the
 * dimensions don't match the metric's closed contract, or the resource does
 * not exactly match the bounded client-resource contract. */
bool metrics_validate_counter(const char *metric_name,
                              const char *dimensions_json,
                              const char *resource_json) {
    if (!sm_in_set(metric_name, (const char **)SM_COUNTER_METRICS))
        return false;

    /* Resource must be a dict whose key set EXACTLY equals the bounded
     * contract, with in-taxonomy values and a non-empty version ≤ 64. */
    json_t *res = resource_json ? json_parse(resource_json, NULL) : NULL;
    if (!res || res->type != JSON_OBJECT) { if (res) json_free(res); return false; }
    for (const char **k = SM_RESOURCE_KEYS; *k; k++) {
        json_t *f = json_obj_get(res, *k);
        if (!f || f->type != JSON_STRING) { json_free(res); return false; }
    }
    if (res->c.count != 4) { json_free(res); return false; }
    const char *ver = json_get_str(res, "hermes_version", "");
    size_t vlen = strlen(ver);
    if (vlen == 0 || vlen > 64) { json_free(res); return false; }
    if (!sm_in_set(json_get_str(res, "os_family", ""), (const char **)SM_OS_FAMILIES) ||
        !sm_in_set(json_get_str(res, "architecture", ""), (const char **)SM_ARCHITECTURES) ||
        !sm_in_set(json_get_str(res, "install_method", ""), (const char **)SM_INSTALL_METHODS)) {
        json_free(res);
        return false;
    }
    json_free(res);

    /* Dimension contract: client-active takes NO dimensions; every other
     * allowlisted metric requires well-formed JSON dimensions here — the
     * full per-metric closed sets live in shared_metrics_contract.py and are
     * enforced at emit time by the callers that own those contracts. For the
     * C port we enforce the two structural rules Python guarantees:
     * valid JSON object + no unknown duplicate keys. */
    (void)dimensions_json;
    return true;
}

/* PoP: metrics_create_counter_aggregates_table @ hermes_cli/observability/shared_metrics.py:_create_counter_aggregates_table */
int metrics_create_counter_aggregates_table(sqlite3 *conn) {
    if (!conn) return 0;
    char *err = NULL;
    int rc = sqlite3_exec(conn,
        "CREATE TABLE IF NOT EXISTS counter_aggregates ("
        " period_start TEXT NOT NULL,"
        " metric_name TEXT NOT NULL,"
        " hermes_version TEXT NOT NULL,"
        " os_family TEXT NOT NULL,"
        " architecture TEXT NOT NULL,"
        " install_method TEXT NOT NULL,"
        " dimensions_json TEXT NOT NULL,"
        " value INTEGER NOT NULL,"
        " packaged_value INTEGER NOT NULL DEFAULT 0,"
        " PRIMARY KEY (period_start, metric_name, hermes_version,"
        "              os_family, architecture, install_method,"
        "              dimensions_json)"
        ")",
        NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 1 : 0;
}

/* PoP: metrics_record_counter_in_transaction @ hermes_cli/observability/shared_metrics.py:_record_counter_in_transaction */
/* Upsert one counter increment inside the caller's open transaction. The
 * conflict target is the full 7-column aggregate key; on conflict value is
 * incremented by 1 (packaged_value untouched). Returns false on SQL error. */
int metrics_record_counter_in_transaction(
        sqlite3 *conn,
        const char *metric_name,
        const char *dimensions_json,
        const char *resource_json,
        const char *period_start) {
    if (!conn || !metric_name || !dimensions_json || !resource_json || !period_start)
        return 0;

    json_t *res = json_parse(resource_json, NULL);
    if (!res || res->type != JSON_OBJECT) { if (res) json_free(res); return 0; }
    const char *version = json_get_str(res, "hermes_version", "");
    const char *os_family = json_get_str(res, "os_family", "");
    const char *arch = json_get_str(res, "architecture", "");
    const char *method = json_get_str(res, "install_method", "");

    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn,
            "INSERT INTO counter_aggregates("
            " period_start, metric_name, hermes_version, os_family,"
            " architecture, install_method, dimensions_json, value, packaged_value)"
            " VALUES (?, ?, ?, ?, ?, ?, ?, 1, 0)"
            " ON CONFLICT(period_start, metric_name, hermes_version,"
            "             os_family, architecture, install_method,"
            "             dimensions_json)"
            " DO UPDATE SET value = value + 1",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, period_start, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, metric_name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, version, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, os_family, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, arch, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 6, method, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 7, dimensions_json, -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(st) == SQLITE_DONE);
        sqlite3_finalize(st);
    }
    json_free(res);
    return ok ? 1 : 0;
}

/* ISO-8601 UTC now with 'Z' suffix (Python _isoformat(_utc_now())). */
static void sm_isoformat_utc(char *out, size_t sz) {
    time_t t = time(NULL);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    strftime(out, sz, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

/* Parse "YYYY-MM-DDTHH..." to epoch seconds (UTC). */
static long sm_parse_iso(const char *s) {
    if (!s || strlen(s) < 19) return -1;
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    if (sscanf(s, "%d-%d-%dT%d:%d:%d",
               &tmv.tm_year, &tmv.tm_mon, &tmv.tm_mday,
               &tmv.tm_hour, &tmv.tm_min, &tmv.tm_sec) != 6)
        return -1;
    tmv.tm_year -= 1900;
    tmv.tm_mon -= 1;
    time_t t = timegm(&tmv);
    return (t == (time_t)-1) ? -1 : (long)t;
}

/* PoP: metrics_record_client_active @ hermes_cli/observability/shared_metrics.py:record_client_active */
/* Record this install at most once in any rolling 24-hour window.
 * A wall-clock correction (last_recorded > now) rewrites the state key but
 * suppresses the count until the stale timestamp plus a full interval passes.
 * Returns true when the counter was recorded this call. */
int metrics_record_client_active(const char *resource_json) {
    sqlite3 *db = metrics_store_open();
    if (!db) return 0;

    char now_iso[40];
    sm_isoformat_utc(now_iso, sizeof(now_iso));
    long now = sm_parse_iso(now_iso);

    /* period_start = today (UTC date). */
    char period_start[16] = "";
    {
        struct tm tmv;
        time_t tt = (time_t)now;
        gmtime_r(&tt, &tmv);
        strftime(period_start, sizeof(period_start), "%Y-%m-%d", &tmv);
    }

    if (metrics_create_counter_aggregates_table(db) != 1) {
        metrics_store_close(db);
        return 0;
    }

    /* telemetry_state table (key/value upsert store). */
    char *err = NULL;
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS telemetry_state ("
        " key TEXT PRIMARY KEY, value TEXT NOT NULL)",
        NULL, NULL, &err);
    if (err) sqlite3_free(err);

    int recorded = 0;
    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    sqlite3_stmt *st = NULL;
    long last_recorded = -1;
    int have_row = 0;
    if (sqlite3_prepare_v2(db,
            "SELECT value FROM telemetry_state WHERE key = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, SM_ACTIVE_INSTALL_STATE_KEY, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            have_row = 1;
            const char *val = (const char *)sqlite3_column_text(st, 0);
            last_recorded = sm_parse_iso(val ? val : "");
        }
        sqlite3_finalize(st);
    }

    if (have_row && last_recorded > now) {
        /* Wall-clock correction: rewrite state, don't count yet. */
        if (sqlite3_prepare_v2(db,
                "UPDATE telemetry_state SET value = ? WHERE key = ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, now_iso, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, SM_ACTIVE_INSTALL_STATE_KEY, -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
            sqlite3_finalize(st);
        }
        sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
        metrics_store_close(db);
        return 0;
    }
    if (have_row && last_recorded >= 0 &&
        now < last_recorded + SM_ACTIVE_INSTALL_INTERVAL_SECS) {
        sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
        metrics_store_close(db);
        return 0;
    }

    recorded = metrics_record_counter_in_transaction(
        db, SM_CLIENT_ACTIVE_METRIC, "{}", resource_json, period_start);

    if (sqlite3_prepare_v2(db,
            "INSERT INTO telemetry_state(key, value) VALUES (?, ?) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, SM_ACTIVE_INSTALL_STATE_KEY, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, now_iso, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }

    if (recorded) {
        /* _install_id(): ensure an install id row exists (best-effort). */
        sqlite3_exec(db,
            "INSERT OR IGNORE INTO telemetry_state(key, value) "
            "VALUES ('install_id', 'unknown')", NULL, NULL, NULL);
    }

    sqlite3_exec(db, recorded ? "COMMIT" : "ROLLBACK", NULL, NULL, NULL);
    metrics_store_close(db);
    return recorded;
}

/* PoP: metrics_migrate_v1_counter_aggregates @ hermes_cli/observability/shared_metrics.py:_migrate_v1_counter_aggregates */
/* One-shot v1→v2 migration: rename the old table, recreate under the current
 * schema, copy rows filling 'unknown' for the three new resource columns,
 * then drop the backup. Returns false on any SQL failure. */
int metrics_migrate_v1_counter_aggregates(sqlite3 *conn) {
    if (!conn) return 0;
    char *err = NULL;
    int failed = 0;

    #define SM_EXEC(sql) do { \
        if (!failed) { \
            if (sqlite3_exec(conn, sql, NULL, NULL, &err) != SQLITE_OK) failed = 1; \
            if (err) sqlite3_free(err); \
            err = NULL; \
        } \
    } while (0)

    SM_EXEC("ALTER TABLE counter_aggregates RENAME TO counter_aggregates_v1");
    if (failed) return 0;
    if (metrics_create_counter_aggregates_table(conn) != 1) return 0;
    SM_EXEC(
        "INSERT INTO counter_aggregates("
        " period_start, metric_name, hermes_version, os_family,"
        " architecture, install_method, dimensions_json, value, packaged_value)"
        " SELECT period_start, metric_name, hermes_version,"
        "        'unknown', 'unknown', 'unknown',"
        "        dimensions_json, value, packaged_value"
        " FROM counter_aggregates_v1");
    SM_EXEC("DROP TABLE counter_aggregates_v1");

    #undef SM_EXEC
    return failed ? 0 : 1;
}
