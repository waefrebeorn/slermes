/*
 * plugin_storage.c — canonical C11 port of plugins/plugin_storage.py.
 *
 * Per-plugin durable storage convention: one data root per plugin under
 * <hermes home>/plugin-data/<name>/ — owned by the user, untouched by
 * install/update/remove. Secrets are deliberately NOT part of this
 * convention (credential reads go through agent.secret_scope / .env).
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sqlite3.h>

/*
 * Mirrors the plugin-name shape `hermes plugins install` accepts; rejects
 * PoP: pstore_validate_name @ plugins/plugin_storage.py:_validate_name
 */
int pstore_validate_name(const char *name) {
    if (!name || !*name) return 1;
    size_t n = strlen(name);
    if (n > 64) return 1;
    if (!(name[0]=='.' || ( (name[0]>='a'&&name[0]<='z') || (name[0]>='A'&&name[0]<='Z') || (name[0]>='0'&&name[0]<='9'))))
        return 1;
    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        int ok = (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-';
        if (!ok) return 1;
    }
    if (strstr(name, "..")) return 1;
    return 0;
}

/*
 * Returns (and creates) <hermes home>/plugin-data/<name>/ in out.
 * PoP: pstore_data_dir @ plugins/plugin_storage.py:plugin_data_dir
 */
int pstore_data_dir(const char *name, char *out, size_t out_sz) {
    if (pstore_validate_name(name) != 0) {
        hermes_log(LOG_ERROR, "plugin_storage", "invalid plugin name for storage: %s", name);
        return 1;
    }
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home || !*home) home = "/tmp";
    snprintf(out, out_sz, "%s/plugin-data/%s", home, name);
    /* mkdir -p the chain */
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", out);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return 2;
    return 0;
}

/*
 * Opens <data dir>/<filename> with WAL + foreign_keys=ON (created on first
 * PoP: pstore_db_open @ plugins/plugin_storage.py:plugin_db
 */
sqlite3 *pstore_db_open(const char *name, const char *filename) {
    if (!filename || !*filename || strchr(filename, '/') || strstr(filename, "..")) {
        hermes_log(LOG_ERROR, "plugin_storage", "invalid plugin db filename: %s",
                   filename ? filename : "(null)");
        return NULL;
    }
    char dir[1024], path[1200];
    if (pstore_data_dir(name, dir, sizeof dir) != 0) return NULL;
    snprintf(path, sizeof path, "%s/%s", dir, filename);
    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        hermes_log(LOG_ERROR, "plugin_storage", "cannot open %s", path);
        if (db) sqlite3_close(db);
        return NULL;
    }
    char *errmsg = NULL;
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;"
                     "PRAGMA foreign_keys=ON;", NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    return db;
}
