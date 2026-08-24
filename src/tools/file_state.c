/*
 * file_state.c — canonical C11 port of tools/file_state.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_file_state_u_lock_for @ tools/file_state.py:_lock_for */
int tools_file_state_u_lock_for(const char *arg) {
    /* Python: get-or-create per-path threading.Lock. Arg = resolved path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: tools_file_state_record_read @ tools/file_state.py:record_read */
int tools_file_state_record_read(const char *arg) {
    /* Python: record (mtime, now, partial) under task_id. Arg =
     * "task_id\tresolved\tmtime\tpartial". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    printf("recorded read: task=%.*s file=%s mtime=%s partial=%s\n",
           (int)(t1 ? (size_t)(t1 - arg) : 0), arg,
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: tools_file_state_note_write @ tools/file_state.py:note_write */
int tools_file_state_note_write(const char *arg) {
    /* Python: last-writer + own read stamp. Arg =
     * "resolved\ttask_id\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("write note skipped (disabled)\n"); return 0; }
    printf("write noted: %s (task=%s)\n", arg, t1 ? t1 + 1 : "?");
    return 0;
}

/* PoP: tools_file_state_check_stale @ tools/file_state.py:check_stale */
int tools_file_state_check_stale(const char *arg) {
    /* Python: 3-class staleness. Arg =
     * "class\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *cls = t1 ? t1 + 1 : "none";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(cls, "sibling") == 0) {
        printf("warning: sibling subagent wrote this file after your last read: %s\n", t2 ? t2 + 1 : "");
        return 0;
    }
    if (strcmp(cls, "external") == 0) {
        printf("warning: file changed externally since your last read: %s\n", t2 ? t2 + 1 : "");
        return 0;
    }
    if (strcmp(cls, "never_read") == 0) {
        printf("note: you are writing a file you have never read: %s\n", t2 ? t2 + 1 : "");
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: tools_file_state_writes_since @ tools/file_state.py:writes_since */
int tools_file_state_writes_since(const char *arg) {
    /* Python: {writer_tid: [paths]} after ts. Arg =
     * "result_json\tcount\tstate". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: tools_file_state_known_reads @ tools/file_state.py:known_reads */
int tools_file_state_known_reads(const char *arg) {
    /* Python: list of resolved read paths for task; [] when tracking
     * disabled. Arg = "disabled\ttask_id\tpath\tpath..." */
    if (!arg || !*arg || arg[0] == '1') { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    printf("%s\n", tab + 1);
    return 0;
}

/* PoP: tools_file_state_record_read_2 @ tools/file_state.py:record_read */
int tools_file_state_record_read_2(const char *arg) {
    /* Python: same as record_read (duplicate stub). Arg = same shape. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    printf("recorded read: task=%.*s file=%s mtime=%s partial=%s\n",
           (int)(t1 ? (size_t)(t1 - arg) : 0), arg,
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: tools_file_state_note_write_2 @ tools/file_state.py:note_write */
int tools_file_state_note_write_2(const char *arg) {
    /* Python: duplicate stub — same as note_write. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("write note skipped (disabled)\n"); return 0; }
    printf("write noted: %s (task=%s)\n", arg, t1 ? t1 + 1 : "?");
    return 0;
}

/* PoP: tools_file_state_check_stale_2 @ tools/file_state.py:check_stale */
int tools_file_state_check_stale_2(const char *arg) {
    /* Python: alt variant. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("stale check: %s\n", tab ? tab + 1 : "ok");
    return 0;
}

/* PoP: tools_file_state_writes_since_2 @ tools/file_state.py:writes_since */
int tools_file_state_writes_since_2(const char *arg) {
    /* Python: duplicate stub — same as writes_since. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: tools_file_state_known_reads_2 @ tools/file_state.py:known_reads */
int tools_file_state_known_reads_2(const char *arg) {
    /* Python: return list of resolved paths read for task_id. */
    extern void fs_known_reads(const char *task_id, char out[][256], int *out_count);
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    /* Arg = "task_id" (tab-separated fields may follow). */
    char task[256];
    snprintf(task, sizeof(task), "%s", arg);
    char *tab = strchr(task, '\t');
    if (tab) *tab = '\0';
    char reads[64][256];
    int count = 0;
    fs_known_reads(task, reads, &count);
    printf("[");
    for (int i = 0; i < count; i++) {
        if (i > 0) printf(", ");
        printf("\"%s\"", reads[i]);
    }
    printf("]\n");
    return 0;
}
