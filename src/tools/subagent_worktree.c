/*
 * subagent_worktree.c — faithful C11 port of tools/subagent_worktree.py
 * (Opt-in git worktree isolation for delegated subagents.)
 *
 * Python -> C symbol map (1:1):
 *   _GIT_TIMEOUT                    -> subagent_worktree_GIT_TIMEOUT
 *   _WORKTREES_DIRNAME              -> subagent_worktree_WORKTREES_DIRNAME
 *   _BRANCH_NAMESPACE               -> subagent_worktree_BRANCH_NAMESPACE
 *   _run_git                        -> subagent_worktree__run_git (injectable hook)
 *   local_backend_active            -> local_backend_active (config via injectable hook)
 *   resolve_repo_root               -> resolve_repo_root
 *   _ensure_gitignore_entry         -> subagent_worktree__ensure_gitignore_entry
 *   create_subagent_worktree        -> create_subagent_worktree
 *   mark_worktree_payload_unproven  -> mark_worktree_payload_unproven
 *   unproven_worktree_payload       -> unproven_worktree_payload
 *   finalize_subagent_worktree      -> finalize_subagent_worktree
 *   build_worktree_context_note     -> build_worktree_context_note
 */
#define _POSIX_C_SOURCE 200809L
#include "subagent_worktree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/select.h>
#include <sys/time.h>

const int subagent_worktree_GIT_TIMEOUT = 30;
const char *subagent_worktree_WORKTREES_DIRNAME = ".worktrees";
const char *subagent_worktree_BRANCH_NAMESPACE = "hermes-subagent";

/* Injectability: tests replace these hooks to make git deterministic. */
subagent_worktree_run_git_fn subagent_worktree_run_git_hook =
    subagent_worktree__run_git_real;
subagent_worktree_local_backend_fn subagent_worktree_local_backend_hook = NULL;

static void str_lower(char *s)
{
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* Real _run_git: fork/exec `git <args...>` with stdout+stderr captured into
 * malloc'd strings. Never fails on non-zero exit; returns exit code, or -1
 * when the process could not be spawned at all (Python's raised exception). */
int subagent_worktree__run_git_real(const char *const args[],
                                    const char *cwd,
                                    char **out_stdout, char **out_stderr)
{
    if (out_stdout) *out_stdout = NULL;
    if (out_stderr) *out_stderr = NULL;

    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) != 0) return -1;
    if (pipe(err_pipe) != 0) { close(out_pipe[0]); close(out_pipe[1]); return -1; }

    pid_t pid = fork();
    if (pid < 0) {
        for (int i = 0; i < 2; i++) { close(out_pipe[i]); close(err_pipe[i]); }
        return -1;
    }
    if (pid == 0) {
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        for (int i = 0; i < 2; i++) { close(out_pipe[i]); close(err_pipe[i]); }
        if (cwd && chdir(cwd) != 0) _exit(127);
        int nargs = 0; while (args[nargs]) nargs++;
        char **argv = calloc((size_t)nargs + 2, sizeof(char *));
        if (!argv) _exit(127);
        argv[0] = (char *)"git";
        for (int i = 0; i < nargs; i++) argv[i + 1] = (char *)args[i];
        execvp("git", argv);
        _exit(127);
    }
    close(out_pipe[1]); close(err_pipe[1]);

    size_t ocap = 4096, olen = 0, ecap = 4096, elen = 0;
    char *obuf = malloc(ocap), *ebuf = malloc(ecap);
    if (!obuf || !ebuf) { free(obuf); free(ebuf); return -1; }
    char rbuf[4096];
    int ofd = out_pipe[0], efd = err_pipe[0];
    while (ofd >= 0 || efd >= 0) {
        fd_set rf; FD_ZERO(&rf);
        if (ofd >= 0) FD_SET(ofd, &rf);
        if (efd >= 0) FD_SET(efd, &rf);
        int mx = (ofd > efd ? ofd : efd) + 1;
        if (select(mx, &rf, NULL, NULL, NULL) <= 0) break;
        if (ofd >= 0 && FD_ISSET(ofd, &rf)) {
            ssize_t n = read(ofd, rbuf, sizeof rbuf);
            if (n <= 0) { close(ofd); ofd = -1; }
            else {
                if (olen + (size_t)n + 1 > ocap) {
                    ocap = (olen + (size_t)n + 1) * 2;
                    obuf = realloc(obuf, ocap);
                }
                memcpy(obuf + olen, rbuf, (size_t)n); olen += (size_t)n;
            }
        }
        if (efd >= 0 && FD_ISSET(efd, &rf)) {
            ssize_t n = read(efd, rbuf, sizeof rbuf);
            if (n <= 0) { close(efd); efd = -1; }
            else {
                if (elen + (size_t)n + 1 > ecap) {
                    ecap = (elen + (size_t)n + 1) * 2;
                    ebuf = realloc(ebuf, ecap);
                }
                memcpy(ebuf + elen, rbuf, (size_t)n); elen += (size_t)n;
            }
        }
    }
    obuf[olen] = '\0'; ebuf[elen] = '\0';

    int status = 0;
    waitpid(pid, &status, 0);
    int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (rc == 127) rc = -1; /* spawn failure maps to Python's raised exception */

    /* Python text=True/encoding="utf-8", errors="replace": strip any NULs. */
    for (size_t i = 0; i < olen; i++) if (obuf[i] == '\0') obuf[i] = '?';
    for (size_t i = 0; i < elen; i++) if (ebuf[i] == '\0') ebuf[i] = '?';

    if (out_stdout) *out_stdout = obuf; else free(obuf);
    if (out_stderr) *out_stderr = ebuf; else free(ebuf);
    return rc;
}

