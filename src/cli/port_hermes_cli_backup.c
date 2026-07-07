/*
 * port_hermes_cli_backup.c — C port of selected helpers from
 * hermes_cli/backup.py.
 *
 * Only dependency-light, faithful re-implementations are ported here.
 * Zip/SQLite-backup paths are deferred to their respective subsystems.
 /* Zip/SQLite-backup paths: zip-writing, plugin-loading, and os.walk snapshot
  * trees are deferred; the SQLite safe-copy + JSON/FS prune helpers below are
  * ported faithfully. */

 #include "hermes_logger.h"
 #include "libjson/json.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <limits.h>
 #include <ctype.h>
 #include <time.h>
 #include <dirent.h>
 #include <sys/stat.h>
 #include <unistd.h>
 #include <sqlite3.h>

 /* Resolve the shared Hermes home dir (HERMES_HOME / SLERMES_HOME / HOME). */
 static void backup_hermes_home_dir(char *out, size_t sz)
 {
     const char *h = getenv("HERMES_HOME");
     if (h && h[0]) { snprintf(out, sz, "%s", h); return; }
     h = getenv("SLERMES_HOME");
     if (h && h[0]) { snprintf(out, sz, "%s", h); return; }
     h = getenv("HOME");
     snprintf(out, sz, "%s/.hermes", h ? h : ".");
 }

/* --- Excluded directory names (any level) --- */
static const char *EXCLUDED_DIRS[] = {
    "hermes-agent", "__pycache__", ".git", "node_modules", "backups",
    "checkpoints", ".venv", "venv", "site-packages", ".cache", ".tox",
    ".nox", ".pytest_cache", ".mypy_cache", ".ruff_cache", NULL,
};
/* --- Excluded file basenames --- */
static const char *EXCLUDED_NAMES[] = {
    "gateway.pid", "cron.pid", "gateway_state.json", "gateway.lock",
    "processes.json", NULL,
};
/* --- Excluded file-name suffixes --- */
static const char *EXCLUDED_SUFFIXES[] = {
    ".pyc", ".pyo", ".db-wal", ".db-shm", ".db-journal", NULL,
};

/* Count path components (split on '/'). */
static int path_part_count(const char *p)
{
    int n = 0;
    if (p && *p) n = 1;
    for (const char *c = p; *c; c++) if (*c == '/') n++;
    return n;
}

/* Extract the i-th path component (0-based) into out (caller-sized). */
static void path_part_at(const char *p, int idx, char *out, size_t sz)
{
    int cur = 0;
    const char *start = p;
    while (cur < idx) {
        while (*start && *start != '/') start++;
        if (!*start) { out[0] = '\0'; return; }
        start++; /* skip '/' */
        cur++;
    }
    const char *end = start;
    while (*end && *end != '/') end++;
    size_t len = (size_t)(end - start);
    if (len >= sz) len = sz - 1;
    memcpy(out, start, len);
    out[len] = '\0';
}

/*
 * PoP: backup_should_exclude @ hermes_cli/backup.py:_should_exclude
 * Returns 1 if rel_path (relative to hermes root) should be skipped. */
int backup_should_exclude(const char *rel_path)
{
    if (!rel_path) return 0;
    int nparts = path_part_count(rel_path);
    for (int i = 0; i < nparts; i++) {
        char part[256];
        path_part_at(rel_path, i, part, sizeof(part));
        for (int k = 0; EXCLUDED_DIRS[k]; k++) {
            if (strcmp(part, EXCLUDED_DIRS[k]) != 0) continue;
            /* "hermes-agent" only matches at the root level (first component). */
            if (strcmp(part, "hermes-agent") == 0 && i != 0) continue;
            return 1;
        }
    }
    /* basename */
    const char *slash = strrchr(rel_path, '/');
    const char *name = slash ? slash + 1 : rel_path;
    for (int k = 0; EXCLUDED_NAMES[k]; k++)
        if (strcmp(name, EXCLUDED_NAMES[k]) == 0) return 1;
    for (int k = 0; EXCLUDED_SUFFIXES[k]; k++) {
        size_t nl = strlen(name), sl = strlen(EXCLUDED_SUFFIXES[k]);
        if (nl >= sl && strcmp(name + nl - sl, EXCLUDED_SUFFIXES[k]) == 0) return 1;
    }
    return 0;
}

