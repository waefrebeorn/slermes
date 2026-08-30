/* ── profile gap-fillers for hermes_cli/profiles.py ────────────────────── */
/*
 * Implements functions that are DA-1 (c_location=None) in the parity scan:
 *   profile_create_profile              — new-profile factory (clone / bootstrap)
 *   profile_delete_profile               — remove a profile directory tree
 *   profile_export_profile                — dump a profile as a portable archive
 *   profile_import_profile                — restore an exported profile into ~/.hermes
 *   profile_rename_profile                — mv a profile dir + patch active_profile
 *   profile_archive_profile               — (helper) build a tarball of a profile dir
 *   profile_describe_profile              — text summary of a profile's meta + model
 *   profile_list_profiles                 — enumerate profiles with their metadata
 *
 * Naming convention: cfn = profile_<python_stem> (matches existing profile_*
 * functions already in src/hermes_cli/profiles.c).
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <dirent.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#include "hermes_logger.h"
#include "slermes_home.h"
#include "profile_store.h"
#include "libjson/json.h"

/* ── help functions ────────────────────────────────────────────── */
static char *gap_profile_dir_for(const char *name) {
    char *root = profile_profiles_root();
    if (!root) return NULL;
    size_t n = strlen(root) + 1 + (name ? strlen(name) : 0) + 1;
    char *d = malloc(n);
    if (!d) { free(root); return NULL; }
    snprintf(d, n, "%s/%s", root, name ? name : "");
    free(root);
    return d;
}

static bool mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) return false;
    for (size_t i = 1; i <= len; i++) {
        if (tmp[i] == '/' || i == len) {
            tmp[i] = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return false;
            tmp[i] = '/';
        }
    }
    return true;
}

static bool path_exists(const char *p) {
    struct stat st;
    return (stat(p, &st) == 0);
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *r = malloc(n);
    if (r) memcpy(r, s, n);
    return r;
}

/* ── profile_create_profile ────────────────────────────────────── */
/*
 * PoP: profile_create_profile @ hermes_cli/profiles.py:create_profile
 */
char *profile_create_profile(const char *name) {
    char *canon = profile_normalize_name(name);
    if (!canon) return NULL;

    char *err = NULL;
    if (!profile_validate_name(canon, &err)) {
        free(canon); free(err); return NULL;
    }
    free(err);

    if (strcmp(canon, "default") == 0) {
        free(canon);
        hermes_log(LOG_ERROR, "profiles", "Cannot create profile 'default' - built-in");
        return NULL;
    }

    char *dir = gap_profile_dir_for(canon);
    if (!dir) { free(canon); return NULL; }

    if (path_exists(dir)) {
        hermes_log(LOG_ERROR, "profiles", "Profile '%s' already exists", canon);
        free(dir); free(canon); return NULL;
    }

    if (!mkdir_p(dir, 0755)) {
        hermes_log(LOG_ERROR, "profiles", "Failed to create profile dir '%s'", dir);
        free(dir); free(canon); return NULL;
    }

    /* seed subdirs */
    const char *subdirs[] = { "skills", "sessions", "memory", "cache", ".env", NULL };
    for (int i = 0; subdirs[i]; i++) {
        char *sub = malloc(strlen(dir) + 1 + strlen(subdirs[i]) + 1);
        if (!sub) continue;
        snprintf(sub, strlen(dir) + 1 + strlen(subdirs[i]) + 1, "%s/%s", dir, subdirs[i]);
        mkdir_p(sub, 0755);
        free(sub);
    }

    /* seed .env */
    char *env_path = malloc(strlen(dir) + 6);
    if (env_path) {
        snprintf(env_path, strlen(dir) + 6, "%s/.env", dir);
        FILE *f = fopen(env_path, "w");
        if (f) {
            fputs("# Per-profile secrets for this Hermes profile.\n"
                  "# API keys and tokens set here override the shell environment.\n"
                  "# Behavioral settings belong in config.yaml, not here.\n", f);
            fclose(f);
            chmod(env_path, 0600);
        }
        free(env_path);
    }

    /* seed SOUL.md */
    char *soul_path = malloc(strlen(dir) + 8);
    if (soul_path) {
        snprintf(soul_path, strlen(dir) + 8, "%s/SOUL.md", dir);
        FILE *f = fopen(soul_path, "w");
        if (f) {
            fputs("# SOUL.md - the personality guiding this Hermes profile.\n"
                  "#\n"
                  "# Edit this file to shape how Hermes speaks and behaves.\n", f);
            fclose(f);
        }
        free(soul_path);
    }

    /* write active_profile */
    char *active_path = malloc(strlen(dir) + 14);
    if (active_path) {
        snprintf(active_path, strlen(dir) + 14, "%s/active_profile", dir);
        FILE *f = fopen(active_path, "w");
        if (f) { fprintf(f, "%s\n", canon); fclose(f); }
        free(active_path);
    }

    hermes_log(LOG_INFO, "profiles", "Created profile '%s' at %s", canon, dir);
    char *result = xstrdup(dir);
    free(dir);
    free(canon);
    return result;
}

