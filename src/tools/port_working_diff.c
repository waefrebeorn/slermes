/*
 * port_working_diff.c — C port of tools/working_diff.py
 *
 * Working-tree git diff collection shared by the CLI and gateway /diff.
 * Faithful port of the 4-function Python module:
 *   _run, _untracked_files, _untracked_diff, collect_working_diff
 *
 * Modes:
 *   working (default): unstaged changes + untracked files
 *   staged:            changes staged for commit (git diff --cached)
 *   all:               everything since HEAD + untracked files
 *
 * Untracked files are folded in via `git diff --no-index /dev/null <file>`
 * so brand-new files show up as additions (mirrors Codex CLI /diff).
 */
#include "port_working_diff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#define GIT_TIMEOUT_S       15
#define GIT_TIMEOUT_S_STAT  5
#define MAX_UNTRACKED_FILES 50
#define OUT_BUF_LEN         4096

/* ── _run: run git, returning (returncode, stdout). Never raises. ────── */
/* PoP: wd_run @ tools/working_diff.py:_run */
static int wd_run(const char *cwd, const char **args, int arg_count,
                  char *out, size_t out_len) {
    /* Build: git -c core.quotePath=false <args...> */
    size_t need = 64;
    for (int i = 0; i < arg_count; i++) need += strlen(args[i]) + 4;
    char *cmd = malloc(need);
    if (!cmd) return -1;
    size_t off = 0;
    int n = snprintf(cmd, need, "git -c core.quotePath=false");
    off = (size_t)(n > 0 ? n : 0);
    for (int i = 0; i < arg_count; i++) {
        /* Quote each arg with shell single-quotes (paths with spaces). */
        size_t arg_len = strlen(args[i]);
        cmd[off++] = ' ';
        cmd[off++] = '\'';
        for (size_t j = 0; j < arg_len; j++) {
            if (args[i][j] == '\'') {
                /* '\'' inside single quotes: close, escape, reopen */
                memcpy(cmd + off, "'\\''", 4);
                off += 4;
            } else {
                cmd[off++] = args[i][j];
            }
        }
        cmd[off++] = '\'';
    }
    cmd[off] = '\0';

    char cd_cmd[need + 64];
    /* Wrap the cd+git in a subshell with stderr merged so cd failures (e.g.
     * nonexistent cwd) are captured into the output stream instead of
     * escaping to the harness's own stderr. This mirrors Python's
     * subprocess.run(..., cwd=...) which raises a clean OSError. */
    snprintf(cd_cmd, sizeof(cd_cmd), "(cd '%s' && %s) 2>&1", cwd, cmd);
    free(cmd);

    FILE *fp = popen(cd_cmd, "r");
    if (!fp) return -1;
    if (out && out_len > 0) {
        size_t got = fread(out, 1, out_len - 1, fp);
        out[got] = '\0';
    }
    int rc = pclose(fp);
    return rc;
}