/*
 * PoP: backup_should_skip_file @ hermes_cli/backup.py:_should_skip_backup_file
 * Returns 1 when a candidate file should not be written to a backup. */
int backup_should_skip_file(const char *abs_path, const char *rel_path, const char *out_path)
{
    if (backup_should_exclude(rel_path)) return 1;

    /* zipfile.write() follows symlinks — skip links to avoid copying data
     * from outside HERMES_HOME. */
    struct stat st;
    if (lstat(abs_path, &st) == 0 && S_ISLNK(st.st_mode)) return 1;

    /* Don't write the backup archive into itself. */
    char ra[PATH_MAX], ro[PATH_MAX];
    if (realpath(abs_path, ra) && realpath(out_path, ro) && strcmp(ra, ro) == 0)
        return 1;
    return 0;
}

/* ===========================================================================
 * SQLite safe copy + snapshot/prune/cron helpers (faithful to backup.py)
 * =========================================================================== */

/*
 * PoP: backup_safe_copy_db @ hermes_cli/backup.py:_safe_copy_db
 * Safely copy a SQLite DB via the online backup API (handles WAL mode),
 * falling back to a raw byte copy on failure. Returns 1 on success. */
int backup_safe_copy_db(const char *src, const char *dst)
{
    if (!src || !dst) return 0;
    sqlite3 *src_db = NULL, *dst_db = NULL;
    char uri[PATH_MAX + 16];
    snprintf(uri, sizeof(uri), "file:%s?mode=ro", src);
    if (sqlite3_open(uri, &src_db) != SQLITE_OK) {
        if (src_db) sqlite3_close(src_db);
        goto raw;
    }
    if (sqlite3_open(dst, &dst_db) != SQLITE_OK) {
        if (dst_db) sqlite3_close(dst_db);
        sqlite3_close(src_db);
        goto raw;
    }
    sqlite3_backup *bk = sqlite3_backup_init(dst_db, "main", src_db, "main");
    if (!bk) {
        sqlite3_close(dst_db);
        sqlite3_close(src_db);
        goto raw;
    }
    int rc = sqlite3_backup_step(bk, -1);
    if (rc != SQLITE_DONE && rc != SQLITE_OK) {
        sqlite3_backup_finish(bk);
        sqlite3_close(dst_db);
        sqlite3_close(src_db);
        goto raw;
    }
    sqlite3_backup_finish(bk);
    sqlite3_close(dst_db);
    sqlite3_close(src_db);
    return 1;

raw:
    {
        FILE *fi = fopen(src, "rb");
        if (!fi) return 0;
        FILE *fo = fopen(dst, "wb");
        if (!fo) { fclose(fi); return 0; }
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fi)) > 0)
            fwrite(buf, 1, n, fo);
        int ok = (ferror(fi) || ferror(fo)) ? 0 : 1;
        fclose(fi);
        fclose(fo);
        return ok;
    }
}

/* Minimal ZIP central-directory reader: collect entry names into a malloc'd
 * NULL-terminated array (caller frees with backup_zip_names_free). */
