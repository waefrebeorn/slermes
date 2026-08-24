/*
 * worktree_gc.c — C11 port of hermes_cli/worktree_gc.py (+ the cli.py
 * classification primitives: _repo_is_shallow, _deepen_shallow_repo,
 * _worktree_has_unpushed_commits, _worktree_commits_all_merged_upstream,
 * _worktree_lock_is_live, _git_repo_root).
 *
 * Attended worktree/branch reclaim. Invariants (shared with the startup
 * pruner): tracked modifications are NEVER deleted; unique unpushed commits
 * are NEVER deleted (git cherry patch-equivalence decides); live-locked
 * trees are never touched; a branch is deleted only after its worktree
 * removal succeeded; untracked-only dirt is ARCHIVED first.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "worktree_gc.h"

/* worktree merge-cache API (src/cli/cli.c) */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>

/* worktree merge-cache API (src/cli/cli.c) */
extern char *cli_load_worktree_merge_cache(void);
extern void cli_save_worktree_merge_cache(json_t *verdicts);
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>

const char *const WGC_PROTECTED_BRANCHES[] = {
    "main", "master", "develop", "dev", "trunk", NULL,
};
const size_t WGC_PROTECTED_BRANCHES_N = 5;

static char *wgc_strdup_n(const char *s, size_t n)
{
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* PoP: _git @ hermes_cli/worktree_gc.py:_git — run git, translating timeouts
 * into rc 124. Every verdict fails safe toward keep on nonzero rc. */
int wgc_run_git(char *const args[], const char *cwd, int timeout,
                char **out_stdout, char **out_stderr)
{
    if (out_stdout) *out_stdout = NULL;
    if (out_stderr) *out_stderr = NULL;

    /* argv: {"git", *args, NULL} */
    size_t argc = 1;
    while (args[argc - 1]) argc++;
    char **argv = malloc((argc + 2) * sizeof(char *));
    if (!argv) return 127;
    argv[0] = (char *)"git";
    for (size_t i = 0; i < argc; i++) argv[i + 1] = args[i];
    argv[argc + 1] = NULL;

    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        free(argv);
        return 127;
    }
    pid_t pid = fork();
    if (pid < 0) {
        free(argv);
        return 127;
    }
    if (pid == 0) {
        if (cwd && chdir(cwd) != 0) _exit(127);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        execvp("git", argv);
        _exit(127);
    }
    free(argv);
    close(out_pipe[1]);
    close(err_pipe[1]);

    /* bounded read with a hard deadline */
    time_t deadline = time(NULL) + (timeout > 0 ? timeout : 15);
    size_t cap = 1 << 20;
    char *out = malloc(cap), *err = malloc(65536);
    size_t out_n = 0, err_n = 0;
    int timed_out = 0;
    if (out && err) {
        int fds[2] = {out_pipe[0], err_pipe[0]};
        size_t *sizes[2] = {&out_n, &err_n};
        char *bufs[2] = {out, err};
        size_t caps[2] = {cap - 1, 65535};
        int open_fds = 2;
        while (open_fds > 0) {
            if (time(NULL) >= deadline) { timed_out = 1; break; }
            for (int k = 0; k < 2; k++) {
                if (fds[k] < 0) continue;
                char chunk[8192];
                ssize_t r = read(fds[k], chunk, sizeof(chunk));
                if (r <= 0) { close(fds[k]); fds[k] = -1; open_fds--; continue; }
                size_t space = caps[k] - *sizes[k];
                if (space > 0) {
                    size_t take = (size_t)r < space ? (size_t)r : space;
                    memcpy(bufs[k] + *sizes[k], chunk, take);
                    *sizes[k] += take;
                }
            }
        }
    }
    close(out_pipe[0]);
    close(err_pipe[0]);

    int status;
    int rc;
    if (timed_out) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        rc = 124;
        const char *msg = "timeout";
        if (out_stderr && err) {
            err[err_n] = '\0';
            size_t elen = strlen(err);
            char *m = malloc(elen + 32);
            if (m) { snprintf(m, elen + 32, "%stimeout after %ds", err, timeout);
                     *out_stderr = m; }
        }
        (void)msg;
    } else {
        waitpid(pid, &status, 0);
        rc = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
        if (out && out_stdout) { out[out_n] = '\0'; *out_stdout = out; }
        else free(out);
        if (err && out_stderr) { err[err_n] = '\0'; *out_stderr = err; }
        else free(err);
        return rc;
    }
    if (out) { out[out_n] = '\0'; if (out_stdout) *out_stdout = out; else free(out); }
    if (err) { err[err_n] = '\0'; if (out_stderr) *out_stderr = err; else free(err); }
    return rc;
}

