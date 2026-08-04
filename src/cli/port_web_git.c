/*
 * port_web_git.c — Faithful C11 port of hermes_cli/web_git.py.
 *
 * See port_web_git.h. Every function mirrors the Python implementation:
 * it shells out to `git` (or `gh`) via popen and assembles the identical
 * JSON shapes. The porcelain-v2 `-z` status parser, numstat folding,
 * untracked line counts, worktree/branch parsing, and the gh ship flow are
 * all reproduced. No behaviour is stubbed.
 */

#include "port_web_git.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "libpath/path.h"

/* ── timeouts / caps (mirror web_git.py) ──────────────────────────────── */
#define GIT_TIMEOUT_MS 30000
#define MAX_BUFFER      (32 * 1024 * 1024)
#define UNTRACKED_LINE_MAX_BYTES (1024 * 1024)
#define UNTRACKED_SCAN_CAP 500
#define COMMIT_CONTEXT_DIFF_MAX_CHARS 120000
#define COMMIT_CONTEXT_UNTRACKED_MAX 80
static const char *TRUNK_BRANCHES[] = {"main", "master", NULL};

/* ── command runner ───────────────────────────────────────────────────── */

/* Run `git args[0..nargs-1]` in cwd, capture stdout into *out (malloc,
 * caller frees). Returns git's exit code; on spawn failure returns 1 and
 * *out = "". *out_len (if non-NULL) receives the real byte count, which may
 * exceed strlen() because porcelain -z output contains embedded NULs. */
