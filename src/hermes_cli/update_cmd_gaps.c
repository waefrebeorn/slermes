/* ───────────────────────── gap fillers ────────────────────────── */
/* Implements hermes_cli/update_cmd.py gap functions that have no C
 * equivalent yet (c_location=None, DA-1). Each follows the naming
 * convention uc_<python_method_without_leading_underscore>.              */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include "hermes_logger.h"

/* ================================================================== */
/*  Concurrent instance management                                    */
/* ================================================================== */

/* PoP: uc_classify_concurrent_instance @ hermes_cli/update_cmd.py:_classify_concurrent_instance */
/* Classify a running process as a concurrent hermes instance.
 * Returns true if the exe basename matches hermes/hermes.exe. */
bool uc_classify_concurrent_instance(const char *exe_path)
{
    if (!exe_path || !*exe_path) return false;
    const char *base = exe_path;
    for (const char *p = exe_path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    return strcasecmp(base, "hermes") == 0 ||
           strcasecmp(base, "hermes.exe") == 0;
}

/* ================================================================== */
/*  SQLite sidecar cleanup                                            */
/* ================================================================== */

/* PoP: uc_clear_stale_sqlite_sidecars @ hermes_cli/update_cmd.py:_clear_stale_sqlite_sidecars */
/* Remove stale SQLite -wal/-journal/-shm sidecar files. Best-effort. */
void uc_clear_stale_sqlite_sidecars(const char *db_path)
{
    if (!db_path || !*db_path) return;
    char sidecar[512];
    snprintf(sidecar, sizeof(sidecar), "%s-wal", db_path);
    unlink(sidecar);
    snprintf(sidecar, sizeof(sidecar), "%s-shm", db_path);
    unlink(sidecar);
    snprintf(sidecar, sizeof(sidecar), "%s-journal", db_path);
    unlink(sidecar);
}

/* ================================================================== */
/*  EOL normalisation                                                 */
/* ================================================================== */

/* PoP: uc_normalise_managed_eol @ hermes_cli/update_cmd.py:_normalise_managed_eol */
/* Normalise EOL in a managed file (CRLF → LF). Returns true if modified. */
bool uc_normalise_managed_eol(const char *path)
{
    if (!path) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return false; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return false; }
    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    buf[rd] = '\0';

    bool changed = false;
    size_t w = 0;
    for (size_t i = 0; i < rd; i++) {
        if (buf[i] == '\r' && i + 1 < rd && buf[i+1] == '\n') {
            changed = true;
            i++;
            buf[w++] = '\n';
        } else {
            buf[w++] = buf[i];
        }
    }
    if (changed) {
        FILE *out = fopen(path, "wb");
        if (out) { fwrite(buf, 1, w, out); fclose(out); }
    }
    free(buf);
    return changed;
}

/* ================================================================== */
/*  Format helpers                                                    */
/* ================================================================== */

/* Update stage constants (mirrors update_cmd.py internal stage ints) */
enum uc_stage {
    UC_STAGE_PREUPDATE = 1,
    UC_STAGE_BACKUP,
    UC_STAGE_FETCH,
    UC_STAGE_MERGE,
    UC_STAGE_POSTUPDATE,
    UC_STAGE_ZIP,
    UC_STAGE_DEPLOY,
};

/* PoP: uc_format_update_failure_stage @ hermes_cli/update_cmd.py:_format_update_failure_stage */
const char *uc_format_update_failure_stage_label(int stage)
{
    switch (stage) {
        case UC_STAGE_PREUPDATE:  return "pre-update";
        case UC_STAGE_BACKUP:     return "backup";
        case UC_STAGE_FETCH:      return "fetch";
        case UC_STAGE_MERGE:      return "merge";
        case UC_STAGE_POSTUPDATE: return "post-update";
        case UC_STAGE_ZIP:        return "zip";
        case UC_STAGE_DEPLOY:     return "deploy";
        default:                  return "unknown";
    }
}

