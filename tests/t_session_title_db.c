/*
 * t_session_title_db.c — behavioral test for the libdb-backed session title
 * plumbing in src/cli/port_session_title.c (port of the hermes_state.py
 * SessionDB title surface). Self-verifying; exit 0 = all assertions pass.
 *
 * Covers the Python contract:
 *   set_session_title: write, overwrite, missing-session -> False,
 *     duplicate title -> ValueError (CONFLICT), too-long -> INVALID
 *   get_session_title: present / empty->None / missing->None
 *   set_auto_title_if_empty: writes once, then predicate skips
 *   compression-ancestor transfer: title moves off hidden ancestor
 *   get_next_title_in_lineage: base, "#2", max+1
 */

#include "session_title.h"
#include "db.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

static void mk_session(db_t *db, const char *id, const char *title,
                       const char *parent, const char *end_reason) {
    db_save(db, id, "{\"messages\":[]}");
    session_meta_t meta;
    db_meta_init(&meta);
    if (title) snprintf(meta.title, sizeof(meta.title), "%s", title);
    if (parent) snprintf(meta.parent_id, sizeof(meta.parent_id), "%s", parent);
    if (end_reason) snprintf(meta.end_reason, sizeof(meta.end_reason), "%s", end_reason);
    db_save_meta(db, id, &meta);
}

int main(void) {
    char tmpl[] = "/tmp/t_session_title_XXXXXX";
    char *dir = mkdtemp(tmpl);
    if (!dir) { fprintf(stderr, "mkdtemp failed\n"); return 2; }
    db_t *db = db_open(dir, NULL);
    if (!db) { fprintf(stderr, "db_open failed\n"); return 2; }

    /* --- get on missing session -> None --- */
    CHECK(session_title_get(db, "nope") == NULL, "get missing -> NULL");

    /* --- set + get roundtrip --- */
    mk_session(db, "s1", NULL, NULL, NULL);
    CHECK(session_title_get(db, "s1") == NULL, "empty title -> NULL");
    CHECK(session_title_set(db, "s1", "  My   Title  ") == SESSION_TITLE_OK,
          "set sanitized title");
    char *t = session_title_get(db, "s1");
    CHECK(t && strcmp(t, "My Title") == 0, "get returns sanitized 'My Title'");
    free(t);

    /* --- overwrite --- */
    CHECK(session_title_set(db, "s1", "Renamed") == SESSION_TITLE_OK, "overwrite");
    t = session_title_get(db, "s1");
    CHECK(t && strcmp(t, "Renamed") == 0, "overwrite visible");
    free(t);

    /* --- missing session -> NOT_FOUND (Python returns False) --- */
    CHECK(session_title_set(db, "ghost", "x") == SESSION_TITLE_NOT_FOUND,
          "set on missing -> NOT_FOUND");

    /* --- duplicate -> CONFLICT (Python raises ValueError) --- */
    mk_session(db, "s2", NULL, NULL, NULL);
    CHECK(session_title_set(db, "s2", "Renamed") == SESSION_TITLE_CONFLICT,
          "duplicate title -> CONFLICT");

    /* --- too long -> INVALID (Python raises ValueError) --- */
    char longt[256];
    memset(longt, 'a', 150); longt[150] = '\0';
    CHECK(session_title_set(db, "s2", longt) == SESSION_TITLE_INVALID,
          "too-long title -> INVALID");

    /* --- auto-if-empty: first write lands, second skips --- */
    CHECK(session_title_set_auto_if_empty(db, "s2", "Auto Title") == SESSION_TITLE_OK,
          "auto title on empty -> OK");
    CHECK(session_title_set_auto_if_empty(db, "s2", "Other Auto") == SESSION_TITLE_SKIPPED,
          "auto title on non-empty -> SKIPPED (manual/existing wins)");
    t = session_title_get(db, "s2");
    CHECK(t && strcmp(t, "Auto Title") == 0, "first auto title retained");
    free(t);

    /* --- compression-ancestor transfer --- */
    mk_session(db, "old", "Carried Title", NULL, "compression");
    mk_session(db, "cont", NULL, "old", NULL);
    CHECK(session_title_set(db, "cont", "Carried Title") == SESSION_TITLE_OK,
          "title transfers from compression ancestor");
    t = session_title_get(db, "old");
    CHECK(t == NULL, "ancestor title cleared after transfer");
    t = session_title_get(db, "cont");
    CHECK(t && strcmp(t, "Carried Title") == 0, "continuation holds title");
    free(t);

    /* --- non-ancestor duplicate still conflicts --- */
    mk_session(db, "s3", NULL, NULL, NULL);
    CHECK(session_title_set(db, "s3", "Carried Title") == SESSION_TITLE_CONFLICT,
          "unrelated duplicate still CONFLICT");

    /* --- next-in-lineage --- */
    char *n = session_title_next_in_lineage(db, "Fresh Name");
    CHECK(n && strcmp(n, "Fresh Name") == 0, "lineage: unused base -> base");
    free(n);
    n = session_title_next_in_lineage(db, "Carried Title");
    CHECK(n && strcmp(n, "Carried Title #2") == 0, "lineage: used base -> #2");
    free(n);
    CHECK(session_title_set(db, "s3", "Carried Title #2") == SESSION_TITLE_OK,
          "set deduped #2 title");
    n = session_title_next_in_lineage(db, "Carried Title");
    CHECK(n && strcmp(n, "Carried Title #3") == 0, "lineage: max+1 -> #3");
    free(n);
    n = session_title_next_in_lineage(db, "Carried Title #2");
    CHECK(n && strcmp(n, "Carried Title #3") == 0, "lineage: suffixed input -> #3");
    free(n);

    db_close(db);
    printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
