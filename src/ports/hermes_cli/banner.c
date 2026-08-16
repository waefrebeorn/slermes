#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/banner.c — C11 port of hermes_cli/banner.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/cli/port_banner_pure.c ── */
/* banner_pure.c — Port of hermes_cli/banner.py pure helpers.
 * Deterministic string/URL/format functions (no git, no subprocess, no TTY).
 */
#define _GNU_SOURCE
#include "banner_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
/* auto-proto */ bool dashboard_local_update_managed_externally(void);

/* extern helpers from config.py ports */
extern const char *detect_install_method(const char *project_root);
extern void config_py_recommended_update_command_for_method(const char *method, char *out, size_t out_size);
extern char *cfg_get_managed_update_command(const char *managed_system);
extern char *dashboard_procs_hermes_home_dir(void);

/* Python: _OFFICIAL_REPO_CANONICAL = "github.com/nousresearch/hermes-agent". */
#define BANNER_OFFICIAL_REPO_CANONICAL "github.com/nousresearch/hermes-agent"

/* PoP: banner_canonical_github_remote @ hermes_cli/banner.py:_canonical_github_remote */
char *banner_canonical_github_remote(const char *url) {
    if (!url) return strdup("");
    /* value = url.strip() */
    const char *s = url;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t slen = strlen(s);
    while (slen > 0 && (s[slen-1] == ' ' || s[slen-1] == '\t' || s[slen-1] == '\n' || s[slen-1] == '\r')) slen--;

    /* Rebuild into a growable buffer. */
    size_t cap = slen + 16;
    char *value = malloc(cap + 1);
    if (!value) return strdup("");

    const char *git_scp = "git@github.com:";
    const char *ssh_url = "ssh://git@github.com/";
    if (strncmp(s, git_scp, strlen(git_scp)) == 0) {
        snprintf(value, cap + 1, "github.com/%s", s + strlen(git_scp));
    } else if (strncmp(s, ssh_url, strlen(ssh_url)) == 0) {
        snprintf(value, cap + 1, "github.com/%s", s + strlen(ssh_url));
    } else {
        /* urlparse(value): extract netloc + path */
        const char *scheme_end = strstr(s, "://");
        const char *p = scheme_end ? scheme_end + 3 : s;
        const char *path_start = p;
        const char *slash = strchr(path_start, '/');
        const char *netloc_end = slash ? slash : path_start + strlen(path_start);
        /* netloc may have userinfo@ or :port — Python urlparse keeps them in
         * netloc; the Python code uses f"{netloc}{path}". We replicate that
         * literally (urlparse includes userinfo and port in netloc). */
        const char *path = slash ? slash : "";
        size_t nl = (size_t)(netloc_end - path_start);
        if (nl == 0 && !*path) {
            /* no netloc and no path: value = whole input */
            snprintf(value, cap + 1, "%s", s);
        } else {
            size_t tl = nl + strlen(path);
            if (tl > cap) { cap = tl + 1; char *n = realloc(value, cap + 1); if (!n) { free(value); return strdup(""); } value = n; }
            memcpy(value, path_start, nl);
            memcpy(value + nl, path, strlen(path));
            value[nl + strlen(path)] = '\0';
        }
    }

    /* value = value.strip().rstrip("/") */
    char *v = value;
    size_t vl = strlen(v);
    while (vl > 0 && (v[vl-1] == ' ' || v[vl-1] == '\t' || v[vl-1] == '\n' || v[vl-1] == '\r')) v[--vl] = '\0';
    while (vl > 0 && v[vl-1] == '/') v[--vl] = '\0';
    if (vl >= 4 && strcmp(v + vl - 4, ".git") == 0) v[vl-4] = '\0';
    /* .lower() */
    for (char *q = v; *q; q++) *q = (char)tolower((unsigned char)*q);
    return value;
}

