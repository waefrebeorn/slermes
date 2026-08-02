/*
 * port_curator_backup_remaining.c — Port of agent/curator_backup.py
 * snapshot surface. Real file/dir ops: skills backup, manifest
 * read/write, pruning, listing, restore, cron link reconciliation.
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
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static const char *hermes_home_dir(void) {
    const char *h = getenv("HERMES_HOME");
    if (h && *h) return h;
    h = getenv("HOME");
    if (h && *h) return h;
    return ".";
}

/* PoP: _backups_dir @ agent/curator_backup.py:_backups_dir */
char *cub_backups_dir(void) {
    /* Python: <home>/skills/.curator_backups. */
    char *out = NULL;
    asprintf(&out, "%s/skills/.curator_backups", hermes_home_dir());
    return out;
}

/* PoP: _skills_dir @ agent/curator_backup.py:_skills_dir */
char *cub_skills_dir(void) {
    char *out = NULL;
    asprintf(&out, "%s/skills", hermes_home_dir());
    return out;
}

/* PoP: _cron_jobs_file @ agent/curator_backup.py:_cron_jobs_file */
char *cub_cron_jobs_file(void) {
    char *out = NULL;
    asprintf(&out, "%s/cron/jobs.json", hermes_home_dir());
    return out;
}

/* PoP: _backup_cron_jobs_into @ agent/curator_backup.py:_backup_cron_jobs_into */
char *cub_backup_cron_jobs_into(const char *dest) {
    /* Python: copy jobs.json → dest/cron-jobs.json. */
    if (!dest) return NULL;
    char *src = cub_cron_jobs_file();
    char *dst = NULL;
    asprintf(&dst, "%s/cron-jobs.json", dest);
    char *out = NULL;
    if (access(src, F_OK) == 0) {
        FILE *f = fopen(src, "rb");
        FILE *w = fopen(dst, "wb");
        if (f && w) {
            char buf[8192];
            size_t r;
            size_t total = 0;
            while ((r = fread(buf, 1, sizeof(buf), f)) > 0) {
                fwrite(buf, 1, r, w);
                total += r;
            }
            asprintf(&out, "{\"copied\": true, \"bytes\": %zu}", total);
            fclose(w);
        }
        if (f) fclose(f);
    } else {
        asprintf(&out, "{\"copied\": false, \"error\": \"no jobs.json\"}");
    }
    free(src);
    free(dst);
    return out;
}

/* PoP: _utc_id @ agent/curator_backup.py:_utc_id */
char *cub_utc_id(const char *now_iso) {
    /* Python: 2026-05-01T13-05-42Z filesystem-safe. */
    if (now_iso) {
        char *out = strdup(now_iso);
        if (!out) return NULL;
        for (char *p = out; *p; p++)
            if (*p == ':') *p = '-';
        return out;
    }
    time_t t = time(NULL);
    struct tm g;
    gmtime_r(&t, &g);
    char *out = NULL;
    asprintf(&out, "%04d-%02d-%02dT%02d-%02d-%02dZ",
             g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
             g.tm_hour, g.tm_min, g.tm_sec);
    return out;
}

/* PoP: _load_config @ agent/curator_backup.py:_load_config */
char *cub_load_config(const char *config_path) {
    /* Python: config.yaml read. */
    if (!config_path) return strdup("{}");
    FILE *f = fopen(config_path, "r");
    if (!f) return strdup("{}");
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = 0;
    if (buf) { r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
    fclose(f);
    if (!buf) return strdup("{}");
    return buf;
}

/* PoP: is_enabled @ agent/curator_backup.py:is_enabled */
bool cub_is_enabled(const char *config_yaml) {
    /* Python: default ON. */
    if (!config_yaml) return true;
    const char *p = strstr(config_yaml, "curator_backup");
    if (!p) return true;
    const char *colon = strchr(p, ':');
    if (!colon) return true;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    char *l = lowerdup(v);
    bool r = l && (strcmp(l, "false") == 0 || strcmp(l, "0") == 0 || strcmp(l, "off") == 0) ? false : true;
    free(l);
    return r;
}

/* PoP: _count_skill_files @ agent/curator_backup.py:_count_skill_files */
long cub_count_skill_files(const char *base) {
    /* Python: SKILL.md count under base. */
    if (!base) return 0;
    DIR *d = opendir(base);
    if (!d) return 0;
    long count = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char *sub = NULL;
        asprintf(&sub, "%s/%s", base, e->d_name);
        if (e->d_type == DT_DIR) {
            char *md = NULL;
            asprintf(&md, "%s/SKILL.md", sub);
            if (access(md, F_OK) == 0) count++;
            else count += cub_count_skill_files(sub);
            free(md);
        }
        free(sub);
    }
    closedir(d);
    return count;
}

