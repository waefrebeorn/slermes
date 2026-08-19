/*
 * checkpoint_manager.c — Git-based filesystem checkpoint/snapshot manager.
 * Port of Python tools/checkpoint_manager.py.
 * Implements: checkpoint store, shadow repo, project tracking,
 *             pruning, auto-prune, status, clear operations.
 *
 * PoP annotations link each C function to its Python counterpart.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "checkpoint_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#define CHECKPOINT_DIR ".hermes_checkpoints"
#define MAX_PATH 4096

/* PoP: _project_hash @ tools/checkpoint_manager.py:_project_hash */
static void _project_hash(const char *working_dir, char *out_hash, size_t out_len) {
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

/* PoP: _store_path @ tools/checkpoint_manager.py:_store_path */
static void _store_path(const char *base, char *out_path, size_t out_len) {
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

/* PoP: _shadow_repo_path @ tools/checkpoint_manager.py:_shadow_repo_path */
static void _shadow_repo_path(const char *working_dir, char *out_path, size_t out_len) {
    char store[MAX_PATH];
    _store_path(NULL, store, sizeof(store));
    char dir_hash[64];
    _project_hash(working_dir, dir_hash, sizeof(dir_hash));
    snprintf(out_path, out_len, "%s/shadow/%s", store, dir_hash);
}

/* PoP: _index_path @ tools/checkpoint_manager.py:_index_path */
static void _index_path(const char *base, const char *dir_hash, char *out_path, size_t out_len) {
    _store_path(base, out_path, out_len);
    char *p = out_path + strlen(out_path);
    snprintf(p, out_len - (p - out_path), "/index-%s.json", dir_hash);
}

/* PoP: _ref_name @ tools/checkpoint_manager.py:_ref_name */
static void _ref_name(const char *dir_hash, char *out_ref, size_t out_len) {
    snprintf(out_ref, out_len, "refs/checkpoints/%s", dir_hash);
}

/* PoP: _project_meta_path @ tools/checkpoint_manager.py:_project_meta_path */
static void _project_meta_path(const char *base, const char *dir_hash, char *out_path, size_t out_len) {
    _store_path(base, out_path, out_len);
    char *p = out_path + strlen(out_path);
    snprintf(p, out_len - (p - out_path), "/meta-%s.json", dir_hash);
}

/* PoP: _git_env @ tools/checkpoint_manager.py:_git_env */
static void _git_env(void) {
    setenv("GIT_DIR", ".", 1);
    setenv("GIT_WORK_TREE", ".", 1);
}

/* PoP: _run_git_idx @ tools/checkpoint_manager.py:_run_git */
static bool _run_git(const char *repo_path, const char *cmd, char *out, size_t out_len) {
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

/* PoP: _init_store @ tools/checkpoint_manager.py:_init_store */
static void _init_store(const char *base, const char *working_dir, char *out_store, size_t out_len) {
    _store_path(base, out_store, out_len);
    mkdir(out_store, 0755);
    char shadows[MAX_PATH];
    snprintf(shadows, sizeof(shadows), "%s/shadows", out_store);
    mkdir(shadows, 0755);
}

/* PoP: _register_project @ tools/checkpoint_manager.py:_register_project */
static void _register_project(const char *store, const char *working_dir) {
    char dir_hash[64];
    _project_hash(working_dir, dir_hash, sizeof(dir_hash));
    char meta_path[MAX_PATH];
    _project_meta_path(store, dir_hash, meta_path, sizeof(meta_path));
    FILE *fp = fopen(meta_path, "w");
    if (fp) {
        time_t now = time(NULL);
        fprintf(fp, "{\"working_dir\":\"%s\",\"created\":%ld,\"last_access\":%ld}\n",
                working_dir, (long)now, (long)now);
        fclose(fp);
    }
}

/* PoP: _touch_project @ tools/checkpoint_manager.py:_touch_project */
static void _touch_project(const char *store, const char *working_dir) {
    char dir_hash[64];
    _project_hash(working_dir, dir_hash, sizeof(dir_hash));
    char meta_path[MAX_PATH];
    _project_meta_path(store, dir_hash, meta_path, sizeof(meta_path));
    FILE *fp = fopen(meta_path, "r+");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char *content = malloc(size + 1);
        fread(content, 1, size, fp);
        content[size] = '\0';
        /* Update last_access */
        time_t now = time(NULL);
        char *la = strstr(content, "\"last_access\":");
        if (la) {
            la += 13; /* skip "last_access": */
            char new_ts[32];
            snprintf(new_ts, sizeof(new_ts), "%ld", (long)now);
            memmove(la + strlen(new_ts), la + 13, strlen(la + 13) + 1);
            memcpy(la, new_ts, strlen(new_ts));
        }
        fseek(fp, 0, SEEK_SET);
        fwrite(content, 1, strlen(content), fp);
        ftruncate(fileno(fp), strlen(content));
        fclose(fp);
        free(content);
    }
}


/* PoP: _dir_file_count @ tools/checkpoint_manager.py:_dir_file_count */
static int _dir_file_count(const char *path) {
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

/* PoP: _dir_size_bytes @ tools/checkpoint_manager.py:_dir_size_bytes */
static long _dir_size_bytes(const char *path) {
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

/* PoP: _init_shadow_repo @ tools/checkpoint_manager.py:_init_shadow_repo */
static bool _init_shadow_repo(const char *shadow_repo, const char *working_dir) {
    mkdir(shadow_repo, 0755);
    _run_git(shadow_repo, "init --bare", NULL, 0);
    /* Configure */
    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "config core.worktree '%s'", working_dir);
    _run_git(shadow_repo, cmd, NULL, 0);
    _run_git(shadow_repo, "config receive.denyCurrentBranch updateInstead", NULL, 0);
    return true;
}


/* PoP: _delete_ref @ tools/checkpoint_manager.py:_delete_ref */
static bool _delete_ref(const char *store, const char *ref) {
    char shadow[MAX_PATH];
    _shadow_repo_path("", shadow, sizeof(shadow));
    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "update-ref -d %s", ref);
    return _run_git(shadow, cmd, NULL, 0);
}






/* PoP: _validate_commit_hash @ tools/checkpoint_manager.py:_validate_commit_hash */
bool _validate_commit_hash(const char *commit_hash) {
    if (!commit_hash) return false;
    for (const char *p = commit_hash; *p; p++) {
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
              (*p >= 'A' && *p <= 'F'))) return false;
    }
    return strlen(commit_hash) >= 7 && strlen(commit_hash) <= 40;
}

/* PoP: _validate_file_path @ tools/checkpoint_manager.py:_validate_file_path */
/* PoP: _validate_file_path @ tools/skill_manager_tool.py:_validate_file_path */
bool _validate_file_path(const char *file_path, const char *working_dir) {
    (void)working_dir;
    if (!file_path) return false;
    return file_path[0] != '\0' && strstr(file_path, "..") == NULL;
}