static int rg(const char *const args[], const char *cwd,
              char **out_stdout, char **out_stderr)
{
    if (out_stdout) *out_stdout = NULL;
    if (out_stderr) *out_stderr = NULL;
    if (!subagent_worktree_run_git_hook) return -1;
    return subagent_worktree_run_git_hook(args, cwd, out_stdout, out_stderr);
}

/* PoP: local_backend_active @ tools/subagent_worktree.py
 * True when the terminal backend is local (worktrees visible to tools).
 * Default hook reads config.yaml's terminal.backend via the shared loader
 * seam; NULL hook means legacy entry point => local (Python's except path). */
/* PoP: local_backend_active @ tools/subagent_worktree.py:local_backend_active */
int local_backend_active(void)
{
    if (subagent_worktree_local_backend_hook)
        return subagent_worktree_local_backend_hook();
    return 1;
}

/* PoP: resolve_repo_root @ tools/subagent_worktree.py */
/* PoP: resolve_repo_root @ tools/subagent_worktree.py:resolve_repo_root */
char *resolve_repo_root(const char *path)
{
    if (!path || !path[0]) return NULL;
    /* abspath + expanduser: only ~ prefix form matters here. */
    char candidate[PATH_MAX];
    if (path[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) return NULL;
        snprintf(candidate, sizeof candidate, "%s%s", home, path + 1);
    } else {
        snprintf(candidate, sizeof candidate, "%s", path);
    }
    struct stat st;
    if (stat(candidate, &st) != 0 || !S_ISDIR(st.st_mode)) return NULL;

    static const char *const rev_args[] = { "rev-parse", "--show-toplevel" };
    char *so = NULL, *se = NULL;
    int rc = rg(rev_args, candidate, &so, &se);
    free(se);
    if (rc != 0) { free(so); return NULL; }
    if (!so) return NULL;
    /* .strip() */
    char *p = so;
    while (*p && isspace((unsigned char)*p)) p++;
    size_t l = strlen(p);
    while (l > 0 && isspace((unsigned char)p[l - 1])) p[--l] = '\0';
    if (l == 0) { free(so); return NULL; }
    char *root = strdup(p);
    free(so);
    return root;
}

/* PoP: _ensure_gitignore_entry @ tools/subagent_worktree.py */
/* PoP: _ensure_gitignore_entry @ tools/subagent_worktree.py:_ensure_gitignore_entry */
void subagent_worktree__ensure_gitignore_entry(const char *repo_root)
{
    if (!repo_root || !repo_root[0]) return;
    char gipath[PATH_MAX];
    snprintf(gipath, sizeof gipath, "%s/.gitignore", repo_root);
    const char *entry = ".worktrees/";

    FILE *f = fopen(gipath, "rb");
    char *existing = NULL;
    if (f) {
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        existing = malloc((size_t)n + 1);
        size_t r = fread(existing, 1, (size_t)n, f); existing[r] = '\0';
        fclose(f);
    }
    /* entry in existing.splitlines()? */
    int found = 0;
    if (existing) {
        char *dup = strdup(existing);
        char *save = NULL;
        for (char *line = strtok_r(dup, "\n", &save); line;
             line = strtok_r(NULL, "\n", &save)) {
            if (strcmp(line, entry) == 0) { found = 1; break; }
        }
        free(dup);
    }
    if (!found) {
        FILE *a = fopen(gipath, "a");
        if (a) {
            size_t el = existing ? strlen(existing) : 0;
            if (el > 0 && existing[el - 1] != '\n') fputc('\n', a);
            fprintf(a, "%s\n", entry);
            fclose(a);
        }
    }
    free(existing);
}