/* ── profile_delete_profile ────────────────────────────────────── */
/*
 * PoP: profile_delete_profile @ hermes_cli/profiles.py:delete_profile
 */
bool profile_delete_profile(const char *name) {
    char *canon = profile_normalize_name(name);
    if (!canon) return false;

    if (strcmp(canon, "default") == 0) {
        hermes_log(LOG_ERROR, "profiles", "Cannot delete default profile");
        free(canon); return false;
    }

    char *dir = gap_profile_dir_for(canon);
    if (!dir) { free(canon); return false; }

    if (!path_exists(dir)) {
        hermes_log(LOG_ERROR, "profiles", "Profile '%s' does not exist", canon);
        free(dir); free(canon); return false;
    }

    /* Best-effort recursive delete */
    bool ok = false;
    for (int attempt = 0; attempt < 3; attempt++) {
        ok = (rmdir(dir) == 0 || errno == ENOENT);
        if (ok) break;
        DIR *d = opendir(dir);
        if (!d) break;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            char *child = malloc(strlen(dir) + 2 + strlen(ent->d_name) + 1);
            if (!child) continue;
            snprintf(child, strlen(dir) + 2 + strlen(ent->d_name) + 1, "%s/%s", dir, ent->d_name);
            if (ent->d_type == DT_DIR) {
                profile_delete_profile(ent->d_name);
            } else {
                unlink(child);
            }
            free(child);
        }
        closedir(d);
        errno = 0;
        if (rmdir(dir) == 0) { ok = true; break; }
        if (errno == ENOENT) { ok = true; break; }
        if (errno != ENOTEMPTY && errno != EEXIST) break;
        usleep(300000 * (attempt + 1));
    }

    if (ok) hermes_log(LOG_INFO, "profiles", "Deleted profile '%s'", canon);
    else     hermes_log(LOG_ERROR, "profiles", "Failed to delete profile '%s'", canon);

    free(dir);
    free(canon);
    return ok;
}

/* ── profile_export_profile ────────────────────────────────────── */
/*
 * PoP: profile_export_profile @ hermes_cli/profiles.py:export_profile
 */
char *profile_export_profile(const char *name) {
    char *canon = profile_normalize_name(name);
    if (!canon) return NULL;

    char *dir = gap_profile_dir_for(canon);
    if (!dir || !path_exists(dir)) {
        free(dir); free(canon);
        hermes_log(LOG_ERROR, "profiles", "Profile '%s' not found for export", canon);
        return NULL;
    }

    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char stamp[64];
    strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm_buf);

    char archive_path[PATH_MAX];
    snprintf(archive_path, sizeof(archive_path),
             "/tmp/hermes_profile_%s_%s.tar", canon, stamp);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "tar cf \"%s\" -C \"%s/..\" \"%s\" 2>/dev/null",
             archive_path, dir, canon);

    int rc = system(cmd);
    if (rc != 0 || !path_exists(archive_path)) {
        hermes_log(LOG_ERROR, "profiles", "tar failed for profile '%s' export", canon);
        free(dir); free(canon); return NULL;
    }

    hermes_log(LOG_INFO, "profiles", "Exported profile '%s' -> %s", canon, archive_path);
    char *result = xstrdup(archive_path);
    free(dir);
    free(canon);
    return result;
}

