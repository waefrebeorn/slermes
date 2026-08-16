/*
 * ports/gateway/code_skew.c — C11 port of gateway/code_skew.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/gateway/port_code_skew_ports.c ── */
/*
 * port_code_skew_remaining.c — Port of gateway/code_skew.py drift surface.
 * REAL git-rev fingerprints, boot snapshot, skew detection.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: csk_fingerprint @ gateway/code_skew.py:_fingerprint */
char *csk_fingerprint(const char *repo_dir) {
    /* Python: git:<ref>:<sha> — REAL git rev-parse. */
    if (!repo_dir) return NULL;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "git -C %s rev-parse --short HEAD 2>/dev/null", repo_dir);
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;
    char sha[128] = "";
    if (fgets(sha, sizeof(sha), f)) {
        size_t n = strlen(sha);
        while (n && (sha[n-1] == '\n' || sha[n-1] == ' ')) sha[--n] = '\0';
    }
    pclose(f);
    char ref[256] = "HEAD";
    snprintf(cmd, sizeof(cmd), "git -C %s rev-parse --abbrev-ref HEAD 2>/dev/null", repo_dir);
    f = popen(cmd, "r");
    if (f) {
        if (fgets(ref, sizeof(ref), f)) {
            size_t n = strlen(ref);
            while (n && (ref[n-1] == '\n' || ref[n-1] == ' ')) ref[--n] = '\0';
        }
        pclose(f);
    }
    if (!*sha) return NULL;
    char *out = NULL;
    asprintf(&out, "git:%s:%s", ref, sha);
    return out;
}

/* PoP: csk_record_boot_fingerprint @ gateway/code_skew.py:record_boot_fingerprint */
char *csk_record_boot_fingerprint(const char *repo_dir) {
    /* Python: snapshot at startup, idempotent. */
    char *fp = csk_fingerprint(repo_dir);
    printf("boot fingerprint recorded (%s)\n", fp ? fp : "none");
    return fp ? fp : strdup("");
}

/* PoP: csk_short @ gateway/code_skew.py:_short */
char *csk_short(const char *fingerprint) {
    /* Python: git:<ref>:<sha> → compact label. */
    if (!fingerprint) return strdup("");
    const char *p = strstr(fingerprint, ":");
    if (!p) return strdup(fingerprint);
    p = strchr(p + 1, ':');
    if (!p) return strdup(fingerprint);
    return strdup(p + 1);
}

/* PoP: csk_detect_code_skew @ gateway/code_skew.py:detect_code_skew */
char *csk_detect_code_skew(const char *boot_fp, const char *repo_dir) {
    /* Python: drift since boot. */
    if (!boot_fp || !*boot_fp || !repo_dir) return NULL;
    char *disk = csk_fingerprint(repo_dir);
    if (!disk) return NULL;
    char *b = csk_short(boot_fp);
    char *d = csk_short(disk);
    char *out = NULL;
    if (strcmp(b, d) != 0)
        asprintf(&out, "{\"boot\": \"%s\", \"disk\": \"%s\", \"drifted\": true}", b, d);
    else
        asprintf(&out, "{\"boot\": \"%s\", \"disk\": \"%s\", \"drifted\": false}", b, d);
    free(disk);
    free(b);
    free(d);
    return out;
}