/* PoP: _write_manifest @ agent/curator_backup.py:_write_manifest */
int cub_write_manifest(const char *snap_dir, const char *manifest_json) {
    /* Python: manifest.json write. */
    if (!snap_dir || !manifest_json) return -1;
    char *path = NULL;
    asprintf(&path, "%s/manifest.json", snap_dir);
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); free(path); return -1; }
    fwrite(manifest_json, 1, strlen(manifest_json), w);
    fputc('\n', w);
    fclose(w);
    int rc = rename(tmp, path);
    if (rc != 0) unlink(tmp);
    free(tmp);
    free(path);
    return rc == 0 ? 0 : -1;
}

/* PoP: _prune_old @ agent/curator_backup.py:_prune_old */
char *cub_prune_old(const char *backups_dir, long keep) {
    /* Python: delete regular snapshots beyond newest keep. */
    if (!backups_dir) return strdup("[]");
    DIR *d = opendir(backups_dir);
    if (!d) return strdup("[]");
    char **names = NULL;
    long count = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        names = realloc(names, (size_t)(count + 1) * sizeof(char *));
        if (!names) break;
        names[count++] = strdup(e->d_name);
    }
    closedir(d);
    /* sort descending (newest first) */
    for (long i = 0; i < count; i++)
        for (long j = i + 1; j < count; j++)
            if (strcmp(names[j], names[i]) > 0) {
                char *t = names[i]; names[i] = names[j]; names[j] = t;
            }
    size_t cap = 256, len = 0;
    char *out = malloc(cap);
    if (!out) { for (long i = 0; i < count; i++) free(names[i]); free(names); return strdup("[]"); }
    strcpy(out, "[");
    bool first = true;
    for (long i = keep; i < count; i++) {
        char *path = NULL;
        asprintf(&path, "%s/%s", backups_dir, names[i]);
        /* skip partial snapshots (no manifest) */
        char *mf = NULL;
        asprintf(&mf, "%s/manifest.json", path);
        if (access(mf, F_OK) == 0) {
            char *cmd = NULL;
            asprintf(&cmd, "rm -rf %s", path);
            if (cmd) { system(cmd); free(cmd); }
            size_t need = len + strlen(names[i]) + 8;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) { free(mf); free(path); break; }
                out = nb;
            }
            if (!first) strcat(out, ",");
            strcat(out, "\"");
            strcat(out, names[i]);
            strcat(out, "\"");
            first = false;
            len = strlen(out);
        }
        free(mf);
        free(path);
    }
    strcat(out, "]");
    for (long i = 0; i < count; i++) free(names[i]);
    free(names);
    return out;
}

/* PoP: _read_manifest @ agent/curator_backup.py:_read_manifest */
char *cub_read_manifest(const char *snap_dir) {
    /* Python: manifest.json or {}. */
    if (!snap_dir) return strdup("{}");
    char *path = NULL;
    asprintf(&path, "%s/manifest.json", snap_dir);
    char *out = NULL;
    FILE *f = fopen(path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n > 0) {
            char *buf = malloc((size_t)n + 1);
            size_t r = 0;
            if (buf) { r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; out = buf; }
        }
        fclose(f);
    }
    free(path);
    return out ? out : strdup("{}");
}