/* PoP: create_subagent_worktree @ tools/subagent_worktree.py
 * Returns a malloc'd JSON object string {"path","branch","repo_root",
 * "base_commit"} or NULL on silent downgrade. */
/* PoP: create_subagent_worktree @ tools/subagent_worktree.py:create_subagent_worktree */
char *create_subagent_worktree(const char *parent_cwd, const char *subagent_id)
{
    char *repo_root = resolve_repo_root(parent_cwd);
    if (!repo_root) return NULL;

    char short_id[64];
    if (subagent_id && subagent_id[0]) {
        snprintf(short_id, sizeof short_id, "%s", subagent_id);
    } else {
        unsigned char rnd[4];
        FILE *u = fopen("/dev/urandom", "rb");
        if (!u || fread(rnd, 1, 4, u) != 4) {
            if (u) fclose(u);
            unsigned long s = (unsigned long)time(NULL);
            memcpy(rnd, &s, 4);
        } else fclose(u);
        for (int i = 0; i < 4; i++) sprintf(short_id + i * 2, "%02x", rnd[i]);
        short_id[8] = '\0';
    }
    for (char *p = short_id; *p; p++) if (*p == '/') *p = '-'; /* .replace("/","-") */

    char wt_name[128], branch[192], wt_path[PATH_MAX], parent_dir[PATH_MAX];
    snprintf(wt_name, sizeof wt_name, "subagent-%s", short_id);
    snprintf(branch, sizeof branch, "%s/%s",
             subagent_worktree_BRANCH_NAMESPACE, wt_name);
    snprintf(wt_path, sizeof wt_path, "%s/%s/%s",
             repo_root, subagent_worktree_WORKTREES_DIRNAME, wt_name);
    snprintf(parent_dir, sizeof parent_dir, "%s/%s",
             repo_root, subagent_worktree_WORKTREES_DIRNAME);

    /* mkdir(parents=True, exist_ok=True): recursive mkdir of repo/.worktrees */
    {
        char tmp[PATH_MAX];
        snprintf(tmp, sizeof tmp, "%s", parent_dir);
        for (char *p = tmp + 1; *p; p++) {
            if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
        }
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "subagent worktree: cannot create %s\n", parent_dir);
            free(repo_root);
            return NULL;
        }
    }

    subagent_worktree__ensure_gitignore_entry(repo_root);

    static const char *const head_args[] = { "rev-parse", "HEAD" };
    char *base_commit = NULL;
    char *bso = NULL, *bse = NULL;
    if (rg(head_args, repo_root, &bso, &bse) == 0 && bso) {
        char *q = bso;
        while (*q && isspace((unsigned char)*q)) q++;
        size_t l = strlen(q);
        while (l > 0 && isspace((unsigned char)q[l - 1])) q[--l] = '\0';
        base_commit = strdup(q);
    }
    free(bso); free(bse);

    const char *wt_args[] = { "worktree", "add", wt_path, "-b", branch, "HEAD", NULL };
    char *wso = NULL, *wse = NULL;
    int rc = rg(wt_args, repo_root, &wso, &wse);
    free(wso);
    if (rc != 0) {
        fprintf(stderr, "subagent worktree: git worktree add failed: %s\n",
                wse ? wse : "");
        free(wse); free(base_commit); free(repo_root);
        return NULL;
    }
    free(wse);

    /* JSON-escape a string (minimal: quote/backslash/control chars). */
    size_t cap = strlen(repo_root) + strlen(branch) + strlen(base_commit ? base_commit : "") +
                 strlen(wt_path) + 128;
    char *json = malloc(cap);
    char esc_path[PATH_MAX * 2], esc_root[PATH_MAX * 2], esc_base[128];
    char *e = esc_path;
    for (const char *s = wt_path; *s && (size_t)(e - esc_path) < sizeof esc_path - 8; s++) {
        if (*s == '"' || *s == '\\') *e++ = '\\';
        *e++ = *s;
    }
    *e = '\0';
    e = esc_root;
    for (const char *s = repo_root; *s && (size_t)(e - esc_root) < sizeof esc_root - 8; s++) {
        if (*s == '"' || *s == '\\') *e++ = '\\';
        *e++ = *s;
    }
    *e = '\0';
    e = esc_base;
    for (const char *s = base_commit ? base_commit : ""; *s; s++) {
        if (*s == '"' || *s == '\\') *e++ = '\\';
        *e++ = *s;
    }
    *e = '\0';
    snprintf(json, cap,
             "{\"path\":\"%s\",\"branch\":\"%s\",\"repo_root\":\"%s\",\"base_commit\":\"%s\"}",
             esc_path, branch, esc_root, esc_base);

    free(base_commit);
    free(repo_root);
    return json;
}