/* ── profile_import_profile ────────────────────────────────────── */
/*
 * PoP: profile_import_profile @ hermes_cli/profiles.py:import_profile
 */
bool profile_import_profile(const char *archive_path, const char *name) {
    char *canon = profile_normalize_name(name);
    if (!canon || !archive_path || !path_exists(archive_path)) {
        free(canon);
        hermes_log(LOG_ERROR, "profiles", "Cannot import profile - invalid args");
        return false;
    }

    char *dest = gap_profile_dir_for(canon);
    if (!dest) { free(canon); return false; }

    if (path_exists(dest)) {
        hermes_log(LOG_ERROR, "profiles", "Profile '%s' already exists - remove first", canon);
        free(dest); free(canon); return false;
    }

    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "tar xf \"%s\" -C \"%s/..\" 2>/dev/null",
             archive_path, dest);

    int rc = system(cmd);
    if (rc != 0 || !path_exists(dest)) {
        hermes_log(LOG_ERROR, "profiles", "tar import failed for profile '%s'", canon);
        free(dest); free(canon);
        return false;
    }

    hermes_log(LOG_INFO, "profiles", "Imported profile '%s' from %s", canon, archive_path);
    free(dest);
    free(canon);
    return true;
}

/* ── profile_rename_profile ────────────────────────────────────── */
/*
 * PoP: profile_rename_profile @ hermes_cli/profiles.py:rename_profile
 */
bool profile_rename_profile(const char *old_name, const char *new_name) {
    char *old_canon = profile_normalize_name(old_name);
    char *new_canon = profile_normalize_name(new_name);
    if (!old_canon || !new_canon || strcmp(old_canon, new_canon) == 0) {
        free(old_canon); free(new_canon);
        return false;
    }

    if (strcmp(new_canon, "default") == 0) {
        hermes_log(LOG_ERROR, "profiles", "Cannot rename to 'default'");
        free(old_canon); free(new_canon); return false;
    }

    char *old_dir = gap_profile_dir_for(old_canon);
    char *new_dir = gap_profile_dir_for(new_canon);
    if (!old_dir || !new_dir || !path_exists(old_dir)) {
        hermes_log(LOG_ERROR, "profiles", "Source profile '%s' not found", old_name);
        free(old_dir); free(new_dir); free(old_canon); free(new_canon);
        return false;
    }

    if (path_exists(new_dir)) {
        hermes_log(LOG_ERROR, "profiles", "Target profile '%s' already exists", new_name);
        free(old_dir); free(new_dir); free(old_canon); free(new_canon);
        return false;
    }

    if (rename(old_dir, new_dir) != 0) {
        hermes_log(LOG_ERROR, "profiles", "rename(%s, %s) failed: %s", old_dir, new_dir, strerror(errno));
        free(old_dir); free(new_dir); free(old_canon); free(new_canon);
        return false;
    }

    /* patch active_profile if needed */
    char *active_path = NULL;
    char *home = slermes_home();
    if (home) {
        active_path = malloc(strlen(home) + 17);
        if (active_path) snprintf(active_path, strlen(home) + 17, "%s/active_profile", home);
    }
    if (active_path && path_exists(active_path)) {
        FILE *f = fopen(active_path, "r");
        if (f) {
            char buf[256];
            if (fgets(buf, sizeof(buf), f)) {
                buf[strcspn(buf, "\n")] = '\0';
                if (strcmp(buf, old_canon) == 0) {
                    fclose(f);
                    f = fopen(active_path, "w");
                    if (f) { fprintf(f, "%s\n", new_canon); fclose(f); }
                } else {
                    fclose(f);
                }
            } else {
                fclose(f);
            }
        }
        free(active_path);
    }
    free(home);

    hermes_log(LOG_INFO, "profiles", "Renamed profile '%s' -> '%s'", old_name, new_name);
    free(old_dir); free(new_dir);
    free(old_canon); free(new_canon);
    return true;
}

/* ── profile_archive_profile ───────────────────────────────────── */
/*
 * PoP: profile_archive_profile @ hermes_cli/profiles.py:archive_profile
 */
char *profile_archive_profile(const char *name) {
    return profile_export_profile(name);
}