/* PoP: list_backups @ agent/curator_backup.py:list_backups */
char *cub_list_backups(void) {
    /* Python: restorable snapshots, newest first. */
    char *dir = cub_backups_dir();
    DIR *d = opendir(dir);
    if (!d) { free(dir); return strdup("[]"); }
    char **names = NULL;
    long count = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        names = realloc(names, (size_t)(count + 1) * sizeof(char *));
        if (!names) break;
        names[count++] = strdup(e->d_name);
    }
    closedir(d);
    for (long i = 0; i < count; i++)
        for (long j = i + 1; j < count; j++)
            if (strcmp(names[j], names[i]) > 0) {
                char *t = names[i]; names[i] = names[j]; names[j] = t;
            }
    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    if (!out) { for (long i = 0; i < count; i++) free(names[i]); free(names); free(dir); return strdup("[]"); }
    strcpy(out, "[");
    bool first = true;
    for (long i = 0; i < count; i++) {
        char *snap = NULL;
        asprintf(&snap, "%s/%s", dir, names[i]);
        char *mf = cub_read_manifest(snap);
        if (strcmp(mf, "{}") != 0) {
            size_t need = len + strlen(names[i]) + strlen(mf) + 8;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) { free(mf); free(snap); break; }
                out = nb;
            }
            if (!first) strcat(out, ",");
            strcat(out, "{\"id\": \"");
            strcat(out, names[i]);
            strcat(out, "\", \"manifest\": ");
            strcat(out, mf);
            strcat(out, "}");
            first = false;
            len = strlen(out);
        }
        free(mf);
        free(snap);
    }
    strcat(out, "]");
    for (long i = 0; i < count; i++) free(names[i]);
    free(names);
    free(dir);
    return out;
}

/* PoP: _resolve_backup @ agent/curator_backup.py:_resolve_backup */
char *cub_resolve_backup(const char *backup_id) {
    /* Python: requested path or newest. */
    char *dir = cub_backups_dir();
    if (backup_id && *backup_id) {
        char *out = NULL;
        asprintf(&out, "%s/%s", dir, backup_id);
        free(dir);
        return out;
    }
    DIR *d = opendir(dir);
    if (!d) { free(dir); return NULL; }
    char best[512] = "";
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (strcmp(e->d_name, best) > 0) snprintf(best, sizeof(best), "%s", e->d_name);
    }
    closedir(d);
    char *out = NULL;
    if (*best) asprintf(&out, "%s/%s", dir, best);
    free(dir);
    return out;
}

/* PoP: _restore_cron_skill_links @ agent/curator_backup.py:_restore_cron_skill_links */
int cub_restore_cron_skill_links(const char *snap_dir) {
    /* Python: reconcile backed-up cron skill links (no overwrite). */
    if (!snap_dir) return -1;
    char *src = NULL;
    asprintf(&src, "%s/cron-jobs.json", snap_dir);
    char *dst = cub_cron_jobs_file();
    if (access(src, F_OK) == 0 && access(dst, F_OK) != 0) {
        char *cmd = NULL;
        asprintf(&cmd, "cp %s %s", src, dst);
        if (cmd) { system(cmd); free(cmd); }
    }
    free(src);
    free(dst);
    return 0;
}

/* PoP: rollback @ agent/curator_backup.py:rollback */
char *cub_rollback(const char *backup_id) {
    /* Python: restore skills from snapshot. */
    char *snap = cub_resolve_backup(backup_id);
    if (!snap) return strdup("{\"success\": false, \"error\": \"no backup\"}");
    char *skills = cub_skills_dir();
    char *cmd = NULL;
    asprintf(&cmd, "rm -rf %s && cp -a %s/skills %s 2>/dev/null",
             skills, snap, skills);
    int rc = system(cmd);
    free(cmd);
    cub_restore_cron_skill_links(snap);
    char *out = NULL;
    asprintf(&out, "{\"success\": %s, \"snapshot\": \"%s\"}", rc == 0 ? "true" : "false", snap);
    free(snap);
    free(skills);
    return out;
}

/* PoP: format_size @ agent/curator_backup.py:format_size */
char *cub_format_size(double n) {
    /* Python: human size. */
    char *out = NULL;
    if (n < 1024) asprintf(&out, "%.0f B", n);
    else if (n < 1024 * 1024) asprintf(&out, "%.1f KB", n / 1024);
    else if (n < 1024.0 * 1024 * 1024) asprintf(&out, "%.1f MB", n / (1024 * 1024));
    else asprintf(&out, "%.1f GB", n / (1024.0 * 1024 * 1024));
    return out;
}

/* PoP: summarize_backups @ agent/curator_backup.py:summarize_backups */
char *cub_summarize_backups(void) {
    /* Python: human summary. */
    char *rows = cub_list_backups();
    if (!rows || strcmp(rows, "[]") == 0) {
        free(rows);
        return strdup("No curator snapshots yet.");
    }
    long count = 0;
    for (const char *p = rows; *p; p++) if (*p == '{') count++;
    char *out = NULL;
    asprintf(&out, "%ld curator snapshot(s) available.", count);
    free(rows);
    return out;
}
