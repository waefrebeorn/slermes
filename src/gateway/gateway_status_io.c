/*
 * gateway_status_io.c - status file / record IO helpers.
 *
 * Split from status.c (soul.md: self-contained modules). JSON file read /
 * write, pid + lock record handling and process-start-time lookup.
 * No file-scope state.
 */
#include "gateway_status_io.h"
#include "slermes_home.h"
#include "json.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>   /* flock */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>



/* PoP: gwstatus_get_process_start_time @ gateway/status.py:_get_process_start_time */
/* PoP: gwstatus_get_process_start_time @ gateway/status.py:get_process_start_time */
/* Python's public get_process_start_time is a thin wrapper over the private
 * _get_process_start_time; both collapse to this one C entry point.
 * Linux: field 22 of /proc/<pid>/stat (clock ticks since boot). The comm
 * field (2) can contain spaces/parens, so scan from the closing ')'. */
long gwstatus_get_process_start_time(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char buf[4096];
    size_t got = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (got == 0) return -1;
    buf[got] = '\0';
    /* comm is enclosed in parens and may contain spaces/parens; the last ')'
     * ends field 2. Fields after it are space-separated; start_time is
     * field 22, i.e. the 20th token after the closing paren. */
    char *rparen = strrchr(buf, ')');
    if (!rparen) return -1;
    char *p = rparen + 1;
    /* Token indices after the paren: field 3 = state, ... field 22 = start.
     * That's the 20th whitespace-delimited token starting at field 3. */
    int field = 2; /* we are positioned right after field 2 (comm) */
    char *tok = strtok(p, " \t\n");
    while (tok) {
        field++;
        if (field == 22) {
            errno = 0;
            long v = strtol(tok, NULL, 10);
            if (errno) return -1;
            return v;
        }
        tok = strtok(NULL, " \t\n");
    }
    return -1;
}


/* ── JSON record helpers ─────────────────────────────────────────────── */

/* Read + parse a JSON object file. Returns json_t* (caller json_free) or NULL. */ json_t *read_json_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    /* strip */
    char *s = buf;
    while (*s == ' ' || *s == '\n' || *s == '\t' || *s == '\r') s++;
    if (!*s) { free(buf); return NULL; }
    json_t *node = json_parse(s, NULL);
    free(buf);
    if (!node) return NULL;
    if (node->type != JSON_OBJECT) { json_free(node); return NULL; }
    return node;
}


/* PoP: write_json_file @ gateway/status.py:_write_json_file */ int write_json_file(const char *path, json_t *payload) {
    ensure_parent_dir(path);
    char *ser = json_serialize(payload);
    if (!ser) return -1;
    char tmp[1300];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(ser); return -1; }
    size_t len = strlen(ser);
    size_t wrote = fwrite(ser, 1, len, f);
    int fd = fileno(f);
    if (fd >= 0) fsync(fd);
    fclose(f);
    free(ser);
    if (wrote != len) { unlink(tmp); return -1; }
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}


/* PoP: build_pid_record @ gateway/status.py:_build_pid_record */ json_t *build_pid_record(void) {
    json_t *o = json_object();
    json_set(o, "pid", json_number((double)getpid()));
    json_set(o, "kind", json_string(GATEWAY_KIND));
    /* argv: read from /proc/self/cmdline for faithful parity with sys.argv. */
    json_t *argv = json_array();
    char *cmd = read_process_cmdline(getpid());
    if (cmd) {
        char *toks[256];
        int n = split_command(cmd, toks, 256);
        for (int i = 0; i < n; i++) json_append(argv, json_string(toks[i]));
        free_tokens(toks, n);
        free(cmd);
    }
    json_set(o, "argv", argv);
    long st = gwstatus_get_process_start_time(getpid());
    if (st >= 0) json_set(o, "start_time", json_number((double)st));
    else json_set(o, "start_time", json_null());
    return o;
}


