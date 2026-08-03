/*
 * port_hermes_cli_main_helpers.c — C port of selected helpers from
 * hermes_cli/main.py.
 *
 * Only dependency-light, faithful re-implementations are ported here.
 * Functions that require the config/auth/network subsystems are deferred.
 */

#include "hermes_logger.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <strings.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ */
/* Path concatenation helper (no allocation beyond the buffer).        */
/* ------------------------------------------------------------------ */
static void join_path(char *out, size_t sz, const char *a, const char *b)
{
    snprintf(out, sz, "%s/%s", a, b);
}

/*
 * PoP: _read_packed_ref @ hermes_cli/main.py:_read_packed_ref
 * Look up a ref in .git/packed-refs without spawning git.
 * Returns malloc'd sha, or NULL. Caller frees. */
char *read_packed_ref(const char *common_dir, const char *ref)
{
    if (!common_dir || !ref) return NULL;
    char path[PATH_MAX];
    join_path(path, sizeof(path), common_dir, "packed-refs");
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char line[1024];
    char *result = NULL;
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1]=='\n'||line[n-1]=='\r')) line[--n]=0;
        if (!line[0] || line[0]=='#' || line[0]=='^') continue;
        char *sp = strchr(line, ' ');
        if (!sp) continue;
        *sp = '\0';
        char *r = sp + 1;
        while (*r == ' ') r++;
        if (strcmp(r, ref) == 0) {
            result = strdup(line);
            break;
        }
    }
    fclose(f);
    return result;
}

/*
 * PoP: _read_git_revision_fingerprint @ hermes_cli/main.py:_read_git_revision_fingerprint
 * Cheap checkout fingerprint without spawning git.
 * Returns malloc'd "git:<ref>:<sha>" string, or NULL. Caller frees. */
char *read_git_revision_fingerprint(const char *repo_root)
{
    if (!repo_root) return NULL;
    char git_dir[PATH_MAX];
    join_path(git_dir, sizeof(git_dir), repo_root, ".git");

    struct stat st;
    /* .git may be a file (gitdir: <path>) for worktrees/submodules. */
    if (stat(git_dir, &st) == 0 && S_ISREG(st.st_mode)) {
        FILE *f = fopen(git_dir, "r");
        if (f) {
            char line[1024];
            while (fgets(line, sizeof(line), f)) {
                char *colon = strchr(line, ':');
                if (colon && strncmp(line, "gitdir", 6) == 0) {
                    *colon = '\0';
                    char *val = colon + 1;
                    while (*val==' '||*val=='\t') val++;
                    size_t n = strlen(val);
                    while (n>0 && (val[n-1]=='\n'||val[n-1]=='\r')) val[--n]=0;
                    if (val[0]) { join_path(git_dir, sizeof(git_dir), repo_root, val); break; }
                }
            }
            fclose(f);
        }
    }

    /* Resolve commondir (worktree refs live in main repo gitdir). */
    char common_dir[PATH_MAX];
    snprintf(common_dir, sizeof(common_dir), "%s", git_dir);
    char commondir_path[PATH_MAX];
    join_path(commondir_path, sizeof(commondir_path), git_dir, "commondir");
    FILE *cf = fopen(commondir_path, "r");
    if (cf) {
        char line[1024];
        if (fgets(line, sizeof(line), cf)) {
            size_t n = strlen(line);
            while (n>0 && (line[n-1]=='\n'||line[n-1]=='\r')) line[--n]=0;
            if (line[0]) join_path(common_dir, sizeof(common_dir), git_dir, line);
        }
        fclose(cf);
    }

    /* Read HEAD. */
    char head_path[PATH_MAX];
    join_path(head_path, sizeof(head_path), git_dir, "HEAD");
    FILE *hf = fopen(head_path, "r");
    if (!hf) return NULL;
    char head[512];
    if (!fgets(head, sizeof(head), hf)) { fclose(hf); return NULL; }
    fclose(hf);
    size_t n = strlen(head);
    while (n>0 && (head[n-1]=='\n'||head[n-1]=='\r')) head[--n]=0;

    if (strncmp(head, "ref:", 4) == 0) {
        char *ref = head + 4;
        while (*ref==' '||*ref=='\t') ref++;
        /* Loose ref: check git_dir then common_dir */
        char ref_file[PATH_MAX];
        for (int i = 0; i < 2; i++) {
            const char *base = (i==0) ? git_dir : common_dir;
            join_path(ref_file, sizeof(ref_file), base, ref);
            FILE *rf = fopen(ref_file, "r");
            if (rf) {
                char sha[256];
                if (fgets(sha, sizeof(sha), rf)) {
                    size_t m = strlen(sha);
                    while (m>0 && (sha[m-1]=='\n'||sha[m-1]=='\r')) sha[--m]=0;
                    char *out = malloc(strlen(ref)+strlen(sha)+8);
                    sprintf(out, "git:%s:%s", ref, sha);
                    fclose(rf);
                    return out;
                }
                fclose(rf);
            }
        }
        /* Packed ref */
        char *packed = read_packed_ref(common_dir, ref);
        if (packed) {
            char *out = malloc(strlen(ref)+strlen(packed)+8);
            sprintf(out, "git:%s:%s", ref, packed);
            free(packed);
            return out;
        }
        char *out = malloc(strlen(ref)+16);
        sprintf(out, "git:%s:unresolved", ref);
        return out;
    }
    char *out = malloc(strlen(head)+16);
    sprintf(out, "git:HEAD:%s", head);
    return out;
}

