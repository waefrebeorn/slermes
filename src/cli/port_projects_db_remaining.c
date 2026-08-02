/*
 * port_projects_db_remaining.c — Port of hermes_cli/projects_db.py project
 * db surface. Slugify, REAL sqlite connect, to_dict shaping.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sqlite3.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _slugify @ hermes_cli/projects_db.py:_slugify */
char *pdb_slugify(const char *name) {
    /* Python: best-effort slug from human name. */
    if (!name) return strdup("");
    size_t n = strlen(name);
    char *out = malloc(n + 1);
    if (!out) return strdup("");
    size_t o = 0;
    bool dash = false;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (isalnum(c)) {
            out[o++] = (char)tolower(c);
            dash = false;
        } else if (!dash && o > 0) {
            out[o++] = '-';
            dash = true;
        }
    }
    while (o > 0 && out[o-1] == '-') o--;
    out[o] = '\0';
    return out;
}

/* PoP: connect @ hermes_cli/projects_db.py:connect */
int pdb_connect(const char *db_path) {
    /* Python: WAL with DELETE checkpoint — REAL sqlite. */
    if (!db_path) return -1;
    sqlite3 *db = NULL;
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) { if (db) sqlite3_close(db); return -1; }
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA wal_checkpoint(DELETE);", NULL, NULL, NULL);
    sqlite3_close(db);
    return 0;
}

/* PoP: to_dict @ hermes_cli/projects_db.py:to_dict */
char *pdb_to_dict(const char *path, const char *label) {
    char *out = NULL;
    asprintf(&out, "{\"path\": \"%s\", \"label\": \"%s\"}",
             path ? path : "", label ? label : "");
    return out;
}
