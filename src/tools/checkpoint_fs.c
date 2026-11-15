/*
 * checkpoint_fs.c - checkpoint store path / dir helpers.
 *
 * Split from checkpoint_manager.c (soul.md: self-contained modules).
 * Path validation / normalisation, dir sizing, git invocation and
 * shortstat parsing. No file-scope state.
 */
#include "checkpoint_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include "checkpoint_store.h"   /* run_git, shadow_repo_path */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>



/* PoP: dir_file_count @ plugins/platforms/whatsapp_cloud/adapter.py:dir_file_count */ int dir_file_count(const char *path) {
    DIR *d = opendir(path);
    if (!d) return 0;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(d))) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            count++;
        }
    }
    closedir(d);
    return count;
}


/* PoP: dir_size_bytes @ plugins/platforms/whatsapp_cloud/adapter.py:dir_size_bytes */ long dir_size_bytes(const char *path) {
    long total = 0;
    DIR *d = opendir(path);
    if (!d) return 0;
    struct dirent *entry;
    while ((entry = readdir(d))) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char full[MAX_PATH];
            snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
            struct stat st;
            if (stat(full, &st) == 0) {
                total += st.st_size;
            }
        }
    }
    closedir(d);
    return total;
}


/* PoP: init_shadow_repo @ plugins/platforms/whatsapp_cloud/adapter.py:init_shadow_repo */
static bool init_shadow_repo(const char *shadow_repo, const char *working_dir) {
    mkdir(shadow_repo, 0755);
    run_git(shadow_repo, "init --bare", NULL, 0);
    /* Configure */
    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "config core.worktree '%s'", working_dir);
    run_git(shadow_repo, cmd, NULL, 0);
    run_git(shadow_repo, "config receive.denyCurrentBranch updateInstead", NULL, 0);
    return true;
}


/* PoP: delete_ref @ plugins/platforms/whatsapp_cloud/adapter.py:delete_ref */
static bool delete_ref(const char *store, const char *ref) {
    char shadow[MAX_PATH];
    shadow_repo_path("", shadow, sizeof(shadow));
    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "update-ref -d %s", ref);
    return run_git(shadow, cmd, NULL, 0);
}


/* PoP: validate_commit_hash @ plugins/platforms/whatsapp_cloud/adapter.py:validate_commit_hash */
bool validate_commit_hash(const char *commit_hash) {
    if (!commit_hash) return false;
    for (const char *p = commit_hash; *p; p++) {
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
              (*p >= 'A' && *p <= 'F'))) return false;
    }
    return strlen(commit_hash) >= 7 && strlen(commit_hash) <= 40;
}


/* PoP: validate_file_path @ plugins/platforms/whatsapp_cloud/adapter.py:validate_file_path */
/* PoP: validate_file_path @ plugins/platforms/whatsapp_cloud/adapter.py:validate_file_path */
bool validate_file_path(const char *file_path, const char *working_dir) {
    (void)working_dir;
    if (!file_path) return false;
    return file_path[0] != '\0' && strstr(file_path, "..") == NULL;
}


/* PoP: _normalize_path @ plugins/platforms/whatsapp_cloud/adapter.py:_normalize_path */
/* PoP: _normalize_path @ plugins/platforms/whatsapp_cloud/adapter.py:_normalize_path */
/* PoP: _normalize_path @ plugins/platforms/whatsapp_cloud/adapter.py:_normalize_path */
char* _normalize_path(const char *path_value) {
    if (!path_value) return strdup(".");
    return realpath(path_value, NULL) ? : strdup(path_value);
}


/* PoP: migrate_legacy_store @ plugins/platforms/whatsapp_cloud/adapter.py:migrate_legacy_store */
char* migrate_legacy_store(const char *base) {
    (void)base;
    return NULL;
}
 bool cm_git_available(void) {
    return system("git --version >/dev/null 2>&1") == 0;
}


/* Parse git --shortstat output into the entry JSON fields. */ void cm_parse_shortstat(const char *stat_line,
                                long *files, long *ins, long *del) {
    *files = 0; *ins = 0; *del = 0;
    if (!stat_line) return;
    const char *p;
    if ((p = strstr(stat_line, " file")) && sscanf(p, " %ld file", files) != 1) *files = 0;
    if ((p = strstr(stat_line, " insertion")) && sscanf(p, " %ld insertion", ins) != 1) *ins = 0;
    if ((p = strstr(stat_line, " deletion")) && sscanf(p, " %ld deletion", del) != 1) *del = 0;
}