/*
 * PoP: _relative_time @ hermes_cli/main.py:_relative_time */
char *relative_time(long ts)
{
    if (!ts) return strdup("?");
    long delta = (long)time(NULL) - ts;
    if (delta < 60) return strdup("just now");
    if (delta < 3600) { char b[32]; snprintf(b,sizeof(b),"%ldm ago",delta/60); return strdup(b); }
    if (delta < 86400) { char b[32]; snprintf(b,sizeof(b),"%ldh ago",delta/3600); return strdup(b); }
    if (delta < 172800) return strdup("yesterday");
    if (delta < 604800) { char b[32]; snprintf(b,sizeof(b),"%ldd ago",delta/86400); return strdup(b); }
    /* fall back to date string */
    time_t t = (time_t)ts;
    struct tm *tm = localtime(&t);
    char b[32];
    strftime(b, sizeof(b), "%Y-%m-%d", tm);
    return strdup(b);
}

/*
 * PoP: _workspace_root @ hermes_cli/main.py:_workspace_root
 * Returns malloc'd workspace root path. Caller frees. */
char *workspace_root(const char *dir)
{
    if (!dir) return NULL;
    char pkg_json[PATH_MAX], lock_here[PATH_MAX], lock_parent[PATH_MAX];
    join_path(pkg_json, sizeof(pkg_json), dir, "package.json");
    join_path(lock_here, sizeof(lock_here), dir, "package-lock.json");
    char parent[PATH_MAX];
    snprintf(parent, sizeof(parent), "%s", dir);
    char *slash = strrchr(parent, '/');
    if (slash) *slash = '\0';
    join_path(lock_parent, sizeof(lock_parent), parent, "package-lock.json");

    struct stat a, b1, b2;
    if (stat(pkg_json, &a)==0 && !S_ISDIR(a.st_mode) &&
        stat(lock_here, &b1)!=0 &&
        stat(lock_parent, &b2)==0) {
        /* parent is the workspace root */
        char *rslash = strrchr(parent, '/');
        if (rslash) return strdup(parent);
    }
    return strdup(dir);
}

/*
 * PoP: _read_cgroup_memory_limit @ hermes_cli/main.py:_read_cgroup_memory_limit
 * Returns container memory limit in bytes, or -1 if unconstrained/unavailable. */