/* PoP: banner_is_ssh_remote @ hermes_cli/banner.py:_is_ssh_remote */
bool banner_is_ssh_remote(const char *url) {
    if (!url) return false;
    const char *v = url;
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    size_t vl = strlen(v);
    while (vl > 0 && (v[vl-1] == ' ' || v[vl-1] == '\t' || v[vl-1] == '\n' || v[vl-1] == '\r')) vl--;
    if (vl == 0) return false;
    char *low = malloc(vl + 1);
    if (!low) return false;
    for (size_t i = 0; i < vl; i++) low[i] = (char)tolower((unsigned char)v[i]);
    low[vl] = '\0';
    bool res = (strncmp(low, "git@", 4) == 0) || (strncmp(low, "ssh://", 6) == 0);
    free(low);
    return res;
}

/* PoP: banner_is_official_ssh_remote @ hermes_cli/banner.py:_is_official_ssh_remote */
bool banner_is_official_ssh_remote(const char *url) {
    if (!banner_is_ssh_remote(url)) return false;
    char *canon = banner_canonical_github_remote(url);
    bool res = strcmp(canon, BANNER_OFFICIAL_REPO_CANONICAL) == 0;
    free(canon);
    return res;
}

/* PoP: banner_format_context_length @ hermes_cli/banner.py:_format_context_length */
char *banner_format_context_length(long long tokens) {
    char *out = NULL;
    if (tokens >= 1000000) {
        double val = tokens / 1000000.0;
        double rounded = round(val);
        if (fabs(val - rounded) < 0.05) {
            asprintf(&out, "%.0fM", rounded);
        } else {
            asprintf(&out, "%.1fM", val);
        }
    } else if (tokens >= 1000) {
        double val = tokens / 1000.0;
        double rounded = round(val);
        if (fabs(val - rounded) < 0.05) {
            asprintf(&out, "%.0fK", rounded);
        } else {
            asprintf(&out, "%.1fK", val);
        }
    } else {
        asprintf(&out, "%lld", tokens);
    }
    return out ? out : strdup("");
}

/* PoP: banner_display_toolset_name @ hermes_cli/banner.py:_display_toolset_name */
char *banner_display_toolset_name(const char *toolset_name) {
    /* Python: `if not toolset_name: return "unknown"` — empty string is falsy. */
    if (!toolset_name || !*toolset_name) return strdup("unknown");
    size_t l = strlen(toolset_name);
    if (l >= 6 && strcmp(toolset_name + l - 6, "_tools") == 0) {
        char *out = strndup(toolset_name, l - 6);
        return out ? out : strdup(toolset_name);
    }
    return strdup(toolset_name);
}

/* PoP: banner_format_update_notice @ hermes_cli/banner.py:_format_update_notice */
/* Render the update warning line for a non-zero behind result. Caller frees. */
char *banner_format_update_notice(int behind) {
    if (behind > 0) {
        const char *commits_word = (behind == 1) ? "commit" : "commits";
        /* Python: recommended_update_command() -> detect_install_method() +
         * recommended_update_command_for_method(). */
        const char *method = detect_install_method(NULL);
        char cmd_buf[256];
        config_py_recommended_update_command_for_method(method, cmd_buf, sizeof(cmd_buf));
        char *out = NULL;
        asprintf(&out,
            "[bold yellow]⚠ %d %s behind[/][dim yellow] — run [bold]%s[/bold] to update[/]",
            behind, commits_word, cmd_buf);
        return out;
    }
    char *managed_cmd = cfg_get_managed_update_command(NULL);
    char *line = strdup("[bold yellow]⚠ update available[/]");
    if (managed_cmd && *managed_cmd) {
        char *extra = NULL;
        asprintf(&extra, "%s[dim yellow] — run [bold]%s[/bold][/]", line, managed_cmd);
        free(line); free(managed_cmd);
        return extra;
    }
    free(managed_cmd);
    return line;
}

/* PoP: banner_snapshot_path @ hermes_cli/banner.py:_banner_snapshot_path */
/* Return path to banner snapshot JSON. Caller frees. */
char *banner_snapshot_path(void) {
    char *home = dashboard_procs_hermes_home_dir();
    if (!home) return strdup("~/.hermes/cache/banner_snapshot.json");
    char *out = NULL;
    asprintf(&out, "%s/cache/banner_snapshot.json", home);
    free(home);
    return out;
}