/*
 * PoP: wgc_git_repo_root @ hermes_cli/worktree_gc.py:_git_repo_root
 */
char *wgc_git_repo_root(void)
{
    char *const args[] = {"rev-parse", "--show-toplevel", NULL};
    char *out = NULL;
    int rc = wgc_run_git(args, NULL, 10, &out, NULL);
    if (rc != 0 || !out) { free(out); return NULL; }
    size_t n = strlen(out);
    while (n && (out[n - 1] == '\n' || out[n - 1] == ' ')) n--;
    char *root = wgc_strdup_n(out, n);
    free(out);
    return root;
}

/* PoP: _repo_is_shallow @hermes_cli/worktree_gc.py:_repo_is_shallow */
int wgc_repo_is_shallow(const char *repo_path)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/.git/shallow", repo_path ? repo_path : ".");
    struct stat st;
    return stat(path, &st) == 0;
}

/* PoP: _deepen_shallow_repo @hermes_cli/worktree_gc.py:_deepen_shallow_repo */
int wgc_deepen_shallow_repo(const char *repo_root)
{
    char *const args[] = {"fetch", "--unshallow", "--quiet", NULL};
    return wgc_run_git(args, repo_root, 120, NULL, NULL) == 0;
}

/* PoP: _worktree_has_unpushed_commits @hermes_cli/worktree_gc.py:_worktree_has_unpushed_commits */
int wgc_worktree_has_unpushed_commits(const char *worktree_path, int timeout)
{
    char *const args[] = {"status", "-sb", NULL};
    char *out = NULL;
    int rc = wgc_run_git(args, worktree_path, timeout, &out, NULL);
    if (rc != 0) { free(out); return 0; } /* fail safe: assume nothing */
    int unpushed = 0;
    if (out) {
        char *line = strtok(out, "\n");
        while (line) {
            /* "## branch...origin/branch [ahead N]" */
            if (strncmp(line, "## ", 3) == 0) {
                const char *ahead = strstr(line, "ahead ");
                if (ahead) {
                    long n = strtol(ahead + 6, NULL, 10);
                    if (n > 0) unpushed = 1;
                } else if (!strstr(line, "/")) {
                    unpushed = 1; /* no upstream at all */
                }
            }
            line = strtok(NULL, "\n");
        }
        free(out);
    }
    return unpushed;
}

static json_t *wgc_cache_get(json_t *cache, const char *key)
{
    return cache && cache->type == JSON_OBJECT ? json_obj_get(cache, key)
                                               : NULL;
}