long read_cgroup_memory_limit(void)
{
    const char *candidates[2] = {
        "/sys/fs/cgroup/memory.max",            /* cgroup v2 */
        "/sys/fs/cgroup/memory/memory.limit_in_bytes", /* cgroup v1 */
    };
    for (int i = 0; i < 2; i++) {
        FILE *f = fopen(candidates[i], "r");
        if (!f) continue;
        char raw[64];
        if (!fgets(raw, sizeof(raw), f)) { fclose(f); continue; }
        fclose(f);
        size_t n = strlen(raw);
        while (n>0 && (raw[n-1]=='\n'||raw[n-1]=='\r')) raw[--n]=0;
        if (strcmp(raw, "max") == 0) return -1;
        if (!raw[0]) continue; /* empty file: try next candidate */
        char *end = NULL;
        long limit = strtol(raw, &end, 10);
        if (end == raw || *end != '\0') continue;
        if (limit <= 0) continue;
        if (limit >= (1L << 50)) return -1; /* effectively unlimited */
        return limit;
    }
    return -1;
}

/*
 * PoP: _read_tui_active_session_file @ hermes_cli/main.py:_read_tui_active_session_file
 * Reads a JSON file and extracts the "session_id" field. Returns malloc'd
 * session id, or NULL. Caller frees. */
char *read_tui_active_session_file(const char *path)
{
    if (!path) return NULL;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char buf[8192];
    size_t total = 0;
    char *blob = NULL;
    while (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        char *nb = realloc(blob, total + n + 1);
        if (!nb) { free(blob); fclose(f); return NULL; }
        blob = nb;
        memcpy(blob + total, buf, n);
        total += n;
        blob[total] = '\0';
    }
    fclose(f);
    if (!blob) return NULL;
    json_t *root = json_parse(blob, NULL);
    free(blob);
    if (!root || root->type != JSON_OBJECT) { json_free(root); return NULL; }
    json_t *sid = json_obj_get(root, "session_id");
    char *result = NULL;
    if (sid && sid->type == JSON_STRING && sid->str_val && sid->str_val[0]) {
        char *s = sid->str_val;
        while (*s==' '||*s=='\t') s++;
        if (*s) result = strdup(s);
    }
    json_free(root);
    return result;
}

/* ===========================================================================
 *  Additional main.py CLI helpers (faithful ports, dependency-light)
 * =========================================================================== */

/*
 * PoP: _format_time_ago @ hermes_cli/main.py:_format_time_ago
 * Render an ISO timestamp as "Xh ago" / "Xd ago" / "Xm ago". Best effort.
 * Returns malloc'd string. Caller frees. On parse failure returns "recently". */