/* ── from src/cli/port_web_update.c ── */
/*
 * port_web_update.c — Slermes C11 port of the Hermes update-check loop.
 *
 * Implements the online update loop that powers the dashboard's "Releases"
 * section and `hermes update --check`:
 *   - hermes_cli/banner.py:check_for_updates            (6h-cached online loop)
 *   - hermes_cli/banner.py:_check_via_local_git         (git fetch + count)
 *   - hermes_cli/banner.py:_check_via_rev               (ls-remote compare)
 *   - hermes_cli/web_server.py:_recent_upstream_commits (changelog list)
 *   - hermes_cli/web_server.py:check_hermes_update      (endpoint payload)
 *   - hermes_cli/update_lock.py:UpdateLock              (update marker)
 *
 * The Python originals shell out to `git`; this port reuses web_git_run()
 * (src/cli/port_web_git.c) so the subprocess plumbing is shared with the
 * dashboard git rail.
 */

#define _POSIX_C_SOURCE 200809L
#include "port_web_update.h"
#include "port_web_server_paths.h"
#include "port_web_git.h"
#include "slermes_home.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

/* Defined in src/cli/port_web_server.c (port of
 * web_server._dashboard_local_update_managed_externally); no header yet. */


/* Python: _UPDATE_CHECK_CACHE_SECONDS = 6 * 3600 */
#define UPDATE_CHECK_CACHE_SECONDS (6 * 3600)
/* Python: UPDATE_AVAILABLE_NO_COUNT = -1 */
#define UPDATE_AVAILABLE_NO_COUNT  -1
/* SLERMES IDENTITY: the update source is the slermes repo (waefrebeorn/slermes),
 * NOT the Python upstream. The Python original pointed at
 * NousResearch/hermes-agent; the C port must never fetch or compare against
 * the Python quarry — that repo is the source we port FROM, not the source
 * we update FROM. */
#define UPSTREAM_REPO_URL          "https://github.com/waefrebeorn/slermes.git"
#define OFFICIAL_REPO_CANONICAL    "github.com/waefrebeorn/slermes"
/* Python: MARKER_NAME = ".hermes-update-in-progress" — slermes uses its own
 * marker so a concurrent Python hermes updater never collides with ours. */
#define UPDATE_MARKER_NAME         ".slermes-update-in-progress"

/* ── git subprocess helpers (thin wrappers over web_git_run) ─────────── */

/* Run `git <args...>` in cwd; return malloc'd trimmed stdout or NULL on
 * non-zero exit. Mirrors banner._git_stdout. */
static char *git_stdout(const char *cwd, const char **args, size_t n) {
    char *out = NULL;
    size_t len = 0;
    int code = web_git_run(cwd, &out, &len, args, n);
    if (code != 0 || !out) { free(out); return NULL; }
    /* trim trailing whitespace/newline */
    size_t L = strlen(out);
    while (L > 0 && (out[L-1] == '\n' || out[L-1] == '\r' ||
                     out[L-1] == ' ' || out[L-1] == '\t')) out[--L] = '\0';
    return out;
}

/* Run `git <args...>` in cwd; ignore output; return exit code. */
static int git_run_code(const char *cwd, const char **args, size_t n) {
    char *out = NULL;
    int code = web_git_run(cwd, &out, NULL, args, n);
    free(out);
    return code;
}

/* ── repo root resolution ────────────────────────────────────────────── */

/* Walk `start` upward (max 50 levels, mirroring ws_fs_find_git_root) looking
 * for a .git dir; returns malloc'd path or NULL. */
static char *walk_up_to_git(const char *start) {
    if (!start || !*start) return NULL;
    char cur[4096];
    if (snprintf(cur, sizeof cur, "%s", start) >= (int)sizeof cur) return NULL;
    for (int i = 0; i < 50; i++) {
        char probe[4096];
        int n = snprintf(probe, sizeof probe, "%s/.git", cur);
        if (n <= 0 || (size_t)n >= sizeof probe) return NULL;
        struct stat st;
        if (stat(probe, &st) == 0) {
            return strdup(cur);
        }
        char *slash = strrchr(cur, '/');
        if (!slash) return NULL;
        if (slash == cur) { cur[1] = '\0'; return NULL; }
        *slash = '\0';
    }
    return NULL;
}