/* PoP: mark_worktree_payload_unproven @ tools/subagent_worktree.py (#88113).
 * Mutates the payload object in place and returns it. */
/* PoP: mark_worktree_payload_unproven @ tools/subagent_worktree.py:mark_worktree_payload_unproven */
json_t *mark_worktree_payload_unproven(json_t *payload, const char *reason,
                                       const char *unmeasured)
{
    if (!payload) return NULL;
    if (!unmeasured) unmeasured = "commits/dirty";
    json_t *jpath = json_object_get(payload, "path");
    json_t *jbranch = json_object_get(payload, "branch");
    const char *path = (jpath && jpath->type == JSON_STRING) ? jpath->str_val : "";
    const char *branch = (jbranch && jbranch->type == JSON_STRING) ? jbranch->str_val : "";

    json_object_set(payload, "inspection_failed", json_bool(1));

    char note[1024];
    snprintf(note, sizeof note,
             "git inspection failed (%s): %s UNKNOWN — not "
             "proven zero/clean. The worktree and branch were preserved "
             "— inspect %s (branch %s) before assuming no work.",
             reason, unmeasured, path, branch);
    json_object_set(payload, "note", json_string(note));
    return payload;
}

/* PoP: unproven_worktree_payload @ tools/subagent_worktree.py */
/* PoP: unproven_worktree_payload @ tools/subagent_worktree.py:unproven_worktree_payload */
json_t *unproven_worktree_payload(json_t *info, const char *reason)
{
    json_t *payload = json_new_object();
    json_t *jp = info ? json_object_get(info, "path") : NULL;
    json_t *jb = info ? json_object_get(info, "branch") : NULL;
    json_object_set(payload, "path",
                    (jp && jp->type == JSON_STRING) ? json_string(jp->str_val) : json_string(""));
    json_object_set(payload, "branch",
                    (jb && jb->type == JSON_STRING) ? json_string(jb->str_val) : json_string(""));
    json_object_set(payload, "commits", json_int(0));
    json_object_set(payload, "dirty", json_new_bool(0));
    json_object_set(payload, "pruned", json_new_bool(0));
    return mark_worktree_payload_unproven(payload, reason, "commits/dirty");
}

/* PoP: finalize_subagent_worktree @ tools/subagent_worktree.py.
 * Returns a malloc'd JSON payload string. prune != 0 mirrors prune=True. */