/* ── from src/gateway/port_gateway_code_skew.c ── */
/*
 * port_gateway_code_skew.c — Port of Python gateway/code_skew.py
 *
 /*
  * port_gateway_code_skew.c — Port of Python gateway/code_skew.py
  *
  * Detect when the gateway is running stale code after a hot git pull.
  */

 #include "hermes_gateway_core.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <stdbool.h>
 #include <pthread.h>
 #include <unistd.h>

 static char _boot_fingerprint[256] = {0};
 static pthread_mutex_t _boot_fingerprint_mutex = PTHREAD_MUTEX_INITIALIZER;

 /* Helper: read git revision fingerprint from .git directory */
 static const char *read_git_revision_fingerprint(const char *project_root) {
     if (!project_root) return NULL;
    
     static char fingerprint[256];
     char git_dir[512];
     char head_file[512];
    
     snprintf(git_dir, sizeof(git_dir), "%s/.git", project_root);
     if (access(git_dir, F_OK) != 0) {
         return NULL;
     }
    
     /* Try to read HEAD reference */
     snprintf(head_file, sizeof(head_file), "%s/HEAD", git_dir);
     FILE *f = fopen(head_file, "r");
     if (!f) return NULL;
    
     char line[256];
     if (fgets(line, sizeof(line), f)) {
         fclose(f);
        
         /* Parse HEAD - could be a ref or direct SHA */
         if (strncmp(line, "ref: ", 5) == 0) {
             char *ref = line + 5;
             char *newline = strchr(ref, '\n');
             if (newline) *newline = '\0';
             char *ref_end = strchr(ref, '\r');
             if (ref_end) *ref_end = '\0';
            
             char ref_file[512];
             snprintf(ref_file, sizeof(ref_file), "%s/%s", git_dir, ref);
             FILE *rf = fopen(ref_file, "r");
             if (rf) {
                 char sha[64];
                 if (fgets(sha, sizeof(sha), rf)) {
                     fclose(rf);
                     char *sha_newline = strchr(sha, '\n');
                     if (sha_newline) *sha_newline = '\0';
                     snprintf(fingerprint, sizeof(fingerprint), "git:%s:%s", ref, sha);
                     return fingerprint;
                 }
                 fclose(rf);
             }
             snprintf(fingerprint, sizeof(fingerprint), "git:%s:unresolved", ref);
             return fingerprint;
         } else {
             /* Direct SHA in HEAD */
             char *newline = strchr(line, '\n');
             if (newline) *newline = '\0';
             char *sha_end = strchr(line, '\r');
             if (sha_end) *sha_end = '\0';
             snprintf(fingerprint, sizeof(fingerprint), "git:detached:%s", line);
             return fingerprint;
         }
     }
     fclose(f);
     return NULL;
 }

 /* Port of Python: _fingerprint */
 const char *code_skew_fingerprint(void) {
     /* Use the project root - in slermes this is the slermes directory */
     const char *project_root = "/home/wubu/hermes-agent-dev/slermes";
    
     const char *fp = read_git_revision_fingerprint(project_root);
     if (fp) {
         return fp;
     }
     return NULL;
 }

 /* Port of Python: record_boot_fingerprint */
 void code_skew_record_boot_fingerprint(void) {
     pthread_mutex_lock(&_boot_fingerprint_mutex);
     if (_boot_fingerprint[0] == '\0') {
         const char *fp = code_skew_fingerprint();
         if (fp) {
             snprintf(_boot_fingerprint, sizeof(_boot_fingerprint), "%s", fp);
         }
     }
     pthread_mutex_unlock(&_boot_fingerprint_mutex);
 }

 /* Port of Python: _short */
 static const char *code_skew_short(const char *fingerprint) {
     if (!fingerprint) return "unknown";
    
     const char *sha = strrchr(fingerprint, ':');
     if (sha) sha++;
     else sha = fingerprint;
    
     if (sha && strcmp(sha, "unresolved") != 0 && strlen(sha) > 10) {
         static char short_sha[16];
         strncpy(short_sha, sha, 10);
         short_sha[10] = '\0';
         return short_sha;
     }
     return sha;
 }

 /* Port of Python: detect_code_skew */
 bool code_skew_detect_code_skew(char *boot_rev_out, size_t boot_sz, char *disk_rev_out, size_t disk_sz) {
     pthread_mutex_lock(&_boot_fingerprint_mutex);
    
     if (_boot_fingerprint[0] == '\0') {
         pthread_mutex_unlock(&_boot_fingerprint_mutex);
         return false;
     }
    
     const char *current = code_skew_fingerprint();
     if (!current || strcmp(current, _boot_fingerprint) == 0) {
         pthread_mutex_unlock(&_boot_fingerprint_mutex);
         return false;
     }
    
     const char *boot_short = code_skew_short(_boot_fingerprint);
     const char *disk_short = code_skew_short(current);
    
     if (boot_rev_out && boot_sz > 0) {
         snprintf(boot_rev_out, boot_sz, "%s", boot_short);
     }
     if (disk_rev_out && disk_sz > 0) {
         snprintf(disk_rev_out, disk_sz, "%s", disk_short);
     }
    
     pthread_mutex_unlock(&_boot_fingerprint_mutex);
     return true;
 }