static char **backup_zip_list_names(const char *path, int *count_out)
{
    *count_out = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char eocd[22];
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long fsize = ftell(f);
    long scan = fsize - 22;
    if (scan < 0) scan = 0;
    int found = 0;
    for (; scan >= 0; scan--) {
        if (fseek(f, scan, SEEK_SET) != 0) break;
        if (fread(eocd, 1, 22, f) != 22) break;
        unsigned sig = (unsigned char)eocd[0] | ((unsigned)eocd[1] << 8)
                     | ((unsigned)eocd[2] << 16) | ((unsigned)eocd[3] << 24);
        if (sig == 0x06054b50) { found = 1; break; }
    }
    if (!found) { fclose(f); return NULL; }
    unsigned total = (unsigned char)eocd[10] | ((unsigned)eocd[11] << 8);
    unsigned cd_off = (unsigned char)eocd[16] | ((unsigned)eocd[17] << 8)
                    | ((unsigned)eocd[18] << 16) | ((unsigned)eocd[19] << 24);
    if (fseek(f, cd_off, SEEK_SET) != 0) { fclose(f); return NULL; }
    char **names = calloc((size_t)total + 1, sizeof(char *));
    if (!names) { fclose(f); return NULL; }
    int got = 0;
    for (unsigned i = 0; i < total; i++) {
        char hdr[46];
        if (fread(hdr, 1, 46, f) != 46) break;
        unsigned sig = (unsigned char)hdr[0] | ((unsigned)hdr[1] << 8)
                     | ((unsigned)hdr[2] << 16) | ((unsigned)hdr[3] << 24);
        if (sig != 0x02014b50) break;
        unsigned nlen = (unsigned char)hdr[28] | ((unsigned)hdr[29] << 8);
        unsigned elen = (unsigned char)hdr[30] | ((unsigned)hdr[31] << 8);
        unsigned clen = (unsigned char)hdr[32] | ((unsigned)hdr[33] << 8);
        char *nm = malloc((size_t)nlen + 1);
        if (!nm) break;
        if (fread(nm, 1, nlen, f) != nlen) { free(nm); break; }
        nm[nlen] = '\0';
        if (fseek(f, (long)elen + clen, SEEK_CUR) != 0) { free(nm); break; }
        names[got++] = nm;
    }
    names[got] = NULL;
    *count_out = got;
    fclose(f);
    return names;
}

static void backup_zip_names_free(char **names)
{
    if (!names) return;
    for (int i = 0; names[i]; i++) free(names[i]);
    free(names);
}

/*
 * PoP: backup_validate_zip @ hermes_cli/backup.py:_validate_backup_zip
 * Returns 1 if the zip looks like a Hermes backup. On failure sets *reason to
 * a malloc'd message (caller frees) and returns 0. */
int backup_validate_zip(const char *path, char **reason)
{
    if (reason) *reason = NULL;
    int n = 0;
    char **names = backup_zip_list_names(path, &n);
    if (!names || n == 0) {
        if (reason) *reason = strdup("zip archive is empty");
        backup_zip_names_free(names);
        return 0;
    }
    int found = 0;
    for (int i = 0; names[i]; i++) {
        const char *base = strrchr(names[i], '/');
        base = base ? base + 1 : names[i];
        if (strcmp(base, "config.yaml") == 0 ||
            strcmp(base, ".env") == 0 ||
            strcmp(base, "state.db") == 0) {
            found = 1;
            break;
        }
    }
    backup_zip_names_free(names);
    if (!found) {
        if (reason) *reason = strdup(
            "zip does not appear to be a Hermes backup "
            "(no config.yaml, .env, or state databases found)");
        return 0;
    }
    return 1;
}

/*
 * PoP: backup_detect_zip_prefix @ hermes_cli/backup.py:_detect_prefix
 * Detect a common wrapping directory prefix (".hermes/" or "hermes/").
 * Returns malloc'd prefix (incl. trailing '/') or empty string. Caller frees. */