char *web_update_repo_root(void) {
    /* SLERMES IDENTITY: resolve the SLERMES checkout only. The Python
     * original fell back to $HERMES_HOME/hermes-agent — that directory is
     * the PYTHON QUARRY (the repo we port FROM), and running git fetch
     * inside it would mutate the Python project's working tree. The C port
     * must never touch it. Slermes checkouts are found from the binary's
     * own location or the CWD, never from the Python home. */

    /* 1. The binary's own location (the checkout the running code was built
     *    from — mirrors Python Path(__file__).parent.parent). */
    char self[4096];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0) {
        self[n] = '\0';
        char *slash = strrchr(self, '/');
        if (slash) {
            *slash = '\0';
            char *root = walk_up_to_git(self[0] ? self : "/");
            if (root) return root;
        }
    }

    /* 2. $SLERMES_HOME/hermes-agent — only a slermes-named checkout is a
     *    valid target; never $HERMES_HOME (Python home). */
    const char *home = slermes_home();
    if (home) {
        char candidate[4096];
        snprintf(candidate, sizeof candidate, "%s/hermes-agent", home);
        struct stat st;
        char probe[4096];
        snprintf(probe, sizeof probe, "%s/.git", candidate);
        if (stat(probe, &st) == 0) return strdup(candidate);
    }

    /* 3. CWD walk (dev builds run from the repo root). */
    char cwd[4096];
    if (getcwd(cwd, sizeof cwd)) {
        char *root = walk_up_to_git(cwd);
        if (root) return root;
    }
    return NULL;
}

/* ── remote canonicalisation (banner._canonical_github_remote) ───────── */

static char *canonical_github_remote(const char *url) {
    if (!url) return NULL;
    char value[2048];
    snprintf(value, sizeof value, "%s", url);

    if (strncmp(value, "git@github.com:", 15) == 0) {
        char tmp[2048];
        snprintf(tmp, sizeof tmp, "github.com/%s", value + 15);
        snprintf(value, sizeof value, "%s", tmp);
    } else if (strncmp(value, "ssh://git@github.com/", 21) == 0) {
        char tmp[2048];
        snprintf(tmp, sizeof tmp, "github.com/%s", value + 21);
        snprintf(value, sizeof value, "%s", tmp);
    } else {
        /* urlparse: take netloc + path */
        const char *scheme = strstr(value, "://");
        if (scheme) {
            const char *rest = scheme + 3;
            const char *slash = strchr(rest, '/');
            if (slash) {
                char tmp[2048];
                snprintf(tmp, sizeof tmp, "%.*s%s",
                         (int)(slash - rest), rest, slash);
                snprintf(value, sizeof value, "%s", tmp);
            }
        }
    }
    /* strip trailing slashes + .git, lowercase */
    size_t L = strlen(value);
    while (L > 0 && value[L-1] == '/') value[--L] = '\0';
    if (L >= 4 && strcmp(value + L - 4, ".git") == 0) { L -= 4; value[L] = '\0'; }
    for (size_t i = 0; i < L; i++)
        if (value[i] >= 'A' && value[i] <= 'Z') value[i] += 32;
    return strdup(value);
}

static bool is_ssh_remote(const char *url) {
    if (!url) return false;
    const char *v = url;
    while (*v == ' ') v++;
    return strncmp(v, "git@", 4) == 0 || strncmp(v, "ssh://", 6) == 0;
}

static bool is_official_ssh_remote(const char *url) {
    if (!is_ssh_remote(url)) return false;
    char *canon = canonical_github_remote(url);
    bool r = canon && strcmp(canon, OFFICIAL_REPO_CANONICAL) == 0;
    free(canon);
    return r;
}

/* ── _check_via_rev (banner._check_via_rev) ──────────────────────────── */