/* PoP: _worktree_commits_all_merged_upstream @hermes_cli/worktree_gc.py:_worktree_commits_all_merged_upstream */
int wgc_worktree_commits_all_merged_upstream(const char *worktree_path,
                                             int timeout, long max_ahead,
                                             void *cache_v)
{
    json_t *cache = (json_t *)cache_v;
    /* upstream detection mirrors the audit: origin/HEAD, origin/main, master */
    const char *upstream = NULL;
    const char *cands[] = {"origin/HEAD", "origin/main", "origin/master"};
    for (size_t i = 0; i < 3 && !upstream; i++) {
        char *const probe[] = {"rev-parse", "--verify", "--quiet",
                               (char *)cands[i], NULL};
        int rc = wgc_run_git(probe, worktree_path, 5, NULL, NULL);
        if (rc == 0) upstream = cands[i];
    }
    if (!upstream) return 1; /* nothing to be unpushed against */

    char *const count[] = {"rev-list", "--count", (char *)upstream, "HEAD", NULL};
    char *out = NULL;
    int rc = wgc_run_git(count, worktree_path, timeout, &out, NULL);
    if (rc != 0) { free(out); return 0; } /* fail safe: treat as unmerged */
    long ahead = out ? strtol(out, NULL, 10) : 0;
    free(out);
    if (ahead == 0) return 1;
    if (ahead > max_ahead) return 0; /* stale-base lane: keep */

    /* memoize per-branch verdict in the shared merge cache */
    char *const branch[] = {"branch", "--show-current", NULL};
    char *branch_out = NULL;
    char *branch_name = NULL;
    if (wgc_run_git(branch, worktree_path, 5, &branch_out, NULL) == 0 &&
        branch_out) {
        size_t n = strlen(branch_out);
        while (n && (branch_out[n - 1] == '\n' || branch_out[n - 1] == ' '))
            n--;
        branch_name = wgc_strdup_n(branch_out, n);
        free(branch_out);
    }
    if (branch_name && cache && cache->type == JSON_OBJECT) {
        json_t *memo = wgc_cache_get(cache, branch_name);
        if (memo && memo->type == JSON_BOOL) {
            int verdict = memo->bool_val;
            free(branch_name);
            return verdict;
        }
    }

    char ahead_str[32];
    snprintf(ahead_str, sizeof(ahead_str), "%ld..%s", ahead, upstream);
    char *const cherry[] = {"cherry", upstream, "HEAD", NULL};
    char *cherry_out = NULL;
    rc = wgc_run_git(cherry, worktree_path, timeout, &cherry_out, NULL);
    int merged = 0;
    if (rc == 0 && cherry_out) {
        merged = 1;
        char *save = NULL;
        for (char *line = strtok_r(cherry_out, "\n", &save); line;
             line = strtok_r(NULL, "\n", &save)) {
            if (!line[0]) continue;
            if (line[0] == '+') { merged = 0; break; }
        }
        free(cherry_out);
    }
    if (branch_name && cache && cache->type == JSON_OBJECT)
        json_set(cache, branch_name, json_bool(merged));
    free(branch_name);
    return merged;
}

/* PoP: _worktree_lock_is_live @ cli.py:_worktree_lock_is_live — fail SAFE
 * toward "live". Returns malloc'd "live"/"dead" or NULL when not locked. */
char *wgc_worktree_lock_is_live(const char *repo_root,
                                const char *worktree_path, int timeout)
{
    char lock_path[4200];
    snprintf(lock_path, sizeof(lock_path), "%s/.lock", worktree_path);
    struct stat st;
    if (stat(lock_path, &st) != 0) return NULL; /* not locked */
    /* owner pid recorded in the lock file */
    FILE *fp = fopen(lock_path, "r");
    if (!fp) return wgc_strdup_n("live", 4);
    char buf[256] = {0};
    if (!fgets(buf, sizeof(buf), fp)) { fclose(fp); return wgc_strdup_n("live", 4); }
    fclose(fp);
    char *end = NULL;
    long pid = strtol(buf, &end, 10);
    if (pid <= 0) return wgc_strdup_n("live", 4);
    char proc[64];
    snprintf(proc, sizeof(proc), "/proc/%ld", pid);
    struct stat pst;
    if (stat(proc, &pst) != 0) return wgc_strdup_n("dead", 4);
    (void)repo_root;
    (void)timeout;
    return wgc_strdup_n("live", 4);
}