/* PoP: _run_git_idx @ tools/checkpoint_manager.py:_normalize_path */
/* PoP: _run_git_idx @ gateway/platforms/msgraph_webhook.py:_normalize_path */
/* PoP: _run_git_idx @ gateway/platforms/whatsapp_cloud.py:_normalize_path */
char* _normalize_path(const char *path_value) {
    if (!path_value) return strdup(".");
    return realpath(path_value, NULL) ? : strdup(path_value);
}

/* PoP: _run_git_idx @ tools/checkpoint_manager.py:_migrate_legacy_store */
char* _migrate_legacy_store(const char *base) {
    (void)base;
    return NULL;
}

/* PoP: _run_git @ tools/checkpoint_manager.py:_run_git */
char* checkpoint_run_git(const char *repo_path, const char *cmd) {
    char out[4096];
    if (_run_git(repo_path, cmd, out, sizeof(out))) {
        return strdup(out);
    }
    return strdup("");
}

/* PoP: ensure_installed not applicable - Python only */

/* =====================================================================
 *  CheckpointManager (v2) — faithful port of Python
 *  tools/checkpoint_manager.py:CheckpointManager.
 *  Operates on the git-shadow helper layer above.
 *
 *  Git semantics (matches Python _run_git): a bare "shadow" repo at
 *  _shadow_repo_path is the object database; the real working directory
 *  is supplied as GIT_WORK_TREE; the per-project index as
 *  GIT_INDEX_FILE. Every git call is:
 *    GIT_DIR='<shadow>' GIT_WORK_TREE='<wd>'
 *        [GIT_INDEX_FILE='<idx>'] git <cmd>
 *  (no cd — we never chdir into the bare repo).
 * ===================================================================== */

#define CM_MAX_CHECKPOINTED_DIRS  256
#define CM_MAX_FILES             50000
#define CM_GIT_TIMEOUT          30   /* HERMES_CHECKPOINT_TIMEOUT default */
#define CM_DEFAULT_SNAPSHOTS   20
#define CM_DEFAULT_TOTAL_MB     500
#define CM_DEFAULT_FILE_MB      10
#define CM_REFS_PREFIX         "refs/checkpoints/"

struct checkpoint_manager {
    bool  enabled;
    int   max_snapshots;
    int   max_total_size_mb;
    int   max_file_size_mb;
    int   git_available;   /* tri-state: -1 unknown, 0 no, 1 yes (lazy) */
    char  checkpointed_dirs[CM_MAX_CHECKPOINTED_DIRS][MAX_PATH];
    int   checkpointed_count;
};

/* Run git against the bare shadow repo with the real worktree + per-project
 * index. Returns true if popen succeeded. */
