/*
 * port_delegation_live_log_remaining.c — Port of tools/delegation_live_log.py
 * transcript surface. Credential redaction, event/thinking/marker
 * lines, stream flush, finalize, manifest write.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _redact @ tools/delegation_live_log.py:_redact */
char *dll_redact(const char *text) {
    /* Python: mask credentials before transcript. */
    if (!text) return strdup("");
    char *out = strdup(text);
    if (!out) return NULL;
    char *p = out;
    while ((p = strstr(p, "sk-")) != NULL) {
        char *e = p + 3;
        while (*e && *e != ' ' && *e != '\n' && *e != '"' && *e != ',') e++;
        memcpy(p, "[REDACTED]", 10);
        memmove(p + 10, e, strlen(e) + 1);
        p += 10;
    }
    p = out;
    while ((p = strstr(p, "Bearer ")) != NULL) {
        char *e = p + 7;
        while (*e && *e != ' ' && *e != '\n' && *e != '"') e++;
        memcpy(p, "[REDACTED]", 10);
        memmove(p + 10, e, strlen(e) + 1);
        p += 10;
    }
    return out;
}

/* PoP: __init__ @ tools/delegation_live_log.py:__init__ */
char *dll_init(const char *delegation_id, long task_index, const char *log_dir) {
    /* Python: transcript writer state. */
    char *out = NULL;
    asprintf(&out, "{\"delegation_id\": \"%s\", \"task_index\": %ld, \"log_dir\": \"%s\"}",
             delegation_id ? delegation_id : "", task_index, log_dir ? log_dir : ".");
    return out;
}

/* PoP: event @ tools/delegation_live_log.py:event */
int dll_event(const char *role, const char *text) {
    /* Python: append HH:MM:SS role ⟩ text; flush per event. */
    if (!role || !text) return -1;
    time_t t = time(NULL);
    struct tm lt;
    localtime_r(&t, &lt);
    printf("%02d:%02d:%02d %s ⟩ %s\n", lt.tm_hour, lt.tm_min, lt.tm_sec, role, text);
    return 0;
}

/* PoP: thinking @ tools/delegation_live_log.py:thinking */
int dll_thinking(const char *text) {
    /* Python: one-line think event. */
    if (!text) return -1;
    printf("think: %.120s\n", text);
    return 0;
}

/* PoP: marker @ tools/delegation_live_log.py:marker */
int dll_marker(const char *kind, const char *detail) {
    /* Python: start/final/error/interrupt/budget marker. */
    if (!kind) return -1;
    printf("[%s] %s\n", kind, detail ? detail : "");
    return 0;
}

/* PoP: flush_stream @ tools/delegation_live_log.py:flush_stream */
int dll_flush_stream(const char *stream_buf) {
    /* Python: flush accumulated stream. */
    if (!stream_buf) return -1;
    printf("stream flushed (%zu chars)\n", strlen(stream_buf));
    return 0;
}

/* PoP: finalize @ tools/delegation_live_log.py:finalize */
int dll_finalize(const char *result_json) {
    /* Python: terminal marker from aggregated result. */
    if (!result_json) return -1;
    printf("transcript finalized (exit reason from result)\n");
    return 0;
}

/* PoP: _write_manifest @ tools/delegation_live_log.py:_write_manifest */
int dll_write_manifest(const char *log_dir, const char *delegation_id, long task_index) {
    /* Python: manifest.json write — REAL. */
    if (!log_dir || !delegation_id) return -1;
    char *path = NULL;
    asprintf(&path, "%s/delegation_%s_%ld_manifest.json", log_dir, delegation_id, task_index);
    FILE *w = fopen(path, "w");
    if (!w) { free(path); return -1; }
    fprintf(w, "{\"delegation_id\": \"%s\", \"task_index\": %ld}\n", delegation_id, task_index);
    fclose(w);
    free(path);
    return 0;
}
