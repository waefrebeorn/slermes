/* session_recovery_main.c — snapshot+inspect, top-level inspect/recover
 * orchestration and report writing. Faithful port of
 * hermes_cli/session_recovery.py (main slice). Destination initialization
 * REUSES the hermes_state stack (hermes_state_db_open bootstraps the full
 * current schema v23 + FTS layout — the C analog of SessionDB(db_path=...)).
 */
#define _GNU_SOURCE
#include "session_recovery_internal.h"
#include "hermes_state_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

static bool sr_fingerprint_equal(const json_t *a, const json_t *b) {
    char *sa = json_serialize(a);
    char *sb = json_serialize(b);
    bool eq = sa && sb && strcmp(sa, sb) == 0;
    free(sa);
    free(sb);
    return eq;
}

static void sr_rm_rf(const char *dir) {
    char cmd[4200];
    /* dir is a mkdtemp path we created — safe to remove recursively */
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    (void)!system(cmd);
}

/* PoP: sr_snapshot_and_inspect @ hermes_cli/session_recovery.py:_snapshot_and_inspect */
static int sr_snapshot_and_inspect(const char *source, const char *work_root,
                                   char **temp_dir_out,
                                   char **snapshot_source_out,
                                   json_t **inspection_out,
                                   char *err, size_t elen) {
    json_t *before = sr_source_fingerprint(source);

    char tmpl[4200];
    snprintf(tmpl, sizeof(tmpl), "%s/hermes-session-recovery-XXXXXX",
             work_root);
    char *temp_dir = mkdtemp(tmpl);
    if (!temp_dir) {
        sr_set_err(err, elen, "cannot create temp dir under %s", work_root);
        json_free(before);
        return SESSION_RECOVERY_SAFETY_ERROR;
    }
    temp_dir = strdup(temp_dir);

    char *snapshot_source = NULL;
    json_t *copied = NULL;
    int rc = sr_copy_source_bundle(source, temp_dir, &snapshot_source,
                                   &copied, err, elen);
    if (rc != 0) {
        sr_rm_rf(temp_dir);
        free(temp_dir);
        json_free(before);
        return rc;
    }

    json_t *after = sr_source_fingerprint(source);
    bool unchanged = sr_fingerprint_equal(before, after);
    json_free(after);
    if (!unchanged) {
        sr_set_err(err, elen,
            "The source database bundle changed while it was being copied. "
            "Stop every Hermes process using this profile and retry.");
        sr_rm_rf(temp_dir);
        free(temp_dir);
        free(snapshot_source);
        json_free(copied);
        json_free(before);
        return SESSION_RECOVERY_SAFETY_ERROR;
    }

    sqlite3 *conn = NULL;
    json_t *inspection = NULL;
    if (sqlite3_open_v2(snapshot_source, &conn,
                        SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK) {
        sqlite3_busy_timeout(conn, 1000);
        inspection = sr_inspect_connection(conn);
        sqlite3_close(conn);
    } else {
        if (conn) sqlite3_close(conn);
        inspection = json_object();
        json_set(inspection, "tables", json_object());
        json_t *errs = json_array();
        json_append(errs, json_string("cannot open snapshot database"));
        json_set(inspection, "errors", errs);
        json_set(inspection, "warnings", json_array());
        json_set(inspection, "recoverable", json_bool(false));
    }
    json_set(inspection, "source_bundle", copied);
    json_set(inspection, "source_fingerprint", before);

    *temp_dir_out = temp_dir;
    *snapshot_source_out = snapshot_source;
    *inspection_out = inspection;
    return 0;
}

/* PoP: session_recovery_inspect @ hermes_cli/session_recovery.py:inspect_session_database */
json_t *session_recovery_inspect(const char *source_path,
                                 const char *work_dir,
                                 session_recovery_status_t *status,
                                 char *errbuf, size_t errbuf_len) {
    if (status) *status = SESSION_RECOVERY_OK;
    char *source = NULL, *output = NULL, *work_root = NULL;
    int rc = sr_validate_paths(source_path, NULL, work_dir,
                               &source, &output, &work_root,
                               errbuf, errbuf_len);
    if (rc != 0) {
        if (status) *status = (session_recovery_status_t)rc;
        return NULL;
    }

    json_t *disk_space = sr_disk_space_preflight(source, work_root, NULL,
                                                 errbuf, errbuf_len);
    if (!disk_space) {
        if (status) *status = SESSION_RECOVERY_SAFETY_ERROR;
        free(source); free(work_root);
        return NULL;
    }

    char *temp_dir = NULL, *snapshot = NULL;
    json_t *inspection = NULL;
    rc = sr_snapshot_and_inspect(source, work_root, &temp_dir, &snapshot,
                                 &inspection, errbuf, errbuf_len);
    if (rc != 0) {
        if (status) *status = (session_recovery_status_t)rc;
        json_free(disk_space);
        free(source); free(work_root);
        return NULL;
    }

    json_t *report = json_object();
    json_set(report, "operation", json_string("inspect"));
    json_set(report, "source", json_string(source));
    json_set(report, "disk_space", disk_space);
    /* **inspection merge */
    for (size_t i = 0; i < inspection->c.count; i++)
        json_set(report, inspection->c.keys[i],
                 json_copy(inspection->c.items[i]));
    json_t *now_fp = sr_source_fingerprint(source);
    json_set(report, "source_unchanged",
             json_bool(sr_fingerprint_equal(
                 now_fp, json_obj_get(inspection, "source_fingerprint"))));
    json_free(now_fp);
    json_free(inspection);

    sr_rm_rf(temp_dir);
    free(temp_dir);
    free(snapshot);
    free(source);
    free(work_root);
    return report;
}

/* PoP: session_recovery_recover @ hermes_cli/session_recovery.py:recover_session_database */
json_t *session_recovery_recover(const char *source_path,
                                 const char *output_path,
                                 const char *work_dir,
                                 int chunk_size,
                                 session_recovery_progress_cb progress_cb,
                                 void *progress_ud,
                                 bool allow_partial,
                                 session_recovery_status_t *status,
                                 char *errbuf, size_t errbuf_len) {
    if (status) *status = SESSION_RECOVERY_OK;
    if (chunk_size <= 0) {
        sr_set_err(errbuf, errbuf_len, "chunk_size must be greater than zero");
        if (status) *status = SESSION_RECOVERY_SAFETY_ERROR;
        return NULL;
    }
    char *source = NULL, *output = NULL, *work_root = NULL;
    int rc = sr_validate_paths(source_path, output_path, work_dir,
                               &source, &output, &work_root,
                               errbuf, errbuf_len);
    if (rc != 0) {
        if (status) *status = (session_recovery_status_t)rc;
        return NULL;
    }
    char *odup = strdup(output);
    char *output_parent = strdup(dirname(odup));
    free(odup);
    json_t *disk_space = sr_disk_space_preflight(source, work_root,
                                                 output_parent,
                                                 errbuf, errbuf_len);
    free(output_parent);
    if (!disk_space) {
        if (status) *status = SESSION_RECOVERY_SAFETY_ERROR;
        goto fail_paths;
    }

    char *temp_dir = NULL, *snapshot = NULL;
    json_t *inspection = NULL;
    rc = sr_snapshot_and_inspect(source, work_root, &temp_dir, &snapshot,
                                 &inspection, errbuf, errbuf_len);
    if (rc != 0) {
        if (status) *status = (session_recovery_status_t)rc;
        json_free(disk_space);
        goto fail_paths;
    }

    json_t *tables = json_obj_get(inspection, "tables");
    if (!json_get_bool(inspection, "recoverable", false) && !allow_partial) {
        char reasons[900] = "";
        const json_t *errs = json_obj_get(inspection, "errors");
        for (size_t i = 0; errs && i < json_len(errs); i++) {
            size_t off = strlen(reasons);
            snprintf(reasons + off, sizeof(reasons) - off, "%s%s",
                     i ? "; " : "", json_get(errs, i)->str_val);
        }
        sr_set_err(errbuf, errbuf_len,
                   "Required canonical tables are not readable: %s",
                   reasons[0] ? reasons : "unknown source error");
        if (status) *status = SESSION_RECOVERY_SOURCE_ERROR;
        goto fail_snapshot;
    }
    if (allow_partial) {
        char missing[64] = "";
        const char *required[2] = { "sessions", "messages" };
        for (size_t i = 0; i < 2; i++) {
            const json_t *tr = json_obj_get(tables, required[i]);
            if (!tr || !json_get_bool(tr, "available", false)) {
                size_t off = strlen(missing);
                snprintf(missing + off, sizeof(missing) - off, "%s%s",
                         off ? ", " : "", required[i]);
            }
        }
        if (missing[0]) {
            sr_set_err(errbuf, errbuf_len,
                "Partial recovery still requires readable table schemas "
                "for: %s", missing);
            if (status) *status = SESSION_RECOVERY_SOURCE_ERROR;
            goto fail_snapshot;
        }
    }

    {
        /* Destination init: SessionDB(db_path=output) — full v23 schema. */
        bool has_topic_tables = false;
        for (size_t i = 0; i < 2; i++) {
            const json_t *tr = json_obj_get(tables, SR_TOPIC_TABLES[i]);
            if (tr && json_get_bool(tr, "available", false))
                has_topic_tables = true;
        }
        hermes_state_db_t *dest_db = hermes_state_db_open(output);
        if (!dest_db) {
            sr_set_err(errbuf, errbuf_len,
                       "failed to initialize destination database: %s", output);
            if (status) *status = SESSION_RECOVERY_ERROR;
            goto fail_snapshot;
        }
        if (has_topic_tables)
            hermes_state_apply_telegram_topic_migration(dest_db);
        hermes_state_db_close(dest_db);
    }

    sqlite3 *src = NULL, *dst = NULL;
    if (sqlite3_open_v2(snapshot, &src, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK ||
        sqlite3_open_v2(output, &dst, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        sr_set_err(errbuf, errbuf_len, "failed to open copy connections");
        if (status) *status = SESSION_RECOVERY_ERROR;
        if (src) sqlite3_close(src);
        if (dst) sqlite3_close(dst);
        goto fail_snapshot;
    }
    sqlite3_busy_timeout(src, 1000);
    sqlite3_busy_timeout(dst, 1000);
    sqlite3_exec(src, "PRAGMA writable_schema=ON", NULL, NULL, NULL);
    sqlite3_exec(dst, "PRAGMA foreign_keys=OFF", NULL, NULL, NULL);

    json_t *copy_report = json_object();
    for (size_t i = 0; i < 6; i++) {
        const char *table = SR_CANONICAL_TABLES[i];
        const json_t *tr = json_obj_get(tables, table);
        const json_t *rows = tr ? json_obj_get(tr, "rows") : NULL;
        json_t *r = allow_partial
            ? sr_copy_table_salvage(src, dst, table, chunk_size, progress_cb,
                                    progress_ud, rows, "INSERT", false)
            : sr_copy_table(src, dst, table, chunk_size, progress_cb,
                            progress_ud, rows);
        json_set(copy_report, table, r);
    }
    {
        const json_t *tr = json_obj_get(tables, "state_meta");
        if (tr && json_get_bool(tr, "available", false)) {
            const json_t *rows = json_obj_get(tr, "rows");
            json_t *r = allow_partial
                ? sr_copy_state_meta_salvage(src, dst, chunk_size, progress_cb,
                                             progress_ud, rows)
                : sr_copy_state_meta(src, dst, chunk_size, progress_cb,
                                     progress_ud, rows);
            json_set(copy_report, "state_meta", r);
        } else {
            json_t *r = json_object();
            json_set(r, "status", json_string("missing"));
            json_set(r, "copied_rows", json_number(0));
            json_set(copy_report, "state_meta", r);
        }
    }
    for (size_t i = 0; i < 2; i++) {
        const char *table = SR_TOPIC_TABLES[i];
        const json_t *tr = json_obj_get(tables, table);
        if (!tr || !json_get_bool(tr, "available", false)) {
            json_t *r = json_object();
            json_set(r, "status", json_string("missing"));
            json_set(r, "copied_rows", json_number(0));
            json_set(copy_report, table, r);
            continue;
        }
        const json_t *rows = json_obj_get(tr, "rows");
        json_t *r = allow_partial
            ? sr_copy_table_salvage(src, dst, table, chunk_size, progress_cb,
                                    progress_ud, rows, "INSERT", false)
            : sr_copy_table(src, dst, table, chunk_size, progress_cb,
                            progress_ud, rows);
        json_set(copy_report, table, r);
    }

    json_t *orphan_cleanup = allow_partial ? sr_cleanup_partial_orphans(dst)
                                           : NULL;
    json_t *derived_metadata = sr_finalize_derived_metadata(dst);
    sqlite3_close(src);
    sqlite3_close(dst);

    json_t *expected_counts = json_object();
    for (size_t i = 0; i < 6; i++) {
        const json_t *tr = json_obj_get(tables, SR_CANONICAL_TABLES[i]);
        const json_t *rows = tr ? json_obj_get(tr, "rows") : NULL;
        json_set(expected_counts, SR_CANONICAL_TABLES[i],
                 rows && rows->type != JSON_NULL ? json_number(rows->num_val)
                                                 : json_null());
    }
    json_t *verification = sr_verify_recovered_database(
        output, expected_counts, copy_report, allow_partial, orphan_cleanup);
    json_free(expected_counts);

    json_t *now_fp = sr_source_fingerprint(source);
    bool source_unchanged = sr_fingerprint_equal(
        now_fp, json_obj_get(inspection, "source_fingerprint"));
    json_free(now_fp);
    if (!source_unchanged) {
        json_t *verrs = json_obj_get(verification, "errors");
        if (verrs)
            json_append(verrs, json_string(
                "the source database bundle changed during recovery"));
        json_set(verification, "complete", json_bool(false));
    }

    json_t *report = json_object();
    json_set(report, "operation", json_string("recover"));
    json_set(report, "allow_partial", json_bool(allow_partial));
    json_set(report, "source", json_string(source));
    json_set(report, "output", json_string(output));
    json_set(report, "source_bundle",
             json_copy(json_obj_get(inspection, "source_bundle")));
    json_set(report, "source_fingerprint",
             json_copy(json_obj_get(inspection, "source_fingerprint")));
    json_set(report, "source_unchanged", json_bool(source_unchanged));
    json_set(report, "disk_space", disk_space);
    {
        json_t *insp = json_object();
        const json_t *jm = json_obj_get(inspection, "journal_mode");
        json_set(insp, "journal_mode", jm ? json_copy(jm) : json_null());
        json_set(insp, "tables", json_copy(tables));
        json_set(insp, "errors",
                 json_copy(json_obj_get(inspection, "errors")));
        json_set(insp, "warnings",
                 json_copy(json_obj_get(inspection, "warnings")));
        json_set(report, "inspection", insp);
    }
    json_set(report, "copy", copy_report);
    json_set(report, "orphan_cleanup",
             orphan_cleanup ? orphan_cleanup : json_null());
    json_set(report, "derived_metadata", derived_metadata);
    bool v_complete = json_get_bool(verification, "complete", false);
    bool v_loss = json_get_bool(verification, "loss_detected", false);
    bool v_healthy = json_get_bool(verification, "healthy", false);
    json_set(report, "verification", verification);
    json_set(report, "complete", json_bool(v_complete && source_unchanged));
    json_set(report, "partial", json_bool(v_loss));
    json_set(report, "verified", json_bool(v_healthy && source_unchanged));
    json_set(report, "installed", json_bool(false));

    json_free(inspection);
    sr_rm_rf(temp_dir);
    free(temp_dir);
    free(snapshot);
    free(source);
    free(output);
    free(work_root);
    return report;

fail_snapshot:
    json_free(disk_space);
    json_free(inspection);
    sr_rm_rf(temp_dir);
    free(temp_dir);
    free(snapshot);
fail_paths:
    free(source);
    free(output);
    free(work_root);
    return NULL;
}

/* PoP: session_recovery_write_report @ hermes_cli/session_recovery.py:write_recovery_report */
char *session_recovery_write_report(const char *path, const json_t *report,
                                    char *errbuf, size_t errbuf_len) {
    char *destination = sr_resolved_output_path(path, errbuf, errbuf_len);
    if (!destination) return NULL;
    /* open "x": exclusive create */
    int fd = open(destination, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
        sr_set_err(errbuf, errbuf_len, "cannot create report %s: %s",
                   destination, strerror(errno));
        free(destination);
        return NULL;
    }
    FILE *f = fdopen(fd, "w");
    char *text = json_serialize_pretty(report, 2);
    fputs(text ? text : "{}", f);
    fputc('\n', f);
    free(text);
    fclose(f);
    return destination;
}