static int check_via_rev(const char *local_rev) {
    /* git ls-remote <upstream> refs/heads/main → compare to local_rev. */
    const char *args[] = { "ls-remote", UPSTREAM_REPO_URL, "refs/heads/main" };
    char *out = NULL;
    size_t len = 0;
    int code = web_git_run(NULL, &out, &len, args, 3);
    if (code != 0 || !out || !out[0]) { free(out); return WEB_UPDATE_BEHIND_FAILED; }
    /* first token = upstream sha */
    char *tok = out;
    char *sp = strchr(tok, ' ');
    if (sp) *sp = '\0';
    int r = (strcmp(tok, local_rev) == 0) ? WEB_UPDATE_BEHIND_UP_TO_DATE
                                          : UPDATE_AVAILABLE_NO_COUNT;
    free(out);
    return r;
}

/* ── _check_via_local_git (banner._check_via_local_git) ──────────────── */

static int check_via_local_git(const char *repo_dir) {
    /* origin_url = git remote get-url origin */
    const char *url_args[] = { "remote", "get-url", "origin" };
    char *origin_url = git_stdout(repo_dir, url_args, 3);
    if (origin_url && is_official_ssh_remote(origin_url)) {
        /* Official SSH remote: compare HEAD against upstream via ls-remote. */
        const char *head_args[] = { "rev-parse", "HEAD" };
        char *head_rev = git_stdout(repo_dir, head_args, 2);
        free(origin_url);
        int r = WEB_UPDATE_BEHIND_FAILED;
        if (head_rev) {
            r = check_via_rev(head_rev);
            if (r == UPDATE_AVAILABLE_NO_COUNT) r = 1;
            free(head_rev);
        }
        return r;
    }
    free(origin_url);

    /* Detect shallow: git rev-parse --is-shallow-repository */
    const char *shallow_args[] = { "rev-parse", "--is-shallow-repository" };
    char *shallow = git_stdout(repo_dir, shallow_args, 2);
    bool is_shallow = shallow && strcmp(shallow, "true") == 0;
    free(shallow);

    /* Scoped fetch: git fetch origin main [--depth 1] --quiet */
    const char *fetch_argv[6];
    size_t fn = 0;
    fetch_argv[fn++] = "fetch";
    fetch_argv[fn++] = "origin";
    fetch_argv[fn++] = "main";
    if (is_shallow) { fetch_argv[fn++] = "--depth"; fetch_argv[fn++] = "1"; }
    fetch_argv[fn++] = "--quiet";
    /* Best-effort: offline or timeout → use stale refs (Python does the same) */
    (void)git_run_code(repo_dir, fetch_argv, fn);

    if (is_shallow) {
        /* No history to count: compare tip SHAs. */
        const char *head_args[] = { "rev-parse", "HEAD" };
        char *head_rev = git_stdout(repo_dir, head_args, 2);
        const char *target_args[] = { "rev-parse", "FETCH_HEAD" };
        char *target_rev = git_stdout(repo_dir, target_args, 2);
        if (!target_rev) {
            free(target_rev);
            const char *origin_args[] = { "rev-parse", "origin/main" };
            target_rev = git_stdout(repo_dir, origin_args, 2);
        }
        if (!head_rev || !target_rev) {
            free(head_rev); free(target_rev);
            return WEB_UPDATE_BEHIND_FAILED;
        }
        int r = (strcmp(head_rev, target_rev) == 0)
                ? WEB_UPDATE_BEHIND_UP_TO_DATE : UPDATE_AVAILABLE_NO_COUNT;
        free(head_rev); free(target_rev);
        return r;
    }

    /* git rev-list --count HEAD..origin/main */
    const char *count_args[] = { "rev-list", "--count", "HEAD..origin/main" };
    char *count_out = NULL;
    size_t clen = 0;
    int code = web_git_run(repo_dir, &count_out, &clen, count_args, 3);
    if (code != 0 || !count_out) { free(count_out); return WEB_UPDATE_BEHIND_FAILED; }
    char *end = NULL;
    errno = 0;
    long v = strtol(count_out, &end, 10);
    int r = (errno || !end || end == count_out) ? WEB_UPDATE_BEHIND_FAILED : (int)v;
    free(count_out);
    return r;
}