char *backup_detect_zip_prefix(const char *path)
{
    int n = 0;
    char **names = backup_zip_list_names(path, &n);
    if (!names || n == 0) { backup_zip_names_free(names); return strdup(""); }
    char *firsts[512];
    int fc = 0;
    for (int i = 0; names[i] && fc < 512; i++) {
        const char *p = names[i];
        const char *slash = strchr(p, '/');
        if (slash) {
            size_t len = (size_t)(slash - p);
            int dup = 0;
            for (int k = 0; k < fc; k++)
                if (strncmp(firsts[k], p, len) == 0 && firsts[k][len] == '\0') {
                    dup = 1; break;
                }
            if (!dup) {
                char *s = malloc(len + 1);
                memcpy(s, p, len); s[len] = '\0';
                firsts[fc++] = s;
            }
        }
    }
    char *result = strdup("");
    if (fc == 1) {
        const char *fp = firsts[0];
        if (strcmp(fp, ".hermes") == 0 || strcmp(fp, "hermes") == 0) {
            size_t len = strlen(fp);
            char *pre = malloc(len + 2);
            snprintf(pre, len + 2, "%s/", fp);
            free(result);
            result = pre;
        }
    }
    for (int k = 0; k < fc; k++) free(firsts[k]);
    backup_zip_names_free(names);
    return result;
}

/*
 * PoP: backup_quick_snapshot_root @ hermes_cli/backup.py:_quick_snapshot_root
 * Returns malloc'd path <hermes_home>/state-snapshots. Caller frees. */
char *backup_quick_snapshot_root(const char *hermes_home)
{
    char home[PATH_MAX];
    if (hermes_home && hermes_home[0]) snprintf(home, sizeof(home), "%s", hermes_home);
    else backup_hermes_home_dir(home, sizeof(home));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/state-snapshots", home);
    return strdup(out);
}

/*
 * PoP: backup_pre_update_backup_dir @ hermes_cli/backup.py:_pre_update_backup_dir
 * Returns malloc'd path <hermes_home>/backups. Caller frees. */
char *backup_pre_update_backup_dir(const char *hermes_home)
{
    char home[PATH_MAX];
    if (hermes_home && hermes_home[0]) snprintf(home, sizeof(home), "%s", hermes_home);
    else backup_hermes_home_dir(home, sizeof(home));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/backups", home);
    return strdup(out);
}

/*
 * PoP: backup_count_cron_jobs @ hermes_cli/backup.py:_count_cron_jobs
 * Count jobs in a cron/jobs.json file. Returns -1 if missing/unparseable
 * (means "unknown", never "zero"), else the job count. */
int backup_count_cron_jobs(const char *path)
{
    if (!path) return -1;
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    char *err = NULL;
    json_t *doc = json_parse_file(path, &err);
    if (err) { free(err); return -1; }
    if (!doc) return -1;
    int result = -1;
    if (doc->type == JSON_OBJECT) {
        json_t *jobs = json_obj_get(doc, "jobs");
        if (jobs && jobs->type == JSON_ARRAY) result = (int)json_len(jobs);
        else result = -1;
    } else if (doc->type == JSON_ARRAY) {
        result = (int)json_len(doc);
    }
    json_free(doc);
    return result;
}

/*
 * PoP: backup_restore_cron_jobs_if_emptied @ hermes_cli/backup.py:restore_cron_jobs_if_emptied
 * Restore cron/jobs.json from a quick snapshot if the live file now has
 * FEWER jobs than the snapshot. Returns 1 if restored, 0 otherwise. */
int backup_restore_cron_jobs_if_emptied(const char *snapshot_id, const char *hermes_home)
{
    if (!snapshot_id || !snapshot_id[0]) return 0;
    char home[PATH_MAX];
    if (hermes_home && hermes_home[0]) snprintf(home, sizeof(home), "%s", hermes_home);
    else backup_hermes_home_dir(home, sizeof(home));

    char live[PATH_MAX];
    snprintf(live, sizeof(live), "%s/cron/jobs.json", home);
    int live_count = backup_count_cron_jobs(live);
    if (live_count < 0) return 0;

    char snap[PATH_MAX];
    snprintf(snap, sizeof(snap), "%s/state-snapshots/%s/cron/jobs.json", home, snapshot_id);
    int snap_count = backup_count_cron_jobs(snap);
    if (snap_count <= 0) return 0;

    if (live_count >= snap_count) return 0;

    char cmd[PATH_MAX * 2 + 64];
    snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\" 2>/dev/null", snap, live);
    int rc = system(cmd);
    return rc == 0 ? 1 : 0;
}

