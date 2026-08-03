/*
 * port_code_skew_remaining.c — Port of gateway/code_skew.py drift surface.
 * REAL git-rev fingerprints, boot snapshot, skew detection.
 */
#define _POSIX_C_SOURCE 200809L
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

/* PoP: _fingerprint @ gateway/code_skew.py:_fingerprint */
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

/* PoP: record_boot_fingerprint @ gateway/code_skew.py:record_boot_fingerprint */
char *csk_record_boot_fingerprint(const char *repo_dir) {
    /* Python: snapshot at startup, idempotent. */
    char *fp = csk_fingerprint(repo_dir);
    printf("boot fingerprint recorded (%s)\n", fp ? fp : "none");
    return fp ? fp : strdup("");
}

/* PoP: _short @ gateway/code_skew.py:_short */
char *csk_short(const char *fingerprint) {
    /* Python: git:<ref>:<sha> → compact label. */
    if (!fingerprint) return strdup("");
    const char *p = strstr(fingerprint, ":");
    if (!p) return strdup(fingerprint);
    p = strchr(p + 1, ':');
    if (!p) return strdup(fingerprint);
    return strdup(p + 1);
}

/* PoP: detect_code_skew @ gateway/code_skew.py:detect_code_skew */
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
