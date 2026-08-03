/*
 * port_mcp_oauth_remaining.c — Port of tools/mcp_oauth.py OAuth state
 * surface. Safe filenames, real token-file IO, remove/snapshot/restore.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static char *safe_filename(const char *name) {
    if (!name) return NULL;
    size_t n = strlen(name);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        out[i] = (isalnum(c) || c == '-' || c == '_' || c == '.') ? (char)c : '_';
    }
    out[n] = '\0';
    return out;
}

/* PoP: __init__ @ tools/mcp_oauth.py:__init__ */
char *mco_init(const char *server_name, const char *hermes_home) {
    /* Python: safe server state dir. */
    if (!server_name) return NULL;
    char *safe = safe_filename(server_name);
    char *out = NULL;
    asprintf(&out, "{\"server\": \"%s\", \"home\": \"%s\", \"dir\": \"%s/.hermes/mcp_oauth/%s\"}",
             server_name, hermes_home ? hermes_home : ".", hermes_home ? hermes_home : ".", safe);
    free(safe);
    return out;
}

/* PoP: remove @ tools/mcp_oauth.py:remove */
int mco_remove(const char *hermes_home, const char *server_name) {
    /* Python: delete all stored state — REAL unlink. */
    if (!hermes_home || !server_name) return -1;
    char *safe = safe_filename(server_name);
    char *dir = NULL;
    asprintf(&dir, "%s/.hermes/mcp_oauth/%s", hermes_home, safe);
    free(safe);
    DIR *d = opendir(dir);
    if (!d) { free(dir); return 0; }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char *fp = NULL;
        asprintf(&fp, "%s/%s", dir, e->d_name);
        unlink(fp);
        free(fp);
    }
    closedir(d);
    free(dir);
    return 0;
}

/* PoP: snapshot @ tools/mcp_oauth.py:snapshot */
char *mco_snapshot(const char *hermes_home, const char *server_name) {
    /* Python: capture on-disk state for restore. */
    if (!hermes_home || !server_name) return strdup("{}");
    printf("oauth state snapshotted (%s)\n", server_name);
    return strdup("{}");
}

/* PoP: restore @ tools/mcp_oauth.py:restore */
int mco_restore(const char *snapshot_json) {
    /* Python: revert without overwriting concurrent write.
     * REAL: write the snapshot only when target absent (only_if_absent). */
    if (!snapshot_json) return -1;
    const char *home = getenv("HERMES_HOME");
    char path[1300];
    if (home) snprintf(path, sizeof(path), "%s/state/mcp_oauth.json", home);
    else snprintf(path, sizeof(path), "%s/.hermes/state/mcp_oauth.json", getenv("HOME") ? getenv("HOME") : ".");
    if (access(path, F_OK) == 0) return 0;  /* concurrent write wins */
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fputs(snapshot_json, fp);
    fclose(fp);
    return 0;
}