/* List a directory's entries sorted by name into a malloc'd array.
 * dirs_only=1 -> directories only; 0 -> regular files only.
 * Returns array and sets *count_out. Caller frees. */
static char **backup_list_sorted(const char *root, int dirs_only, int *count_out)
{
    char **arr = NULL;
    int cap = 0, cnt = 0;
    DIR *d = opendir(root);
    if (!d) { *count_out = 0; return NULL; }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", root, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        int is_dir = S_ISDIR(st.st_mode);
        if (dirs_only && !is_dir) continue;
        if (!dirs_only && !S_ISREG(st.st_mode)) continue;
        if (cnt >= cap) {
            cap = cap ? cap * 2 : 16;
            char **na = realloc(arr, (size_t)cap * sizeof(char *));
            if (!na) break;
            arr = na;
        }
        arr[cnt++] = strdup(e->d_name);
    }
    closedir(d);
    for (int i = 1; i < cnt; i++) {
        char *key = arr[i];
        int j = i - 1;
        while (j >= 0 && strcmp(arr[j], key) > 0) { arr[j + 1] = arr[j]; j--; }
        arr[j + 1] = key;
    }
    *count_out = cnt;
    return arr;
}

static void backup_strv_free(char **arr, int n)
{
    if (!arr) return;
    for (int i = 0; i < n; i++) free(arr[i]);
    free(arr);
}

/*
 * PoP: backup_prune_quick_snapshots @ hermes_cli/backup.py:prune_quick_snapshots
 * Public wrapper: Remove oldest snapshot dirs beyond keep. Returns count deleted. */
int backup_prune_quick_snapshots(const char *root, int keep)
{
    struct stat st;
    if (!root || stat(root, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
    int n = 0;
    char **dirs = backup_list_sorted(root, 1, &n);
    int deleted = 0;
    for (int i = 0; i < n - keep; i++) {
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", root, dirs[i]);
        char cmd[PATH_MAX * 2 + 32];
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" 2>/dev/null", full);
        if (system(cmd) == 0) deleted++;
    }
    backup_strv_free(dirs, n);
    return deleted;
}

/* Generic prefix-based zip pruner shared by pre-update / pre-migration. */
static int backup_prune_prefixed_zips(const char *dir, const char *prefix, int keep)
{
    struct stat st;
    if (!dir || stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
    int n = 0;
    char **files = backup_list_sorted(dir, 0, &n);
    int deleted = 0;
    size_t plen = strlen(prefix);
    int seen = 0;
    for (int i = n - 1; i >= 0; i--) {
        size_t fl = strlen(files[i]);
        int is_zip = fl >= 4 && strcmp(files[i] + fl - 4, ".zip") == 0;
        if (strncmp(files[i], prefix, plen) != 0 || !is_zip) continue;
        seen++;
        if (seen <= keep) continue;
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, files[i]);
        if (unlink(full) == 0) deleted++;
    }
    backup_strv_free(files, n);
    return deleted;
}

/*
 * PoP: backup_prune_pre_update_backups @ hermes_cli/backup.py:_prune_pre_update_backups
 * Remove oldest pre-update-*.zip beyond keep (floor keep to 1). */
int backup_prune_pre_update_backups(const char *dir, int keep)
{
    if (keep < 1) keep = 1;
    return backup_prune_prefixed_zips(dir, "pre-update-", keep);
}

/*
 * PoP: backup_prune_pre_migration_backups @ hermes_cli/backup.py:_prune_pre_migration_backups
 * Remove oldest pre-migration-*.zip beyond keep (floor keep to 0). */
int backup_prune_pre_migration_backups(const char *dir, int keep)
{
    if (keep < 0) keep = 0;
    return backup_prune_prefixed_zips(dir, "pre-migration-", keep);
}