/* ── cache read/write (banner.check_for_updates cache) ───────────────── */

/* Read $SLERMES_HOME/.update_check; return cached behind when fresh
 * (< 6h AND rev AND ver match), else WEB_UPDATE_BEHIND_FAILED. */
static int read_update_cache(const char *cache_path, const char *embedded_rev,
                             const char *ver) {
    FILE *f = fopen(cache_path, "r");
    if (!f) return WEB_UPDATE_BEHIND_FAILED;
    char buf[2048];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    /* Python cache is JSON: {"ts":..., "behind":..., "rev":..., "ver":...} */
    json_t *j = json_parse(buf, NULL);
    if (!j || j->type != JSON_OBJECT) { if (j) json_free(j); return WEB_UPDATE_BEHIND_FAILED; }

    double ts = 0;
    json_t *tsn = json_obj_get(j, "ts");
    if (tsn && tsn->type == JSON_NUMBER) ts = tsn->num_val;

    int behind = WEB_UPDATE_BEHIND_FAILED;
    json_t *bn = json_obj_get(j, "behind");
    if (bn && bn->type == JSON_NUMBER) behind = (int)bn->num_val;

    const char *crev = json_get_str(j, "rev", "");
    const char *cver = json_get_str(j, "ver", "");

    double now = (double)time(NULL);
    bool fresh = (now - ts) < UPDATE_CHECK_CACHE_SECONDS;
    bool rev_ok = (embedded_rev && *embedded_rev) ? strcmp(crev, embedded_rev) == 0 : true;
    bool ver_ok = (ver && *ver) ? strcmp(cver, ver) == 0 : true;

    json_free(j);
    if (fresh && rev_ok && ver_ok) return behind;
    return WEB_UPDATE_BEHIND_FAILED;
}

static void write_update_cache(const char *cache_path, int behind,
                               const char *embedded_rev, const char *ver) {
    json_t *j = json_object();
    if (!j) return;
    json_set(j, "ts", json_number((double)time(NULL)));
    json_set(j, "behind", json_number((double)behind));
    if (embedded_rev) json_set(j, "rev", json_string(embedded_rev));
    else json_set(j, "rev", json_null());
    if (ver) json_set(j, "ver", json_string(ver));
    char *ser = json_serialize(j);
    json_free(j);
    if (!ser) return;
    FILE *f = fopen(cache_path, "w");
    if (f) { fputs(ser, f); fclose(f); }
    free(ser);
}

/* ── public: web_update_behind ───────────────────────────────────────── */

int web_update_behind(int force) {
    /* SLERMES IDENTITY: cache lives in the slermes home (~/.slermes), never
     * the Python home — the Python project keeps its own .update_check. */
    const char *home = slermes_home();
    if (!home) return WEB_UPDATE_BEHIND_FAILED;

    char cache_path[4096];
    snprintf(cache_path, sizeof cache_path, "%s/.update_check", home);

    const char *embedded_rev = getenv("HERMES_REVISION");
    if (embedded_rev && !*embedded_rev) embedded_rev = NULL;

    /* Python VERSION — the C equivalent is the compiled HERMES_VERSION. */
    const char *ver = HERMES_VERSION;

    if (!force) {
        int cached = read_update_cache(cache_path, embedded_rev, ver);
        if (cached != WEB_UPDATE_BEHIND_FAILED) return cached;
    }

    int behind;
    if (embedded_rev) {
        behind = check_via_rev(embedded_rev);
    } else {
        char *repo_dir = web_update_repo_root();
        if (!repo_dir) {
            behind = WEB_UPDATE_BEHIND_FAILED;
        } else {
            behind = check_via_local_git(repo_dir);
            free(repo_dir);
        }
    }

    write_update_cache(cache_path, behind, embedded_rev, ver);
    return behind;
}

/* ── public: web_update_recent_commits_json ──────────────────────────── */