/* ── _untracked_files: git ls-files --others --exclude-standard ──────── */
/* PoP: wd_untracked_files @ tools/working_diff.py:_untracked_files */
static char **wd_untracked_files(const char *cwd, int *count_out) {
    char out[OUT_BUF_LEN];
    int rc = wd_run(cwd, (const char *[]){ "ls-files", "--others", "--exclude-standard" }, 3,
                    out, sizeof(out));
    *count_out = 0;
    if (rc != 0) return NULL;

    char **files = NULL;
    int count = 0, cap = 0;
    char *save = NULL;
    for (char *line = strtok_r(out, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        /* strip trailing \r (Windows line endings in git output) */
        size_t l = strlen(line);
        while (l && (line[l-1] == '\r')) line[--l] = '\0';
        if (!line[0]) continue;
        if (count == cap) {
            cap = cap ? cap * 2 : 16;
            char **nf = realloc(files, (size_t)cap * sizeof(char *));
            if (!nf) break;
            files = nf;
        }
        files[count++] = strdup(line);
    }
    *count_out = count;
    return files;
}

static void wd_free_files(char **files, int count) {
    if (!files) return;
    for (int i = 0; i < count; i++) free(files[i]);
    free(files);
}

/* ── _untracked_diff: render untracked files as new-file diffs ───────── */
/* PoP: wd_untracked_diff @ tools/working_diff.py:_untracked_diff */
static void wd_untracked_diff(const char *cwd, char **files, int count,
                              char *out, size_t out_len) {
    out[0] = '\0';
    size_t off = 0;
    int shown = count < MAX_UNTRACKED_FILES ? count : MAX_UNTRACKED_FILES;
    for (int i = 0; i < shown; i++) {
        /* git diff --no-index -- /dev/null <file> — exits 1 on difference,
         * which is the success path here; keep the output. */
        char chunk[OUT_BUF_LEN];
        int rc = wd_run(cwd, (const char *[]){ "diff", "--no-index", "--", "/dev/null", files[i] }, 5,
                        chunk, sizeof(chunk));
        (void)rc;
        /* strip trailing whitespace/newlines */
        size_t l = strlen(chunk);
        while (l && (chunk[l-1] == '\n' || chunk[l-1] == ' ' || chunk[l-1] == '\r'))
            chunk[--l] = '\0';
        if (l && off < out_len - 1) {
            size_t room = out_len - off - 1;
            size_t take = l < room ? l : room;
            memcpy(out + off, chunk, take);
            off += take;
            if (off < out_len - 1) out[off++] = '\n';
        }
    }
    if (count > MAX_UNTRACKED_FILES && off < out_len - 1) {
        off += (size_t)snprintf(out + off, out_len - off,
                                "... (%d more untracked files not shown)",
                                count - MAX_UNTRACKED_FILES);
    }
    out[off] = '\0';
}

/* ── collect_working_diff: main entry ────────────────────────────────── */
/* PoP: wd_collect_working_diff @ tools/working_diff.py:collect_working_diff */
wd_diff_result_t wd_collect_working_diff(const char *cwd, const char *mode,
                                         char *const *paths, int path_count) {
    wd_diff_result_t res = { .success = false, .stat = NULL, .diff = NULL,
                             .untracked = NULL, .untracked_count = 0,
                             .empty = false, .error = NULL };

    if (!cwd || !*cwd) cwd = ".";
    const char *eff_mode = mode ? mode : "working";

    if (strcmp(eff_mode, "working") != 0 && strcmp(eff_mode, "staged") != 0 &&
        strcmp(eff_mode, "all") != 0) {
        char err[128];
        snprintf(err, sizeof(err), "Unknown mode '%s'. Use: working, staged, all",
                 eff_mode);
        res.error = strdup(err);
        return res;
    }

    /* git available? */
    if (access("/usr/bin/git", X_OK) != 0 && access("/bin/git", X_OK) != 0) {
        res.error = strdup("git is not installed or not on PATH.");
        return res;
    }

    /* inside a work tree? */
    char probe[64];
    int code = wd_run(cwd, (const char *[]){ "rev-parse", "--is-inside-work-tree" }, 2,
                      probe, sizeof(probe));
    if (code == -1 || strstr(probe, "cd:") != NULL) {
        /* spawn failure (cd into nonexistent cwd, git missing) — Python
         * surfaces this as "git failed: <OSError>" via subprocess. */
        char err[192];
        snprintf(err, sizeof(err), "git failed: cannot run git in '%s'", cwd);
        res.error = strdup(err);
        return res;
    }
    if (code != 0) {
        res.error = strdup("Not a git repository.");
        return res;
    }

    /* base args per mode */
    const char *base_args[3];
    int base_count;
    if (strcmp(eff_mode, "staged") == 0) {
        base_args[0] = "diff"; base_args[1] = "--cached"; base_count = 2;
    } else if (strcmp(eff_mode, "all") == 0) {
        base_args[0] = "diff"; base_args[1] = "HEAD"; base_count = 2;
    } else {
        base_args[0] = "diff"; base_count = 1;
    }

    /* pathspec: ["--", *paths] if paths else [] */
    char **stat_args = malloc((size_t)(base_count + path_count + 3) * sizeof(char *));
    char **diff_args = malloc((size_t)(base_count + path_count + 3) * sizeof(char *));
    if (!stat_args || !diff_args) {
        free(stat_args); free(diff_args);
        res.error = strdup("out of memory");
        return res;
    }
    int sa = 0, da = 0;
    for (int i = 0; i < base_count; i++) {
        stat_args[sa++] = (char *)base_args[i];
        diff_args[da++] = (char *)base_args[i];
    }
    stat_args[sa++] = "--stat";
    if (path_count > 0) {
        stat_args[sa++] = "--";
        diff_args[da++] = "--";
        for (int i = 0; i < path_count; i++) {
            stat_args[sa++] = paths[i];
            diff_args[da++] = paths[i];
        }
    }
    stat_args[sa] = NULL;
    diff_args[da] = NULL;

    char stat_out[OUT_BUF_LEN], diff_out[OUT_BUF_LEN * 4];
    stat_out[0] = '\0'; diff_out[0] = '\0';
    wd_run(cwd, (const char **)stat_args, sa, stat_out, sizeof(stat_out));
    wd_run(cwd, (const char **)diff_args, da, diff_out, sizeof(diff_out));
    free(stat_args);
    free(diff_args);

    /* trim trailing whitespace */
    size_t sl = strlen(stat_out);
    while (sl && (stat_out[sl-1] == '\n' || stat_out[sl-1] == ' ')) stat_out[--sl] = '\0';
    /* Python does .strip() — leading whitespace too (git stat lines carry a
     * leading space for the alignment column). */
    size_t lead = 0;
    while (stat_out[lead] == ' ' || stat_out[lead] == '\t') lead++;
    if (lead) memmove(stat_out, stat_out + lead, sl - lead + 1);
    size_t dl = strlen(diff_out);
    while (dl && (diff_out[dl-1] == '\n' || diff_out[dl-1] == ' ')) diff_out[--dl] = '\0';
    size_t dlead = 0;
    while (diff_out[dlead] == ' ' || diff_out[dlead] == '\t') dlead++;
    if (dlead) memmove(diff_out, diff_out + dlead, dl - dlead + 1);

    char *untracked_diff = NULL;
    char **untracked = NULL;
    int untracked_count = 0;
    if ((strcmp(eff_mode, "working") == 0 || strcmp(eff_mode, "all") == 0) &&
        path_count == 0) {
        untracked = wd_untracked_files(cwd, &untracked_count);
        if (untracked_count > 0) {
            char ud[OUT_BUF_LEN * 2];
            wd_untracked_diff(cwd, untracked, untracked_count, ud, sizeof(ud));
            if (ud[0]) {
                size_t ul = strlen(ud);
                while (ul && (ud[ul-1] == '\n')) ud[--ul] = '\0';
                size_t total = dl + (dl ? 1 : 0) + ul + 1;
                untracked_diff = malloc(total);
                if (untracked_diff) {
                    size_t o = 0;
                    if (dl) { memcpy(untracked_diff, diff_out, dl); o = dl; untracked_diff[o++] = '\n'; }
                    memcpy(untracked_diff + o, ud, ul);
                    o += ul;
                    untracked_diff[o] = '\0';
                }
            }
        }
    }

    const char *final_diff = untracked_diff ? untracked_diff : diff_out;
    res.success = true;
    res.stat = strdup(stat_out);
    res.diff = strdup(final_diff);
    if (untracked_count > 0) {
        res.untracked = malloc((size_t)untracked_count * sizeof(char *));
        for (int i = 0; i < untracked_count; i++) res.untracked[i] = strdup(untracked[i]);
        res.untracked_count = untracked_count;
    }
    if (!stat_out[0] && !final_diff[0] && untracked_count == 0)
        res.empty = true;

    free(untracked_diff);
    wd_free_files(untracked, untracked_count);
    return res;
}

/* ── result lifecycle ────────────────────────────────────────────────── */
void wd_diff_result_free(wd_diff_result_t *res) {
    if (!res) return;
    free(res->stat);
    free(res->diff);
    if (res->untracked) {
        for (int i = 0; i < res->untracked_count; i++) free(res->untracked[i]);
        free(res->untracked);
    }
    free(res->error);
    memset(res, 0, sizeof(*res));
}