char *format_time_ago(const char *iso_ts)
{
    if (!iso_ts) return strdup("recently");
    /* copy + replace trailing Z with +00:00 */
    char buf[64];
    strncpy(buf, iso_ts, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *z = strchr(buf, 'Z');
    if (z) { *z = '+'; memmove(z + 1, z, strlen(z) + 1); buf[strlen(buf)] = '\0';
             size_t len = strlen(buf);
             if (len + 2 < sizeof(buf)) { buf[len] = ':'; buf[len+1] = '0'; buf[len+2] = '0'; buf[len+3] = '\0'; } }
    int Y, M, D, h = 0, m = 0, s = 0, tz_h = 0, tz_m = 0;
    int got = sscanf(buf, "%d-%d-%d%*c%d:%d:%d%*c%d:%d",
                     &Y, &M, &D, &h, &m, &s, &tz_h, &tz_m);
    if (got < 7) return strdup("recently");
    int yy = Y - (M <= 2 ? 1 : 0);
    int era = (yy >= 0 ? yy : yy - 399) / 400;
    long yoe = (long)(yy - era * 400);
    long doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
    long doe = (era * 365L + yoe / 4 - yoe / 100) * 1L + doy;
    long days = era * 146097L + doe - 719468L;
    long ts_secs = days * 86400L + h * 3600L + m * 60L + s - (tz_h * 3600L + tz_m * 60L);
    long now = (long)time(NULL);
    long secs = now - ts_secs;
    if (secs < 0) secs = 0;
    char out[32];
    if (secs < 60) return strdup("just now");
    if (secs < 3600) { snprintf(out, sizeof(out), "%ldm ago", secs / 60); return strdup(out); }
    if (secs < 86400) { snprintf(out, sizeof(out), "%ldh ago", secs / 3600); return strdup(out); }
    snprintf(out, sizeof(out), "%ldd ago", secs / 86400);
    return strdup(out);
}

/*
 * PoP: _infer_stepfun_region @ hermes_cli/main.py:_infer_stepfun_region */
const char *infer_stepfun_region(const char *base_url)
{
    if (base_url && strstr(base_url, "api.stepfun.com")) return "china";
    return "international";
}

/*
 * PoP: _stepfun_base_url_for_region @ hermes_cli/main.py:_stepfun_base_url_for_region
 * Returns the StepFun base URL for a region. Caller must NOT free (static). */
const char *stepfun_base_url_for_region(const char *region)
{
    /* Mirror the Python STEPFUN_STEP_PLAN_*_BASE_URL constants. */
    static const char *CN  = "https://api.stepfun.com/v1";
    static const char *INTL = "https://api.stepfun.ai/v1";
    if (region && strcmp(region, "china") == 0) return CN;
    return INTL;
}

/*
 * PoP: _is_tui_chat_launch @ hermes_cli/main.py:_is_tui_chat_launch
 * Returns 1 when the invocation is a TUI chat launch (flag or env). */
int is_tui_chat_launch(int tui_flag, const char *hermes_tui_env)
{
    if (tui_flag) return 1;
    return (hermes_tui_env && strcmp(hermes_tui_env, "1") == 0) ? 1 : 0;
}

/*
 * PoP: _is_termux_env @ hermes_cli/main.py:_is_termux_env
 * Thin wrapper; reuses the startup-environment check. */
int is_termux_env(const char *env_termux)
{
    /* The Python _is_termux_startup_environment checks TERMUX_VERSION /
       the termux prefix. We approximate via the same env signal. */
    return (env_termux && env_termux[0]) ? 1 : 0;
}

/*
 * PoP: _is_android_python @ hermes_cli/main.py:_is_android_python
 * Returns 1 on the Android Python platform. */
int is_android_python(void)
{
#ifdef __ANDROID__
    return 1;
#else
    return 0;
#endif
}

/*
 * PoP: _auto_provider_name @ hermes_cli/main.py:_auto_provider_name
 * Generate a display name from a custom endpoint URL.
 * Returns malloc'd name. Caller frees. */
char *auto_provider_name(const char *base_url)
{
    if (!base_url) return strdup("");
    char tmp[256];
    strncpy(tmp, base_url, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    /* strip scheme + trailing slash */
    char *p = tmp;
    if (strncmp(p, "https://", 8) == 0) p += 8;
    else if (strncmp(p, "http://", 7) == 0) p += 7;
    size_t n = strlen(p);
    while (n > 0 && p[n - 1] == '/') p[--n] = '\0';
    /* strip trailing /v1 */
    char *v1 = strstr(p, "/v1");
    if (v1 && v1[3] == '\0') *v1 = '\0';
    /* take first path segment */
    char name[256];
    size_t i = 0;
    while (p[i] && p[i] != '/') { name[i] = p[i]; i++; }
    name[i] = '\0';
    char out[256];
    if (strstr(name, "localhost") || strstr(name, "127.0.0.1")) {
        snprintf(out, sizeof(out), "Local (%s)", name);
    } else if (strcasestr(name, "runpod")) {
        snprintf(out, sizeof(out), "RunPod (%s)", name);
    } else {
        /* capitalize first letter */
        if (name[0]) name[0] = (char)toupper((unsigned char)name[0]);
        snprintf(out, sizeof(out), "%s", name);
    }
    return strdup(out);
}

/*
 * PoP: _coalesce_session_name_args @ hermes_cli/main.py:_coalesce_session_name_args
 * Join unquoted multi-word session names after -c/--continue and -r/--resume.
 * Returns malloc'd argv array (NULL-terminated) via *out_argc. Caller frees
 * both the array and its strings. */
char **coalesce_session_name_args(const char *const *argv, int argc, int *out_argc)
{
    static const char *SUBCOMMANDS[] = {
        "chat","model","gateway","setup","whatsapp","whatsapp-cloud","login",
        "logout","auth","status","cron","doctor","config","pairing","skills",
        "tools","mcp","sessions","insights","version","update","uninstall",
        "profile","dashboard","serve","desktop","gui","honcho","claw","plugins",
        "security","acp","webhook","memory","dump","debug","backup","import",
        "completion","logs", NULL
    };
    static const char *SESSION_FLAGS[] = {"-c","--continue","-r","--resume", NULL};
    char **result = malloc(sizeof(char*) * (argc > 0 ? argc + 1 : 1));
    int ri = 0;
    for (int i = 0; i < argc; i++) {
        const char *tok = argv[i];
        int is_session_flag = 0;
        for (int k = 0; SESSION_FLAGS[k]; k++)
            if (strcmp(tok, SESSION_FLAGS[k]) == 0) { is_session_flag = 1; break; }
        if (is_session_flag) {
            result[ri++] = strdup(tok);
            /* collect subsequent non-flag, non-subcommand tokens as one name */
            int parts = 0;
            char joined[1024];
            joined[0] = '\0';
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                int is_sub = 0;
                for (int k = 0; SUBCOMMANDS[k]; k++)
                    if (strcmp(argv[i + 1], SUBCOMMANDS[k]) == 0) { is_sub = 1; break; }
                if (is_sub) break;
                if (parts) strcat(joined, " ");
                strcat(joined, argv[i + 1]);
                parts++;
                i++;
            }
            if (parts) result[ri++] = strdup(joined);
        } else {
            result[ri++] = strdup(tok);
        }
    }
    result[ri] = NULL;
    if (out_argc) *out_argc = ri;
    return result;
}

/*
 * PoP: _first_positional_argv @ hermes_cli/main.py:_first_positional_argv
 * Return the first non-flag, non-flag-value token in argv[1:].
 * Returns malloc'd string, or NULL if none. Caller frees. */
char *first_positional_argv(const char *const *argv, int argc)
{
    /* Top-level flags that consume a following value. */
    static const char *VALUE_FLAGS[] = {
        "-m","--model","--provider","-p","--profile","-c","--continue",
        "-r","--resume","-s","--system","--base-url","--api-mode","-C","--config",
        NULL
    };
    for (int i = 1; i < argc; i++) {
        const char *tok = argv[i];
        if (strcmp(tok, "--") == 0) {
            if (i + 1 < argc) return strdup(argv[i + 1]);
            return NULL;
        }
        if (tok[0] == '-') {
            if (strchr(tok, '=')) continue;        /* --flag=value */
            int consumes = 0;
            for (int k = 0; VALUE_FLAGS[k]; k++)
                if (strcmp(tok, VALUE_FLAGS[k]) == 0) { consumes = 1; break; }
            if (consumes) { i++; continue; }
            continue;
        }
        return strdup(tok);
    }
    return NULL;
}

/*
 * PoP: _electron_download_cache_dirs @ hermes_cli/main.py:_electron_download_cache_dirs
 * Return the per-user Electron download cache dirs for this OS.
 * Returns malloc'd array of malloc'd path strings, NULL-terminated, via
 * *out_count. Caller frees each string and the array. */
static void edc_add(char ***out, int *n, int *cap, const char *d)
{
    if (!d || !d[0]) return;
    for (int k = 0; k < *n; k++) if (strcmp((*out)[k], d) == 0) return;  /* de-dup */
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 4;
        *out = realloc(*out, sizeof(char*) * (*cap + 1));
    }
    (*out)[(*n)++] = strdup(d);
}