/* PoP: uc_update_complete_message @ hermes_cli/update_cmd.py:_update_complete_message */
/* Completion line with version transition. Returns malloc'd string. */
char *uc_update_complete_message(const char *pre_version)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    char *post_version = NULL;
    if (home) {
        char pvpath[512];
        snprintf(pvpath, sizeof(pvpath), "%s/pyproject.toml", home);
        /* Try reading version from pyproject.toml */
        FILE *f = fopen(pvpath, "r");
        if (f) {
            char line[256];
            bool in_project = false;
            while (fgets(line, sizeof(line), f)) {
                if (line[0] == '[') {
                    in_project = (strcmp(line, "[project]\n") == 0);
                    continue;
                }
                if (in_project && strncmp(line, "version", 7) == 0) {
                    char *eq = strchr(line, '=');
                    if (eq) {
                        eq++;
                        while (*eq && isspace((unsigned char)*eq)) eq++;
                        if (*eq == '"') eq++;
                        char *end = eq;
                        while (*end && *end != '"' && *end != '\n') end++;
                        size_t len = end - eq;
                        if (len > 0) {
                            post_version = malloc(len + 1);
                            if (post_version) {
                                memcpy(post_version, eq, len);
                                post_version[len] = '\0';
                            }
                        }
                    }
                    break;
                }
            }
            fclose(f);
        }
    }

    char *msg = NULL;
    if (pre_version && post_version && strcmp(pre_version, post_version) != 0) {
        asprintf(&msg, "\xe2\x9c\x93 Update complete! (v%s \xe2\x86\x92 v%s)",
                 pre_version, post_version);
    } else if (post_version) {
        asprintf(&msg, "\xe2\x9c\x93 Update complete! (v%s)", post_version);
    } else {
        msg = strdup("\xe2\x9c\x93 Update complete!");
    }
    free(post_version);
    return msg;
}

/* PoP: uc_service_restart_sec @ hermes_cli/update_cmd.py:_service_restart_sec */
/* Read the unit's RestartSec in seconds via systemd. */
double uc_service_restart_sec(const char *svc_name)
{
    if (!svc_name || !*svc_name) return 0.0;
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "systemctl show '%s' -p RestartUSec --value 2>/dev/null", svc_name);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0.0;
    char val[64] = {0};
    if (fgets(val, sizeof(val), fp)) {
        pclose(fp);
        double us = atof(val);
        return us > 0 ? us / 1000000.0 : 0.0;
    }
    pclose(fp);
    return 0.0;
}

/* ================================================================== */
/*  Desktop / gateway process management                              */
/* ================================================================== */

/* PoP: uc_desktop_app_present @ hermes_cli/update_cmd.py:_desktop_app_present */
/* True when the desktop app binary exists in the expected location. */
bool uc_desktop_app_present(const char *home)
{
    if (!home) return false;
    char path[512];
    snprintf(path, sizeof(path), "%s/hermes-desktop", home);
    struct stat st;
    return stat(path, &st) == 0;
}

/* PoP: uc_orphaned_desktop_backend_pids @ hermes_cli/update_cmd.py:_orphaned_desktop_backend_pids */
/* Find desktop backend PIDs that survived a parent exit.
 * Returns malloc'd array of PIDs, *out_count set. Caller frees. */
pid_t *uc_orphaned_desktop_backend_pids(int *out_count)
{
    if (!out_count) return NULL;
    *out_count = 0;
    pid_t *pids = malloc(256 * sizeof(pid_t));
    if (!pids) return NULL;
    int count = 0;
    DIR *d = opendir("/proc");
    if (!d) { free(pids); return NULL; }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && count < 256) {
        if (!isdigit((unsigned char)ent->d_name[0])) continue;
        pid_t pid = atoi(ent->d_name);
        char cmdpath[256];
        snprintf(cmdpath, sizeof(cmdpath), "/proc/%d/cmdline", pid);
        char cmdline[1024] = {0};
        FILE *f = fopen(cmdpath, "r");
        if (f) {
            size_t n = fread(cmdline, 1, sizeof(cmdline)-1, f);
            (void)n;
            fclose(f);
            if (strstr(cmdline, "hermes-desktop")) {
                char statpath[256];
                snprintf(statpath, sizeof(statpath), "/proc/%d/stat", pid);
                FILE *sf = fopen(statpath, "r");
                if (sf) {
                    char statbuf[1024];
                    if (fgets(statbuf, sizeof(statbuf), sf)) {
                        int ppid = 0;
                        char *pp = strrchr(statbuf, ')');
                        if (pp) {
                            sscanf(pp + 2, "%*c %d", &ppid);
                            if (ppid == 1) {
                                pids[count++] = pid;
                            }
                        }
                    }
                    fclose(sf);
                }
            }
        }
    }
    closedir(d);
    *out_count = count;
    return pids;
}