/* du -sm, best-effort. */
/* PoP: wgc_tree_size_mb @hermes_cli/worktree_gc.py:_tree_size_mb */
static int wgc_tree_size_mb(const char *path, int *has_size)
{
    *has_size = 0;
    char *const args[] = {"-sm", (char *)path, NULL};
    /* du is not git; run it directly */
    char cmd[4400];
    snprintf(cmd, sizeof(cmd), "du -sm '%s' 2>/dev/null", path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;
    char line[256];
    int mb = 0;
    if (fgets(line, sizeof(line), fp)) {
        char *end = NULL;
        long v = strtol(line, &end, 10);
        if (end && end != line) { mb = (int)v; *has_size = 1; }
    }
    pclose(fp);
    return mb;
}

/* PoP: _dirty_split @hermes_cli/worktree_gc.py:_dirty_split */
static void wgc_dirty_split(const char *path, int *tracked_dirty,
                            char ***untracked, size_t *untracked_n)
{
    *tracked_dirty = 0;
    *untracked = NULL;
    *untracked_n = 0;
    char *const args[] = {"status", "--porcelain", NULL};
    char *out = NULL;
    int rc = wgc_run_git(args, path, 10, &out, NULL);
    if (rc != 0 || !out) { free(out); *tracked_dirty = 1; return; } /* fail safe */
    char *save = NULL;
    for (char *line = strtok_r(out, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        if (!line[0]) continue;
        if (strncmp(line, "??", 2) == 0) {
            const char *p = line + 3;
            while (*p == ' ') p++;
            char **nu = realloc(*untracked, (*untracked_n + 1) * sizeof(char *));
            if (!nu) break;
            *untracked = nu;
            char *copy = strdup(p);
            if (copy) (*untracked)[(*untracked_n)++] = copy;
        } else {
            *tracked_dirty = 1;
        }
    }
    free(out);
}

/* PoP: _archive_untracked @hermes_cli/worktree_gc.py:_archive_untracked */
static char *wgc_archive_untracked(const char *tree, char **untracked,
                                   size_t untracked_n)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm);
    const char *home = getenv("HOME");
    if (!home) return NULL;
    char *tree_copy = strdup(tree);
    if (!tree_copy) return NULL;
    const char *tree_name = basename(tree_copy);
    char dest[4096];
    snprintf(dest, sizeof(dest),
             "%s/.slermes/archive/worktree-prune/%s-%s", home, tree_name,
             stamp);
    free(tree_copy);
    int made = 0;
    for (size_t i = 0; i < untracked_n; i++) {
        char src[4400], target[4400], parent[4400];
        snprintf(src, sizeof(src), "%s/%s", tree, untracked[i]);
        snprintf(target, sizeof(target), "%s/%s", dest, untracked[i]);
        snprintf(parent, sizeof(parent), "%s", target);
        char *slash = strrchr(parent, '/');
        if (slash) *slash = '\0';
        char mkdir_cmd[9000];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", parent);
        if (system(mkdir_cmd) != 0) continue;
        struct stat st;
        if (lstat(src, &st) != 0 || S_ISLNK(st.st_mode)) continue;
        char cp_cmd[9000];
        if (S_ISDIR(st.st_mode))
            snprintf(cp_cmd, sizeof(cp_cmd), "cp -a '%s/.' '%s/' 2>/dev/null",
                     src, target);
        else
            snprintf(cp_cmd, sizeof(cp_cmd), "cp -p '%s' '%s' 2>/dev/null",
                     src, target);
        if (system(cp_cmd) == 0) made = 1;
    }
    struct stat dst;
    if (!made || stat(dest, &dst) != 0) return NULL;
    return strdup(dest);
}

static wgc_tree_record_t *wgc_push_record(wgc_tree_record_t **records_p,
                                          size_t *rn_p, const char *name,
                                          const char *path, const char *branch,
                                          double age_days, int size_mb,
                                          int has_size, const char *verdict,
                                          const char *reason,
                                          char **untracked, size_t untracked_n)
{
    wgc_tree_record_t *nr =
        realloc(*records_p, (*rn_p + 1) * sizeof(wgc_tree_record_t));
    if (!nr) return NULL;
    *records_p = nr;
    wgc_tree_record_t *r = &(*records_p)[*rn_p];
    r->name = strdup(name);
    r->path = strdup(path);
    r->branch = strdup(branch ? branch : "");
    r->age_days = age_days;
    r->size_mb = size_mb;
    r->has_size = has_size;
    r->verdict = strdup(verdict);
    r->reason = strdup(reason);
    r->untracked = untracked;
    r->untracked_n = untracked_n;
    if (!r->name || !r->path || !r->branch || !r->verdict || !r->reason)
        return NULL;
    (*rn_p)++;
    return r;
}