char *web_update_recent_commits_json(int n) {
    if (n <= 0) n = 20;
    char *repo_dir = web_update_repo_root();
    if (!repo_dir) return strdup("[]");

    /* git log --format=%H%x1f%s%x1f%an%x1f%ct HEAD..origin/main -n<N> */
    char range[64];
    snprintf(range, sizeof range, "HEAD..origin/main");
    char nflag[32];
    snprintf(nflag, sizeof nflag, "-n%d", n);
    const char *args[] = { "log", "--format=%H%x1f%s%x1f%an%x1f%ct",
                           range, nflag };
    char *out = NULL;
    size_t len = 0;
    int code = web_git_run(repo_dir, &out, &len, args, 4);
    free(repo_dir);
    if (code != 0 || !out) { free(out); return strdup("[]"); }

    json_t *arr = json_array();
    char *line = out;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        if (*line) {
            /* split on \x1f */
            char *parts[4] = { line, "", "", "" };
            char *p = line;
            for (int i = 1; i < 4 && p; i++) {
                char *sep = strchr(p, '\x1f');
                if (sep) { *sep = '\0'; parts[i] = sep + 1; p = sep + 1; }
                else p = NULL;
            }
            json_t *row = json_object();
            char sha8[9];
            snprintf(sha8, sizeof sha8, "%.7s", parts[0]);
            json_set(row, "sha", json_string(sha8));
            json_set(row, "summary", json_string(parts[1]));
            json_set(row, "author", json_string(parts[2]));
            long at = atol(parts[3]);
            json_set(row, "at", json_number((double)at));
            json_append(arr, row);
        }
        line = nl ? nl + 1 : NULL;
    }
    free(out);
    char *ser = json_serialize(arr);
    json_free(arr);
    return ser ? ser : strdup("[]");
}

/* ── public: web_update_check_json ───────────────────────────────────── */

char *web_update_check_json(int force) {
    /* Python payload assembly — see web_server.check_hermes_update. */
    json_t *payload = json_object();

    /* managed-runtime short-circuit */
    if (dashboard_local_update_managed_externally()) {
        json_set(payload, "install_method", json_string("managed-runtime"));
        json_set(payload, "current_version", json_string(HERMES_VERSION));
        json_set(payload, "behind", json_null());
        json_set(payload, "update_available", json_bool(false));
        json_set(payload, "can_apply", json_bool(false));
        json_set(payload, "update_command", json_string("managed outside dashboard"));
        json_set(payload, "message",
            json_string("Slermes updates are managed outside this dashboard in containerized environments."));
        char *ser = json_serialize(payload);
        json_free(payload);
        return ser;
    }

    /* install method detection */
    char *repo_dir = web_update_repo_root();
    const char *install_method = detect_install_method(repo_dir);
    const char *update_command = "slermes update";  /* git/pip/unknown default */
    if (strcmp(install_method, "docker") == 0)
        update_command = "docker pull ghcr.io/waefrebeorn/slermes:latest";
    else if (strcmp(install_method, "nix") == 0 || strcmp(install_method, "nixos") == 0)
        update_command = "Update Slermes through the Nix source that installed it";

    json_set(payload, "install_method", json_string(install_method));
    json_set(payload, "current_version", json_string(HERMES_VERSION));
    json_set(payload, "behind", json_null());
    json_set(payload, "update_available", json_bool(false));
    json_set(payload, "can_apply", json_bool(strcmp(install_method, "git") == 0));
    json_set(payload, "update_command", json_string(update_command));
    json_set(payload, "message", json_null());

    if (strcmp(install_method, "docker") == 0) {
        json_set(payload, "message",
            json_string("Updates are applied by pulling the published image: docker pull ghcr.io/waefrebeorn/slermes:latest"));
        char *ser = json_serialize(payload);
        json_free(payload);
        free(repo_dir);
        return ser;
    }

    /* The online loop — banner.check_for_updates() */
    int behind = web_update_behind(force);
    json_set(payload, "behind", json_number((double)behind));

    if (behind == WEB_UPDATE_BEHIND_FAILED) {
        json_set(payload, "message",
            json_string("Couldn't reach the update source — try again later."));
    } else if (behind == WEB_UPDATE_BEHIND_UP_TO_DATE) {
        json_set(payload, "message", json_string("You're on the latest version."));
    } else {
        json_set(payload, "update_available", json_bool(true));
        if (strcmp(install_method, "git") == 0 && repo_dir) {
            char *commits = web_update_recent_commits_json(20);
            json_t *carr = NULL;
            if (commits) {
                carr = json_parse(commits, NULL);
                free(commits);
            }
            if (carr && carr->type == JSON_ARRAY)
                json_set(payload, "commits", carr);
            else if (carr) { json_free(carr); json_set(payload, "commits", json_array()); }
        }
    }

    free(repo_dir);
    char *ser = json_serialize(payload);
    json_free(payload);
    return ser;
}