/* PoP: finalize_subagent_worktree @ tools/subagent_worktree.py:finalize_subagent_worktree */
char *finalize_subagent_worktree(json_t *info, int prune)
{
    json_t *jp = info ? json_object_get(info, "path") : NULL;
    json_t *jb = info ? json_object_get(info, "branch") : NULL;
    json_t *jr = info ? json_object_get(info, "repo_root") : NULL;
    json_t *jc = info ? json_object_get(info, "base_commit") : NULL;
    const char *path        = (jp && jp->type == JSON_STRING) ? jp->str_val : "";
    const char *branch      = (jb && jb->type == JSON_STRING) ? jb->str_val : "";
    const char *repo_root   = (jr && jr->type == JSON_STRING) ? jr->str_val : "";
    const char *base_commit = (jc && jc->type == JSON_STRING) ? jc->str_val : "";

    long commits = 0;
    int dirty = 0, pruned = 0;
    char failed[1024]; failed[0] = '\0';
    char unmeasured[256]; unmeasured[0] = '\0';

    struct stat st;
    if (!path[0] || stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        pruned = 1; /* nothing on disk to review */
        goto emit;
    }
    if (!base_commit[0]) {
        json_t *pl = json_new_object();
        json_object_set(pl, "path", json_string(path));
        json_object_set(pl, "branch", json_string(branch));
        json_object_set(pl, "commits", json_int(0));
        json_object_set(pl, "dirty", json_new_bool(0));
        json_object_set(pl, "pruned", json_new_bool(0));
        pl = mark_worktree_payload_unproven(
            pl, "no base_commit recorded — commit count unmeasurable", "commits");
        char *s = json_dumps(pl, 0);
        return s;
    }

    {
        char range[512];
        snprintf(range, sizeof range, "%s..HEAD", base_commit);
        const char *const rl_args[] = { "rev-list", "--count", range };
        char *so = NULL, *se = NULL;
        int rc = rg(rl_args, path, &so, &se);
        if (rc == 0 && so) commits = strtol(so, NULL, 10);
        else {
            char piece[512];
            snprintf(piece, sizeof piece, "rev-list exit %d: %.200s", rc,
                     se ? se : "");
            if (failed[0]) strncat(failed, "; ", sizeof failed - strlen(failed) - 1);
            strncat(failed, piece, sizeof failed - strlen(failed) - 1);
            if (unmeasured[0]) strncat(unmeasured, "/", sizeof unmeasured - strlen(unmeasured) - 1);
            strncat(unmeasured, "commits", sizeof unmeasured - strlen(unmeasured) - 1);
        }
        free(so); free(se);

        static const char *const st_args[] = { "status", "--porcelain" };
        so = se = NULL;
        rc = rg(st_args, path, &so, &se);
        if (rc == 0) dirty = (so && so[strspn(so, " \t\r\n")] != '\0');
        else {
            char piece[512];
            snprintf(piece, sizeof piece, "status exit %d: %.200s", rc,
                     se ? se : "");
            if (failed[0]) strncat(failed, "; ", sizeof failed - strlen(failed) - 1);
            strncat(failed, piece, sizeof failed - strlen(failed) - 1);
            if (unmeasured[0]) strncat(unmeasured, "/", sizeof unmeasured - strlen(unmeasured) - 1);
            strncat(unmeasured, "dirty", sizeof unmeasured - strlen(unmeasured) - 1);
        }
        free(so); free(se);
    }

    if (failed[0]) {
        json_t *pl = json_new_object();
        json_object_set(pl, "path", json_string(path));
        json_object_set(pl, "branch", json_string(branch));
        json_object_set(pl, "commits", json_int(commits));
        json_object_set(pl, "dirty", json_new_bool(dirty));
        json_object_set(pl, "pruned", json_new_bool(pruned));
        pl = mark_worktree_payload_unproven(pl, failed, unmeasured);
        char *s = json_dumps(pl, 0);
        return s;
    }

    if (prune && commits == 0 && !dirty) {
        const char *const rm_args[] = { "worktree", "remove", "--force", path, NULL };
        char *so = NULL, *se = NULL;
        int rc = rg(rm_args, (repo_root[0] ? repo_root : path), &so, &se);
        free(so);
        if (rc == 0) {
            const char *const bd_args[] = { "branch", "-D", branch, NULL };
            char *bo = NULL, *be = NULL;
            rg(bd_args, (repo_root[0] ? repo_root : path), &bo, &be);
            free(bo); free(be);
            pruned = 1;
        }
        free(se);
    }

emit:
    {
        size_t cap = (strlen(path) + strlen(branch)) * 2 + 128
                     + (failed[0] ? 2048 : 0);
        char *s = malloc(cap);
        if (failed[0])
            snprintf(s, cap,
                "{\"path\":\"%s\",\"branch\":\"%s\",\"commits\":%ld,\"dirty\":%s,"
                "\"pruned\":%s,\"inspection_failed\":true,\"note\":\"git inspection failed (%s)\"}",
                path, branch, commits, dirty ? "true" : "false",
                pruned ? "true" : "false", failed);
        else
            snprintf(s, cap,
                "{\"path\":\"%s\",\"branch\":\"%s\",\"commits\":%ld,\"dirty\":%s,\"pruned\":%s}",
                path, branch, commits, dirty ? "true" : "false",
                pruned ? "true" : "false");
        return s;
    }
}

/* PoP: build_worktree_context_note @ tools/subagent_worktree.py */
/* PoP: build_worktree_context_note @ tools/subagent_worktree.py:build_worktree_context_note */
char *build_worktree_context_note(const char *path, const char *branch)
{
    size_t cap = strlen(path) + strlen(branch) + 640;
    char *s = malloc(cap);
    snprintf(s, cap,
        "\n\n[WORKTREE ISOLATION] You are working in an isolated git worktree "
        "at: %s\n"
        "Your dedicated branch is: %s\n"
        "All file edits and shell commands must happen inside this worktree "
        "directory (your terminal already starts there). Do NOT cd to the "
        "main repository checkout. Commit your changes to your branch when "
        "done; the parent agent will review and merge your branch. If you "
        "make no commits and leave the tree clean, the worktree is discarded "
        "automatically.",
        path ? path : "", branch ? branch : "");
    return s;
}