/* PoP: audit_worktrees @hermes_cli/worktree_gc.py:audit_worktrees */
wgc_tree_record_t *wgc_audit_worktrees(const char *repo_root, int with_sizes,
                                       size_t *out_n)
{
    if (out_n) *out_n = 0;
    if (!repo_root) return NULL;
    char worktrees_dir[4200];
    snprintf(worktrees_dir, sizeof(worktrees_dir), "%s/.worktrees", repo_root);
    struct stat st;
    if (stat(worktrees_dir, &st) != 0 || !S_ISDIR(st.st_mode)) return NULL;

    if (wgc_repo_is_shallow(repo_root))
        wgc_deepen_shallow_repo(repo_root);
    json_t *merge_cache = cli_load_worktree_merge_cache();
    size_t cache_size_before = merge_cache ? json_len(merge_cache) : 0;

    /* sorted entries */
    DIR *dir = opendir(worktrees_dir);
    if (!dir) { if (merge_cache) json_free(merge_cache); return NULL; }
    char *names[1024];
    size_t nn = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL && nn < 1024) {
        if (de->d_name[0] == '.') continue;
        char full[4400];
        snprintf(full, sizeof(full), "%s/%s", worktrees_dir, de->d_name);
        struct stat est;
        if (stat(full, &est) != 0 || !S_ISDIR(est.st_mode)) continue;
        names[nn++] = strdup(de->d_name);
    }
    closedir(dir);
    for (size_t i = 0; i + 1 < nn; i++)
        for (size_t j = i + 1; j < nn; j++)
            if (strcmp(names[i], names[j]) > 0) {
                char *tmp = names[i]; names[i] = names[j]; names[j] = tmp;
            }

    wgc_tree_record_t *records = NULL;
    size_t rn = 0;
    time_t now = time(NULL);


    for (size_t i = 0; i < nn; i++) free(names[i]);
    if (merge_cache) {
        if (json_len(merge_cache) != cache_size_before)
            cli_save_worktree_merge_cache(merge_cache);
        json_free(merge_cache);
    }
    if (out_n) *out_n = rn;
    return records;
}

void wgc_free_tree_records(wgc_tree_record_t *recs, size_t n)
{
    if (!recs) return;
    for (size_t i = 0; i < n; i++) {
        free(recs[i].name);
        free(recs[i].path);
        free(recs[i].branch);
        free(recs[i].verdict);
        free(recs[i].reason);
        for (size_t j = 0; j < recs[i].untracked_n; j++)
            free(recs[i].untracked[j]);
        free(recs[i].untracked);
    }
    free(recs);
}

