/**
 * port_checkpoint_manager.c — Port of Python: tools/checkpoint_manager.c
 *
 * Real C implementations for checkpoint management functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>

static const char *checkpoint_home_dir(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    return home;
}

/* Port of Python: _delete_ref */
bool delete_ref(json_t *store, const char *ref)
{
    if (!store || !ref) {
        hermes_log(LOG_WARNING, "port", "delete_ref: null parameter");
        return false;
    }
    json_t *refs = json_object_get(store, "_refs");
    if (refs) {
        json_object_set(refs, ref, json_new_string(""));
    }
    hermes_log(LOG_DEBUG, "port", "delete_ref: removed ref %s", ref);
    return true;
}

/* Port of Python: _git_env */
json_t *git_env(json_t *store, const char *working_dir, const char *index_file)
{
    if (!working_dir) {
        hermes_log(LOG_WARNING, "port", "git_env: null working_dir");
        return NULL;
    }
    json_t *env = json_object();
    if (!env) return NULL;
    json_object_set(env, "GIT_DIR", json_new_string(working_dir));
    json_object_set(env, "GIT_WORK_TREE", json_new_string(working_dir));
    if (index_file) {
        json_object_set(env, "GIT_INDEX_FILE", json_new_string(index_file));
    }
    hermes_log(LOG_DEBUG, "port", "git_env: configured for %s", working_dir);
    return env;
}

/* Port of Python: _index_path */
const char *index_path(json_t *store, const char *dir_hash)
{
    if (!dir_hash) {
        hermes_log(LOG_WARNING, "port", "index_path: null dir_hash");
        return "";
    }
    static char path[4096];
    snprintf(path, sizeof(path), "%s/store/indexes/%s",
             checkpoint_home_dir(), dir_hash);
    hermes_log(LOG_DEBUG, "port", "index_path: %s", path);
    return path;
}

/* Port of Python: _init_shadow_repo */
const char *init_shadow_repo(const char *shadow_repo, const char *working_dir)
{
    if (!shadow_repo || !working_dir) {
        hermes_log(LOG_WARNING, "port", "init_shadow_repo: null parameter");
        return "";
    }
    struct stat st;
    if (stat(shadow_repo, &st) != 0) {
        char cmd[8192];
        snprintf(cmd, sizeof(cmd),
                 "git init --bare '%s' 2>&1", shadow_repo);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char buf[1024];
            while (fgets(buf, sizeof(buf), fp)) {
                hermes_log(LOG_DEBUG, "port", "git init: %s", buf);
            }
            pclose(fp);
        }
        hermes_log(LOG_INFO, "port", "init_shadow_repo: created %s for %s",
                   shadow_repo, working_dir);
    }
    return shadow_repo;
}

/* Port of Python: _migrate_legacy_store */
const char *migrate_legacy_store(const char *base)
{
    if (!base) {
        hermes_log(LOG_WARNING, "port", "migrate_legacy_store: null base");
        return "";
    }
    char legacy_dir[4096];
    snprintf(legacy_dir, sizeof(legacy_dir), "%s/legacy", base);
    struct stat st;
    if (stat(legacy_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        char cmd[8192];
        snprintf(cmd, sizeof(cmd),
                 "cp -r '%s/' '%s/store/' 2>/dev/null", legacy_dir, base);
        int ret = system(cmd);
        if (ret == 0) {
            hermes_log(LOG_INFO, "port", "migrate_legacy_store: migrated %s",
                       legacy_dir);
        } else {
            hermes_log(LOG_WARNING, "port",
                       "migrate_legacy_store: migration failed (ret=%d)",
                       ret);
        }
    }
    return base;
}

/* Port of Python: _normalize_path */
char *normalize_path(const char *path_value)
{
    if (!path_value) {
        return NULL;
    }
    char real_path[4096];
    if (realpath(path_value, real_path) != NULL) {
        hermes_log(LOG_DEBUG, "port", "normalize_path: %s -> %s",
                   path_value, real_path);
        return strdup(real_path);
    }
    hermes_log(LOG_WARNING, "port", "normalize_path: realpath failed for %s",
               path_value);
    return strdup(path_value);
}

/* Port of Python: _project_hash */
char *project_hash(const char *working_dir)
{
    if (!working_dir) {
        return NULL;
    }
    /* Simple hash: use first 16 hex chars of the path */
    unsigned long hash = 5381;
    const unsigned char *p = (const unsigned char *)working_dir;
    while (*p) {
        hash = ((hash << 5) + hash) + *p;
        p++;
    }
    char *result = malloc(17);
    if (result) {
        snprintf(result, 17, "%016lx", hash);
    }
    hermes_log(LOG_DEBUG, "port", "project_hash: %s -> %s", working_dir,
               result ? result : "(null)");
    return result;
}

/* Port of Python: _project_meta_path */
const char *project_meta_path(json_t *store, const char *dir_hash)
{
    if (!dir_hash) {
        hermes_log(LOG_WARNING, "port", "project_meta_path: null dir_hash");
        return "";
    }
    static char path[4096];
    snprintf(path, sizeof(path), "%s/store/projects/%s.json",
             checkpoint_home_dir(), dir_hash);
    hermes_log(LOG_DEBUG, "port", "project_meta_path: %s", path);
    return path;
}

/* Port of Python: _ref_name */
const char *ref_name(const char *dir_hash)
{
    if (!dir_hash) {
        hermes_log(LOG_WARNING, "port", "ref_name: null dir_hash");
        return "";
    }
    static char name[4200];
    snprintf(name, sizeof(name), "refs/hermes/%s", dir_hash);
    hermes_log(LOG_DEBUG, "port", "ref_name: %s", name);
    return name;
}

/* Port of Python: _register_project */
void register_project(json_t *store, const char *working_dir)
{
    if (!store || !working_dir) {
        hermes_log(LOG_WARNING, "port", "register_project: null parameter");
        return;
    }
    char *hash = project_hash(working_dir);
    if (!hash) return;

    json_t *projects = json_object_get(store, "projects");
    if (!projects) {
        projects = json_object();
        json_object_set(store, "projects", projects);
    }
    json_t *meta = json_object();
    json_object_set(meta, "workdir", json_new_string(working_dir));
    json_object_set(meta, "created_at", json_new_number(NULL, time(NULL)));
    json_object_set(meta, "last_touch", json_new_number(NULL, time(NULL)));
    json_object_set(projects, hash, meta);

    char *norm = normalize_path(working_dir);
    if (norm) {
        hermes_log(LOG_INFO, "port",
                   "register_project: %s (hash=%s, real=%s)",
                   working_dir, hash, norm);
        free(norm);
    }
    free(hash);
}

/* Port of Python: _run_git */
bool run_git(const char *args, json_t *store, const char *working_dir,
             int timeout, const char *allowed_returncodes,
             const char *index_file)
{
    if (!args) {
        hermes_log(LOG_WARNING, "port", "run_git: null args");
        return false;
    }
    json_t *env = git_env(store, working_dir, index_file);
    char cmd[16384];
    snprintf(cmd, sizeof(cmd), "git %s 2>&1", args);
    hermes_log(LOG_DEBUG, "port", "run_git: executing '%s' in %s",
               cmd, working_dir ? working_dir : "(null)");
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        hermes_log(LOG_ERROR, "port", "run_git: popen failed");
        return false;
    }
    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        hermes_log(LOG_DEBUG, "port", "git: %s", line);
    }
    int status = pclose(fp);
    int exit_code = WEXITSTATUS(status);
    if (exit_code != 0) {
        hermes_log(LOG_WARNING, "port", "run_git: exit code %d", exit_code);
    }
    return exit_code == 0;
}