/* PoP: pid_from_record @ gateway/status.py:_pid_from_record */ pid_t pid_from_record(json_t *record) {
    if (!record || record->type != JSON_OBJECT) return -1;
    json_t *p = json_obj_get(record, "pid");
    if (!p) return -1;
    if (p->type == JSON_NUMBER) return (pid_t)p->num_val;
    if (p->type == JSON_STRING) {
        char *end = NULL;
        long v = strtol(p->str_val, &end, 10);
        if (end && end != p->str_val) return (pid_t)v;
    }
    return -1;
}


/* PoP: read_pid_record @ gateway/status.py:_read_pid_record */ json_t *read_pid_record(const char *pid_path) {
    char defbuf[1200];
    if (!pid_path) pid_path = get_pid_path(defbuf, sizeof(defbuf));
    FILE *f = fopen(pid_path, "rb");
    if (!f) return NULL;
    char raw[8192];
    size_t got = fread(raw, 1, sizeof(raw) - 1, f);
    fclose(f);
    if (got == 0) return NULL;
    raw[got] = '\0';
    char *s = raw;
    while (*s == ' ' || *s == '\n' || *s == '\t' || *s == '\r') s++;
    size_t L = strlen(s);
    while (L > 0 && (s[L-1] == ' ' || s[L-1] == '\n' || s[L-1] == '\t' || s[L-1] == '\r')) s[--L] = '\0';
    if (!*s) return NULL;
    json_t *node = json_parse(s, NULL);
    if (node) {
        if (node->type == JSON_OBJECT) return node;
        if (node->type == JSON_NUMBER) {
            json_t *o = json_object();
            json_set(o, "pid", json_number(node->num_val));
            json_free(node);
            return o;
        }
        json_free(node);
        return NULL;
    }
    /* Not JSON: try bare integer. */
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end && end != s) {
        json_t *o = json_object();
        json_set(o, "pid", json_number((double)v));
        return o;
    }
    return NULL;
}


/* -----------------------------------------------------------------------
 * PoP: read_gateway_lock_record @ gateway/status.py:_read_gateway_lock_record
 * The gateway lock file carries the same record shape as the PID file.
 * --------------------------------------------------------------------- */ json_t *read_gateway_lock_record(const char *lock_path) {
    char buf[1200];
    const char *path = lock_path;
    if (!path) { get_gateway_lock_path(NULL, buf, sizeof(buf)); path = buf; }
    return read_pid_record(path);
}


/* -----------------------------------------------------------------------
 * PoP: write_gateway_lock_record @ gateway/status.py:_write_gateway_lock_record
 * Rewrite an open lock fd from offset 0 with the current PID record + fsync.
 * --------------------------------------------------------------------- */ void write_gateway_lock_record(int fd) {
    if (fd < 0) return;
    json_t *record = build_pid_record();
    char *ser = json_serialize(record);
    json_free(record);
    if (!ser) return;
    if (lseek(fd, 0, SEEK_SET) == 0 && ftruncate(fd, 0) == 0) {
        size_t len = strlen(ser);
        ssize_t w = write(fd, ser, len);
        (void)w;
        fsync(fd);
    }
    free(ser);
}


/* -----------------------------------------------------------------------
 * PoP: cleanup_invalid_pid_path @ gateway/status.py:_cleanup_invalid_pid_path
 * Force-unlink a stale PID file and its sibling lock metadata (called after
 * the runtime lock is confirmed inactive, so the metadata is known dead).
 * --------------------------------------------------------------------- */ void cleanup_invalid_pid_path(const char *pid_path, bool cleanup_stale) {
    if (!cleanup_stale || !pid_path) return;
    unlink(pid_path);
    char lock[1200];
    get_gateway_lock_path(pid_path, lock, sizeof(lock));
    unlink(lock);
}


/* Is /proc/<pid> in a stopped state (T/t)? */ bool proc_is_stopped(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    char line[256];
    bool stopped = false;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "State:", 6) == 0) {
            /* State:\t<char> ... */
            char *p = line + 6;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == 'T' || *p == 't') stopped = true;
            break;
        }
    }
    fclose(f);
    return stopped;
}


/* Long value or LONG_MIN if key absent/null. */ long record_start_time(json_t *rec) {
    json_t *st = json_obj_get(rec, "start_time");
    if (!st || st->type != JSON_NUMBER) return LONG_MIN;
    return (long)st->num_val;
}