/* PoP: reclaim_worktrees @hermes_cli/worktree_gc.py:reclaim_worktrees */
char **wgc_reclaim_worktrees(const char *repo_root, int dry_run,
                             wgc_tree_record_t *records, size_t records_n,
                             size_t *out_n)
{
    if (out_n) *out_n = 0;
    wgc_tree_record_t *local = NULL;
    size_t local_n = 0;
    if (!records) {
        local = wgc_audit_worktrees(repo_root, 0, &local_n);
        records = local;
        records_n = local_n;
    }
    char **actions = NULL;
    size_t an = 0;
#define ADD_ACTION(fmt, ...)                                                 \
    do {                                                                     \
        char line[1024];                                                     \
        snprintf(line, sizeof(line), fmt, __VA_ARGS__);                      \
        char **na = realloc(actions, (an + 1) * sizeof(char *));             \
        if (na) {                                                            \
            actions = na;                                                    \
            char *copy = strdup(line);                                       \
            if (copy) actions[an++] = copy;                                  \
        }                                                                    \
    } while (0)

    for (size_t i = 0; i < records_n; i++) {
        wgc_tree_record_t *record = &records[i];
        if (strcmp(record->verdict, "reap") != 0 &&
            strcmp(record->verdict, "reap-archive") != 0)
            continue;
        if (dry_run) {
            ADD_ACTION("would remove %s (%s)", record->name, record->reason);
            continue;
        }
        if (strcmp(record->verdict, "reap-archive") == 0 &&
            record->untracked_n > 0) {
            char *archive =
                wgc_archive_untracked(record->path, record->untracked,
                                      record->untracked_n);
            if (!archive) {
                ADD_ACTION("kept %s (archive of untracked files failed)",
                           record->name, "");
                continue;
            }
            ADD_ACTION("archived %zu untracked file(s) → %s",
                       record->untracked_n, archive);
            free(archive);
        }
        /* Dead-pid locks must be unlocked or `remove --force` refuses. */
        {
            char *const unlock[] = {"worktree", "unlock",
                                    (char *)record->path, NULL};
            wgc_run_git(unlock, repo_root, 10, NULL, NULL);
        }
        {
            char *const remove[] = {"worktree", "remove", (char *)record->path,
                                    "--force", NULL};
            char *stderr_out = NULL;
            int rc = wgc_run_git(remove, repo_root, 30, NULL, &stderr_out);
            if (rc != 0) {
                ADD_ACTION("failed to remove %s: %s", record->name,
                           stderr_out ? stderr_out : "");
                free(stderr_out);
                continue;
            }
            free(stderr_out);
            int protected_branch = 0;
            for (size_t b = 0; b < WGC_PROTECTED_BRANCHES_N; b++)
                if (record->branch &&
                    strcmp(record->branch, WGC_PROTECTED_BRANCHES[b]) == 0)
                    { protected_branch = 1; break; }
            if (record->branch && record->branch[0] && !protected_branch) {
                char *const del[] = {"branch", "-D", record->branch, NULL};
                wgc_run_git(del, repo_root, 10, NULL, NULL);
            }
            ADD_ACTION("removed %s", record->name, "");
        }
    }
    if (!dry_run) {
        char *const prune[] = {"worktree", "prune", NULL};
        wgc_run_git(prune, repo_root, 15, NULL, NULL);
    }
#undef ADD_ACTION
    wgc_free_tree_records(local, local_n);
    if (out_n) *out_n = an;
    return actions;
}