/* ── profile_describe_profile ──────────────────────────────────── */
/*
 * PoP: profile_describe_profile @ hermes_cli/profiles.py:describe_profile
 */
char *profile_describe_profile(const char *name) {
    char *canon = profile_normalize_name(name);
    if (!canon) return NULL;

    char *dir = gap_profile_dir_for(canon);
    if (!dir || !path_exists(dir)) {
        free(dir); free(canon);
        return xstrdup("(profile not found)");
    }

    char *model = NULL, *provider = NULL, *desc = NULL, *dist_name = NULL, *dist_ver = NULL;
    profile_read_config_model(dir, &model, &provider);
    profile_read_profile_meta(dir, &desc, NULL);

    char *buf = malloc(4096);
    if (!buf) { free(dir); free(canon); free(model); free(provider); free(desc); return NULL; }

    int pos = 0;
    pos += snprintf(buf + pos, 4096 - pos, "Profile: %s\n", canon);
    pos += snprintf(buf + pos, 4096 - pos, "Path:    %s\n", dir);
    if (model) pos += snprintf(buf + pos, 4096 - pos, "Model:   %s", model);
    if (provider) pos += snprintf(buf + pos, 4096 - pos, " (%s)", provider);
    if (model || provider) pos += snprintf(buf + pos, 4096 - pos, "\n");
    if (desc)    pos += snprintf(buf + pos, 4096 - pos, "Desc:    %s\n", desc);

    int skill_count = profile_count_skills(dir);
    if (skill_count >= 0) {
        pos += snprintf(buf + pos, 4096 - pos, "Skills:  %d\n", skill_count);
    }

    char *ddir_name = NULL, *ddir_ver = NULL, *ddir_src = NULL;
    profile_read_distribution_meta(dir, &ddir_name, &ddir_ver, &ddir_src);
    if (ddir_name) {
        pos += snprintf(buf + pos, 4096 - pos, "Dist:    %s", ddir_name);
        if (ddir_ver) pos += snprintf(buf + pos, 4096 - pos, "@%s", ddir_ver);
        pos += snprintf(buf + pos, 4096 - pos, "\n");
        if (ddir_src) pos += snprintf(buf + pos, 4096 - pos, "From:    %s\n", ddir_src);
    }

    free(model); free(provider); free(desc);
    free(ddir_name); free(ddir_ver); free(ddir_src);
    free(dir);
    free(canon);
    return buf;
}

/* ── profile_list_profiles ─────────────────────────────────────── */
/*
 * PoP: profile_list_profiles @ hermes_cli/profiles.py:list_profiles
 */
char *profile_list_profiles(void) {
    char *root = profile_profiles_root();
    if (!root || !path_exists(root)) {
        free(root);
        return xstrdup("(no profiles directory)");
    }

    char *buf = malloc(8192);
    if (!buf) { free(root); return NULL; }
    int pos = 0;
    pos += snprintf(buf + pos, 8192 - pos, "Profiles:\n");

    DIR *d = opendir(root);
    if (!d) { free(root); free(buf); return xstrdup("(cannot list profiles)"); }

    struct dirent *ent;
    bool any = false;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.' || ent->d_type != DT_DIR) continue;
        bool valid = true;
        for (const char *p = ent->d_name; *p; p++) {
            if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_') {
                valid = false; break;
            }
        }
        if (!valid) continue;
        any = true;

        char *dp = malloc(strlen(root) + 1 + strlen(ent->d_name) + 1);
        if (!dp) continue;
        snprintf(dp, strlen(root) + 1 + strlen(ent->d_name) + 1, "%s/%s", root, ent->d_name);

        char *md = NULL, *pd = NULL;
        profile_read_profile_meta(dp, &md, NULL);

        pos += snprintf(buf + pos, 8192 - pos, "  - %s", ent->d_name);
        if (md) pos += snprintf(buf + pos, 8192 - pos, "  (%s)", md);
        pos += snprintf(buf + pos, 8192 - pos, "\n");

        free(md);
        free(dp);
    }
    closedir(d);

    if (!any) {
        pos += snprintf(buf + pos, 8192 - pos, "  (none)\n");
    }

    free(root);
    return buf;
}
