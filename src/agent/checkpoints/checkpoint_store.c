/*
 * checkpoint_store.c - checkpoint store paths and git invocation.
 *
 * Split from checkpoint_manager.c (soul.md: self-contained modules).
 * Store/shadow-repo path construction and the git subprocess wrappers. No
 * file-scope state.
 */
#include "tools/checkpoint_store.h"
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


/* PoP: project_hash @ agent/checkpoints/manager.py:_project_hash */ void project_hash(const char *working_dir, char *out_hash, size_t out_len) {
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


/* PoP: _store_path @ agent/checkpoints/manager.py:_store_path */ void _store_path(const char *base, char *out_path, size_t out_len) {
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


/* PoP: shadow_repo_path @ agent/checkpoints/manager.py:shadow_repo_path */ void shadow_repo_path(const char *working_dir, char *out_path, size_t out_len) {
    char store[MAX_PATH];
    _store_path(NULL, store, sizeof(store));
    char dir_hash[64];
    project_hash(working_dir, dir_hash, sizeof(dir_hash));
    snprintf(out_path, out_len, "%s/shadow/%s", store, dir_hash);
}


/* PoP: _index_path @ agent/checkpoints/manager.py:_index_path */ void _index_path(const char *base, const char *dir_hash, char *out_path, size_t out_len) {
    _store_path(base, out_path, out_len);
    char *p = out_path + strlen(out_path);
    snprintf(p, out_len - (p - out_path), "/index-%s.json", dir_hash);
}


/* PoP: ref_name @ agent/checkpoints/manager.py:_ref_name */ void ref_name(const char *dir_hash, char *out_ref, size_t out_len) {
    snprintf(out_ref, out_len, "refs/checkpoints/%s", dir_hash);
}


/* PoP: project_meta_path @ agent/checkpoints/manager.py:_project_meta_path */ void project_meta_path(const char *base, const char *dir_hash, char *out_path, size_t out_len) {
    _store_path(base, out_path, out_len);
    char *p = out_path + strlen(out_path);
    snprintf(p, out_len - (p - out_path), "/meta-%s.json", dir_hash);
}


/* PoP: git_env_json @ agent/checkpoints/manager.py:_git_env
 * python RETURNS an env dict; it never touches os.environ. The previous C mutated the
 * process environment with GIT_DIR="." / GIT_WORK_TREE=".", so every later popen in the
 * process (unrelated git calls included) was redirected at the wrong repository, and the
 * values were wrong even for this use: git must point at the shared STORE with the real
 * worktree, plus config isolation so a user's commit.gpgsign never fires mid-session.
 * Pure: given the three inputs, produce the assignments. */
json_t *git_env_json(const char *store, const char *working_dir, const char *index_file) {
    json_t *o = json_object();
    if (!o) return NULL;
    json_set(o, "GIT_DIR", json_string(store ? store : "."));
    json_set(o, "GIT_WORK_TREE", json_string(working_dir && *working_dir ? working_dir : "."));
    json_set(o, "GIT_CONFIG_NOSYSTEM", json_string("1"));
    json_set(o, "GIT_CONFIG_GLOBAL", json_string("/dev/null"));
    json_set(o, "GIT_CONFIG_SYSTEM", json_string("/dev/null"));
    if (index_file && *index_file) json_set(o, "GIT_INDEX_FILE", json_string(index_file));
    return o;
}

/* PoP: git_env_value @ agent/checkpoints/manager.py:_git_env
 * Single-key view of the same mapping, so the three values python owns can be
 * compared one at a time (python also carries config-isolation vars sourced from
 * build_subprocess_env, which are added here unconditionally). */
const char *git_env_value(const char *key, const char *store, const char *working_dir,
                          const char *index_file) {
    static char buf[4096];
    json_t *o = git_env_json(store, working_dir, index_file);
    if (!o || !key) { if (o) json_free(o); return NULL; }
    const json_t *v = json_obj_get(o, key);
    if (!v || v->type != JSON_STRING) { json_free(o); return NULL; }
    snprintf(buf, sizeof buf, "%s", v->str_val ? v->str_val : "");
    json_free(o);
    return buf;
}

/* shlex.quote, exactly as python's shlex does it (ASCII word chars only). */
static void cps_shquote(const char *in, char *out, size_t outsz) {
    size_t p = 0;
    int safe = (in && *in);
    for (const unsigned char *c = (const unsigned char *)(in ? in : ""); *c && safe; c++) {
        if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9')) continue;
        if (strchr("_@%+=:,./-", *c)) continue;
        safe = 0;
    }
    if (safe) { snprintf(out, outsz, "%s", in); return; }
    if (p + 1 < outsz) out[p++] = '\'';
    for (const char *c = in ? in : ""; *c && p + 6 < outsz; c++) {
        if (*c == '\'') { memcpy(out + p, "'\"'\"'", 5); p += 5; }
        else out[p++] = *c;
    }
    if (p + 1 < outsz) out[p++] = '\'';
    out[p < outsz ? p : outsz - 1] = '\0';
}

/* PoP: git_env_shell @ agent/checkpoints/manager.py:_git_env (shell face)
 * Renders git_env_json as leading VAR=value assignments for a popen command line. */
void git_env_shell(const char *store, const char *working_dir, const char *index_file,
                   char *out, size_t outsz) {
    static const char *keys[] = {"GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE",
                                 "GIT_CONFIG_NOSYSTEM", "GIT_CONFIG_GLOBAL", "GIT_CONFIG_SYSTEM"};
    json_t *o = git_env_json(store, working_dir, index_file);
    size_t p = 0; out[0] = '\0';
    for (int i = 0; o && i < 6; i++) {
        const json_t *v = json_obj_get(o, keys[i]);
        if (!v || v->type != JSON_STRING) continue;
        char q[2048];
        cps_shquote(v->str_val, q, sizeof q);
        int n = snprintf(out + p, p < outsz ? outsz - p : 0, "%s%s=%s",
                         i ? " " : "", keys[i], q);
        if (n > 0) p += (size_t)n;
    }
    if (o) json_free(o);
}


/* PoP: run_git @ agent/checkpoints/manager.py:_run_git */ bool run_git(const char *repo_path, const char *cmd, char *out, size_t out_len) {
    char full_cmd[MAX_PATH * 2];
    char qrepo[MAX_PATH * 2];
    cps_shquote(repo_path ? repo_path : ".", qrepo, sizeof qrepo);
    snprintf(full_cmd, sizeof(full_cmd), "git -C %s %s 2>&1", qrepo, cmd ? cmd : "");
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
 * index. Returns true if popen succeeded. */ bool run_git_idx(const char *shadow_repo, const char *working_dir,
                          const char *index_file, const char *cmd,
                          char *out, size_t out_len) {
    char full_cmd[MAX_PATH * 4];
    char env[MAX_PATH * 3];
    git_env_shell(shadow_repo, working_dir, index_file, env, sizeof env);
    snprintf(full_cmd, sizeof(full_cmd), "%s git %s 2>&1", env, cmd ? cmd : "");
    FILE *fp = popen(full_cmd, "r");
    if (!fp) return false;
    if (out && out_len > 0) {
        size_t n = fread(out, 1, out_len - 1, fp);
        out[n] = '\0';
    }
    pclose(fp);
    return true;
}