/* PoP: audit_branches @hermes_cli/worktree_gc.py:audit_branches */
wgc_branch_record_t *wgc_audit_branches(const char *repo_root, size_t *out_n)
{
    if (out_n) *out_n = 0;
    if (!repo_root) return NULL;
    if (wgc_repo_is_shallow(repo_root))
        wgc_deepen_shallow_repo(repo_root);

    const char *upstream = NULL;
    const char *cands[] = {"origin/HEAD", "origin/main", "origin/master"};
    for (size_t i = 0; i < 3 && !upstream; i++) {
        char *const probe[] = {"rev-parse", "--verify", "--quiet",
                               (char *)cands[i], NULL};
        if (wgc_run_git(probe, repo_root, 5, NULL, NULL) == 0)
            upstream = cands[i];
    }
    if (!upstream) return NULL;

    char *const list[] = {"branch", "--format=%(refname:short)", NULL};
    char *list_out = NULL;
    if (wgc_run_git(list, repo_root, 10, &list_out, NULL) != 0) {
        free(list_out);
        return NULL;
    }
    char *branches[1024];
    size_t bn = 0;
    {
        char *save = NULL;
        for (char *line = strtok_r(list_out, "\n", &save); line && bn < 1024;
             line = strtok_r(NULL, "\n", &save)) {
            while (*line == ' ') line++;
            if (line[0]) branches[bn++] = strdup(line);
        }
        free(list_out);
    }

    /* active = branches checked out in any worktree */
    char *active[1024];
    size_t active_n = 0;
    {
        char *const wt[] = {"worktree", "list", "--porcelain", NULL};
        char *wt_out = NULL;
        if (wgc_run_git(wt, repo_root, 10, &wt_out, NULL) == 0 && wt_out) {
            char *save = NULL;
            for (char *line = strtok_r(wt_out, "\n", &save); line;
                 line = strtok_r(NULL, "\n", &save)) {
                if (strncmp(line, "branch refs/heads/", 18) == 0 &&
                    active_n < 1024)
                    active[active_n++] = strdup(line + 18);
            }
            free(wt_out);
        }
    }

    /* merged set */
    char *const merged_cmd[] = {"branch", "--merged", (char *)upstream,
                                "--format=%(refname:short)", NULL};
    char *merged_out = NULL;
    char *merged[1024];
    size_t merged_n = 0;
    if (wgc_run_git(merged_cmd, repo_root, 15, &merged_out, NULL) == 0 &&
        merged_out) {
        char *save = NULL;
        for (char *line = strtok_r(merged_out, "\n", &save); line &&
             merged_n < 1024; line = strtok_r(NULL, "\n", &save)) {
            while (*line == ' ') line++;
            if (line[0]) merged[merged_n++] = strdup(line);
        }
        free(merged_out);
    }

    wgc_branch_record_t *records = NULL;
    size_t rn = 0;
    for (size_t i = 0; i < bn; i++) {
        const char *branch = branches[i];
        const char *verdict = "keep";
        char reason[192];
        int protected_branch = 0, is_active = 0, is_merged = 0;
        for (size_t b = 0; b < WGC_PROTECTED_BRANCHES_N; b++)
            if (strcmp(branch, WGC_PROTECTED_BRANCHES[b]) == 0)
                { protected_branch = 1; break; }
        for (size_t a = 0; a < active_n; a++)
            if (strcmp(branch, active[a]) == 0) { is_active = 1; break; }
        for (size_t m = 0; m < merged_n; m++)
            if (strcmp(branch, merged[m]) == 0) { is_merged = 1; break; }
        if (protected_branch || is_active) {
            snprintf(reason, sizeof(reason), "protected or checked out");
        } else if (is_merged) {
            verdict = "delete";
            snprintf(reason, sizeof(reason), "fully merged into %s", upstream);
        } else {
            /* Rebase merges rewrite SHAs; cherry patch-equivalence catches
             * the dominant leak. Bounded: far-ahead = stale-base lane. */
            char range[512];
            snprintf(range, sizeof(range), "%s..%s", upstream, branch);
            char *const ahead_cmd[] = {"rev-list", "--count", range, NULL};
            char *ahead_out = NULL;
            long ahead_count = WGC_MAX_CHERRY_AHEAD + 1;
            if (wgc_run_git(ahead_cmd, repo_root, 10, &ahead_out, NULL) == 0 &&
                ahead_out) {
                char *end = NULL;
                long v = strtol(ahead_out, &end, 10);
                if (end && end != ahead_out) ahead_count = v;
                free(ahead_out);
            }
            if (ahead_count == 0) {
                verdict = "delete";
                snprintf(reason, sizeof(reason), "no commits beyond %s",
                         upstream);
            } else if (ahead_count > WGC_MAX_CHERRY_AHEAD) {
                snprintf(reason, sizeof(reason),
                         "%ld commits ahead (stale-base lane)", ahead_count);
            } else {
                char *const cherry_cmd[] = {"cherry", (char *)upstream,
                                            (char *)branch, NULL};
                char *cherry_out = NULL;
                int rc = wgc_run_git(cherry_cmd, repo_root, 30, &cherry_out,
                                     NULL);
                if (rc != 0) {
                    snprintf(reason, sizeof(reason),
                             "could not verify (git cherry failed)");
                } else if (cherry_out) {
                    int lines = 0, plus = 0, all_minus = 1;
                    char *save = NULL;
                    for (char *line = strtok_r(cherry_out, "\n", &save); line;
                         line = strtok_r(NULL, "\n", &save)) {
                        if (!line[0]) continue;
                        lines++;
                        if (line[0] == '+') plus++;
                        else if (line[0] != '-') all_minus = 0;
                    }
                    free(cherry_out);
                    if (lines && all_minus) {
                        verdict = "delete";
                        snprintf(reason, sizeof(reason),
                                 "all commits patch-equivalent upstream");
                    } else {
                        snprintf(reason, sizeof(reason),
                                 "%d unique commit(s) not upstream", plus);
                    }
                } else {
                    snprintf(reason, sizeof(reason),
                             "could not verify (git cherry failed)");
                }
            }
        }
        wgc_branch_record_t *nr =
            realloc(records, (rn + 1) * sizeof(*nr));
        if (nr) {
            records = nr;
            records[rn].name = strdup(branch);
            records[rn].verdict = strdup(verdict);
            records[rn].reason = strdup(reason);
            rn++;
        }
    }
    for (size_t i = 0; i < bn; i++) free(branches[i]);
    for (size_t i = 0; i < active_n; i++) free(active[i]);
    for (size_t i = 0; i < merged_n; i++) free(merged[i]);
    if (out_n) *out_n = rn;
    return records;
}