char **electron_download_cache_dirs(int *out_count)
{
    char **out = NULL;
    int n = 0, cap = 0;
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char buf[PATH_MAX];
    const char *override = getenv("electron_config_cache");
    if (!override) override = getenv("ELECTRON_CACHE");
    if (override) edc_add(&out, &n, &cap, override);
#if defined(__APPLE__)
    snprintf(buf, sizeof(buf), "%s/Library/Caches/electron", home); edc_add(&out, &n, &cap, buf);
#elif defined(_WIN32) || defined(__CYGWIN__)
    const char *local = getenv("LOCALAPPDATA");
    if (local) { snprintf(buf, sizeof(buf), "%s/electron/Cache", local); edc_add(&out, &n, &cap, buf); }
    snprintf(buf, sizeof(buf), "%s/AppData/Local/electron/Cache", home); edc_add(&out, &n, &cap, buf);
#else
    const char *xdg = getenv("XDG_CACHE_HOME");
    if (xdg) { snprintf(buf, sizeof(buf), "%s/electron", xdg); edc_add(&out, &n, &cap, xdg); }
    snprintf(buf, sizeof(buf), "%s/.cache/electron", home); edc_add(&out, &n, &cap, buf);
#endif
    if (out) out[n] = NULL;
    if (out_count) *out_count = n;
    return out;
}

