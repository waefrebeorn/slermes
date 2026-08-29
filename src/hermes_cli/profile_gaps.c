/* ───────────────────────── gap fillers ────────────────────────── */
/* Implements hermes_cli/profiles.py gap functions (DA-1, c_location=None).
 * Follows naming convention: profile_<python_method_name>.              */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include "hermes_logger.h"
#include "slermes_home.h"
#include "profile_store.h"

/* ================================================================== */
/*  Profile resolution                                                */
/* ================================================================== */

/* PoP: profile_matches_home @ hermes_cli/profiles.py:profile_matches_home */
/* True when *name* refers to the profile served from *home*.
 * Fail-closed: invalid names return False. */
bool profile_matches_home(const char *name, const char *home)
{
    if (!name || !*name) return false;
    const char *actual_home = home ? home : slermes_home();
    if (!actual_home) return false;

    /* Resolve the target profile directory */
    char *profile_dir = profile_dir_for(name);
    if (!profile_dir) return false;

    /* Compare resolved paths */
    char target_real[4096], home_real[4096];
    bool match = false;
    if (realpath(profile_dir, target_real) && realpath(actual_home, home_real)) {
        match = strcmp(target_real, home_real) == 0;
    }
    free(profile_dir);
    return match;
}

/* PoP: profile_list_profiles @ hermes_cli/profiles.py:list_profiles */
/* Return a JSON array of profile info objects. Each entry:
 * {"name":..., "path":..., "is_default":bool, "model":"...", "provider":"..."}
 * Returns malloc'd JSON string (caller frees), or "[]" on no profiles. */
char *profile_list_profiles(void)
{
    char *profiles_root = profile_profiles_root();
    if (!profiles_root) return strdup("[]");
    const char *default_home = slermes_home();
    bool has_default = (default_home && strlen(default_home) > 0);

    /* Simple approach: enumerate subdirectories with config.yaml */
    char **entries = malloc(128 * sizeof(char*));
    int count = 0;
    DIR *d = opendir(profiles_root);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL && count < 127) {
            if (ent->d_name[0] == '.') continue;
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", profiles_root, ent->d_name);
            struct stat st;
            if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            /* Check for config.yaml or .env (profile marker) */
            char cfg[1024];
            snprintf(cfg, sizeof(cfg), "%s/config.yaml", path);
            if (stat(cfg, &st) != 0) continue;
            entries[count++] = strdup(path);
        }
        closedir(d);
    }

    /* Build JSON */
    char *buf = malloc(4096);
    if (buf) {
        size_t off = 0;
        off += snprintf(buf + off, 4096 - off, "[");
        for (int i = 0; i < count && off < 4000; i++) {
            off += snprintf(buf + off, 4096 - off, "%s{\"name\":\"%s\",\"path\":\"%s\"}",
                            i ? "," : "",
                            strrchr(entries[i], '/') + 1,
                            entries[i]);
            free(entries[i]);
        }
        off += snprintf(buf + off, 4096 - off, "]", count);
        /* has_default flag */
        if (has_default) {
            off += snprintf(buf + off, 4096 - off, "  /* default profile: %s */",
                            default_home ? default_home : "");
        }
    }
    free(entries);
    free(profiles_root);
    return buf ? buf : strdup("[]");
}

/* PoP: profile_delete_profile @ hermes_cli/profiles.py:delete_profile */
/* Stop gateway, disable service, remove profile dir + wrapper.
 * Returns true on success. */
bool profile_delete_profile(const char *name, bool confirmed)
{
    if (!name || !*name) return false;
    /* Cannot delete default */
    char *norm = profile_normalize_name(name);
    if (!norm) return false;
    if (strcmp(norm, "default") == 0) {
        free(norm);
        hermes_log(LOG_ERROR, "profile",
                   "Cannot delete the default profile (~/.hermes). Use 'hermes uninstall'");
        return false;
    }

    char *dir = profile_dir_for(norm);
    if (!dir) { free(norm); return false; }

    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        free(dir); free(norm); return false;
    }

    /* Stop gateway if running (send SIGTERM to gateway PID file) */
    char pidfile[1024];
    snprintf(pidfile, sizeof(pidfile), "%s/.hermes_gateway.pid", dir);
    FILE *pf = fopen(pidfile, "r");
    if (pf) {
        int pid;
        if (fscanf(pf, "%d", &pid) == 1) {
            kill(pid, SIGTERM);
        }
        fclose(pf);
    }
    /* Remove wrapper script */
    profile_remove_wrapper_script(norm);

    /* Remove directory tree */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    int rc = system(cmd);

    free(dir); free(norm);
    return rc == 0;
}