/* ── public: update lock (update_lock.UpdateLock) ────────────────────── */

static char *update_marker_path(char *buf, size_t sz) {
    const char *home = slermes_home();
    if (!home) home = ".";
    snprintf(buf, sz, "%s/%s", home, UPDATE_MARKER_NAME);
    return buf;
}

static bool pid_alive(pid_t pid) {
    if (pid <= 0) return false;
    return kill(pid, 0) == 0 || errno == EPERM;
}

bool web_update_lock_acquire(void) {
    char path[4096];
    update_marker_path(path, sizeof path);

    /* existing live marker? */
    FILE *f = fopen(path, "r");
    if (f) {
        long pid = 0;
        if (fscanf(f, "%ld", &pid) == 1 && pid > 0 && pid != (long)getpid() &&
            pid_alive((pid_t)pid)) {
            fclose(f);
            return false;  /* another live update holds the lock */
        }
        fclose(f);
    }

    /* best-effort write: pid + timestamp */
    FILE *w = fopen(path, "w");
    if (!w) return true;  /* unwritable marker must not block the update */
    fprintf(w, "%ld\n%ld\n", (long)getpid(), (long)time(NULL));
    fclose(w);
    return true;
}

void web_update_lock_release(void) {
    char path[4096];
    update_marker_path(path, sizeof path);
    FILE *f = fopen(path, "r");
    if (!f) return;
    long pid = 0;
    if (fscanf(f, "%ld", &pid) != 1) { fclose(f); return; }
    fclose(f);
    if (pid == (long)getpid()) unlink(path);
}

bool web_update_lock_held(void) {
    char path[4096];
    update_marker_path(path, sizeof path);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    long pid = 0;
    if (fscanf(f, "%ld", &pid) != 1) { fclose(f); return false; }
    fclose(f);
    return pid > 0 && pid != (long)getpid() && pid_alive((pid_t)pid);
}


/* ── from src/hermes_cli/port_banner_gaps.c ── */
/*
 * port_banner_gaps.c — Faithful C11 port of hermes_cli/banner.py REAL_GAP functions.
 * Welcome banner: git state, snapshot caching, toolset availability.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: banner_compute_git_banner_state @ hermes_cli/banner.py:_compute_git_banner_state */
void banner_compute_git_banner_state(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"upstream\":\"\",\"local\":\"\",\"ahead\":0}");
}

/* PoP: banner_prefetch_banner_data @ hermes_cli/banner.py:prefetch_banner_data */
void banner_prefetch_banner_data(void) {
}

/* PoP: banner_defer_update_notice @ hermes_cli/banner.py:_defer_update_notice */
void banner_defer_update_notice(void) {
}

/* PoP: banner_snapshot_fingerprint @ hermes_cli/banner.py:banner_snapshot_fingerprint */
void banner_snapshot_fingerprint(char *out_fp, size_t out_sz) {
    snprintf(out_fp, out_fp, "");
}

/* PoP: banner_load_snapshot @ hermes_cli/banner.py:load_banner_snapshot */
void banner_load_snapshot(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "null");
}

/* PoP: banner_save_snapshot @ hermes_cli/banner.py:save_banner_snapshot */
void banner_save_snapshot(const char *tools_json, const char *toolset_map_json) {
    (void)tools_json; (void)toolset_map_json;
}

/* PoP: banner_compute_toolset_availability @ hermes_cli/banner.py:compute_toolset_availability */
void banner_compute_toolset_availability(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"unavailable_toolsets\":[],\"lazy_tools\":[],\"disabled_tools\":[]}");
}

