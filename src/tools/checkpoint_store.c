/*
 * checkpoint_store.c - checkpoint store paths and git invocation.
 *
 * Split from checkpoint_manager.c (soul.md: self-contained modules).
 * Store/shadow-repo path construction and the git subprocess wrappers. No
 * file-scope state.
 */
#include "checkpoint_store.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>


/* PoP: _project_hash @ tools/checkpoint_manager.py:_project_hash */ void _project_hash(const char *working_dir, char *out_hash, size_t out_len) {
    if (!working_dir) {
        snprintf(out_hash, out_len, "default");
        return;
    }
    /* Simple hash based on path */
    unsigned long hash = 5381;
    for (const char *p = working_dir; *p; p++) {
        hash = ((hash << 5) + hash) + *p;
    }
    snprintf(out_hash, out_len, "%lx", hash);
}


/* PoP: _store_path @ tools/checkpoint_manager.py:_store_path */ void _store_path(const char *base, char *out_path, size_t out_len) {
    if (base && base[0]) {
        snprintf(out_path, out_len, "%s/%s", base, CHECKPOINT_DIR);
    } else {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(out_path, out_len, "%s/%s", home, CHECKPOINT_DIR);
        } else {
            snprintf(out_path, out_len, "./%s", CHECKPOINT_DIR);
        }
    }
}


/* PoP: _shadow_repo_path @ tools/checkpoint_manager.py:_shadow_repo_path */ void _shadow_repo_path(const char *working_dir, char *out_path, size_t out_len) {
    char store[MAX_PATH];
    _store_path(NULL, store, sizeof(store));
    char dir_hash[64];
    _project_hash(working_dir, dir_hash, sizeof(dir_hash));
    snprintf(out_path, out_len, "%s/shadow/%s", store, dir_hash);
}


/* PoP: _index_path @ tools/checkpoint_manager.py:_index_path */ void _index_path(const char *base, const char *dir_hash, char *out_path, size_t out_len) {
    _store_path(base, out_path, out_len);
    char *p = out_path + strlen(out_path);
    snprintf(p, out_len - (p - out_path), "/index-%s.json", dir_hash);
}


/* PoP: _ref_name @ tools/checkpoint_manager.py:_ref_name */ void _ref_name(const char *dir_hash, char *out_ref, size_t out_len) {
    snprintf(out_ref, out_len, "refs/checkpoints/%s", dir_hash);
}


/* PoP: _project_meta_path @ tools/checkpoint_manager.py:_project_meta_path */ void _project_meta_path(const char *base, const char *dir_hash, char *out_path, size_t out_len) {
    _store_path(base, out_path, out_len);
    char *p = out_path + strlen(out_path);
    snprintf(p, out_len - (p - out_path), "/meta-%s.json", dir_hash);
}


/* PoP: _git_env @ tools/checkpoint_manager.py:_git_env */
static void _git_env(void) {
    setenv("GIT_DIR", ".", 1);
    setenv("GIT_WORK_TREE", ".", 1);
}


/* PoP: _run_git @ tools/checkpoint_manager.py:_run_git */ bool _run_git(const char *repo_path, const char *cmd, char *out, size_t out_len) {
    char full_cmd[MAX_PATH * 2];
    snprintf(full_cmd, sizeof(full_cmd), "cd '%s' && git %s 2>&1", repo_path, cmd);
    FILE *fp = popen(full_cmd, "r");
    if (!fp) return false;
    if (out && out_len > 0) {
        size_t n = fread(out, 1, out_len - 1, fp);
        out[n] = '\0';
    }
    pclose(fp);
    return true;
}


/* Run git against the bare shadow repo with the real worktree + per-project
 * index. Returns true if popen succeeded. */ bool _run_git_idx(const char *shadow_repo, const char *working_dir,
                          const char *index_file, const char *cmd,
                          char *out, size_t out_len) {
    char full_cmd[MAX_PATH * 3];
    char env[MAX_PATH * 3];
    snprintf(env, sizeof(env), "GIT_DIR='%s' GIT_WORK_TREE='%s'",
             shadow_repo, working_dir ? working_dir : ".");
    if (index_file && index_file[0]) {
        size_t l = strlen(env);
        snprintf(env + l, sizeof(env) - l, " GIT_INDEX_FILE='%s'", index_file);
    }
    snprintf(full_cmd, sizeof(full_cmd), "%s git %s 2>&1", env, cmd);
    FILE *fp = popen(full_cmd, "r");
    if (!fp) return false;
    if (out && out_len > 0) {
        size_t n = fread(out, 1, out_len - 1, fp);
        out[n] = '\0';
    }
    pclose(fp);
    return true;
}