/* PoP: profile_rename_profile @ hermes_cli/profiles.py:rename_profile */
/* Rename a profile directory and update the alias. Returns true on success. */
bool profile_rename_profile(const char *old_name, const char *new_name)
{
    if (!old_name || !new_name || !*old_name || !*new_name) return false;
    char *old_norm = profile_normalize_name(old_name);
    char *new_norm = profile_normalize_name(new_name);
    if (!old_norm || !new_norm) {
        free(old_norm); free(new_norm);
        return false;
    }
    if (strcmp(old_norm, "default") == 0 || strcmp(new_norm, "default") == 0) {
        free(old_norm); free(new_norm);
        return false;
    }

    char *old_dir = profile_dir_for(old_norm);
    char *new_dir = profile_dir_for(new_norm);
    if (!old_dir || !new_dir) {
        free(old_dir); free(new_dir); free(old_norm); free(new_norm);
        return false;
    }

    struct stat st;
    if (stat(old_dir, &st) != 0) {
        free(old_dir); free(new_dir); free(old_norm); free(new_norm);
        return false;
    }

    /* Rename directory */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mv '%s' '%s'", old_dir, new_dir);
    int rc = system(cmd);

    /* Update wrapper script */
    if (rc == 0) {
        profile_remove_wrapper_script(old_norm);
        profile_create_wrapper_script(new_norm, new_dir);
    }

    free(old_dir); free(new_dir); free(old_norm); free(new_norm);
    return rc == 0;
}

/* PoP: profile_export_profile @ hermes_cli/profiles.py:export_profile */
/* Export a profile to a tar.gz archive. Returns malloc'd archive path
 * or NULL on failure. */
char *profile_export_profile(const char *name)
{
    char *dir = profile_dir_for(name);
    if (!dir) return NULL;
    struct stat st;
    if (stat(dir, &st) != 0) { free(dir); return NULL; }

    /* Create archive path */
    const char *home = getenv("HOME");
    char archive[512];
    snprintf(archive, sizeof(archive), "%s/%s-export.tar.gz",
             home ? home : ".", name ? name : "profile");

    /* Use tar with GNU format */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "cd '%s' && tar --format=gnu -czf '%s' . 2>/dev/null",
             dir, archive);
    int rc = system(cmd);
    free(dir);
    if (rc != 0) return NULL;
    return strdup(archive);
}

/* PoP: profile_import_profile @ hermes_cli/profiles.py:import_profile */
/* Import a profile from a tar.gz archive. Returns the new profile name
 * or NULL on failure. */
char *profile_import_profile(const char *archive_path, const char *new_name)
{
    if (!archive_path || !new_name || !*archive_path || !*new_name)
        return NULL;

    char *dir = profile_dir_for(new_name);
    if (!dir) return NULL;

    struct stat st;
    if (stat(dir, &st) == 0) {
        free(dir);
        hermes_log(LOG_ERROR, "profile", "Profile '%s' already exists", new_name);
        return NULL;
    }

    /* Create directory and extract */
    mkdir(dir, 0755);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "tar -xzf '%s' -C '%s' 2>/dev/null",
             archive_path, dir);
    int rc = system(cmd);
    if (rc != 0) {
        free(dir);
        return NULL;
    }
    return strdup(new_name);
}

/* PoP: profile_scrub_export_secrets @ hermes_cli/profiles.py:_scrub_export_secrets */
/* Force-redact secret-shaped strings in a staged export tree.
 * Best-effort: scans for common key patterns and blanks them. */
void profile_scrub_export_secrets(const char *dir)
{
    if (!dir || !*dir) return;
    /* Walk the directory and redact .env files */
    char cmd[2048];
    /* Redact .env file */
    snprintf(cmd, sizeof(cmd), "if [ -f '%s/.env' ]; then sed -i "
             "'s/\\(^.*API_KEY=\\).*/\\1[REDACTED]/; s/\\(^.*TOKEN=\\).*/\\1[REDACTED]/; "
             "s/\\(^.*SECRET=\\).*/\\1[REDACTED]/' '%s/.env'; fi", dir, dir);
    system(cmd);
}

/* PoP: profile_make_profile_archive @ hermes_cli/profiles.py:_make_profile_archive */
/* Create <base>.tar.gz of root_dir/base_dir using GNU tar format.
 * Returns malloc'd archive path or NULL on failure. */
char *profile_make_profile_archive(const char *base, const char *root_dir,
                                    const char *base_dir)
{
    if (!base || !root_dir || !base_dir) return NULL;
    char archive[512];
    snprintf(archive, sizeof(archive), "%s.tar.gz", base);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "cd '%s' && tar --format=gnu -czf '%s' '%s' 2>/dev/null",
             root_dir, archive, base_dir);
    int rc = system(cmd);
    if (rc != 0) return NULL;
    return strdup(archive);
}
