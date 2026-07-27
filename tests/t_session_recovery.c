/* t_session_recovery.c — behavioral test for the session_recovery stack.
 * Mirrors tests/hermes_cli/session_recovery invariants from the Python
 * suite: inspect healthy DB, recover full copy, refuse overwrite, refuse
 * unreadable source, partial salvage with orphan cleanup, format_bytes,
 * report writing.
 */
#include "session_recovery.h"
#include "hermes_state_db.h"
#include "sqlite3.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int g_pass = 0;
#define CHECK(cond, name) do { \
    if (cond) { g_pass++; printf("PASS %s\n", name); } \
    else { printf("FAIL %s\n", name); return 1; } \
} while (0)

static const json_t *jget(const json_t *o, const char *k) {
    return json_obj_get(o, k);
}

static void make_source_db(const char *path, int n_msgs) {
    hermes_state_db_t *db = hermes_state_db_open(path);
    hermes_state_create_session(db, "sess-1", "cli");
    for (int i = 0; i < n_msgs; i++) {
        char content[64];
        snprintf(content, sizeof(content), "message %d", i);
        hermes_state_append_message(db, "sess-1",
                                    i % 2 ? "assistant" : "user",
                                    content, NULL, NULL, 0);
    }
    hermes_state_db_close(db);
}