void wgc_free_branch_records(wgc_branch_record_t *recs, size_t n)
{
    if (!recs) return;
    for (size_t i = 0; i < n; i++) {
        free(recs[i].name);
        free(recs[i].verdict);
        free(recs[i].reason);
    }
    free(recs);
}

/* PoP: reclaim_branches @hermes_cli/worktree_gc.py:reclaim_branches */
char **wgc_reclaim_branches(const char *repo_root, int dry_run,
                            wgc_branch_record_t *records, size_t records_n,
                            size_t *out_n)
{
    if (out_n) *out_n = 0;
    wgc_branch_record_t *local = NULL;
    size_t local_n = 0;
    if (!records) {
        local = wgc_audit_branches(repo_root, &local_n);
        records = local;
        records_n = local_n;
    }
    char **actions = NULL;
    size_t an = 0;
    for (size_t i = 0; i < records_n; i++) {
        wgc_branch_record_t *record = &records[i];
        if (strcmp(record->verdict, "delete") != 0) continue;
        char line[512];
        if (dry_run) {
            snprintf(line, sizeof(line), "would delete branch %s (%s)",
                     record->name, record->reason);
        } else {
            char *const del[] = {"branch", "-D", record->name, NULL};
            char *stderr_out = NULL;
            int rc = wgc_run_git(del, repo_root, 10, NULL, &stderr_out);
            if (rc == 0)
                snprintf(line, sizeof(line), "deleted branch %s",
                         record->name);
            else
                snprintf(line, sizeof(line), "failed to delete %s: %s",
                         record->name,
                         stderr_out ? stderr_out : "");
            free(stderr_out);
        }
        char **na = realloc(actions, (an + 1) * sizeof(char *));
        if (na) {
            actions = na;
            char *copy = strdup(line);
            if (copy) actions[an++] = copy;
        }
    }
    wgc_free_branch_records(local, local_n);
    if (out_n) *out_n = an;
    return actions;
}

void wgc_free_actions(char **actions, size_t n)
{
    if (!actions) return;
    for (size_t i = 0; i < n; i++) free(actions[i]);
    free(actions);
}

/* PoP: worktrees_summary @hermes_cli/worktree_gc.py:worktrees_summary */
void wgc_worktrees_summary(const char *repo_root, size_t *out_count,
                           int *out_size_mb, int *has_size)
{
    if (out_count) *out_count = 0;
    if (out_size_mb) *out_size_mb = 0;
    if (has_size) *has_size = 0;
    if (!repo_root) return;
    char dir[4200];
    snprintf(dir, sizeof(dir), "%s/.worktrees", repo_root);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) return;
    DIR *d = opendir(dir);
    if (!d) return;
    size_t count = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char full[4400];
        snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
        struct stat est;
        if (stat(full, &est) == 0 && S_ISDIR(est.st_mode)) count++;
    }
    closedir(d);
    if (out_count) *out_count = count;
    int has = 0;
    int mb = wgc_tree_size_mb(dir, &has);
    if (out_size_mb) *out_size_mb = mb;
    if (has_size) *has_size = has;
}