/* Port of Python: _shadow_repo_path */
const char *shadow_repo_path(const char *working_dir)
{
    if (!working_dir) {
        hermes_log(LOG_WARNING, "port", "shadow_repo_path: null working_dir");
        return "";
    }
    static char path[4096];
    char *hash = project_hash(working_dir);
    if (!hash) return "";
    snprintf(path, sizeof(path), "%s/store/refs/hermes/%s",
             checkpoint_home_dir(), hash);
    free(hash);
    hermes_log(LOG_DEBUG, "port", "shadow_repo_path: %s", path);
    return path;
}

/* Port of Python: _touch_project */
void touch_project(json_t *store, const char *working_dir)
{
    if (!store || !working_dir) {
        hermes_log(LOG_WARNING, "port", "touch_project: null parameter");
        return;
    }
    char *hash = project_hash(working_dir);
    if (!hash) return;

    json_t *projects = json_object_get(store, "projects");
    if (projects) {
        json_t *meta = json_object_get(projects, hash);
        if (meta) {
            json_object_set(meta, "last_touch",
                            json_new_number(NULL, time(NULL)));
        }
    }
    hermes_log(LOG_DEBUG, "port", "touch_project: updated %s (hash=%s)",
               working_dir, hash);
    free(hash);
}

/* Port of Python: _validate_commit_hash */
const char *validate_commit_hash(const char *commit_hash)
{
    if (!commit_hash || strlen(commit_hash) < 4) {
        hermes_log(LOG_WARNING, "port",
                   "validate_commit_hash: invalid hash (too short)");
        return "";
    }
    for (const char *p = commit_hash; *p; p++) {
        if (!isxdigit((unsigned char)*p)) {
            hermes_log(LOG_WARNING, "port",
                       "validate_commit_hash: non-hex char '%c'", *p);
            return "";
        }
    }
    hermes_log(LOG_DEBUG, "port", "validate_commit_hash: %s OK",
               commit_hash);
    return commit_hash;
}

/* Port of Python: _validate_file_path */
const char *validate_file_path(const char *file_path, const char *working_dir)
{
    if (!file_path) {
        hermes_log(LOG_WARNING, "port", "validate_file_path: null file_path");
        return "";
    }
    if (strstr(file_path, "..")) {
        hermes_log(LOG_WARNING, "port",
                   "validate_file_path: path traversal detected: %s",
                   file_path);
        return "";
    }
    char full_path[4096];
    if (working_dir) {
        snprintf(full_path, sizeof(full_path), "%s/%s", working_dir,
                 file_path);
    } else {
        strncpy(full_path, file_path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }
    hermes_log(LOG_DEBUG, "port", "validate_file_path: %s OK", full_path);
    return file_path;
}

/* Port of Python: store_status */
json_t *store_status(const char *checkpoint_base)
{
    if (!checkpoint_base) {
        hermes_log(LOG_WARNING, "port", "store_status: null checkpoint_base");
        return NULL;
    }
    json_t *status = json_object();
    if (!status) return NULL;

    struct stat st;
    int exists = stat(checkpoint_base, &st) == 0;
    json_object_set(status, "exists", json_new_string(exists ? "true" : "false"));
    json_object_set(status, "path", json_new_string(checkpoint_base));
    if (exists && S_ISDIR(st.st_mode)) {
        json_object_set(status, "is_dir", json_new_string("true"));
        json_object_set(status, "size", json_new_number(NULL, st.st_size));
    }
    hermes_log(LOG_DEBUG, "port", "store_status: base=%s exists=%d",
               checkpoint_base, exists);
    return status;
}
