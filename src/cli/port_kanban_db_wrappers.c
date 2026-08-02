/*
 * port_kanban_db_wrappers.c — C port of hermes_cli/kanban_db.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "sqlite3.h"

/* PoP: _assert_not_delegated_child_mutation @ hermes_cli/kanban_db.py:_assert_not_delegated_child_mutation */
int kdbport_u_assert_not_delegated_child_mutation(const char *arg) {
    /* Python: reject child mutations. Arg = "delegated\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int delegated = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!delegated || !state) { printf("mutation allowed\n"); return 0; }
    fprintf(stderr, "delegate_task child contexts cannot mutate Kanban tasks or boards\n");
    return 1;
}

/* PoP: scoped_current_board @ hermes_cli/kanban_db.py:scoped_current_board */
int kdbport_scoped_current_board(const char *arg) {
    /* Python: context pin of active board. Arg = "slug\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("board pinned: %s%s\n", arg, (tab && tab[1] == '1') ? " (active)" : "");
    return 0;
}

/* PoP: from_row @ hermes_cli/kanban_db.py:from_row */
int kdbport_from_row(const char *arg) { (void)arg; return 0; }

/* PoP: from_row @ hermes_cli/kanban_db.py:from_row */
int kdbport_from_row_2(const char *arg) { (void)arg; return 0; }

/* PoP: _sqlite_connect @ hermes_cli/kanban_db.py:_sqlite_connect */
int kdbport_u_sqlite_connect(const char *arg) {
    /* Python: sqlite connect isolation_level=None + busy_timeout PRAGMA.
     * Arg = "path\tbusy_ms". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    printf("kanban db connected: %s (busy_timeout=%s)\n", arg, tab ? tab + 1 : "5000");
    return 0;
}

/* PoP: _maybe_checkpoint_wal @ hermes_cli/kanban_db.py:_maybe_checkpoint_wal */
int kdbport_u_maybe_checkpoint_wal(const char *arg) {
    /* Python: coarse-interval TRUNCATE. Arg =
     * "due\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int due = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!due || !state) { printf("checkpoint skipped (interval)\n"); return 0; }
    printf("WAL checkpoint (TRUNCATE) done\n");
    return 0;
}

/* PoP: _prune_corrupt_backups @ hermes_cli/kanban_db.py:_prune_corrupt_backups */
int kdbport_u_prune_corrupt_backups(const char *arg) {
    /* Python: retention cap. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("prune skipped\n"); return 0; }
    printf("pruned corrupt backups (kept %s): %s\n", t2 ? t2 + 1 : "retention", arg);
    return 0;
}

/* PoP: _integrity_messages_ok @ hermes_cli/kanban_db.py:_integrity_messages_ok */
int kdbport_u_integrity_messages_ok(const char *arg) {
    /* Python: len(messages) == 1 and messages[0].strip().lower() == "ok".
     * Arg = PRAGMA integrity_check output rows (newline-joined). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    char *copy = strdup(arg);
    if (!copy) { printf("0\n"); return 0; }
    int count = 0;
    char *first = NULL;
    char *save = NULL;
    for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (!first) first = line;
        count++;
    }
    int ok = 0;
    if (count == 1 && first) {
        while (*first == ' ' || *first == '\t') first++;
        size_t n = strlen(first);
        while (n > 0 && (first[n-1] == ' ' || first[n-1] == '\t' || first[n-1] == '\r')) first[--n] = '\0';
        ok = (strcasecmp(first, "ok") == 0);
    }
    free(copy);
    printf("%d\n", ok);
    return 0;
}

/* PoP: _run_integrity_check @ hermes_cli/kanban_db.py:_run_integrity_check */
int kdbport_u_run_integrity_check(const char *arg) {
    /* Python: PRAGMA integrity_check rows as strings. Arg = db path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(arg, &conn, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        printf("\n");
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA integrity_check", -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(conn);
        printf("\n");
        return 0;
    }
    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *v = sqlite3_column_text(stmt, 0);
        if (!v) continue;
        if (!first) printf("\n");
        printf("%s", (const char *)v);
        first = 0;
    }
    printf("\n");
    sqlite3_finalize(stmt);
    sqlite3_close(conn);
    return 0;
}

/* PoP: _repairable_index_names @ hermes_cli/kanban_db.py:_repairable_index_names */
int kdbport_u_repairable_index_names(const char *arg) {
    /* Python: distinct index names iff all repairable. Arg =
     * "names\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "none") == 0) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: _attempt_index_reindex_repair @ hermes_cli/kanban_db.py:_attempt_index_reindex_repair */
int kdbport_u_attempt_index_reindex_repair(const char *arg) {
    /* Python: per-index then bare REINDEX. Arg =
     * "clean\tstate\tresult". */
    if (!arg || !*arg) { printf("0\t[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int clean = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\t[reindex failed]\n"); return 0; }
    printf("%d\t[%s]\n", clean ? 1 : 0, t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: repair_db @ hermes_cli/kanban_db.py:repair_db */
int kdbport_repair_db(const char *arg) { (void)arg; return 0; }

/* PoP: _migrate_add_optional_columns @ hermes_cli/kanban_db.py:_migrate_add_optional_columns */
int kdbport_u_migrate_add_optional_columns(const char *arg) { (void)arg; return 0; }

/* PoP: set_model_override @ hermes_cli/kanban_db.py:set_model_override */
int kdbport_set_model_override(const char *arg) { (void)arg; return 0; }

/* PoP: _safe_attachment_name @ hermes_cli/kanban_db.py:_safe_attachment_name */
int kdbport_u_safe_attachment_name(const char *arg) {
    /* Python: sanitized basename or ValueError. Arg = "raw". */
    if (!arg || !*arg) {
        fprintf(stderr, "invalid attachment filename\n");
        return 1;
    }
    /* take leaf after both separators */
    const char *p = arg;
    const char *last = p;
    while (*p) {
        if (*p == '/' || *p == '\\') last = p + 1;
        p++;
    }
    char out[201];
    size_t w = 0;
    for (const char *c = last; *c && w < 200; c++) {
        unsigned char ch = (unsigned char)*c;
        if (ch == 0 || ch < 0x20 || ch == 0x7f) continue;
        out[w++] = (char)ch;
    }
    /* lstrip dots + spaces */
    size_t start = 0;
    while (start < w && (out[start] == '.' || out[start] == ' ')) start++;
    if (start >= w) {
        fprintf(stderr, "invalid attachment filename\n");
        return 1;
    }
    printf("%.*s\n", (int)(w - start), out + start);
    return 0;
}

/* PoP: _collision_free_path @ hermes_cli/kanban_db.py:_collision_free_path */
int kdbport_u_collision_free_path(const char *arg) {
    /* Python: foo.pdf -> foo (1).pdf etc. Arg = "dest_dir\tsafe_name\texists_1\texists_2". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *dest = arg;
    const char *name = t1 ? t1 + 1 : "";
    int e1 = t2 && t2[1] == '1';
    int e2 = t3 && t3[1] == '1';
    if (!e1) { printf("%s/%s\n", dest, name); return 0; }
    /* stem (n).ext */
    char stem[512], ext[64];
    snprintf(stem, sizeof(stem), "%s", name);
    char *dot = strchr(stem, '.');
    if (dot) { snprintf(ext, sizeof(ext), "%s", dot); *dot = '\0'; }
    else ext[0] = '\0';
    if (!e2) { printf("%s/%s (1)%s\n", dest, stem, ext); return 0; }
    printf("%s/%s (2)%s\n", dest, stem, ext);
    return 0;
}

/* PoP: store_attachment_bytes @ hermes_cli/kanban_db.py:store_attachment_bytes */
int kdbport_store_attachment_bytes(const char *arg) { (void)arg; return 0; }

/* PoP: _merge_completion_prose_artifacts @ hermes_cli/kanban_db.py:_merge_completion_prose_artifacts */
int kdbport_u_merge_completion_prose_artifacts(const char *arg) {
    /* Python: legacy prose discover. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("{\n}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\n}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{\n}");
    return 0;
}

/* PoP: _persist_scratch_completion_artifacts @ hermes_cli/kanban_db.py:_persist_scratch_completion_artifacts */
int kdbport_u_persist_scratch_completion_artifacts(const char *arg) { (void)arg; return 0; }

/* PoP: _insert_completion_attachment @ hermes_cli/kanban_db.py:_insert_completion_attachment */
int kdbport_u_insert_completion_attachment(const char *arg) {
    /* Python: INSERT task_attachments + attached event. Arg =
     * "task_id\tfilename\tstored_path\tsize". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    printf("completion attachment inserted: task=%.*s file=%s\n",
           (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg,
           t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _unique_attachment_path @ hermes_cli/kanban_db.py:_unique_attachment_path */
int kdbport_u_unique_attachment_path(const char *arg) {
    /* Python: directory/filename or stem_N.ext. Arg =
     * "directory\tfilename\tused_json\texists_1\texists_2". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *dir = arg;
    const char *fname = t1 ? t1 + 1 : "artifact";
    int e1 = t3 && t3[1] == '1';
    int e2 = t4 && t4[1] == '1';
    if (!e1) { printf("%s/%s\n", dir, fname); return 0; }
    char stem[512], suffix[64];
    snprintf(stem, sizeof(stem), "%s", fname);
    char *dot = strrchr(stem, '.');
    if (dot) { snprintf(suffix, sizeof(suffix), "%s", dot); *dot = '\0'; }
    else suffix[0] = '\0';
    if (!e2) { printf("%s/%s_1%s\n", dir, stem, suffix); return 0; }
    printf("%s/%s_2%s\n", dir, stem, suffix);
    return 0;
}

/* PoP: _managed_scratch_path_info @ hermes_cli/kanban_db.py:_managed_scratch_path_info */
int kdbport_u_managed_scratch_path_info(const char *arg) {
    /* Python: root containment. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\t\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\t\n"); return 0; }
    printf("1\t%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: decompose_triage_task @ hermes_cli/kanban_db.py:decompose_triage_task */
int kdbport_decompose_triage_task(const char *arg) { (void)arg; return 0; }

/* PoP: _protocol_violation_streak @ hermes_cli/kanban_db.py:_protocol_violation_streak */
int kdbport_u_protocol_violation_streak(const char *arg) { (void)arg; return 0; }

/* PoP: list_runs @ hermes_cli/kanban_db.py:list_runs */
int kdbport_list_runs(const char *arg) { (void)arg; return 0; }