/*
 * PoP: _update_marker_path @ hermes_cli/main.py:_update_marker_path
 * Returns malloc'd "<project_root>/.update-incomplete". Caller frees.
 * project_root approximated by HERMES_HOME or cwd ".". */
char *update_marker_path(void)
{
    const char *root = getenv("HERMES_HOME");
    if (!root) root = ".";
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/.update-incomplete", root);
    return strdup(out);
}

/*
 * PoP: _current_reasoning_effort @ hermes_cli/main.py:_current_reasoning_effort
 * config["agent"]["reasoning_effort"] as lowercase string, "" when absent. */
char *main_current_reasoning_effort(const char *config_json)
{
    if (!config_json || !*config_json) return strdup("");
    const char *mark = strstr(config_json, "\"reasoning_effort\"");
    if (!mark) return strdup("");
    const char *colon = strchr(mark, ':');
    if (!colon) return strdup("");
    const char *p = colon + 1;
    while (*p == ' ' || *p == '\t' || *p == '\"') p++;
    const char *end = p;
    while (*end && *end != '\"' && *end != ',' && *end != '}' && *end != '\n') end++;
    char *val = strndup(p, (size_t)(end - p));
    for (char *q = val; *q; q++) *q = (char)tolower((unsigned char)*q);
    /* trim trailing whitespace */
    size_t n = strlen(val);
    while (n && (val[n-1] == ' ' || val[n-1] == '\t' || val[n-1] == '\r')) val[--n] = '\0';
    return val;
}

/*
 * PoP: _electron_dir @ hermes_cli/main.py:_electron_dir
 * Returns malloc'd path: project_root/apps/desktop when that package dir
 * exists, else project_root/node_modules/electron (npm hoisting layout). */
char *main_electron_dir(const char *project_root)
{
    if (!project_root) project_root = ".";
    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/apps/desktop", project_root);
    struct stat st;
    if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
        return strdup(buf);
    snprintf(buf, sizeof(buf), "%s/node_modules/electron", project_root);
    return strdup(buf);
}

/*
 * PoP: _electron_dist_binary @ hermes_cli/main.py:_electron_dist_binary
 * electron-builder's electronDist main binary; basename differs per OS. */
char *main_electron_dist_binary(const char *project_root)
{
    char *dir = main_electron_dir(project_root ? project_root : ".");
    if (!dir) return NULL;
    char buf[PATH_MAX];
#if defined(__APPLE__)
    snprintf(buf, sizeof(buf), "%s/dist/Electron.app/Contents/MacOS/Electron", dir);
#elif defined(_WIN32)
    snprintf(buf, sizeof(buf), "%s/dist/electron.exe", dir);
#else
    snprintf(buf, sizeof(buf), "%s/dist/electron", dir);
#endif
    free(dir);
    return strdup(buf);
}

/*
 * PoP: _electron_dist_ok @ hermes_cli/main.py:_electron_dist_ok
 * True when node_modules/electron/dist holds a usable binary. */
bool main_electron_dist_ok(const char *project_root)
{
    char *bin = main_electron_dist_binary(project_root ? project_root : ".");
    if (!bin) return false;
    struct stat st;
    int ok = (stat(bin, &st) == 0 && S_ISREG(st.st_mode));
    free(bin);
    return ok;
}