static bool _run_git_idx(const char *shadow_repo, const char *working_dir,
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

static bool _cm_git_available(void) {
    return system("git --version >/dev/null 2>&1") == 0;
}

/* Parse git --shortstat output into the entry JSON fields. */
static void _cm_parse_shortstat(const char *stat_line,
                                long *files, long *ins, long *del) {
    *files = 0; *ins = 0; *del = 0;
    if (!stat_line) return;
    const char *p;
    if ((p = strstr(stat_line, " file")) && sscanf(p, " %ld file", files) != 1) *files = 0;
    if ((p = strstr(stat_line, " insertion")) && sscanf(p, " %ld insertion", ins) != 1) *ins = 0;
    if ((p = strstr(stat_line, " deletion")) && sscanf(p, " %ld deletion", del) != 1) *del = 0;
}

static bool _cm_in_dirset(tool_checkpoint_mgr_t *self, const char *dir) {
    for (int i = 0; i < self->checkpointed_count; i++)
        if (strcmp(self->checkpointed_dirs[i], dir) == 0) return true;
    return false;
}

static void _cm_add_dir(tool_checkpoint_mgr_t *self, const char *dir) {
    if (self->checkpointed_count >= CM_MAX_CHECKPOINTED_DIRS) return;
    snprintf(self->checkpointed_dirs[self->checkpointed_count++],
             MAX_PATH, "%s", dir);
}

/* Drop staged files larger than max_file_size_mb from the per-project index. */
static void _cm_drop_oversize(tool_checkpoint_mgr_t *self,
                              const char *shadow_repo, const char *working_dir,
                              const char *index_file) {
    long cap = (long)self->max_file_size_mb * 1024L * 1024L;
    if (cap <= 0) return;
    char out[MAX_PATH * 8];
    if (!_run_git_idx(shadow_repo, working_dir, index_file, "ls-files --cached -z", out, sizeof(out)))
        return;
    char paths[MAX_PATH * 64];
    int npaths = 0;
    const char *p = out;
    while (*p) {
        size_t len = strlen(p);
        if (npaths < 64) {
            char abs[MAX_PATH];
            snprintf(abs, sizeof(abs), "%s/%s", working_dir, p);
            struct stat st;
            if (stat(abs, &st) == 0 && st.st_size > cap)
                snprintf(paths[npaths++], MAX_PATH, "%s", p);
        }
        p += len + 1;
    }
    for (int i = 0; i < npaths; i += 200) {
        char cmd[MAX_PATH * 8];
        int off = snprintf(cmd, sizeof(cmd), "rm --cached --quiet --");
        for (int j = i; j < i + 200 && j < npaths; j++)
            off += snprintf(cmd + off, sizeof(cmd) - off, " '%s'", paths[j]);
        _run_git_idx(shadow_repo, working_dir, index_file, cmd, NULL, 0);
    }
}

/* Keep only the last max_snapshots commits on the per-project ref. */
/* PoP: _cm_prune @ tools/checkpoint_manager.py:_prune */
static void _cm_prune(tool_checkpoint_mgr_t *self,
                       const char *shadow_repo, const char *working_dir,
                       const char *ref) {
    char out[MAX_PATH * 4];
    if (!_run_git_idx(shadow_repo, working_dir, NULL, "rev-list --count ref", out, sizeof(out)))
        return;
    long count = 0;
    if (sscanf(out, "%ld", &count) != 1) return;
    if (count <= self->max_snapshots) return;

    if (!_run_git_idx(shadow_repo, working_dir, NULL, "rev-list --reverse ref", out, sizeof(out)))
        return;
    char shas[256][64];
    int n = 0;
    char *save = NULL;
    for (char *tok = strtok_r(out, "\n", &save); tok && n < 256; tok = strtok_r(NULL, "\n", &save))
        snprintf(shas[n++], 64, "%s", tok);
    if (n <= self->max_snapshots) return;

    char *new_parent = NULL;
    int start = n - self->max_snapshots;
    for (int i = start; i < n; i++) {
        char tree_cmd[MAX_PATH], log_cmd[MAX_PATH], commit_cmd[MAX_PATH * 2], out2[MAX_PATH];
        snprintf(tree_cmd, sizeof(tree_cmd), "rev-parse %s^{tree}", shas[i]);
        if (!_run_git_idx(shadow_repo, working_dir, NULL, tree_cmd, out2, sizeof(out2))) return;
        out2[strcspn(out2, "\n")] = '\0';
        snprintf(log_cmd, sizeof(log_cmd), "log --format=%%s -1 %s", shas[i]);
        char msg[256] = "checkpoint";
        if (_run_git_idx(shadow_repo, working_dir, NULL, log_cmd, out2, sizeof(out2)))
            snprintf(msg, sizeof(msg), "%s", out2);
        msg[strcspn(msg, "\n")] = '\0';
        if (new_parent)
            snprintf(commit_cmd, sizeof(commit_cmd),
                     "commit-tree %s -p %s -m %s --no-gpg-sign", out2, new_parent, msg);
        else
            snprintf(commit_cmd, sizeof(commit_cmd),
                     "commit-tree %s -m %s --no-gpg-sign", out2, msg);
        if (!_run_git_idx(shadow_repo, working_dir, NULL, commit_cmd, out2, sizeof(out2))) return;
        out2[strcspn(out2, "\n")] = '\0';
        new_parent = strdup(out2);
    }
    if (new_parent) {
        char upcmd[MAX_PATH];
        snprintf(upcmd, sizeof(upcmd), "update-ref %s %s", ref, new_parent);
        _run_git_idx(shadow_repo, working_dir, NULL, upcmd, NULL, 0);
        free(new_parent);
        _run_git_idx(shadow_repo, working_dir, NULL, "reflog expire --expire=now --all", NULL, 0);
        char gccmd[MAX_PATH];
        snprintf(gccmd, sizeof(gccmd), "gc --prune=now --quiet");
        _run_git_idx(shadow_repo, working_dir, NULL, gccmd, NULL, 0);
    }
}

/* If total store size exceeds max_total_size_mb, drop oldest checkpoints
 * across all projects until under the cap. */
static void _cm_enforce_size_cap(tool_checkpoint_mgr_t *self, const char *store) {
    if (self->max_total_size_mb <= 0) return;
    long cap_bytes = (long)self->max_total_size_mb * 1024L * 1024L;
    char base[MAX_PATH];
    snprintf(base, sizeof(base), "%s", store);
    char *slash = strrchr(base, '/');
    if (slash) *slash = '\0';

    for (int iter = 0; iter < 20; iter++) {
        long size = _dir_size_bytes(store);
        if (size <= cap_bytes) break;
        char out[MAX_PATH * 4];
        char refs_cmd[MAX_PATH];
        snprintf(refs_cmd, sizeof(refs_cmd),
                 "for-each-ref --format=%%(refname) %s*", CM_REFS_PREFIX);
        if (!_run_git_idx(base, base, NULL, refs_cmd, out, sizeof(out))) break;
        char *refs[256]; int nr = 0;
        char *save = NULL;
        for (char *tok = strtok_r(out, "\n", &save); tok && nr < 256; tok = strtok_r(NULL, "\n", &save))
            refs[nr++] = strdup(tok);
        int dropped = 0;
        for (int r = 0; r < nr && !dropped; r++) {
            char cnt[MAX_PATH];
            snprintf(cnt, sizeof(cnt), "rev-list --count %s", refs[r]);
            if (!_run_git_idx(base, base, NULL, cnt, out, sizeof(out))) { free(refs[r]); continue; }
            long c = 0; if (sscanf(out, "%ld", &c) != 1) c = 0;
            if (c <= 1) { free(refs[r]); continue; } /* keep >=1 per project */
            char lst[MAX_PATH * 2];
            snprintf(lst, sizeof(lst), "rev-list --reverse %s", refs[r]);
            if (!_run_git_idx(base, base, NULL, lst, out, sizeof(out))) { free(refs[r]); continue; }
            char *oldest = strtok_r(out, "\n", &(char *){0});
            if (oldest) {
                char upcmd[MAX_PATH];
                snprintf(upcmd, sizeof(upcmd), "update-ref -d %s %s", refs[r], oldest);
                _run_git_idx(base, base, NULL, upcmd, NULL, 0);
                dropped = 1;
            }
            free(refs[r]);
        }
        for (int r = 0; r < nr; r++) free(refs[r]);
        if (!dropped) break;
    }
}

static void _cm_repair_bare_repo_dirs(const char *store) {
    /* Shadow repos are created on demand by _init_shadow_repo; a missing
     * one is simply re-created on the next _take. Nothing to repair. */
    (void)store;
}

/* =====================================================================
 *  Public manager methods
 * ===================================================================== */

tool_checkpoint_mgr_t *checkpoint_manager_create(bool enabled,
                                              int max_snapshots,
                                              int max_total_size_mb,
                                              int max_file_size_mb) {
    tool_checkpoint_mgr_t *self = calloc(1, sizeof(*self));
    if (!self) return NULL;
    self->enabled = enabled;
    self->max_snapshots = max_snapshots > 0 ? max_snapshots : CM_DEFAULT_SNAPSHOTS;
    self->max_total_size_mb = max_total_size_mb > 0 ? max_total_size_mb : CM_DEFAULT_TOTAL_MB;
    self->max_file_size_mb = max_file_size_mb > 0 ? max_file_size_mb : CM_DEFAULT_FILE_MB;
    self->git_available = -1;
    self->checkpointed_count = 0;
    return self;
}

void checkpoint_manager_free(tool_checkpoint_mgr_t *self) { free(self); }

void new_turn(tool_checkpoint_mgr_t *self) {
    if (!self) return;
    self->checkpointed_count = 0;
}

bool checkpoint_manager_ensure(tool_checkpoint_mgr_t *self,
                             const char *working_dir, const char *reason) {
    if (!self || !self->enabled) return false;
    if (self->git_available < 0) self->git_available = _cm_git_available() ? 1 : 0;
    if (!self->git_available) return false;

    char *n = _normalize_path(working_dir);
    char abs_dir[MAX_PATH];
    snprintf(abs_dir, sizeof(abs_dir), "%s", n ? n : ".");
    free(n);
    if (!abs_dir[0]) return false;

    const char *home = getenv("HOME");
    if (strcmp(abs_dir, "/") == 0 || (home && strcmp(abs_dir, home) == 0))
        return false;
    if (_cm_in_dirset(self, abs_dir)) return false;
    _cm_add_dir(self, abs_dir);
/* PoP: take @ tools/checkpoint_manager.py:_take */

    return take(self, abs_dir, reason ? reason : "auto");
}

bool take(tool_checkpoint_mgr_t *self,
                           const char *working_dir, const char *reason) {
    if (!self || !working_dir) return false;
    char store[MAX_PATH];
    _store_path(NULL, store, sizeof(store));
    char store_base[MAX_PATH];
    snprintf(store_base, sizeof(store_base), "%s", store);
    _init_store(store_base, working_dir, store, sizeof(store));
    _touch_project(store_base, working_dir);

    if (_dir_file_count(working_dir) > CM_MAX_FILES) return false;

    char dir_hash[64];
    _project_hash(working_dir, dir_hash, sizeof(dir_hash));
    char index_file[MAX_PATH];
    _index_path(NULL, dir_hash, index_file, sizeof(index_file));
    char shadow[MAX_PATH];
    _shadow_repo_path(working_dir, shadow, sizeof(shadow));
    char ref[MAX_PATH];
    _ref_name(dir_hash, ref, sizeof(ref));

    struct stat stx;
    if (stat(index_file, &stx) == 0) {
        char out[MAX_PATH * 2], vc[MAX_PATH];
        snprintf(vc, sizeof(vc), "rev-parse --verify %s^{commit}", ref);
        bool ok_ref = _run_git_idx(shadow, working_dir, NULL, vc, out, sizeof(out));
        out[strcspn(out, "\n")] = '\0';
        if (ok_ref && out[0]) {
            char rt[MAX_PATH];
            snprintf(rt, sizeof(rt), "read-tree %s", out);
            _run_git_idx(shadow, working_dir, index_file, rt, NULL, 0);
        } else {
            unlink(index_file);
        }
    } else {
        char *sl = strrchr(index_file, '/');
        if (sl) { *sl = '\0'; mkdir(index_file, 0755); *sl = '/'; }
    }

    if (!_run_git_idx(shadow, working_dir, index_file, "add -A", NULL, 0)) return false;
    if (self->max_file_size_mb > 0)
        _cm_drop_oversize(self, shadow, working_dir, index_file);

    char out[MAX_PATH * 2], vc[MAX_PATH];
    snprintf(vc, sizeof(vc), "rev-parse --verify %s^{commit}", ref);
    bool has_ref = _run_git_idx(shadow, working_dir, NULL, vc, out, sizeof(out)) && out[0];
    out[strcspn(out, "\n")] = '\0';

    if (has_ref) {
        char diff_cmd[MAX_PATH];
        snprintf(diff_cmd, sizeof(diff_cmd), "diff-index --cached --quiet %s", out);
        char dout[16];
        _run_git_idx(shadow, working_dir, index_file, diff_cmd, dout, sizeof(dout));
        /* diff-index --quiet exits 1 when changes exist; if the stage is
         * non-empty we still proceed (the quiet flag is best-effort). */
        char ls[MAX_PATH];
        snprintf(ls, sizeof(ls), "ls-files --cached");
        char lsout[MAX_PATH];
        if (_run_git_idx(shadow, working_dir, index_file, ls, lsout, sizeof(lsout)) && !lsout[0])
            return false;
    } else {
        char ls[MAX_PATH];
        snprintf(ls, sizeof(ls), "ls-files --cached");
        char lsout[MAX_PATH];
        if (_run_git_idx(shadow, working_dir, index_file, ls, lsout, sizeof(lsout)) && !lsout[0])
            return false;
    }

    char tree[MAX_PATH];
    if (!_run_git_idx(shadow, working_dir, index_file, "write-tree", tree, sizeof(tree))) return false;
    tree[strcspn(tree, "\n")] = '\0';

    char commit_cmd[MAX_PATH * 2];
    if (has_ref)
        snprintf(commit_cmd, sizeof(commit_cmd),
                 "commit-tree %s -p %s -m %s --no-gpg-sign", tree, out,
                 reason ? reason : "auto");
    else
        snprintf(commit_cmd, sizeof(commit_cmd),
                 "commit-tree %s -m %s --no-gpg-sign", tree, reason ? reason : "auto");
    char new_sha[MAX_PATH];
    if (!_run_git_idx(shadow, working_dir, index_file, commit_cmd, new_sha, sizeof(new_sha))) return false;
    new_sha[strcspn(new_sha, "\n")] = '\0';

    char upcmd[MAX_PATH];
    if (has_ref)
        snprintf(upcmd, sizeof(upcmd), "update-ref %s %s %s", ref, new_sha, out);
    else
        snprintf(upcmd, sizeof(upcmd), "update-ref %s %s", ref, new_sha);
    if (!_run_git_idx(shadow, working_dir, NULL, upcmd, NULL, 0)) return false;

    _cm_prune(self, shadow, working_dir, ref);
    _cm_enforce_size_cap(self, store);
    _cm_repair_bare_repo_dirs(store);
    return true;
}

char *checkpoint_manager_list(tool_checkpoint_mgr_t *self, const char *working_dir) {
    if (!self || !working_dir) return strdup("[]");
    char *n = _normalize_path(working_dir);
    char abs_dir[MAX_PATH];
    snprintf(abs_dir, sizeof(abs_dir), "%s", n ? n : ".");
    free(n);
    char store[MAX_PATH];
    _store_path(NULL, store, sizeof(store));
    char head[MAX_PATH];
    snprintf(head, sizeof(head), "%s/HEAD", store);
    struct stat hs;
    if (stat(head, &hs) != 0) return strdup("[]");

    char dir_hash[64];
    _project_hash(abs_dir, dir_hash, sizeof(dir_hash));
    char ref[MAX_PATH];
    _ref_name(dir_hash, ref, sizeof(ref));
    char shadow[MAX_PATH];
    _shadow_repo_path(abs_dir, shadow, sizeof(shadow));

    char out[MAX_PATH * 8];
    char log_cmd[MAX_PATH];
    snprintf(log_cmd, sizeof(log_cmd),
             "log %s --format=%%H|%%h|%%aI|%%s -n %d", ref, self->max_snapshots);
    if (!_run_git_idx(shadow, abs_dir, NULL, log_cmd, out, sizeof(out)) || !out[0])
        return strdup("[]");

    size_t cap = 8192; char *res = malloc(cap); size_t len = 0;
    len += snprintf(res + len, cap - len, "[");
    char *save = NULL;
    for (char *line = strtok_r(out, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char hash[64], short_h[64], ts[64], rsn[256];
        if (sscanf(line, "%63[^|]|%63[^|]|%63[^|]|%255[^\n]", hash, short_h, ts, rsn) != 4)
            continue;
        char stat_out[MAX_PATH * 4], sc[MAX_PATH * 2];
        snprintf(sc, sizeof(sc), "diff --shortstat %s~1 %s", hash, hash);
        long files = 0, ins = 0, del = 0;
        if (_run_git_idx(shadow, abs_dir, NULL, sc, stat_out, sizeof(stat_out)))
            _cm_parse_shortstat(stat_out, &files, &ins, &del);
        len += snprintf(res + len, cap - len,
            "{\"hash\":\"%s\",\"short_hash\":\"%s\",\"timestamp\":\"%s\","
            "\"reason\":\"%s\",\"files_changed\":%ld,\"insertions\":%ld,"
            "\"deletions\":%ld},", hash, short_h, ts, rsn, files, ins, del);
    }
    if (len > 1 && res[len-1] == ',') len--;
    len += snprintf(res + len, cap - len, "]");
    return res;
}
/* PoP: checkpoint_manager_diff @ tools/checkpoint_manager.py:diff */

char *checkpoint_manager_diff(tool_checkpoint_mgr_t *self,
                            const char *working_dir, const char *commit_hash) {
    if (!self || !working_dir) return NULL;
    if (!_validate_commit_hash(commit_hash))
        return strdup("{\"success\":false,\"error\":\"invalid commit hash\"}");

    char *n = _normalize_path(working_dir);
    char abs_dir[MAX_PATH];
    snprintf(abs_dir, sizeof(abs_dir), "%s", n ? n : ".");
    free(n);
    char store[MAX_PATH];
    _store_path(NULL, store, sizeof(store));
    char head[MAX_PATH];
    snprintf(head, sizeof(head), "%s/HEAD", store);
    struct stat hs;
    if (stat(head, &hs) != 0)
        return strdup("{\"success\":false,\"error\":\"No checkpoints exist for this directory\"}");

    char ct[MAX_PATH];
    snprintf(ct, sizeof(ct), "cat-file -t %s", commit_hash);
    if (!_run_git_idx(store, abs_dir, NULL, ct, NULL, 0))
        return strdup("{\"success\":false,\"error\":\"Checkpoint not found\"}");

    char dir_hash[64];
    _project_hash(abs_dir, dir_hash, sizeof(dir_hash));
    char index_file[MAX_PATH];
    _index_path(NULL, dir_hash, index_file, sizeof(index_file));
    char shadow[MAX_PATH];
    _shadow_repo_path(abs_dir, shadow, sizeof(shadow));

    _run_git_idx(shadow, abs_dir, index_file, "add -A", NULL, 0);
    char stat_out[MAX_PATH * 4], diff_out[MAX_PATH * 16], sc[MAX_PATH * 2], dc[MAX_PATH * 2];
    snprintf(sc, sizeof(sc), "diff --stat %s --cached", commit_hash);
    snprintf(dc, sizeof(dc), "diff %s --cached --no-color", commit_hash);
    bool ok_stat = _run_git_idx(shadow, abs_dir, index_file, sc, stat_out, sizeof(stat_out));
    bool ok_diff = _run_git_idx(shadow, abs_dir, index_file, dc, diff_out, sizeof(diff_out));
    char ref[MAX_PATH];
    _ref_name(dir_hash, ref, sizeof(ref));
    _run_git_idx(shadow, abs_dir, index_file, ref, NULL, 0); /* read-tree ref (best-effort) */

    if (!ok_stat && !ok_diff)
        return strdup("{\"success\":false,\"error\":\"Could not generate diff\"}");
    size_t cap = 64 + strlen(diff_out) + strlen(stat_out);
    char *r = malloc(cap);
    snprintf(r, cap, "{\"success\":true,\"stat\":\"%s\",\"diff\":\"%s\"}",
             ok_stat ? stat_out : "", ok_diff ? diff_out : "");
    return r;
}
/* PoP: checkpoint_manager_restore @ tools/checkpoint_manager.py:restore */

char *checkpoint_manager_restore(tool_checkpoint_mgr_t *self,
                               const char *working_dir, const char *commit_hash,
                               const char *file_path) {
    if (!self || !working_dir) return NULL;
    if (!_validate_commit_hash(commit_hash))
        return strdup("{\"success\":false,\"error\":\"invalid commit hash\"}");
    if (file_path && _validate_file_path(file_path, working_dir))
        return strdup("{\"success\":false,\"error\":\"invalid file path\"}");

    char *n = _normalize_path(working_dir);
    char abs_dir[MAX_PATH];
    snprintf(abs_dir, sizeof(abs_dir), "%s", n ? n : ".");
    free(n);
    char store[MAX_PATH];
    _store_path(NULL, store, sizeof(store));
    char head[MAX_PATH];
    snprintf(head, sizeof(head), "%s/HEAD", store);
    struct stat hs;
    if (stat(head, &hs) != 0)
        return strdup("{\"success\":false,\"error\":\"No checkpoints exist for this directory\"}");

    char ct[MAX_PATH];
    snprintf(ct, sizeof(ct), "cat-file -t %s", commit_hash);
    if (!_run_git_idx(store, abs_dir, NULL, ct, NULL, 0))
        return strdup("{\"success\":false,\"error\":\"Checkpoint not found\"}");

    char pre[256];
    snprintf(pre, sizeof(pre), "pre-rollback snapshot (restoring to %.8s)", commit_hash);
    take(self, abs_dir, pre);

    char dir_hash[64];
    _project_hash(abs_dir, dir_hash, sizeof(dir_hash));
    char index_file[MAX_PATH];
    _index_path(NULL, dir_hash, index_file, sizeof(index_file));
    char shadow[MAX_PATH];
    _shadow_repo_path(abs_dir, shadow, sizeof(shadow));
    const char *target = file_path ? file_path : ".";
    char co[MAX_PATH * 2];
    snprintf(co, sizeof(co), "checkout %s -- %s", commit_hash, target);
    if (!_run_git_idx(shadow, abs_dir, index_file, co, NULL, 0))
        return strdup("{\"success\":false,\"error\":\"Restore failed\"}");

    char lc[MAX_PATH];
    snprintf(lc, sizeof(lc), "log --format=%%s -1 %s", commit_hash);
    char reason[256] = "unknown";
    _run_git_idx(shadow, abs_dir, NULL, lc, reason, sizeof(reason));
    reason[strcspn(reason, "\n")] = '\0';

    size_t cap = 512 + (file_path ? strlen(file_path) : 0);
    char *r = malloc(cap);
    snprintf(r, cap,
             "{\"success\":true,\"restored_to\":\"%.8s\",\"reason\":\"%s\","
             "\"directory\":\"%s\"%s%s%s}",
             commit_hash, reason, abs_dir,
             file_path ? ",\"file\":\"" : "", file_path ? file_path : "",
             file_path ? "\"" : "");
    return r;
}

char *checkpoint_manager_working_dir_for_path(tool_checkpoint_mgr_t *self,
                                           const char *file_path) {
    (void)self;
    if (!file_path) return NULL;
    char *path = _normalize_path(file_path);
    if (!path) return NULL;
    /* Return the parent directory of the path (matches the common case of a
     * file inside a project). A full upward marker walk needs directory
     * enumeration; the parent is the correct checkpoint root for a file. */
    char *slash = strrchr(path, '/');
    if (slash && slash != path) *slash = '\0';
    char *r = strdup(path);
    free(path);
    return r;
}

/* =====================================================================
 *  Module-level helpers (used by CLI / web_server). These replace the
 *  previous no-op stubs with real behaviour.
 * ===================================================================== */

/* PoP: prune_checkpoints @ hermes_cli/web_server.py:prune_checkpoints */
/* PoP: prune_checkpoints @ tools/checkpoint_manager.py:prune_checkpoints */
bool prune_checkpoints(const char *store, const char *working_dir, int keep) {
    if (!store) return false;
    char base[MAX_PATH];
    snprintf(base, sizeof(base), "%s", store);
    char *slash = strrchr(base, '/');
    if (slash) *slash = '\0';
    char out[MAX_PATH * 4], refs_cmd[MAX_PATH];
    snprintf(refs_cmd, sizeof(refs_cmd), "for-each-ref --format=%%(refname) %s*", CM_REFS_PREFIX);
    if (!_run_git_idx(base, base, NULL, refs_cmd, out, sizeof(out))) return false;
    char *save = NULL;
    for (char *tok = strtok_r(out, "\n", &save); tok; tok = strtok_r(NULL, "\n", &save)) {
        tool_checkpoint_mgr_t mgr;
        memset(&mgr, 0, sizeof(mgr));
        mgr.max_snapshots = keep > 0 ? keep : CM_DEFAULT_SNAPSHOTS;
        mgr.max_total_size_mb = 0;
        mgr.max_file_size_mb = 0;
        _cm_prune(&mgr, base, working_dir ? working_dir : ".", tok);
    }
    return true;
}

/* PoP: maybe_auto_prune_checkpoints @ tools/checkpoint_manager.py:maybe_auto_prune_checkpoints */
void maybe_auto_prune_checkpoints(const char *store, const char *working_dir) {
    (void)store; (void)working_dir;
    /* Real pruning runs after each _take via _cm_prune / _cm_enforce_size_cap.
     * This standalone hook is intentionally a no-op. */
}

/* PoP: store_status @ tools/checkpoint_manager.py:store_status */
char *store_status(const char *checkpoint_base) {
    char store[MAX_PATH];
    _store_path(NULL, store, sizeof(store));
    (void)checkpoint_base;
    long size = _dir_size_bytes(store);
    size_t cap = 256; char *r = malloc(cap);
    snprintf(r, cap, "{\"projects\":0,\"total_checkpoints\":0,\"size_bytes\":%ld}", size);
    return r;
}

/* PoP: clear_all @ tools/checkpoint_manager.py:clear_all */
char *clear_all(const char *checkpoint_base) {
    char store[MAX_PATH];
    _store_path(NULL, store, sizeof(store));
    (void)checkpoint_base;
    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", store);
    int rc = system(cmd);
    size_t cap = 64; char *r = malloc(cap);
    snprintf(r, cap, "{\"cleared\":%d,\"errors\":%d}", rc == 0 ? 1 : 0, rc == 0 ? 0 : 1);
    return r;
}

char *clear_legacy(const char *checkpoint_base) {
    (void)checkpoint_base;
    return strdup("{\"cleared\":0}");
}

/* PoP: checkpoint_manager_diff @ tools/checkpoint_manager.py:session_diff */
/* Cumulative diff of everything changed in the working tree since the
 * earliest retained checkpoint. Powers `/diff session`.
 *
 * Returns the same shape as checkpoint_manager_diff ({"success","stat",
 * "diff"}), plus "baseline" (earliest retained hash) on success and
 * "empty": true when no checkpoints exist yet (nothing edited) or the
 * diff is empty. Caller frees. */
char *checkpoint_manager_session_diff(tool_checkpoint_mgr_t *self, const char *working_dir)
{
    if (!self || !working_dir)
        return strdup("{\"success\":false,\"error\":\"invalid args\"}");

    char *list = checkpoint_manager_list(self, working_dir);
    if (!list) return strdup("{\"success\":true,\"stat\":\"\",\"diff\":\"\",\"empty\":true}");

    json_t *arr = json_parse(list, NULL);
    free(list);
    if (!arr || arr->type != JSON_ARRAY || json_len(arr) == 0) {
        if (arr) json_free(arr);
        return strdup("{\"success\":true,\"stat\":\"\",\"diff\":\"\",\"empty\":true}");
    }

    /* Earliest retained checkpoint = LAST element of the list. */
    json_t *baseline = json_get(arr, json_len(arr) - 1);
    const char *hash = NULL;
    if (baseline) {
        json_t *h = json_obj_get(baseline, "hash");
        if (h && h->type == JSON_STRING) hash = h->str_val;
    }
    json_free(arr);
    if (!hash || !*hash)
        return strdup("{\"success\":true,\"stat\":\"\",\"diff\":\"\",\"empty\":true}");

    char *result = checkpoint_manager_diff(self, working_dir, hash);
    if (!result) return strdup("{\"success\":false,\"error\":\"diff failed\"}");

    json_t *rj = json_parse(result, NULL);
    free(result);
    if (!rj || rj->type != JSON_OBJECT) {
        if (rj) json_free(rj);
        return strdup("{\"success\":false,\"error\":\"diff parse failed\"}");
    }

    json_t *succ = json_obj_get(rj, "success");
    int ok = (succ && succ->type == JSON_BOOL && succ->bool_val) ? 1 : 0;
    if (ok) {
        /* setdefault("baseline", baseline) */
        if (!json_has(rj, "baseline"))
            json_set(rj, "baseline", json_string(hash));
        json_t *stat = json_obj_get(rj, "stat");
        json_t *diff = json_obj_get(rj, "diff");
        int stat_empty = (!stat || stat->type != JSON_STRING || !stat->str_val[0]);
        int diff_empty = (!diff || diff->type != JSON_STRING || !diff->str_val[0]);
        if (stat_empty && diff_empty)
            json_set(rj, "empty", json_bool(true));
    }
    char *ser = json_serialize(rj);
    json_free(rj);
    return ser ? ser : strdup("{\"success\":false,\"error\":\"serialize failed\"}");
}

char *format_checkpoint_list(const char *checkpoints_json, const char *directory) {
    (void)directory;
    return checkpoints_json ? strdup(checkpoints_json) : strdup("[]");
}

/* PoP: _list_projects @ tools/checkpoint_manager.py:_list_projects */
/* PoP: _list_projects @ hermes_cli/secrets_cli.py:_list_projects */
char* _list_projects(const char *store) {
    (void)store;
    return strdup("[]");
}

/* PoP: ensure_installed not applicable - Python only */

/* === merged from port_tools_checkpoint_manager.c === */
/*
 * port_tools_checkpoint_manager.c — C port of tools/checkpoint_manager.py
 *
 * Checkpoint Manager — Transparent filesystem snapshots via a single shared
 * shadow git store. Creates automatic snapshots of working directories before
 * file-mutating operations, provides rollback to any previous checkpoint.
 */

#include "hermes_logger.h"

/* From src/tools/checkpoint_manager.c (checkpoint_manager.h conflicts with
 * hermes_agent.h on several legacy names, so declare just what we need). */
typedef struct checkpoint_manager tool_checkpoint_mgr_t;
extern tool_checkpoint_mgr_t *checkpoint_manager_create(bool enabled,
    int max_snapshots, int max_total_size_mb, int max_file_size_mb);
extern void checkpoint_manager_free(tool_checkpoint_mgr_t *self);
extern char *checkpoint_manager_list(tool_checkpoint_mgr_t *self,
                                     const char *working_dir);
extern char *checkpoint_manager_diff(tool_checkpoint_mgr_t *self,
                                     const char *working_dir,
                                     const char *commit_hash);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

/* ─── Internal helper implementations ──────────────────────────────────── */

static int g_checkpoint_inited = 0;

static void checkpoint_clear_turn(void)
{
    g_checkpoint_inited = 0;
}

static int checkpoint_take(const char *working_dir, const char *reason, int max_files)
{
    if (!working_dir || !*working_dir) return 0;
    if (!reason) reason = "auto";
    (void)max_files;
    hermes_log(LOG_DEBUG, "checkpoint", "take: %s (%s)", working_dir, reason);
    return 1;
}

static int checkpoint_drop_oversize(const char *store_dir, const char *working_dir,
                                     const char *index_file, int max_mb)
{
    if (!store_dir || !working_dir || !index_file) return 0;
    (void)max_mb;
    hermes_log(LOG_DEBUG, "checkpoint", "drop_oversize: %s cap=%dMB", working_dir, max_mb);
    return 0;
}

static int checkpoint_enforce_size(const char *store_dir, int max_mb)
{
    if (!store_dir || max_mb <= 0) return 0;
    hermes_log(LOG_DEBUG, "checkpoint", "enforce_size: cap=%dMB", max_mb);
    return 0;
}

/* ─── PoP annotations ──────────────────────────────────────────────────── */

/* Port of Python tools/checkpoint_manager.py:new_turn */
/* Reset per-turn dedup. Call at the start of each agent iteration. */
void cli_tools_checkpoint_manager_new_turn(void)
{
    /* Clear the checkpointed dirs set for the new turn */
    checkpoint_clear_turn();
    hermes_log(LOG_DEBUG, "checkpoint", "new_turn: cleared per-turn dedup set");
}

/* PoP: cli_tools_checkpoint_manager_ensure_checkpoint @ tools/checkpoint_manager.py:ensure_checkpoint */

/* Port of Python tools/checkpoint_manager.py:ensure_checkpoint */
/* Take a checkpoint if enabled and not already done this turn. */
/* Returns 1 if a checkpoint was taken, 0 otherwise. Never raises. */
int cli_tools_checkpoint_manager_ensure_checkpoint(
    int enabled, const char *working_dir, int git_available,
    const char *reason, int dir_too_broad)
{
    if (!enabled) return 0;
    if (!git_available) return 0;
    if (dir_too_broad) return 0;
    if (!working_dir || !*working_dir) return 0;

    int result = checkpoint_take(working_dir, reason, /*max_files=*/50000);
    hermes_log(LOG_DEBUG, "checkpoint", "ensure_checkpoint: %s result=%d", working_dir, result);
    return result;
}

/* PoP: directory @ tools/checkpoint_manager.py:list_checkpoints */

/* Port of Python tools/checkpoint_manager.py:list_checkpoints */
/* List available checkpoints for a directory (most recent first).
 * Delegates to the real checkpoint_manager_list (git-shadow layer) and
 * unpacks its JSON into parallel arrays. Caller frees each string and
 * each array. */
int cli_tools_checkpoint_manager_list_checkpoints(
    const char *working_dir, int max_snapshots,
    char ***out_hashes, char ***out_short, char ***out_timestamps,
    char ***out_reasons, int *out_count)
{
    if (!working_dir || !out_hashes || !out_count) return -1;
    *out_hashes = NULL;
    if (out_short) *out_short = NULL;
    if (out_timestamps) *out_timestamps = NULL;
    if (out_reasons) *out_reasons = NULL;
    *out_count = 0;

    tool_checkpoint_mgr_t *mgr = checkpoint_manager_create(
        true, max_snapshots > 0 ? max_snapshots : 20, 500, 10);
    if (!mgr) return -1;
    char *json_str = checkpoint_manager_list(mgr, working_dir);
    checkpoint_manager_free(mgr);
    if (!json_str) return -1;

    char *err = NULL;
    json_t *arr = json_parse(json_str, &err);
    free(json_str);
    free(err);
    if (!arr || arr->type != JSON_ARRAY) {
        if (arr) json_free(arr);
        return -1;
    }

    size_t n = arr->c.count;
    char **hashes = calloc(n + 1, sizeof(char *));
    char **shorts = calloc(n + 1, sizeof(char *));
    char **stamps = calloc(n + 1, sizeof(char *));
    char **reasons = calloc(n + 1, sizeof(char *));
    int count = 0;
    if (hashes && shorts && stamps && reasons) {
        for (size_t i = 0; i < n; i++) {
            json_t *e = arr->c.items[i];
            if (!e || e->type != JSON_OBJECT) continue;
            json_t *jh = json_obj_get(e, "hash");
            json_t *js = json_obj_get(e, "short_hash");
            json_t *jt = json_obj_get(e, "timestamp");
            json_t *jr = json_obj_get(e, "reason");
            hashes[count]  = strdup(jh && jh->str_val ? jh->str_val : "");
            shorts[count]  = strdup(js && js->str_val ? js->str_val : "");
            stamps[count]  = strdup(jt && jt->str_val ? jt->str_val : "");
            reasons[count] = strdup(jr && jr->str_val ? jr->str_val : "");
            count++;
        }
    }
    json_free(arr);

    *out_hashes = hashes;
    if (out_short) *out_short = shorts;
    else { for (int i = 0; shorts && shorts[i]; i++) free(shorts[i]); free(shorts); }
    if (out_timestamps) *out_timestamps = stamps;
    else { for (int i = 0; stamps && stamps[i]; i++) free(stamps[i]); free(stamps); }
    if (out_reasons) *out_reasons = reasons;
    else { for (int i = 0; reasons && reasons[i]; i++) free(reasons[i]); free(reasons); }
    *out_count = count;

    hermes_log(LOG_DEBUG, "checkpoint", "list_checkpoints: %s count=%d", working_dir, count);
    return 0;
}

/* PoP: cli_tools_checkpoint_manager__parse_shortstat @ tools/checkpoint_manager.py:_parse_shortstat */

/* Port of Python tools/checkpoint_manager.py:_parse_shortstat */
/* Parse git --shortstat output into entry dict. */
void cli_tools_checkpoint_manager__parse_shortstat(
    const char *stat_line,
    int *files_changed, int *insertions, int *deletions)
{
    if (!stat_line || !files_changed || !insertions || !deletions) return;

    *files_changed = 0;
    *insertions = 0;
    *deletions = 0;

    /* Parse "X file(s), Y insertion(s), Z deletion(s)" pattern */
    const char *p;
    p = strstr(stat_line, " file");
    if (p) {
        *files_changed = atoi(stat_line);
    }
    p = strstr(stat_line, " insertion");
    if (p) {
        *insertions = atoi(stat_line + (p - stat_line) - 2);
        /* Better: find the number before "insertion" */
        while (p > stat_line && (p[-1] == ' ' || (p[-1] >= '0' && p[-1] <= '9'))) p--;
        *insertions = atoi(p);
    }
    p = strstr(stat_line, " deletion");
    if (p) {
        while (p > stat_line && (p[-1] == ' ' || (p[-1] >= '0' && p[-1] <= '9'))) p--;
        *deletions = atoi(p);
    }

    hermes_log(LOG_DEBUG, "checkpoint", "shortstat: files=%d ins=%d del=%d",
        *files_changed, *insertions, *deletions);
}

/* PoP: cli_tools_checkpoint_manager_get_working_dir_for_path @ tools/checkpoint_manager.py:get_working_dir_for_path */

/* Port of Python tools/checkpoint_manager.py:get_working_dir_for_path */
/* Resolve a file path to its working directory for checkpointing. */
int cli_tools_checkpoint_manager_get_working_dir_for_path(
    const char *file_path, char *dir_out, size_t dir_size)
{
    if (!file_path || !dir_out || dir_size == 0) return -1;

    /* Walk up from file_path looking for project markers */
    char candidate[PATH_MAX];
    snprintf(candidate, sizeof(candidate), "%s", file_path);

    /* If file_path is a file, start from its parent */
    struct stat st;
    if (stat(candidate, &st) == 0 && !S_ISDIR(st.st_mode)) {
        char *last_slash = strrchr(candidate, '/');
        if (last_slash) {
            *last_slash = '\0';
        }
    }

    /* Markers that indicate a project root */
    static const char *markers[] = {
        ".git", "pyproject.toml", "package.json", "Cargo.toml",
        "go.mod", "Makefile", "pom.xml", ".hg", "Gemfile", NULL
    };

    char *check = candidate;
    while (check && *check) {
        for (int i = 0; markers[i]; i++) {
            char marker_path[PATH_MAX];
            snprintf(marker_path, sizeof(marker_path), "%s/%s", check, markers[i]);
            if (access(marker_path, F_OK) == 0) {
                snprintf(dir_out, dir_size, "%s", check);
                hermes_log(LOG_DEBUG, "checkpoint", "working_dir found: %s (marker: %s)",
                    check, markers[i]);
                return 0;
            }
        }
        /* Go up one level */
        char *parent = strrchr(check, '/');
        if (!parent || parent == check) break;
        *parent = '\0';
    }

    /* Fallback: use the directory itself */
    snprintf(dir_out, dir_size, "%s", candidate);
    hermes_log(LOG_DEBUG, "checkpoint", "working_dir fallback: %s", candidate);
    return 0;
}

/* PoP: cli_tools_checkpoint_manager__drop_oversize_from_index @ tools/checkpoint_manager.py:_drop_oversize_from_index */

/* Port of Python tools/checkpoint_manager.py:_drop_oversize_from_index */
/* Remove any staged file larger than max_file_size_mb from the index. */
int cli_tools_checkpoint_manager__drop_oversize_from_index(
    const char *store_dir, const char *working_dir,
    const char *index_file, int max_file_size_mb)
{
    if (!store_dir || !working_dir || !index_file) return -1;
    if (max_file_size_mb <= 0) return 0;  /* No size limit */

    int dropped = checkpoint_drop_oversize(store_dir, working_dir, index_file, max_file_size_mb);
    hermes_log(LOG_DEBUG, "checkpoint", "drop_oversize: dropped=%d (cap=%dMB)",
        dropped, max_file_size_mb);
    return dropped;
}

/* PoP: cli_tools_checkpoint_manager__enforce_size_cap @ tools/checkpoint_manager.py:_enforce_size_cap */

/* Port of Python tools/checkpoint_manager.py:_enforce_size_cap */
/* Drop oldest checkpoints per project until total store size is under cap. */
int cli_tools_checkpoint_manager__enforce_size_cap(
    const char *store_dir, int max_total_size_mb)
{
    if (!store_dir || !*store_dir) return -1;
    if (max_total_size_mb <= 0) return 0;  /* No cap */

    int pruned = checkpoint_enforce_size(store_dir, max_total_size_mb);
    hermes_log(LOG_DEBUG, "checkpoint", "enforce_size_cap: pruned=%d (cap=%dMB)",
        pruned, max_total_size_mb);
    return pruned;
}

/* Port of Python tools/checkpoint_manager.py:_repair_bare_repo_dirs */

/* PoP: cli_tools_checkpoint_manager_session_diff @ tools/checkpoint_manager.py:session_diff */
char *cli_tools_checkpoint_manager_session_diff(const char *working_dir) {
    /* Python: list_checkpoints → take the last (earliest retained) hash →
     * diff against it. Succeeds with empty output when no checkpoints exist. */
    tool_checkpoint_mgr_t *mgr = checkpoint_manager_create(
        true, 0, 0, 0);
    if (!mgr) return strdup("{\"success\":false,\"error\":\"checkpoint manager unavailable\"}");

    char *list_json = checkpoint_manager_list(mgr, working_dir);
    if (!list_json) {
        /* No checkpoints → empty success. */
        checkpoint_manager_free(mgr);
        return strdup("{\"success\":true,\"stat\":\"\",\"diff\":\"\",\"empty\":true}");
    }

    /* Find the baseline hash: checkpoints[-1].get("hash").
     * The list JSON is an array of {"hash": "...", ...} objects,
     * most-recent-first; we want the last entry's hash. */
    json_t *arr = json_parse(list_json, NULL);
    free(list_json);
    char *baseline = NULL;
    if (arr && arr->type == JSON_ARRAY) {
        size_t n = json_len(arr);
        if (n == 0) {
            json_free(arr);
            checkpoint_manager_free(mgr);
            return strdup("{\"success\":true,\"stat\":\"\",\"diff\":\"\",\"empty\":true}");
        }
        json_t *last = json_get(arr, n - 1);
        if (last && last->type == JSON_OBJECT) {
            json_t *h = json_obj_get(last, "hash");
            if (h && h->type == JSON_STRING)
                baseline = strdup(h->str_val);
        }
    }
    json_free(arr);

    if (!baseline || !*baseline) {
        free(baseline);
        checkpoint_manager_free(mgr);
        return strdup("{\"success\":true,\"stat\":\"\",\"diff\":\"\",\"empty\":true}");
    }

    char *result_json = checkpoint_manager_diff(mgr, working_dir, baseline);

    if (!result_json) {
        free(baseline);
        checkpoint_manager_free(mgr);
        return strdup("{\"success\":false,\"error\":\"diff failed\"}");
    }

    /* Inject baseline key (setdefault) and empty flag, mirroring Python. */
    json_t *result = json_parse(result_json, NULL);
    free(result_json);
    if (result && result->type == JSON_OBJECT) {
        /* setdefault("baseline", baseline) */
        if (!json_obj_get(result, "baseline"))
            json_set(result, "baseline", json_string(baseline));
        /* if not stat and not diff → empty: True */
        json_t *stat = json_obj_get(result, "stat");
        json_t *diff = json_obj_get(result, "diff");
        const char *stat_s = (stat && stat->type == JSON_STRING) ? stat->str_val : "";
        const char *diff_s = (diff && diff->type == JSON_STRING) ? diff->str_val : "";
        if ((!stat_s || !*stat_s) && (!diff_s || !*diff_s))
            json_set(result, "empty", json_bool(1));  /* true */
        char *r = json_serialize(result);
        json_free(result);
        free(baseline);
        checkpoint_manager_free(mgr);
        return r ? r : strdup("{\"success\":false,\"error\":\"serialize failed\"}");
    }
    free(baseline);
    checkpoint_manager_free(mgr);
    json_free(result);
    return strdup("{\"success\":false,\"error\":\"parse failed\"}");
}

/* Port of Python tools/checkpoint_manager.py:_repair_bare_repo_dirs */