int main(void) {
    char tmpl[] = "/tmp/t_session_recovery_XXXXXX";
    char *dir = mkdtemp(tmpl);
    assert(dir);
    char source[256], output[256], report_path[256];
    snprintf(source, sizeof(source), "%s/state.db", dir);
    snprintf(output, sizeof(output), "%s/recovered.db", dir);
    snprintf(report_path, sizeof(report_path), "%s/report.json", dir);

    /* 1. format_bytes */
    char *fb = session_recovery_format_bytes(1536);
    CHECK(strcmp(fb, "1.5 KiB") == 0, "format_bytes 1536 -> 1.5 KiB");
    free(fb);
    fb = session_recovery_format_bytes(0);
    CHECK(strcmp(fb, "0.0 B") == 0, "format_bytes 0 -> 0.0 B");
    free(fb);
    fb = session_recovery_format_bytes(5LL * 1024 * 1024 * 1024);
    CHECK(strcmp(fb, "5.0 GiB") == 0, "format_bytes 5GiB");
    free(fb);

    /* 2. inspect a healthy source */
    make_source_db(source, 6);
    session_recovery_status_t st;
    char err[1024];
    json_t *insp = session_recovery_inspect(source, NULL, &st, err, sizeof(err));
    CHECK(insp != NULL && st == SESSION_RECOVERY_OK, "inspect returns report");
    CHECK(json_get_bool(insp, "recoverable", false), "inspect recoverable=true");
    {
        const json_t *tables = jget(insp, "tables");
        const json_t *msgs = jget(tables, "messages");
        CHECK(json_get_bool(msgs, "available", false), "messages available");
        const json_t *rows = jget(msgs, "rows");
        CHECK(rows && rows->type == JSON_NUMBER && (int)rows->num_val == 6,
              "messages rows == 6");
        CHECK(json_get_bool(insp, "source_unchanged", false),
              "inspect source_unchanged");
    }
    json_free(insp);

    /* 3. inspect missing source -> safety error */
    json_t *bad = session_recovery_inspect("/nonexistent/state.db", NULL, &st,
                                           err, sizeof(err));
    CHECK(bad == NULL && st == SESSION_RECOVERY_SAFETY_ERROR,
          "inspect missing source refused");

    /* 4. full recover */
    json_t *rec = session_recovery_recover(source, output, NULL, 1000, NULL,
                                           NULL, false, &st, err, sizeof(err));
    if (!rec) fprintf(stderr, "recover error: %s\n", err);
    CHECK(rec != NULL && st == SESSION_RECOVERY_OK, "recover returns report");
    CHECK(json_get_bool(rec, "complete", false), "recover complete=true");
    CHECK(json_get_bool(rec, "verified", false), "recover verified=true");
    CHECK(!json_get_bool(rec, "partial", true), "recover partial=false");
    CHECK(!json_get_bool(rec, "installed", true), "recover installed=false");
    {
        const json_t *copy = jget(rec, "copy");
        const json_t *m = jget(copy, "messages");
        const json_t *status = jget(m, "status");
        CHECK(status && strcmp(status->str_val, "complete") == 0,
              "messages copy status complete");
        CHECK((int)json_get_num(m, "copied_rows", -1) == 6,
              "messages copied_rows == 6");
        const json_t *sm = jget(copy, "state_meta");
        const json_t *smst = jget(sm, "status");
        CHECK(smst && (strcmp(smst->str_val, "complete") == 0),
              "state_meta copy complete");
        const json_t *ver = jget(rec, "verification");
        CHECK(json_get_bool(ver, "healthy", false), "verification healthy");
        const json_t *integ = jget(ver, "integrity_check");
        CHECK(json_len(integ) == 1 &&
              strcmp(json_get(integ, 0)->str_val, "ok") == 0,
              "integrity_check ok");
        const json_t *fm = jget(ver, "fts_meta");
        const json_t *fsv = jget(fm, "fts_storage_version");
        CHECK(fsv && fsv->type == JSON_STRING && strcmp(fsv->str_val, "1") == 0,
              "fts_storage_version stamped");
    }

    /* 5. recovered DB opens with hermes_state and has the messages */
    {
        hermes_state_db_t *rdb = hermes_state_db_open(output);
        CHECK(rdb != NULL, "recovered DB opens via hermes_state");
        hermes_state_db_close(rdb);
        sqlite3 *conn = NULL;
        sqlite3_open(output, &conn);
        sqlite3_stmt *q = NULL;
        sqlite3_prepare_v2(conn, "SELECT COUNT(*) FROM messages", -1, &q, NULL);
        sqlite3_step(q);
        CHECK(sqlite3_column_int(q, 0) == 6, "recovered messages count == 6");
        sqlite3_finalize(q);
        /* FTS rebuilt: content searchable */
        sqlite3_prepare_v2(conn,
            "SELECT COUNT(*) FROM messages_fts WHERE messages_fts MATCH 'message'",
            -1, &q, NULL);
        int rc = sqlite3_step(q);
        CHECK(rc == SQLITE_ROW, "messages_fts queryable");
        sqlite3_finalize(q);
        sqlite3_close(conn);
    }

    /* 6. refuse overwrite: output already exists */
    json_t *rec2 = session_recovery_recover(source, output, NULL, 1000, NULL,
                                            NULL, false, &st, err, sizeof(err));
    CHECK(rec2 == NULL && st == SESSION_RECOVERY_SAFETY_ERROR,
          "recover refuses existing output");
    CHECK(strstr(err, "Refusing to overwrite") != NULL,
          "overwrite error message");

    /* 7. recover output == source refused */
    json_t *rec3 = session_recovery_recover(source, source, NULL, 1000, NULL,
                                            NULL, false, &st, err, sizeof(err));
    CHECK(rec3 == NULL && st == SESSION_RECOVERY_SAFETY_ERROR,
          "recover refuses output==source");

    /* 8. chunk_size <= 0 refused */
    json_t *rec4 = session_recovery_recover(source, "/tmp/never.db", NULL, 0,
                                            NULL, NULL, false, &st, err,
                                            sizeof(err));
    CHECK(rec4 == NULL && st == SESSION_RECOVERY_SAFETY_ERROR,
          "chunk_size=0 refused");

    /* 9. partial salvage: orphan message (session row missing) gets a
     * placeholder session reconstructed */
    char source2[256], output2[256];
    snprintf(source2, sizeof(source2), "%s/state2.db", dir);
    snprintf(output2, sizeof(output2), "%s/recovered2.db", dir);
    make_source_db(source2, 4);
    {
        sqlite3 *conn = NULL;
        sqlite3_open(source2, &conn);
        sqlite3_exec(conn,
            "INSERT INTO messages (session_id, role, content, timestamp) "
            "VALUES ('ghost-session', 'user', 'orphan msg', 123.0)",
            NULL, NULL, NULL);
        sqlite3_exec(conn, "DELETE FROM messages_fts", NULL, NULL, NULL);
        sqlite3_close(conn);
    }
    json_t *rec5 = session_recovery_recover(source2, output2, NULL, 1000, NULL,
                                            NULL, true, &st, err, sizeof(err));
    if (!rec5) fprintf(stderr, "salvage error: %s\n", err);
    CHECK(rec5 != NULL, "partial recover returns report");
    {
        const json_t *oc = jget(rec5, "orphan_cleanup");
        CHECK(oc && oc->type == JSON_OBJECT, "orphan_cleanup present");
        CHECK((int)json_get_num(oc, "sessions_reconstructed", -1) == 1,
              "1 ghost session reconstructed");
        CHECK((int)json_get_num(oc, "messages_retained", -1) == 1,
              "1 orphan message retained");
        CHECK(json_get_bool(rec5, "partial", false),
              "salvage flagged partial (loss detected)");
        const json_t *copy = jget(rec5, "copy");
        const json_t *m = jget(copy, "messages");
        const json_t *mode = jget(m, "mode");
        CHECK(mode && strcmp(mode->str_val, "rowid_range_salvage") == 0,
              "salvage mode used");
        CHECK((int)json_get_num(m, "copied_rows", -1) == 5,
              "salvage copied 5 messages");
        sqlite3 *conn = NULL;
        sqlite3_open(output2, &conn);
        sqlite3_stmt *q = NULL;
        sqlite3_prepare_v2(conn,
            "SELECT title FROM sessions WHERE id='ghost-session'", -1, &q, NULL);
        int rc = sqlite3_step(q);
        CHECK(rc == SQLITE_ROW &&
              strstr((const char *)sqlite3_column_text(q, 0), "[recovered 1]"),
              "placeholder session title");
        sqlite3_finalize(q);
        sqlite3_close(conn);
    }

    /* 10. write report + refuse rewrite */
    char *dest = session_recovery_write_report(report_path, rec, err, sizeof(err));
    CHECK(dest != NULL, "write_report creates file");
    free(dest);
    dest = session_recovery_write_report(report_path, rec, err, sizeof(err));
    CHECK(dest == NULL, "write_report refuses overwrite");

    json_free(rec);
    json_free(rec5);

    printf("ALL %d ASSERTIONS PASS\n", g_pass);
    char cmd[300];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    (void)!system(cmd);
    return 0;
}
