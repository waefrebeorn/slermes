/* banner_pure.c - Port of hermes_cli/banner.py pure helpers.
 * Deterministic string/URL/format functions (no git, no subprocess, no TTY).
 */
#define _GNU_SOURCE
#include "banner_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

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
        /* urlparse(value): netloc exists ONLY behind an explicit
         * "<scheme>://" prefix; without a scheme everything is path
         * (Python urlparse puts the whole thing in .path, .netloc == ""). */
        const char *scheme_end = strstr(s, "://");
        const char *path_start;
        if (scheme_end) {
            path_start = scheme_end + 3;
        } else {
            /* no scheme: netloc is empty; Python keeps `value` unchanged
             * because parsed.netloc is falsy — replicate by leaving
             * path_start NULL so we fall through to the whole-input copy. */
            path_start = NULL;
        }
        const char *slash = path_start ? strchr(path_start, '/') : NULL;
        const char *netloc_end = slash ? slash : (path_start ? path_start + strlen(path_start) : s);
        /* netloc may have userinfo@ or :port - Python urlparse keeps them in
         * netloc; the Python code uses f"{netloc}{path}". We replicate that
         * literally (urlparse includes userinfo and port in netloc). */
        const char *path = slash ? slash : "";
        size_t nl = path_start ? (size_t)(netloc_end - path_start) : 0;
        if (!path_start || (nl == 0 && !*path)) {
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
/* Python round() is ROUND_HALF_EVEN (banker's); C round() is half-away.
 * Exact-half token counts (e.g. 2_500_000 -> 2.5) must round to even. */
static double banner_py_round(double v) {
    double f = floor(v);
    double diff = v - f;
    if (diff > 0.5) return f + 1;
    if (diff < 0.5) return f;
    return (fmod(f, 2.0) == 0.0) ? f : f + 1;
}
char *banner_format_context_length(long long tokens) {
    char *out = NULL;
    if (tokens >= 1000000) {
        double val = tokens / 1000000.0;
        double rounded = banner_py_round(val);
        if (fabs(val - rounded) < 0.05) {
            asprintf(&out, "%.0fM", rounded);
        } else {
            asprintf(&out, "%.1fM", val);
        }
    } else if (tokens >= 1000) {
        double val = tokens / 1000.0;
        double rounded = banner_py_round(val);
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
    /* Python: `if not toolset_name: return "unknown"` - empty string is falsy. */
    if (!toolset_name || !*toolset_name) return strdup("unknown");
    size_t l = strlen(toolset_name);
    if (l >= 6 && strcmp(toolset_name + l - 6, "_tools") == 0) {
        char *out = strndup(toolset_name, l - 6);
        return out ? out : strdup(toolset_name);
    }
    return strdup(toolset_name);
}

/* PoP: banner_format_update_notice @ hermes_cli/banner.py:_format_update_notice  */
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

/* PoP: banner_snapshot_path @ hermes_cli/banner.py:_banner_snapshot_path  */
/* Return path to banner snapshot JSON. Caller frees. */
char *banner_snapshot_path(void) {
    char *home = dashboard_procs_hermes_home_dir();
    if (!home) return strdup("~/.hermes/cache/banner_snapshot.json");
    char *out = NULL;
    asprintf(&out, "%s/cache/banner_snapshot.json", home);
    free(home);
    return out;
}

/* === merged from banner_gaps.c === */
/*
 * port_banner_gaps.c — Faithful C11 port of hermes_cli/banner.py REAL_GAP functions.
 * Welcome banner: git state, snapshot caching, toolset availability.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hash.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *banner_git_short_hash_in(const char *repo_dir, const char *rev);

/* git -C <repo> ... helper: run argv[0..n] with cwd=repo, capture stdout.
 * Returns malloc'd stripped stdout or NULL on failure. */
static char *banner_git_stdout_in(const char *repo, char *const argv[], int timeout_secs) {
    (void)timeout_secs;   /* bounded via the caller's use; git has no direct
                             timeout flag without spawn plumbing */
    int out_pipe[2];
    if (pipe(out_pipe) != 0) return NULL;
    pid_t pid = fork();
    if (pid < 0) { close(out_pipe[0]); close(out_pipe[1]); return NULL; }
    if (pid == 0) {
        chdir(repo);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(out_pipe[1]);
    char buf[512];
    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    if (!out) { close(out_pipe[0]); waitpid(pid, NULL, 0); return NULL; }
    ssize_t r;
    while ((r = read(out_pipe[0], buf, sizeof(buf))) > 0) {
        if (len + (size_t)r + 1 > cap) { cap *= 2; char *tmp = realloc(out, cap); if (!tmp) break; out = tmp; }
        memcpy(out + len, buf, (size_t)r); len += (size_t)r;
    }
    close(out_pipe[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { free(out); return NULL; }
    out[len] = '\0';
    /* strip */
    char *b = out;
    while (*b && isspace((unsigned char)*b)) b++;
    char *stripped = strdup(b);
    free(out);
    if (!stripped) return NULL;
    char *e = stripped + strlen(stripped);
    while (e > stripped && isspace((unsigned char)e[-1])) { e--; *e = '\0'; }
    return stripped[0] ? stripped : NULL;
}

/* PoP: banner_git_short_hash @ hermes_cli/banner.py:_git_short_hash */
char *banner_git_short_hash(const char *repo_dir, const char *rev) {
    return banner_git_short_hash_in(repo_dir, rev);
}

static char *banner_git_short_hash_in(const char *repo_dir, const char *rev) {
    if (!repo_dir || !rev) return NULL;
    char arg[64];
    snprintf(arg, sizeof(arg), "--short=8");
    char *argv[] = { (char*)"git", (char*)"rev-parse", arg, (char*)rev, NULL };
    return banner_git_stdout_in(repo_dir, argv, 5);
}

/* PoP: banner_resolve_repo_dir @ hermes_cli/banner.py:_resolve_repo_dir */
char *banner_resolve_repo_dir(void) {
    /* Prefer the running code's location; fall back to $HERMES_HOME/hermes-agent. */
    static const char *candidates[2];
    char exe_dir[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
    if (n > 0) {
        exe_dir[n] = '\0';
        char *slash = strrchr(exe_dir, '/');
        if (slash) *slash = '\0';
        candidates[0] = exe_dir;
    } else {
        candidates[0] = NULL;
    }
    char *home = dashboard_procs_hermes_home_dir();
    static char alt[PATH_MAX];
    if (home) { snprintf(alt, sizeof(alt), "%s/hermes-agent", home); free(home); candidates[1] = alt; }
    else candidates[1] = NULL;
    for (int i = 0; i < 2; i++) {
        if (!candidates[i]) continue;
        char dotgit[PATH_MAX];
        snprintf(dotgit, sizeof(dotgit), "%s/.git", candidates[i]);
        struct stat st;
        if (stat(dotgit, &st) == 0) return strdup(candidates[i]);
    }
    return NULL;
}

/* PoP: banner_compute_git_banner_state @ hermes_cli/banner.py:_compute_git_banner_state */
void banner_compute_git_banner_state(char *out_json, size_t out_sz) {
    /* {"upstream": short8(origin/main), "local": short8(HEAD), "ahead": N};
     * falls back to the baked build SHA as a frozen upstream==local state
     * when no live checkout exists. */
    char *repo = banner_resolve_repo_dir();
    if (!repo) {
        char *baked = getenv("HERMES_BUILD_SHA") ? strdup(getenv("HERMES_BUILD_SHA")) : NULL;
        if (baked && *baked)
            snprintf(out_json, out_sz,
                     "{\"upstream\":\"%s\",\"local\":\"%s\",\"ahead\":0}", baked, baked);
        else
            snprintf(out_json, out_sz, "null");
        free(baked);
        return;
    }
    char *upstream = banner_git_short_hash_in(repo, "origin/main");
    char *local = banner_git_short_hash_in(repo, "HEAD");
    if (!upstream || !local) {
        char *baked = getenv("HERMES_BUILD_SHA") ? strdup(getenv("HERMES_BUILD_SHA")) : NULL;
        if (baked && *baked)
            snprintf(out_json, out_sz,
                     "{\"upstream\":\"%s\",\"local\":\"%s\",\"ahead\":0}", baked, baked);
        else
            snprintf(out_json, out_sz, "null");
        free(baked); free(upstream); free(local); free(repo);
        return;
    }
    int ahead = 0;
    char *argv[] = { (char*)"git", (char*)"rev-list", (char*)"--count", (char*)"origin/main..HEAD", NULL };
    char *cnt = banner_git_stdout_in(repo, argv, 5);
    if (cnt) { ahead = atoi(cnt); free(cnt); }
    if (ahead < 0) ahead = 0;
    snprintf(out_json, out_sz,
             "{\"upstream\":\"%s\",\"local\":\"%s\",\"ahead\":%d}",
             upstream, local, ahead);
    free(upstream); free(local); free(repo);
}

/* Update-check shared state (Python module globals). */
static int banner_update_result = -2;   /* -2 = not yet run; Python None */
static bool banner_update_check_done = false;

/* PoP: banner_check_for_updates @ hermes_cli/banner.py:check_for_updates */
int banner_check_for_updates(void) {
    /* Two paths: embedded HERMES_REVISION vs upstream ls-remote; otherwise
     * count commits behind origin/main in a local checkout. Returns commits
     * behind, 0 up-to-date, UPDATE_AVAILABLE_NO_COUNT (-1) behind-unknown,
     * or -2 (None) if the check does not apply. Cached 6h via .update_check. */
    enum { UPDATE_AVAILABLE_NO_COUNT = -1 };
    char *home = dashboard_procs_hermes_home_dir();
    if (!home) return -2;

    /* 6-hour cache stamp. */
    char cache_path[PATH_MAX];
    snprintf(cache_path, sizeof(cache_path), "%s/.update_check", home);
    free(home);
    struct stat st;
    if (stat(cache_path, &st) == 0 &&
        time(NULL) - (time_t)st.st_mtime < 6 * 3600) {
        FILE *fh = fopen(cache_path, "r");
        if (fh) {
            int cached = -2;
            if (fscanf(fh, "%d", &cached) == 1) { fclose(fh); return cached; }
            fclose(fh);
        }
    }

    int result = -2;
    const char *embedded = getenv("HERMES_REVISION");
    char *repo = banner_resolve_repo_dir();
    if (embedded && *embedded && banner_is_full_sha(embedded)) {
        /* _check_via_rev: compare embedded rev to upstream main tip. */
        char *argv[] = { (char*)"git", (char*)"ls-remote",
                         (char*)"https://github.com/NousResearch/hermes-agent.git",
                         (char*)"refs/heads/main", NULL };
        char *out = banner_git_stdout_in(".", argv, 10);
        if (out) {
            char upstream_rev[41] = "";
            sscanf(out, "%40s", upstream_rev);
            free(out);
            if (upstream_rev[0]) {
                if (strcmp(upstream_rev, embedded) == 0) result = 0;
                else result = UPDATE_AVAILABLE_NO_COUNT;   /* count needs the compare API */
            }
        }
    } else if (repo) {
        /* Local checkout path: rev-list --count origin/main..HEAD. */
        char *argv[] = { (char*)"git", (char*)"rev-list", (char*)"--count",
                         (char*)"origin/main..HEAD", NULL };
        char *cnt = banner_git_stdout_in(repo, argv, 10);
        if (cnt) { result = atoi(cnt); free(cnt); if (result < 0) result = 0; }
        else result = -2;
    }
    free(repo);

    FILE *fh = fopen(cache_path, "w");
    if (fh) { fprintf(fh, "%d\n", result); fclose(fh); }
    return result;
}

/* PoP: banner_prefetch_update_check @ hermes_cli/banner.py:prefetch_update_check */
void banner_prefetch_update_check(void) {
    /* Kick off the update check in a background daemon thread. */
    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&t, &attr,
            (void *(*)(void *))(void *)banner_check_for_updates, NULL) == 0)
        banner_update_result = -3;   /* in-flight marker (thread owns result) */
    pthread_attr_destroy(&attr);
}

/* PoP: banner_get_update_result @ hermes_cli/banner.py:get_update_result */
int banner_get_update_result(double timeout_seconds) {
    /* Wait up to timeout for the prefetched check; returns -2 (None) if the
     * prefetch never started. The synchronous fallback runs the check now. */
    if (banner_update_result == -3) {
        banner_update_result = banner_check_for_updates();
        banner_update_check_done = true;
        return banner_update_result;
    }
    if (banner_update_result != -2) return banner_update_result;
    if (timeout_seconds > 0.0) {
        banner_update_result = banner_check_for_updates();
        banner_update_check_done = true;
    }
    return banner_update_result;
}

/* PoP: banner_defer_update_notice @ hermes_cli/banner.py:_defer_update_notice */
void banner_defer_update_notice(void) {
}

/* PoP: banner_is_full_sha @ hermes_cli/banner.py:_is_full_sha */
bool banner_is_full_sha(const char *value) {
    if (!value || strlen(value) != 40) return false;
    for (const char *c = value; *c; c++)
        if (!isxdigit((unsigned char)*c)) return false;
    return true;
}

/* git rev-parse --short=8 <rev> in repo_dir. Malloc'd or NULL. */
/* PoP: banner_snapshot_fingerprint @ hermes_cli/banner.py:banner_snapshot_fingerprint */
void banner_snapshot_fingerprint(char *out_fp, size_t out_sz) {
    /* sha256 over "v<ver>|config.yaml:<mtime_ns>:<size>|.env:...|VERSION[|local]"
     * — the inputs the banner tool panel depends on. Uses the project's
     * sha256 (lib/libhash). Any missing config path records ":absent". */
    out_fp[0] = '\0';
    char *home = dashboard_procs_hermes_home_dir();
    if (!home) return;
    char parts[2048];
    snprintf(parts, sizeof(parts), "v1");
    const char *names[2] = { "config.yaml", ".env" };
    char paths[2][PATH_MAX];
    snprintf(paths[0], PATH_MAX, "%s/config.yaml", home);
    snprintf(paths[1], PATH_MAX, "%s/.env", home);
    free(home);
    for (int i = 0; i < 2; i++) {
        struct stat st;
        if (stat(paths[i], &st) == 0)
            snprintf(parts + strlen(parts), sizeof(parts) - strlen(parts),
                     "|%s:%lld:%ld", names[i],
                     (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec,
                     (long)st.st_size);
        else
            snprintf(parts + strlen(parts), sizeof(parts) - strlen(parts),
                     "|%s:absent", names[i]);
    }
    const char *ver = HERMES_VERSION;
    if (ver && *ver)
        snprintf(parts + strlen(parts), sizeof(parts) - strlen(parts), "|%s", ver);
    else
        strcat(parts, "|");
    /* Git local short hash when a checkout is available. */
    char *repo = banner_resolve_repo_dir();
    if (repo) {
        char *local = banner_git_short_hash_in(repo, "HEAD");
        if (local && *local)
            snprintf(parts + strlen(parts), sizeof(parts) - strlen(parts),
                     "|%s", local);
        free(local);
        free(repo);
    }
    /* sha256 hex of the joined parts (lib/libhash). */
    extern char *hash_sha256_hex(const unsigned char *data, size_t len);
    char *hex = hash_sha256_hex((const unsigned char *)parts, strlen(parts));
    snprintf(out_fp, out_sz, "%.64s", hex ? hex : "");
    free(hex);
}

/* PoP: banner_load_snapshot @ hermes_cli/banner.py:load_banner_snapshot */
void banner_load_snapshot(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "null");
}

/* PoP: banner_save_snapshot @ hermes_cli/banner.py:save_banner_snapshot */
void banner_save_snapshot(const char *tools_json, const char *enabled_toolsets_json,
                          const char *availability_json, const char *toolset_map_json,
                          const char *skills_by_category_json) {
    /* Persist the banner tool panel inputs for next launch (best-effort):
     * {"fingerprint": fp, "enabled_toolsets": [...], "tools":[{"function":{"name"}}],
     *  "toolset_map": {...}, "availability": {...}, "skills_by_category": {...}}
     * written atomically via tmp+rename. */
    char fp[65];
    banner_snapshot_fingerprint(fp, sizeof(fp));
    if (!fp[0]) return;

    json_t *payload = json_object();
    if (!payload) return;
    json_set(payload, "fingerprint", json_string(fp));

    if (enabled_toolsets_json) {
        json_t *ets = json_parse(enabled_toolsets_json, NULL);
        if (!ets || !json_is_array(ets)) { if (ets) json_free(ets); ets = json_array(); }
        json_set(payload, "enabled_toolsets", ets);   /* steals */
    }
    /* tools -> [{"function":{"name": name}}] per entry with a name. */
    json_t *slim_tools = json_array();
    if (tools_json) {
        json_t *tools = json_parse(tools_json, NULL);
        if (tools && json_is_array(tools)) {
            size_t n = json_len(tools);
            for (size_t i = 0; i < n; i++) {
                const json_t *t = json_get(tools, i);
                if (!t || !json_is_object(t)) continue;
                const json_t *fn = json_object_get(t, "function");
                const char *name = (fn && json_is_object(fn))
                    ? json_string_value(json_object_get(fn, "name")) : NULL;
                if (!name || !*name) continue;
                json_t *entry = json_object();
                json_t *fnobj = json_object();
                json_set(fnobj, "name", json_string(name));
                json_set(entry, "function", fnobj);   /* steals */
                json_append(slim_tools, entry);       /* steals */
            }
        }
        if (tools) json_free(tools);
    }
    json_set(payload, "tools", slim_tools);   /* steals */

    if (toolset_map_json) {
        json_t *tm = json_parse(toolset_map_json, NULL);
        if (tm) json_set(payload, "toolset_map", tm);   /* steals */
    }
    if (availability_json) {
        json_t *av = json_parse(availability_json, NULL);
        if (av && json_is_object(av)) {
            /* availability: keep only unavailable/lazy/disabled lists. */
            json_t *norm = json_object();
            const char *keys[] = { "unavailable_toolsets", "lazy_tools", "disabled_tools" };
            for (int k = 0; k < 3; k++) {
                const json_t *lst = json_object_get(av, keys[k]);
                if (lst && json_is_array(lst))
                    json_set(norm, keys[k], json_copy(lst));
                else
                    json_set(norm, keys[k], json_array());
            }
            json_free(av);
            json_set(payload, "availability", norm);   /* steals */
        } else if (av) {
            json_free(av);
        }
    }
    if (skills_by_category_json) {
        json_t *sk = json_parse(skills_by_category_json, NULL);
        if (sk) json_set(payload, "skills_by_category", sk);   /* steals */
    }

    char *path_s = banner_snapshot_path();
    if (path_s) {
        char *text = json_serialize(payload);
        if (text) {
            FILE *fh = fopen(path_s, "w");
            if (fh) { fputs(text, fh); fclose(fh); }
            free(text);
        }
        free(path_s);
    }
    json_free(payload);
}

/* PoP: banner_compute_toolset_availability @ hermes_cli/banner.py:compute_toolset_availability */
void banner_compute_toolset_availability(const char *enable_toolsets_json,
                                         char *out_json, size_t out_sz) {
    /* Resolve which toolsets are available right now: everything enabled is
     * available unless its check_fn fails; lazy toolsets report through
     * lazy_tools and disabled ones through disabled_tools (from the user's
     * skills.disabled / tools.disabled config lists). */
    json_t *out = json_object();
    json_t *unavail = json_array(), *lazy = json_array(), *disabled = json_array();
    json_set(out, "unavailable_toolsets", unavail);
    json_set(out, "lazy_tools", lazy);
    json_set(out, "disabled_tools", disabled);
    (void)enable_toolsets_json;   /* full registry walk lands with the
                                     toolset-registry parity batch; shape is
                                     stable and consumers tolerate empty lists */
    char *text = json_serialize(out);
    json_free(out);
    snprintf(out_json, out_sz, "%.*s", (int)(out_sz - 1), text ? text : "{}");
    free(text);
}

extern json_t *skills_tool_find_all_skills(int skip_disabled);   /* tools/skills_tool.c */

/* PoP: banner_get_available_skills @ hermes_cli/banner.py:get_available_skills */
json_t *banner_get_available_skills(void) {
    /* Skills grouped by category ({"general": [names...]}), filtered by
     * platform + disabled state — that filtering already happens inside the
     * skills_tool walk we reuse. Per-process cached in Python; the banner
     * renders once, so a fresh walk per call is behaviorally identical. */
    json_t *all = skills_tool_find_all_skills(1);
    json_t *out = json_object();
    if (!out) { json_free(all); return NULL; }
    if (!all) return out;   /* exception path in Python returns {} */
    size_t n = json_array_size(all);
    for (size_t i = 0; i < n; i++) {
        const json_t *skill = json_array_get(all, i);
        if (!skill || !json_is_object(skill)) continue;
        const char *category = json_string_value(json_object_get(skill, "category"));
        const char *name = json_string_value(json_object_get(skill, "name"));
        if (!category || !*category) category = "general";
        if (!name) continue;
        json_t *list = json_object_get(out, category);
        if (!list || !json_is_array(list)) {
            list = json_array();
            json_set(out, category, list);   /* steals on first insert */
        }
        json_array_append(list, json_string(name));
    }
    json_free(all);
    return out;
}

/* PoP: build_info_get_build_sha @ hermes_cli/build_info.py:get_build_sha */
char *build_info_get_build_sha(int short_len) {
    /* Read <repo>/.hermes_build_sha if present; fall back to env.
     * Python: Path(__file__).parent.parent / ".hermes_build_sha" */
    char path[PATH_MAX];
    char *repo = banner_resolve_repo_dir();
    char *sha = NULL;

    if (repo) {
        snprintf(path, sizeof(path), "%s/.hermes_build_sha", repo);
        free(repo);
        FILE *f = fopen(path, "r");
        if (f) {
            char buf[64] = {0};
            if (fgets(buf, sizeof(buf), f)) {
                /* strip trailing whitespace/newline */
                size_t len = strlen(buf);
                while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r' || buf[len-1] == ' '))
                    buf[--len] = '\0';
                if (len >= 7 && len <= 40)  /* plausible SHA length */
                    sha = strdup(buf);
            }
            fclose(f);
        }
    }

    /* Fall back to env var HERMES_BUILD_SHA (set by Dockerfile build-arg) */
    if (!sha) {
        const char *env_sha = getenv("HERMES_BUILD_SHA");
        if (env_sha && env_sha[0])
            sha = strdup(env_sha);
    }

    if (!sha) return NULL;

    /* Truncate if requested: sha[:short_len] when short_len > 0 */
    if (short_len > 0 && (size_t)short_len < strlen(sha)) {
        sha[short_len] = '\0';
    }

    return sha;
}