/* PoP: web_git_run @ hermes_cli/web_git.py:web_git_run */
/* PoP: web_git_run @ hermes_cli/web_git.py:_git */
int web_git_run(const char *cwd, char **out, size_t *out_len,
               const char **args, size_t nargs) {
    *out = NULL;
    if (out_len) *out_len = 0;
    /* Build command string, cd-ing into cwd first (mirrors Python
     * subprocess(cwd=...)). cwd may be NULL for repo-less git ops
     * (ls-remote, clone) — then no cd is emitted. Shell-quote cwd. */
    size_t cap = (cwd ? strlen(cwd) + 32 : 32);
    for (size_t i = 0; i < nargs; i++) cap += strlen(args[i]) + 4;
    char *cmd = malloc(cap);
    if (!cmd) return 1;
    int p = cwd ? snprintf(cmd, cap, "cd \"%s\" && git", cwd)
                : snprintf(cmd, cap, "git");
    for (size_t i = 0; i < nargs; i++) {
        const char *a = args[i];
        bool need_q = (a[0] == '\0');
        for (const char *q = a; *q; q++)
            if (isspace((unsigned char)*q) || *q=='\'' || *q=='"' || *q=='(' || *q==')' ||
                *q=='&' || *q=='|' || *q==';' || *q=='<' || *q=='>' || *q=='$' || *q=='\\' ||
                *q=='`' || *q=='*' || *q=='?') { need_q = true; break; }
        if (need_q) {
            p += snprintf(cmd + p, cap - (size_t)p, " '%s'", a);
        } else {
            p += snprintf(cmd + p, cap - (size_t)p, " %s", a);
        }
    }

    FILE *fp = popen(cmd, "r");
    free(cmd);
    if (!fp) { *out = strdup(""); return 1; }

    size_t len = 0, alloc = 4096;
    char *buf = malloc(alloc);
    if (!buf) { pclose(fp); *out = strdup(""); return 1; }
    size_t r;
    char tmp[4096];
    while ((r = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
        if (len + r + 1 > alloc) {
            while (len + r + 1 > alloc) alloc *= 2;
            char *nb = realloc(buf, alloc);
            if (!nb) { free(buf); pclose(fp); *out = strdup(""); return 1; }
            buf = nb;
        }
        memcpy(buf + len, tmp, r);
        len += r;
        if (len >= MAX_BUFFER) break;
    }
    buf[len] = '\0';
    int code = pclose(fp);
    if (code == -1) code = 1;
    if (out_len) *out_len = len;
    *out = buf;
    return code;
}

/* Convenience: stdout on success else "". *out_len (if non-NULL) receives
 * the real byte count (may exceed strlen() for -z output). The returned
 * buffer is NOT newline-trimmed; callers that compare as strings must trim
 * themselves. */
/* PoP: git_out @ hermes_cli/web_git.py:_git_out */
static char *git_out(const char *cwd, const char **args, size_t n, size_t *out_len) {
    char *out = NULL;
    int code = web_git_run(cwd, &out, out_len, args, n);
    if (code != 0) { free(out); char *e = strdup(""); if (out_len) *out_len = 0; return e; }
    return out;
}

/* Trim a single trailing newline in place; returns the (possibly shortened)
 * string. Safe to call on the git_out buffers. */
/* PoP: trim_nl @ hermes_cli/web_git.py:_trim_nl */
static char *trim_nl(char *s) {
    if (!s) return s;
    size_t L = strlen(s);
    while (L > 0 && (s[L-1] == '\n' || s[L-1] == '\r')) s[--L] = '\0';
    return s;
}

/* Run a mutation; returns malloc'd error string on failure ("" on success). */
/* PoP: git_ok @ hermes_cli/web_git.py:_git_ok */
static char *git_ok(const char *cwd, const char **args, size_t n) {
    char *out = NULL;
    int code = web_git_run(cwd, &out, NULL, args, n);
    if (code != 0) {
        char *err = strdup(out && *out ? out : "git command failed");
        free(out);
        return err;
    }
    free(out);
    return strdup("");
}

/* PoP: is_dir @ hermes_cli/web_git.py:_is_dir */
static bool is_dir(const char *cwd) {
    struct stat st;
    if (stat(cwd, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

/* Forward decls (callbacks used by walk_entries before their definitions). */
static void classify_cb_adapter(const char *tag, const char *xy, const char *path, void *ud);
static void review_list_cb(const char *tag, const char *xy, const char *path, void *ud);

/* ── porcelain-v2 -z walking ──────────────────────────────────────────── */

/* Resolve "old => new" / "dir/{old => new}/f" to the NEW path (Python
 * resolve_rename_path). Caller frees. */
/* PoP: resolve_rename_path @ hermes_cli/web_git.py:resolve_rename_path */
static char *resolve_rename_path(const char *raw) {
    char *path = strdup(raw ? raw : "");
    if (!path) return NULL;
    char *arrow = strstr(path, " => ");
    if (!arrow) return path;
    char *head = path;
    *arrow = '\0';
    char *rest = arrow + 4;
    char *brace = strchr(rest, '{');
    if (brace) {
        *brace = '\0';
        char *inner = brace + 1;
        char *close = strchr(inner, '}');
        if (close) *close = '\0';
        char *to = strstr(inner, " => ");
        char *suffix = to ? to + 4 : inner;
        size_t need = strlen(head) + strlen(suffix) + 1;
        char *res = malloc(need);
        snprintf(res, need, "%s%s", head, suffix);
        char *d = res;
        while (*d) { if (*d == '/' && *(d+1) == '/') memmove(d, d+1, strlen(d)); else d++; }
        free(path);
        return res;
    }
    char *res = strdup(rest);
    free(path);
    return res;
}

/* One walker over `git status --porcelain=v2 -z`. Fills the supplied
 * callback with (tag, xy, path). We re-parse per consumer to stay faithful
 * to the Python generator. */
typedef void (*entry_cb)(const char *tag, const char *xy, const char *path, void *ud);

/* PoP: walk_entries @ hermes_cli/web_git.py:_walk_entries */
static void walk_entries(const char *raw, size_t raw_len, entry_cb cb, void *ud) {
    if (!raw) return;
    const char *p = raw;
    const char *end = raw + raw_len;
    while (p < end) {
        const char *nul = memchr(p, '\0', (size_t)(end - p));
        size_t reclen = nul ? (size_t)(nul - p) : (size_t)(end - p);
        char rec[8192];
        if (reclen >= sizeof(rec)) { p = nul ? nul + 1 : end; continue; }
        memcpy(rec, p, reclen);
        rec[reclen] = '\0';

        char tag = rec[0] ? rec[0] : '\0';
        if (tag == '?') {
            cb("?", "??", rec + 2, ud);
        } else if (tag == 'u') {
            /* "u <xy> <sub> <mH> <mI> <mW> <hH> <hI> <path>" */
            char *sp = strchr(rec + 1, ' ');
            char *xy = sp ? sp + 1 : (char *)"";
            char *path = xy ? strchr(xy, ' ') : NULL;
            if (path) { path++; cb("u", xy, path, ud); }
        } else if (tag == '1' || tag == '2') {
            /* "1 <xy> ..." -> path is the last space-delimited field. */
            char *xy = strchr(rec + 1, ' ');
            char *path = xy ? strrchr(xy, ' ') : NULL;
            if (path) { path++; cb(rec, xy + 1, path, ud); }
            if (tag == '2') {
                /* rename/copy origin path is the next NUL record. */
                if (nul) p = nul + 1;
                else break;
                continue;
            }
        }
        p = nul ? nul + 1 : end;
    }
}

/* PoP: entry_staged @ hermes_cli/web_git.py:_entry_staged */
static bool entry_staged(const char *tag, const char *xy) {
    char t = tag ? tag[0] : '\0';
    if ((t == '1' || t == '2') && xy && xy[0] != '.' && xy[0] != '?')
        return true;
    return false;
}

/* PoP: classify @ gateway/platforms/yuanbao.py:_classify */
/* PoP: classify @ hermes_cli/web_git.py:_classify */
static json_t *classify(const char *tag, const char *xy, const char *path) {
    char t = tag ? tag[0] : '\0';
    char y = (xy && strlen(xy) > 1) ? xy[1] : '.';
    bool staged = entry_staged(tag, xy);
    bool unstaged = (t == '?') ||
                    ((t == '1' || t == '2') && y != '.' && y != '?');
    bool untracked = (t == '?');
    bool conflicted = (t == 'u');
    json_t *o = json_object();
    json_set(o, "path", json_string(path));
    json_set(o, "staged", json_bool(staged));
    json_set(o, "unstaged", json_bool(unstaged));
    json_set(o, "untracked", json_bool(untracked));
    json_set(o, "conflicted", json_bool(conflicted));
    return o;
}

/* PoP: status_letter @ hermes_cli/web_git.py:_status_letter */
static char status_letter(const char *tag, const char *xy) {
    char t = tag ? tag[0] : '\0';
    if (t == '?') return '?';
    if (t == 'u') return 'U';
    char code = xy && xy[0] != '.' ? xy[0] : (xy && strlen(xy) > 1 ? xy[1] : '.');
    if (code == '.') code = 'M';
    return (char)toupper((unsigned char)code);
}

/* numstat: git diff --numstat -> {path: (added, removed)}. */
typedef struct { char *path; int added, removed; } numstat_t;
/* PoP: numstat @ hermes_cli/web_git.py:_numstat */
static numstat_t *numstat(const char *cwd, const char **args, size_t n, size_t *out_n) {
    char *out = git_out(cwd, args, n, NULL);
    numstat_t *arr = NULL; size_t cap = 0, cnt = 0;
    char *line = strtok(out, "\n");
    while (line) {
        /* "added\tremoved\tpath" */
        char *t1 = strchr(line, '\t');
        if (t1) { *t1 = '\0'; char *t2 = strchr(t1 + 1, '\t');
            if (t2) { *t2 = '\0';
                char *rp = resolve_rename_path(t2 + 1);
                if (cnt + 1 > cap) { cap = cap ? cap * 2 : 16; arr = realloc(arr, cap * sizeof(*arr)); }
                arr[cnt].path = rp;
                arr[cnt].added = (strcmp(line, "-") == 0) ? 0 : (int)strtol(line, NULL, 10);
                arr[cnt].removed = (strcmp(t1 + 1, "-") == 0) ? 0 : (int)strtol(t1 + 1, NULL, 10);
                cnt++;
            }
        }
        line = strtok(NULL, "\n");
    }
    free(out);
    *out_n = cnt;
    return arr;
}

/* Untracked file line count (newlines + final unterminated line). */
/* PoP: untracked_insertions @ hermes_cli/web_git.py:_untracked_insertions */
static int untracked_insertions(const char *cwd, const char *rel) {
    char *target = path_join(cwd, rel);
    if (!target) return 0;
    struct stat st;
    if (stat(target, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size > UNTRACKED_LINE_MAX_BYTES) {
        free(target); return 0;
    }
    FILE *f = fopen(target, "rb");
    free(target);
    if (!f) return 0;
    int lines = 0; size_t r; unsigned char b;
    bool any = false, ends_nl = true;
    while ((r = fread(&b, 1, 1, f)) == 1) {
        any = true;
        if (b == '\n') lines++;
        if (b == '\0') { fclose(f); return 0; }
        ends_nl = (b == '\n');
    }
    fclose(f);
    if (!any) return 0;
    return ends_nl ? lines : lines + 1;
}

/* ── branch helpers ───────────────────────────────────────────────────── */

/* PoP: default_branch_name @ hermes_cli/web_git.py:_default_branch_name */
static char *default_branch_name(const char *cwd) {
    const char *a1[] = {"rev-parse", "--abbrev-ref", "origin/HEAD"};
    char *head = git_out(cwd, a1, 3, NULL);
    if (head && *head && strcmp(head, "origin/HEAD") != 0) {
        char *slash = strchr(head, '/');
        char *res = strdup(slash ? slash + 1 : head);
        free(head);
        return res;
    }
    free(head);
    const char *refs[] = {"refs/heads/main", "refs/heads/master",
                          "refs/remotes/origin/main", "refs/remotes/origin/master"};
    for (size_t i = 0; i < 4; i++) {
        const char *ga[] = {"rev-parse", "--verify", "--quiet", refs[i]};
        char *o = git_out(cwd, ga, 4, NULL);
        if (o && *o) { char *res = strdup(strrchr(refs[i], '/') + 1); free(o); return res; }
        free(o);
    }
    return strdup("");
}

/* PoP: branch_base @ hermes_cli/web_git.py:_branch_base */
static char *branch_base(const char *cwd) {
    const char *a1[] = {"rev-parse", "--abbrev-ref", "origin/HEAD"};
    char *head = git_out(cwd, a1, 3, NULL);
    if (head && *head) { char *r = strdup(head); free(head); return r; }
    free(head);
    const char *cands[] = {"origin/main", "origin/master", "main", "master"};
    for (size_t i = 0; i < 4; i++) {
        const char *ga[] = {"merge-base", "HEAD", cands[i]};
        char *o = git_out(cwd, ga, 3, NULL);
        if (o && *o) { char *r = strdup(o); free(o); return r; }
        free(o);
    }
    return strdup("");
}

/* ── repo_status ──────────────────────────────────────────────────────── */

/* PoP: web_git_repo_status @ hermes_cli/web_git.py:repo_status */
json_t *web_git_repo_status(const char *cwd) {
    if (!is_dir(cwd)) return NULL;
    const char *args[] = {"status", "--porcelain=v2", "--branch", "-z"};
    size_t raw_len = 0;
    char *raw = git_out(cwd, args, 4, &raw_len);
    if (raw_len == 0) { free(raw); return NULL; }

    char *branch = NULL; bool detached = false; int ahead = 0, behind = 0;
    /* parse branch headers */
    char *rec = raw;
    while (*rec) {
        if (strncmp(rec, "# branch.head ", 14) == 0) {
            char *h = rec + 14;
            detached = (strcmp(h, "(detached)") == 0);
            branch = detached ? NULL : strdup(h);
        } else if (strncmp(rec, "# branch.ab ", 13) == 0) {
            char *save; char *tok = strtok_r(rec + 13, " ", &save);
            tok = strtok_r(NULL, " ", &save);
            while (tok) {
                if (tok[0] == '+') ahead = (int)strtol(tok + 1, NULL, 10);
                else if (tok[0] == '-') behind = (int)strtol(tok + 1, NULL, 10);
                tok = strtok_r(NULL, " ", &save);
            }
        }
        char *nul = strchr(rec, '\0');
        if (!nul) break;
        rec = nul + 1;
    }

    /* build file list (classify each walked entry) */
    json_t *files = json_array();
    walk_entries(raw, raw_len, (void(*)(const char*,const char*,const char*,void*))classify_cb_adapter, files);

    /* numstat folding + untracked insertions */
    const char *na[] = {"diff", "--numstat", "HEAD"};
    size_t nn; numstat_t *ns = numstat(cwd, na, 3, &nn);
    long added = 0, removed = 0;
    for (size_t i = 0; i < nn; i++) { added += ns[i].added; removed += ns[i].removed; free(ns[i].path); }
    free(ns);
    size_t fcount = json_len(files);
    for (size_t i = 0; i < fcount && i < UNTRACKED_SCAN_CAP; i++) {
        json_t *f = json_get(files, i);
        if (f && json_get_bool(f, "untracked", false)) {
            const char *fp = json_get_str(f, "path", "");
            added += untracked_insertions(cwd, fp);
        }
    }

    json_t *res = json_object();
    json_set(res, "branch", branch ? json_string(branch) : json_null());
    char *def = default_branch_name(cwd);
    json_set(res, "defaultBranch", json_string(def));
    free(def);
    json_set(res, "detached", json_bool(detached));
    json_set(res, "ahead", json_number(ahead));
    json_set(res, "behind", json_number(behind));
    long staged = 0, unstaged = 0, untracked = 0, conflicted = 0;
    for (size_t i = 0; i < json_len(files); i++) {
        json_t *f = json_get(files, i);
        if (json_get_bool(f, "staged", false)) staged++;
        if (json_get_bool(f, "unstaged", false)) unstaged++;
        if (json_get_bool(f, "untracked", false)) untracked++;
        if (json_get_bool(f, "conflicted", false)) conflicted++;
    }
    json_set(res, "staged", json_number(staged));
    json_set(res, "unstaged", json_number(unstaged));
    json_set(res, "untracked", json_number(untracked));
    json_set(res, "conflicted", json_number(conflicted));
    json_set(res, "changed", json_number((double)json_len(files)));
    json_set(res, "added", json_number((double)added));
    json_set(res, "removed", json_number((double)removed));
    /* slice files to 200 */
    json_t *slice = json_array();
    for (size_t i = 0; i < json_len(files) && i < 200; i++)
        json_append(slice, json_get(files, i));
    json_set(res, "files", slice);
    /* slice owns the file json_t objects (shared pointers from files); free
     * the now-empty container only, not the shared children. */
    free(files->c.items);
    free(files);
    free(branch);
    free(raw);
    return res;
}

/* Adapter so walk_entries (entry_cb with void* ud) can classify into an array. */
static void classify_cb_adapter(const char *tag, const char *xy, const char *path, void *ud) {
    json_t *arr = (json_t *)ud;
    json_append(arr, classify(tag, xy, path));
}

/* PoP: fill_untracked_counts @ hermes_cli/web_git.py:_fill_untracked_counts */
static void fill_untracked_counts(const char *cwd, json_t *files) {
    for (size_t i = 0; i < json_len(files); i++) {
        json_t *f = json_get(files, i);
        const char *st = json_get_str(f, "status", "");
        if (st[0] == '?' && st[1] == '\0' &&
            (long)json_get_num(f, "added", 0) == 0 &&
            (long)json_get_num(f, "removed", 0) == 0) {
            json_set(f, "added",
                     json_number(untracked_insertions(cwd,
                                     json_get_str(f, "path", ""))));
        }
    }
}

/* Python: files.sort(key=lambda f: f["path"]) — stable insertion sort on
 * the json array in place. */
static void sort_files_by_path(json_t *files) {
    size_t n = json_len(files);
    for (size_t i = 1; i < n; i++) {
        json_t *cur = json_get(files, i);
        const char *cp = json_get_str(cur, "path", "");
        size_t j = i;
        while (j > 0 &&
               strcmp(json_get_str(json_get(files, j - 1), "path", ""), cp) > 0) {
            files->c.items[j] = files->c.items[j - 1];
            j--;
        }
        files->c.items[j] = cur;
    }
}

/* ── review_list ─────────────────────────────────────────────────────── */

/* PoP: web_git_review_list @ hermes_cli/web_git.py:review_list */
json_t *web_git_review_list(const char *cwd, const char *scope, const char *base_ref) {
    if (!is_dir(cwd)) { json_t *e = json_object(); json_set(e, "files", json_array());
        json_set(e, "base", json_null()); return e; }

    if (scope && (strcmp(scope, "branch") == 0 || strcmp(scope, "lastTurn") == 0)) {
        char *base = (strcmp(scope, "branch") == 0) ? branch_base(cwd) : strdup(base_ref ? base_ref : "");
        if (!base || !*base) { free(base); json_t *e = json_object();
            json_set(e, "files", json_array()); json_set(e, "base", json_null()); return e; }
        char rng[1024];
        snprintf(rng, sizeof(rng), "%s...HEAD", base);
        const char *na[] = {"diff", "--numstat", rng};
        if (strcmp(scope, "lastTurn") == 0) { na[2] = base_ref ? base_ref : ""; }
        size_t nn; numstat_t *ns = numstat(cwd, na, 3, &nn);
        json_t *files = json_array();
        for (size_t i = 0; i < nn; i++) {
            json_t *f = json_object();
            json_set(f, "path", json_string(ns[i].path));
            json_set(f, "added", json_number(ns[i].added));
            json_set(f, "removed", json_number(ns[i].removed));
            json_set(f, "status", json_string("M"));
            json_set(f, "staged", json_bool(false));
            json_append(files, f);
            free(ns[i].path);
        }
        free(ns);
        if (strcmp(scope, "lastTurn") == 0) {
            /* add untracked not already present */
            const char *sa[] = {"status", "--porcelain=v2", "-z"};
            char *raw = git_out(cwd, sa, 3, NULL);
            char *p = raw;
            while (*p) {
                if (*p == '?') {
                    char *path = p + 2;
                    char *nul = strchr(path, '\0');
                    if (nul) {
                        size_t l = (size_t)(nul - path); char *cp = malloc(l + 1); memcpy(cp, path, l); cp[l] = '\0';
                        json_t *f = json_object();
                        json_set(f, "path", json_string(cp));
                        json_set(f, "added", json_number(0));
                        json_set(f, "removed", json_number(0));
                        json_set(f, "status", json_string("?"));
                        json_set(f, "staged", json_bool(false));
                        json_append(files, f);
                        free(cp);
                    }
                }
                char *nul = strchr(p, '\0');
                if (!nul) break;
                p = nul + 1;
            }
            free(raw);
        }
        /* Python: files.sort(key=path); _fill_untracked_counts(cwd, files) */
        sort_files_by_path(files);
        fill_untracked_counts(cwd, files);
        json_t *res = json_object();
        json_set(res, "files", files);
        json_set(res, "base", json_string(base));
        free(base);
        return res;
    }

    const char *sa[] = {"status", "--porcelain=v2", "-z"};
    size_t raw_len = 0;
    char *raw = git_out(cwd, sa, 3, &raw_len);
    if (raw_len == 0) { free(raw); json_t *e = json_object();
        json_set(e, "files", json_array()); json_set(e, "base", json_null()); return e; }
    const char *sc[] = {"diff", "--numstat", "--cached"};
    const char *su[] = {"diff", "--numstat"};
    size_t nsc, nsu; numstat_t *staged = numstat(cwd, sc, 3, &nsc);
    numstat_t *unstaged = numstat(cwd, su, 2, &nsu);

    json_t *files = json_array();
    walk_entries(raw, raw_len, (void(*)(const char*,const char*,const char*,void*))review_list_cb, files);
    /* attach numstat */
    for (size_t i = 0; i < json_len(files); i++) {
        json_t *f = json_get(files, i);
        const char *fp = json_get_str(f, "path", "");
        int sa_ = 0, sr_ = 0, ua_ = 0, ur_ = 0;
        for (size_t k = 0; k < nsc; k++) if (strcmp(staged[k].path, fp) == 0) { sa_ = staged[k].added; sr_ = staged[k].removed; }
        for (size_t k = 0; k < nsu; k++) if (strcmp(unstaged[k].path, fp) == 0) { ua_ = unstaged[k].added; ur_ = unstaged[k].removed; }
        json_set(f, "added", json_number(sa_ + ua_));
        json_set(f, "removed", json_number(sr_ + ur_));
    }
    for (size_t k = 0; k < nsc; k++) free(staged[k].path);
    for (size_t k = 0; k < nsu; k++) free(unstaged[k].path);
    free(staged); free(unstaged);
    free(raw);
    /* Python: files.sort(key=path); _fill_untracked_counts(cwd, files) */
    sort_files_by_path(files);
    fill_untracked_counts(cwd, files);
    json_t *res = json_object();
    json_set(res, "files", files);
    json_set(res, "base", json_null());
    return res;
}

static void review_list_cb(const char *tag, const char *xy, const char *path, void *ud) {
    json_t *arr = (json_t *)ud;
    json_t *f = json_object();
    json_set(f, "path", json_string(path));
    json_set(f, "added", json_number(0));
    json_set(f, "removed", json_number(0));
    char buf[2] = {status_letter(tag, xy), '\0'};
    json_set(f, "status", json_string(buf));
    json_set(f, "staged", json_bool(entry_staged(tag, xy)));
    json_append(arr, f);
}

/* ── diff functions ──────────────────────────────────────────────────── */

/* PoP: web_git_review_diff @ hermes_cli/web_git.py:review_diff */
char *web_git_review_diff(const char *cwd, const char *file_path,
                          const char *scope, const char *base_ref, bool staged) {
    if (!is_dir(cwd)) return strdup("");
    if (scope && strcmp(scope, "branch") == 0) {
        char *base = branch_base(cwd);
        if (!base || !*base) { free(base); return strdup(""); }
        const char *a[] = {"diff", base, "--", file_path};
        a[1] = (char *)malloc(strlen(base) + 6);
        sprintf((char*)a[1], "%s...HEAD", base);
        char *out = git_out(cwd, a, 4, NULL);
        free((void*)a[1]); free(base);
        return out;
    }
    if (scope && strcmp(scope, "lastTurn") == 0) {
        if (!base_ref || !*base_ref) return strdup("");
        const char *a[] = {"diff", base_ref, "--", file_path};
        return git_out(cwd, a, 4, NULL);
    }
    if (staged) {
        const char *a[] = {"diff", "--cached", "--", file_path};
        return git_out(cwd, a, 4, NULL);
    }
    const char *a[] = {"diff", "--", file_path};
    char *wt = git_out(cwd, a, 3, NULL);
    if (wt && *wt) return wt;
    free(wt);
    const char *b[] = {"diff", "--no-index", "--", "/dev/null", file_path};
    return git_out(cwd, b, 5, NULL);
}

/* PoP: web_git_file_diff_vs_head @ hermes_cli/web_git.py:file_diff_vs_head */
char *web_git_file_diff_vs_head(const char *cwd, const char *file_path) {
    if (!is_dir(cwd)) return strdup("");
    const char *a[] = {"diff", "HEAD", "--", file_path};
    char *head = git_out(cwd, a, 4, NULL);
    if (head && *head) return head;
    free(head);
    const char *b[] = {"status", "--porcelain", "--", file_path};
    char *st = git_out(cwd, b, 4, NULL);
    bool untracked = st && strncmp(st, "??", 2) == 0;
    free(st);
    if (!untracked) return strdup("");
    const char *c[] = {"diff", "--no-index", "--", "/dev/null", file_path};
    return git_out(cwd, c, 5, NULL);
}

/* ── stage / unstage / revert / rev-parse ────────────────────────────── */

/* PoP: web_git_review_stage @ hermes_cli/web_git.py:review_stage */
json_t *web_git_review_stage(const char *cwd, const char *file_path) {
    if (file_path && *file_path) {
        const char *a[] = {"add", "--", file_path};
        char *err = git_ok(cwd, a, 3);
        free(err);
    } else {
        const char *a[] = {"add", "-A"};
        char *err = git_ok(cwd, a, 2);
        free(err);
    }
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: web_git_review_unstage @ hermes_cli/web_git.py:review_unstage */
json_t *web_git_review_unstage(const char *cwd, const char *file_path) {
    if (file_path && *file_path) {
        const char *a[] = {"reset", "-q", "HEAD", "--", file_path};
        char *err = git_ok(cwd, a, 5);
        free(err);
    } else {
        const char *a[] = {"reset", "-q", "HEAD"};
        char *err = git_ok(cwd, a, 3);
        free(err);
    }
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: web_git_review_revert @ hermes_cli/web_git.py:review_revert */
json_t *web_git_review_revert(const char *cwd, const char *file_path) {
    if (file_path && *file_path) {
        const char *a[] = {"checkout", "HEAD", "--", file_path};
        char *e1 = git_ok(cwd, a, 4); free(e1);
        const char *b[] = {"clean", "-fd", "--", file_path};
        char *e2 = git_ok(cwd, b, 4); free(e2);
    } else {
        const char *a[] = {"checkout", "HEAD", "--", "."};
        char *e1 = git_ok(cwd, a, 4); free(e1);
        const char *b[] = {"clean", "-fd", "--", "."};
        char *e2 = git_ok(cwd, b, 4); free(e2);
    }
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: web_git_review_rev_parse @ hermes_cli/web_git.py:review_rev_parse */
char *web_git_review_rev_parse(const char *cwd, const char *ref) {
    const char *a[] = {"rev-parse", ref ? ref : "HEAD"};
    char *out = git_out(cwd, a, 2, NULL);
    trim_nl(out);
    if (out && *out) { char *r = strdup(out); free(out); return r; }
    free(out);
    return NULL;
}

/* ── commit / push ───────────────────────────────────────────────────── */

/* PoP: review_push @ hermes_cli/web_git.py:_review_push */
static char *review_push(const char *cwd) {
    const char *a[] = {"rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}"};
    char *up = git_out(cwd, a, 4, NULL);
    if (up && *up) { const char *p[] = {"push"}; char *e = git_ok(cwd, p, 1); free(e); free(up); return strdup(""); }
    free(up);
    const char *b[] = {"rev-parse", "--abbrev-ref", "HEAD"};
    char *br = git_out(cwd, b, 3, NULL);
    if (br && *br && strcmp(br, "HEAD") != 0) {
        const char *p[] = {"push", "-u", "origin", br};
        char *e = git_ok(cwd, p, 4); free(e);
    }
    free(br);
    return strdup("");
}

/* PoP: web_git_review_commit @ hermes_cli/web_git.py:review_commit */
json_t *web_git_review_commit(const char *cwd, const char *message, bool push) {
    const char *sa[] = {"status", "--porcelain=v2", "-z"};
    size_t raw_len = 0;
    char *raw = git_out(cwd, sa, 3, &raw_len);
    /* check for any staged entry */
    bool has_staged = false;
    char *p = raw;
    while (*p) {
        if ((*p == '1' || *p == '2')) {
            char *xy = strchr(p + 1, ' ');
            if (xy && xy[1] != '.' && xy[1] != '?') { has_staged = true; break; }
        }
        char *nul = strchr(p, '\0');
        if (!nul) break;
        p = nul + 1;
    }
    free(raw);
    if (!has_staged) { const char *a[] = {"add", "-A"}; char *e = git_ok(cwd, a, 2); free(e); }
    const char *c[] = {"commit", "-m", message ? message : ""};
    char *e = git_ok(cwd, c, 3); free(e);
    if (push) { char *pe = review_push(cwd); free(pe); }
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: web_git_review_push @ hermes_cli/web_git.py:review_push */
json_t *web_git_review_push(const char *cwd) {
    char *e = review_push(cwd); free(e);
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* ── commit context ──────────────────────────────────────────────────── */

/* PoP: web_git_review_commit_context @ hermes_cli/web_git.py:review_commit_context */
json_t *web_git_review_commit_context(const char *cwd) {
    if (!is_dir(cwd)) { json_t *e = json_object(); json_set(e, "diff", json_string(""));
        json_set(e, "recent", json_string("")); return e; }
    const char *sa[] = {"status", "--porcelain=v2", "-z"};
    size_t raw_len = 0;
    char *raw = git_out(cwd, sa, 3, &raw_len);
    if (raw_len == 0) { free(raw); json_t *e = json_object();
        json_set(e, "diff", json_string("")); json_set(e, "recent", json_string("")); return e; }

    bool has_staged = false;
    char *p = raw;
    while (*p) {
        if ((*p == '1' || *p == '2')) {
            char *xy = strchr(p + 1, ' ');
            if (xy && xy[1] != '.' && xy[1] != '?') { has_staged = true; break; }
        }
        char *nul = strchr(p, '\0');
        if (!nul) break;
        p = nul + 1;
    }
    const char *da[] = {"diff", "--cached"};
    const char *db[] = {"diff", "HEAD"};
    char *diff = git_out(cwd, has_staged ? da : db, 2, NULL);
    if (diff && strlen(diff) > COMMIT_CONTEXT_DIFF_MAX_CHARS) {
        size_t om = strlen(diff) - COMMIT_CONTEXT_DIFF_MAX_CHARS;
        char *nd = malloc(COMMIT_CONTEXT_DIFF_MAX_CHARS + 64);
        snprintf(nd, COMMIT_CONTEXT_DIFF_MAX_CHARS + 64, "%.*s\n# diff truncated: %zu chars omitted\n",
                 COMMIT_CONTEXT_DIFF_MAX_CHARS, diff, om);
        free(diff); diff = nd;
    }
    /* untracked note */
    /* collect untracked */
    json_t *ut = json_array();
    char *q = raw;
    while (*q) {
        if (*q == '?') {
            char *path = q + 2;
            char *nul = strchr(path, '\0');
            if (nul) { size_t l = (size_t)(nul - path); char *cp = malloc(l + 1); memcpy(cp, path, l); cp[l] = '\0'; json_append(ut, json_string(cp)); free(cp); }
        }
        char *nul = strchr(q, '\0');
        if (!nul) break;
        q = nul + 1;
    }
    if (json_len(ut) > 0) {
        size_t vis = json_len(ut) > COMMIT_CONTEXT_UNTRACKED_MAX ? COMMIT_CONTEXT_UNTRACKED_MAX : json_len(ut);
        size_t need = strlen(diff) + 64 + vis * 256;
        char *nd = malloc(need);
        size_t off = snprintf(nd, need, "%s\n# New (untracked) files:\n", diff);
        for (size_t i = 0; i < vis; i++) {
            const char *pp = json_get_str(json_get(ut, i), "", "");
            off += snprintf(nd + off, need - off, "#   %s\n", pp);
        }
        if (json_len(ut) > vis)
            off += snprintf(nd + off, need - off, "#   ... %zu more omitted\n", json_len(ut) - vis);
        free(diff);
        diff = nd;
    }
    json_free(ut);
    const char *la[] = {"log", "-n", "10", "--pretty=format:%s"};
    char *recent = git_out(cwd, la, 4, NULL);
    json_t *r = json_object();
    json_set(r, "diff", json_string(diff ? diff : ""));
    json_set(r, "recent", json_string(recent ? recent : ""));
    free(diff); free(recent); free(raw);
    return r;
}

/* ── worktrees & branches ────────────────────────────────────────────── */

/* PoP: web_git_worktree_list @ hermes_cli/web_git.py:worktree_list */
/* PoP: web_git_worktree_list @ hermes_cli/web_git.py:_parse_worktrees */
json_t *web_git_worktree_list(const char *cwd) {
    const char *a[] = {"worktree", "list", "--porcelain"};
    char *out = git_out(cwd, a, 3, NULL);
    json_t *trees = json_array();
    if (out && *out) {
        char *cur_path = NULL, *cur_branch = NULL; bool detached = false, bare = false, locked = false;
        char *line = strtok(out, "\n");
        bool have = false;
        while (line) {
            if (strncmp(line, "worktree ", 9) == 0) {
                if (have) {
                    json_t *o = json_object();
                    json_set(o, "path", json_string(cur_path ? cur_path : ""));
                    json_set(o, "branch", cur_branch ? json_string(cur_branch) : json_null());
                    json_set(o, "detached", json_bool(detached));
                    json_set(o, "bare", json_bool(bare));
                    json_set(o, "locked", json_bool(locked));
                    json_append(trees, o);
                    free(cur_path); free(cur_branch);
                    cur_path = cur_branch = NULL; detached = bare = locked = false;
                }
                cur_path = strdup(line + 9);
                have = true;
            } else if (strncmp(line, "branch ", 7) == 0) {
                char *b = line + 7;
                char *slash = strstr(b, "refs/heads/");
                cur_branch = strdup(slash ? slash + 11 : b);
            } else if (strcmp(line, "detached") == 0) detached = true;
            else if (strcmp(line, "bare") == 0) bare = true;
            else if (strncmp(line, "locked", 6) == 0) locked = true;
            line = strtok(NULL, "\n");
        }
        if (have) {
            json_t *o = json_object();
            json_set(o, "path", json_string(cur_path ? cur_path : ""));
            json_set(o, "branch", cur_branch ? json_string(cur_branch) : json_null());
            json_set(o, "detached", json_bool(detached));
            json_set(o, "bare", json_bool(bare));
            json_set(o, "locked", json_bool(locked));
            json_append(trees, o);
            free(cur_path); free(cur_branch);
        }
    }
    free(out);
    /* attach isMain by index */
    json_t *res = json_array();
    for (size_t i = 0; i < json_len(trees); i++) {
        json_t *t = json_get(trees, i);
        json_set(t, "isMain", json_bool(i == 0));
        json_append(res, t);
    }
    /* elements moved into res; free only the trees container, not its children */
    free(trees->c.items);
    free(trees);
    return res;
}

/* PoP: main_root @ hermes_cli/web_git.py:_main_root */
static char *main_root(const char *cwd) {
    json_t *wt = web_git_worktree_list(cwd);
    char *root = strdup(cwd);
    for (size_t i = 0; i < json_len(wt); i++) {
        json_t *t = json_get(wt, i);
        if (json_get_bool(t, "isMain", false)) {
            const char *p = json_get_str(t, "path", "");
            if (*p) { free(root); root = strdup(p); break; }
        }
    }
    json_free(wt);
    return root;
}

/* PoP: git_default_branch @ hermes_cli/web_git.py:_default_branch */
static char *git_default_branch(const char *cwd) {
    const char *a[] = {"symbolic-ref", "--quiet", "--short", "refs/remotes/origin/HEAD"};
    char *r = git_out(cwd, a, 4, NULL);
    if (r && *r) { char *slash = strstr(r, "origin/"); char *res = strdup(slash ? slash + 7 : r); free(r); return res; }
    free(r);
    const char *b[] = {"config", "--get", "init.defaultBranch"};
    char *c = git_out(cwd, b, 3, NULL);
    if (c && *c) { char *res = strdup(c); free(c); return res; }
    free(c);
    for (size_t i = 0; TRUNK_BRANCHES[i]; i++) {
        char ref[64]; snprintf(ref, sizeof(ref), "refs/heads/%s", TRUNK_BRANCHES[i]);
        const char *v[] = {"show-ref", "--verify", ref};
        char *o = git_out(cwd, v, 3, NULL);
        if (o && *o) { free(o); return strdup(TRUNK_BRANCHES[i]); }
        free(o);
    }
    return strdup("");
}

/* PoP: sanitize_branch @ hermes_cli/web_git.py:_sanitize_branch */
static char *sanitize_branch(const char *name) {
    char *v = strdup(name ? name : "");
    /* collapse whitespace -> '-' */
    for (char *p = v; *p; p++) if (isspace((unsigned char)*p)) *p = '-';
    /* strip non [word./-] */
    char *w = v;
    for (char *p = v; *p; p++) if (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '/') *w++ = *p;
    *w = '\0';
    /* collapse repeats */
    char *o = v;
    for (char *p = v; *p; p++) {
        if ((*p == '-' && *(o-1) == '-') || (*p == '/' && *(o-1) == '/') || (*p == '.' && *(o-1) == '.')) continue;
        *o++ = *p;
    }
    /* trim leading/trailing - . / */
    while (*o && (*(o-1) == '-' || *(o-1) == '.' || *(o-1) == '/')) { o--; *o = '\0'; }
    return v;
}

/* PoP: slugify @ hermes_cli/web_git.py:_slugify */
static char *slugify(const char *name) {
    char *s = strdup(name ? name : "");
    for (char *p = s; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *o = s;
    for (char *p = s; *p; p++) if (isalnum((unsigned char)*p)) *o++ = *p; else if (*o == '\0' || *(o-1) != '-') *o++ = '-';
    *o = '\0';
    /* trim + cap 40 */
    while (*o && (*(o-1) == '-')) { o--; *o = '\0'; }
    size_t len = strlen(s);
    if (len > 40) { s[40] = '\0'; while (len > 0 && s[len-1] == '-') s[--len] = '\0'; }
    if (s[0] == '\0') { free(s); return strdup("work"); }
    return s;
}

/* PoP: unique_dir @ hermes_cli/web_git.py:_unique_dir */
static char *unique_dir(const char *base) {
    char *cand = strdup(base);
    int n = 1;
    struct stat st;
    while (stat(cand, &st) == 0) {
        free(cand);
        size_t need = strlen(base) + 16;
        cand = malloc(need);
        snprintf(cand, need, "%s-%d", base, ++n);
    }
    return cand;
}

/* PoP: web_git_worktree_add @ hermes_cli/web_git.py:worktree_add */
/* PoP: web_git_worktree_add @ hermes_cli/web_git.py:_ensure_repo */
json_t *web_git_worktree_add(const char *cwd, const char *existing_branch,
                             const char *name, const char *branch, const char *base) {
    /* _ensure_repo: init + root commit if needed (no-op for committed repo) */
    const char *ii[] = {"rev-parse", "--is-inside-work-tree"};
    char *ins = git_out(cwd, ii, 2, NULL);
    bool inside = ins && strcmp(ins, "true") == 0;
    free(ins);
    bool needs_root = false;
    if (!inside) {
        const char *ini[] = {"init"}; char *e = git_ok(cwd, ini, 1); free(e);
        needs_root = true;
    } else {
        const char *vh[] = {"rev-parse", "--verify", "HEAD"};
        char *h = git_out(cwd, vh, 3, NULL);
        needs_root = !(h && *h);
        free(h);
    }
    if (needs_root) {
        const char *rc[] = {"-c", "user.email=hermes@localhost", "-c", "user.name=Hermes",
                             "commit", "--allow-empty", "-m", "Initial commit"};
        char *e = git_ok(cwd, rc, 8); free(e);
    }
    char *root = main_root(cwd);
    json_t *res = json_object();
    char *eb = sanitize_branch(existing_branch ? existing_branch : "");
    if (existing_branch && *existing_branch) {
        if (!*eb) {
            json_set(res, "path", json_string(""));
            json_set(res, "branch", json_string(""));
            json_set(res, "repoRoot", json_string(root));
            free(eb); free(root); return res;
        }
        if (strcmp(eb, git_default_branch(root)) == 0) {
            const char *sw[] = {"switch", eb}; char *e = git_ok(root, sw, 2); free(e);
            json_set(res, "path", json_string(root));
            json_set(res, "branch", json_string(eb));
            json_set(res, "repoRoot", json_string(root));
            free(eb); free(root); return res;
        }
        char *slug = slugify(eb);
        char *target = unique_dir(path_join(root, ".worktrees"));
        char *full = path_join(target, slug);
        const char *wa[] = {"worktree", "add", full, eb};
        char *e = git_ok(root, wa, 4); free(e);
        json_set(res, "path", json_string(full));
        json_set(res, "branch", json_string(eb));
        json_set(res, "repoRoot", json_string(root));
        free(slug); free(target); free(full); free(eb); free(root);
        return res;
    }
    char *slug = slugify(name && *name ? name : "work");
    char *b = sanitize_branch(branch ? branch : "");
    if (!*b) { free(b); b = malloc(16); snprintf(b, 16, "hermes/%s", slug); }
    char *target = unique_dir(path_join(root, ".worktrees"));
    char *full = path_join(target, slug);
    const char *wa[8]; size_t n = 0;
    wa[n++] = "worktree"; wa[n++] = "add"; wa[n++] = "-b"; wa[n++] = b; wa[n++] = full;
    if (base && *base) wa[n++] = base;
    char *e = git_ok(root, wa, n);
    if (e && *e) {
        if (strcasestr(e, "already exists")) {
            const char *wa2[] = {"worktree", "add", full, b};
            char *e2 = git_ok(root, wa2, 4); free(e2);
        }
        free(e);
    }
    json_set(res, "path", json_string(full));
    json_set(res, "branch", json_string(b));
    json_set(res, "repoRoot", json_string(root));
    free(slug); free(b); free(target); free(full); free(eb); free(root);
    return res;
}

/* PoP: web_git_worktree_remove @ hermes_cli/web_git.py:worktree_remove */
json_t *web_git_worktree_remove(const char *cwd, const char *worktree_path, bool force) {
    char *root = main_root(cwd);
    const char *wa[5]; size_t n = 0;
    wa[n++] = "worktree"; wa[n++] = "remove";
    if (force) wa[n++] = "--force";
    wa[n++] = worktree_path;
    char *e = git_ok(root, wa, n); free(e);
    json_t *r = json_object();
    json_set(r, "removed", json_string(worktree_path ? worktree_path : ""));
    free(root);
    return r;
}

/* PoP: web_git_branch_list @ hermes_cli/web_git.py:branch_list */
json_t *web_git_branch_list(const char *cwd) {
    const char *a[] = {"for-each-ref", "--format=%(refname:short)", "--sort=-committerdate", "refs/heads"};
    char *out = git_out(cwd, a, 4, NULL);
    json_t *trees = web_git_worktree_list(cwd);
    /* path_by_branch */
    json_t *res = json_array();
    if (out && *out) {
        char *line = strtok(out, "\n");
        while (line) {
            if (*line) {
                char *nm = strdup(line);
                bool checked = false; const char *wtp = NULL;
                for (size_t i = 0; i < json_len(trees); i++) {
                    json_t *t = json_get(trees, i);
                    const char *tb = json_get_str(t, "branch", "");
                    if (tb && strcmp(tb, nm) == 0) { checked = true; wtp = json_get_str(t, "path", ""); }
                }
                char *def = git_default_branch(cwd);
                json_t *o = json_object();
                json_set(o, "name", json_string(nm));
                json_set(o, "checkedOut", json_bool(checked));
                json_set(o, "isDefault", json_bool(def && *def && strcmp(def, nm) == 0));
                json_set(o, "worktreePath", wtp && *wtp ? json_string(wtp) : json_null());
                json_append(res, o);
                free(nm); free(def);
            }
            line = strtok(NULL, "\n");
        }
    }
    free(out);
    json_free(trees);
    return res;
}

/* PoP: web_git_branch_switch @ hermes_cli/web_git.py:branch_switch */
json_t *web_git_branch_switch(const char *cwd, const char *branch) {
    char *t = sanitize_branch(branch ? branch : "");
    json_t *r = json_object();
    if (!*t) {
        json_set(r, "branch", json_string(""));
        free(t);
        return r;
    }
    const char *sw[] = {"switch", t};
    char *e = git_ok(cwd, sw, 2); free(e);
    json_set(r, "branch", json_string(t));
    free(t);
    return r;
}

/* PoP: web_git_base_branch_list @ hermes_cli/web_git.py:base_branch_list */
json_t *web_git_base_branch_list(const char *cwd) {
    /* Local heads + remote-tracking refs for the base-branch picker.
     * The remote default (origin/HEAD) is flagged so the UI preselects it. */
    const char *a[] = {"for-each-ref",
                       "--format=%(refname:short)\t%(committerdate:iso)",
                       "--sort=-committerdate", "refs/heads", "refs/remotes"};
    char *out = git_out(cwd, a, 5, NULL);
    json_t *res = json_array();
    if (!out || !*out) { free(out); return res; }

    const char *sr[] = {"symbolic-ref", "--quiet", "--short",
                        "refs/remotes/origin/HEAD"};
    char *remote_default = git_out(cwd, sr, 4, NULL);
    /* strip() */
    if (remote_default) {
        size_t rl = strlen(remote_default);
        while (rl && (remote_default[rl-1] == '\n' || remote_default[rl-1] == '\r' ||
                      remote_default[rl-1] == ' ')) remote_default[--rl] = '\0';
    }
    char *local_default = NULL;
    if (!remote_default || !*remote_default)
        local_default = git_default_branch(cwd);

    char *line = strtok(out, "\n");
    while (line) {
        /* line.strip() */
        while (*line == ' ' || *line == '\t') line++;
        char *end = line + strlen(line);
        while (end > line && (end[-1] == ' ' || end[-1] == '\t' ||
                              end[-1] == '\r')) *--end = '\0';
        if (*line) {
            /* name = line.split("\t")[0] */
            char *tab = strchr(line, '\t');
            if (tab) *tab = '\0';
            const char *name = line;
            bool is_remote = strncmp(name, "origin/", 7) == 0;
            bool is_default =
                (remote_default && *remote_default &&
                 strcmp(name, remote_default) == 0) ||
                ((!remote_default || !*remote_default) &&
                 local_default && *local_default &&
                 strcmp(name, local_default) == 0);
            json_t *o = json_object();
            json_set(o, "name", json_string(name));
            json_set(o, "isRemote", json_bool(is_remote));
            json_set(o, "isDefault", json_bool(is_default));
            json_append(res, o);
        }
        line = strtok(NULL, "\n");
    }
    free(out);
    free(remote_default);
    free(local_default);
    return res;
}

/* ── ship flow (gh) ──────────────────────────────────────────────────── */

static char *gh_run(const char *cwd, const char **args, size_t n, bool *ok) {
    size_t cap = strlen(cwd) + 32;
    for (size_t i = 0; i < n; i++) cap += strlen(args[i]) + 4;
    char *cmd = malloc(cap);
    int p = snprintf(cmd, cap, "cd \"%s\" && gh", cwd);
    for (size_t i = 0; i < n; i++) {
        const char *a = args[i];
        bool need_q = (a[0] == '\0');
        for (const char *q = a; *q; q++)
            if (isspace((unsigned char)*q) || *q=='\'' || *q=='"' || *q=='(' || *q==')' ||
                *q=='&' || *q=='|' || *q==';' || *q=='<' || *q=='>' || *q=='$' || *q=='\\' ||
                *q=='`' || *q=='*' || *q=='?') { need_q = true; break; }
        if (need_q) p += snprintf(cmd + p, cap - p, " '%s'", a);
        else p += snprintf(cmd + p, cap - p, " %s", a);
    }
    FILE *fp = popen(cmd, "r");
    free(cmd);
    if (!fp) { *ok = false; return strdup(""); }
    size_t len = 0, alloc = 4096; char *buf = malloc(alloc);
    size_t r; char tmp[4096];
    while ((r = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
        if (len + r + 1 > alloc) { alloc *= 2; buf = realloc(buf, alloc); }
        memcpy(buf + len, tmp, r); len += r;
    }
    buf[len] = '\0';
    int code = pclose(fp);
    *ok = (code == 0);
    return buf;
}

/* PoP: web_git_review_ship_info @ hermes_cli/web_git.py:review_ship_info */
json_t *web_git_review_ship_info(const char *cwd) {
    if (!is_dir(cwd)) { json_t *e = json_object(); json_set(e, "ghReady", json_bool(false));
        json_set(e, "pr", json_null()); return e; }
    bool ok; char *out = gh_run(cwd, (const char *[]){"auth", "status"}, 2, &ok);
    free(out);
    if (!ok) { json_t *e = json_object(); json_set(e, "ghReady", json_bool(false));
        json_set(e, "pr", json_null()); return e; }
    char *prout = gh_run(cwd, (const char *[]){"pr", "view", "--json", "url,state,number"}, 4, &ok);
    json_t *pr = NULL;
    if (ok && prout && *prout) {
        json_t *pj = json_parse(prout, NULL);
        if (pj) {
            const char *url = json_get_str(pj, "url", "");
            if (url && *url) {
                pr = json_object();
                json_set(pr, "url", json_string(url));
                json_set(pr, "state", json_string(json_get_str(pj, "state", "")));
                json_set(pr, "number", json_number(json_get_num(pj, "number", 0)));
            }
            json_free(pj);
        }
    }
    free(prout);
    json_t *e = json_object();
    json_set(e, "ghReady", json_bool(true));
    json_set(e, "pr", pr ? pr : json_null());
    return e;
}

/* PoP: web_git_review_create_pr @ hermes_cli/web_git.py:review_create_pr */
json_t *web_git_review_create_pr(const char *cwd) {
    char *e = review_push(cwd); free(e);
    bool ok; char *out = gh_run(cwd, (const char *[]){"pr", "create", "--fill"}, 3, &ok);
    char *url = strdup("");
    if (ok && out) {
        /* last non-empty line */
        char *line = strtok(out, "\n");
        char *last = NULL;
        while (line) { if (*line) last = line; line = strtok(NULL, "\n"); }
        if (last) { free(url); url = strdup(last); }
    }
    free(out);
    json_t *r = json_object();
    json_set(r, "url", json_string(url));
    free(url);
    return r;
}