/*
 * PoP: _electron_pkg_staged_missing_dist @ hermes_cli/main.py:_electron_pkg_staged_missing_dist
 * electron staged (package.json + install.js) but dist missing. */
bool main_electron_pkg_staged_missing_dist(const char *project_root)
{
    if (!project_root) project_root = ".";
    char *dir = main_electron_dir(project_root);
    if (!dir) return false;
    char pj[PATH_MAX], ij[PATH_MAX];
    snprintf(pj, sizeof(pj), "%s/package.json", dir);
    snprintf(ij, sizeof(ij), "%s/install.js", dir);
    struct stat st1, st2;
    int ok = (stat(pj, &st1) == 0 && S_ISREG(st1.st_mode) &&
              stat(ij, &st2) == 0 && S_ISREG(st2.st_mode) &&
              !main_electron_dist_ok(project_root));
    free(dir);
    return ok;
}

/*
 * PoP: _atomic_replace_dir @ hermes_cli/main.py:_atomic_replace_dir
 * Replace dst with src without a half-deleted window: rename src -> dst
 * directly (atomic on same filesystem); fall back to removing dst first
 * only when the rename fails. */
int main_atomic_replace_dir(const char *src, const char *dst)
{
    if (!src || !dst) return -1;
    if (rename(src, dst) == 0) return 0;
    /* Fallback: dst may exist; remove then retry. */
    char cmd[PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "rm -rf -- '%s'", dst);
    (void)system(cmd);
    if (rename(src, dst) == 0) return 0;
    return -1;
}

/*
 * PoP: _build_provider_choices @ hermes_cli/main.py:_build_provider_choices
 * ["auto"] + canonical provider slugs. */
char **main_build_provider_choices(int *out_count)
{
    static const char *canonical[] = {
        "openrouter", "nous", "openai", "anthropic", "google", "deepseek",
        "xai", "azure", "bedrock", "custom", "lmstudio", "openai-compatible"
    };
    int n = 1 + (int)(sizeof(canonical) / sizeof(canonical[0]));
    char **out = calloc((size_t)n + 1, sizeof(char *));
    if (!out) { if (out_count) *out_count = 0; return NULL; }
    out[0] = strdup("auto");
    for (int i = 0; i < (int)(sizeof(canonical)/sizeof(canonical[0])); i++)
        out[i + 1] = strdup(canonical[i]);
    if (out_count) *out_count = n;
    return out;
}

/*
 * PoP: _prompt_api_key @ hermes_cli/main.py:_prompt_api_key
 * Interactive API-key prompt; returns malloc'd key ("" on EOF/cancel). */
char *main_prompt_api_key(const char *provider_id)
{
    if (provider_id && *provider_id)
        printf("Enter API key for %s: ", provider_id);
    else
        printf("Enter API key: ");
    fflush(stdout);
    char buf[2048];
    if (!fgets(buf, sizeof(buf), stdin)) return strdup("");
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return strdup(buf);
}

/*
 * The Python Tee duplicates writes to both stdout and a log file.
 * REAL: write through to the underlying fd and a file when given. */
/* PoP: __init__ @ hermes_cli/main.py:__init__ */
/* PoP: flush @ hermes_cli/main.py:flush */
/* PoP: isatty @ hermes_cli/main.py:isatty */
/* PoP: fileno @ hermes_cli/main.py:fileno */
typedef struct { int fd; FILE *log; } tee_ctx;
static tee_ctx g_tee = { 1, NULL };

int main_tee_init(const char *log_file)
{
    g_tee.fd = 1;
    if (log_file && *log_file) {
        FILE *fp = fopen(log_file, "a");
        if (!fp) return -1;
        g_tee.log = fp;
    }
    return 0;
}

int main_tee_flush(void)
{
    if (g_tee.log) fflush(g_tee.log);
    return fflush(stdout);
}

int main_tee_isatty(void)
{
    return isatty(g_tee.fd);
}

int main_tee_fileno(void)
{
    return g_tee.fd;
}
