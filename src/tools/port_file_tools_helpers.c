/*
 * port_file_tools_helpers.c — Port of Python tools/file_tools.py
 *
 * Faithful C implementations of the path-resolution / read-safety / cwd-tracking
 * helpers used by the file tools. These are the pure and near-pure functions;
 * the per-task terminal-env cwd registry is approximated against $TERMINAL_CWD
 * and the process cwd because the C terminal backend tracks a single shared
 * environment (it does not key a live cwd by task_id the way the Python
 * terminal_tool registry does).
 *
 * Each function carries its exact /* PoP: c_func @ tools/file_tools.py:py_func *​/ marker
 * so the parity scanner matches it to the Python symbol.
 */

#include "hermes_core_types.h"
#include "hermes_file_safety.h"
#include "hermes_logger.h"
#include "hermes_tool_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <wordexp.h>
#include <sys/stat.h>

/* ---------------------------------------------------------------------------
 * Constants (mirror Python module-level sentinels)
 * --------------------------------------------------------------------------- */
#define FILE_TOOLS_DEFAULT_MAX_READ_CHARS 200000

static const char *g_blocked_device_paths[] = {
    "/dev/zero", "/dev/random", "/dev/urandom", "/dev/full",
    "/dev/stdin", "/dev/tty", "/dev/console",
    "/dev/stdout", "/dev/stderr",
    "/dev/fd/0", "/dev/fd/1", "/dev/fd/2",
    NULL
};

static const char *g_terminal_cwd_sentinels[] = {
    "", ".", "./", "auto", "cwd", NULL
};

static const char *g_sensitive_path_prefixes[] = {
    "/etc/", "/boot/", "/usr/lib/systemd/",
    "/private/etc/", "/private/var/", NULL
};

static const char *g_sensitive_exact_paths[] = {
    "/var/run/docker.sock", "/run/docker.sock", NULL
};

/* ---------------------------------------------------------------------------
 * Small path helpers
 * --------------------------------------------------------------------------- */
/* Expand a leading ~ (or ~user) the way Python _expand_tilde does. Caller frees. */
static char *file_tools_expand_tilde(const char *path)
{
    if (!path) return strdup("");
    wordexp_t we;
    if (wordexp(path, &we, WRDE_NOCMD) == 0 && we.we_wordc > 0) {
        char *out = strdup(we.we_wordv[0]);
        wordfree(&we);
        return out ? out : strdup(path);
    }
    return strdup(path);
}

/* Normalize: realpath (canonical, resolves symlinks). Caller frees. NULL on failure. */
static char *file_tools_realpath_dup(const char *path)
{
    char resolved[PATH_MAX];
    if (realpath(path, resolved)) return strdup(resolved);
    return NULL;
}

/* ---------------------------------------------------------------------------
 * _get_max_read_chars
 * --------------------------------------------------------------------------- */
static long g_max_read_chars_cached = -1;

/* PoP: _get_max_read_chars @ tools/file_tools.py:_get_max_read_chars */
/* Return the configured max characters per file read (cached). */
int file_tools_get_max_read_chars(void)
{
    if (g_max_read_chars_cached >= 0) return (int)g_max_read_chars_cached;
    int val = tool_config_get_int("file", "read_max_chars", FILE_TOOLS_DEFAULT_MAX_READ_CHARS);
    if (val <= 0) val = FILE_TOOLS_DEFAULT_MAX_READ_CHARS;
    g_max_read_chars_cached = val;
    return val;
}

/* ---------------------------------------------------------------------------
 * _is_blocked_device_path / _is_blocked_device
 * --------------------------------------------------------------------------- */
/* PoP: _is_blocked_device_path @ tools/file_tools.py:_is_blocked_device_path */
/* True for concrete device/fd paths that can hang reads. */
static bool file_tools_is_blocked_device_path(const char *path)
{
    if (!path) return false;
    char *exp = file_tools_expand_tilde(path);
    char resolved[PATH_MAX];
    char norm[PATH_MAX];
    if (realpath(exp, resolved)) {
        snprintf(norm, sizeof(norm), "%s", resolved);
    } else {
        /* normpath fallback (no symlink resolution): collapse . and .. */
        char buf[PATH_MAX]; snprintf(buf, sizeof(buf), "%s", exp);
        char *segs[256]; int n = 0;
        char *tok = strtok(buf, "/");
        while (tok) { if (tok[0]) segs[n++] = tok; tok = strtok(NULL, "/"); }
        char *out = norm;
        int i = 0;
        for (int k = 0; k < n; k++) {
            if (strcmp(segs[k], ".") == 0) continue;
            if (strcmp(segs[k], "..") == 0) { if (i > 0) i--; continue; }
            if (i > 0) *out++ = '/';
            strcpy(out, segs[k]); out += strlen(segs[k]); i++;
        }
        if (i == 0) { norm[0] = '/'; norm[1] = '\0'; } else *out = '\0';
    }
    free(exp);
    for (int i = 0; g_blocked_device_paths[i]; i++)
        if (strcmp(norm, g_blocked_device_paths[i]) == 0) return true;
    size_t L = strlen(norm);
    if (strncmp(norm, "/proc/", 6) == 0) {
        if (L >= 9 && (strcmp(norm + L - 6, "/fd/0") == 0 ||
                       strcmp(norm + L - 6, "/fd/1") == 0 ||
                       strcmp(norm + L - 6, "/fd/2") == 0))
            return true;
        static const char *proc_ends[] = {
            "/environ", "/cmdline", "/maps", "/smaps", "/smaps_rollup",
            "/numa_maps", "/mem", "/auxv", "/pagemap", NULL
        };
        for (int i = 0; proc_ends[i]; i++) {
            size_t el = strlen(proc_ends[i]);
            if (L >= el && strcmp(norm + L - el, proc_ends[i]) == 0) return true;
        }
    }
    return false;
}

/* PoP: _is_blocked_device @ tools/file_tools.py:_is_blocked_device */
/* True if the path would hang the process (infinite output / blocking input). */
static bool file_tools_is_blocked_device(const char *filepath, const char *base_dir)
{
    if (!filepath) return false;
    char *exp = file_tools_expand_tilde(filepath);
    char full[PATH_MAX];
    if (base_dir && base_dir[0] && filepath[0] != '/') {
        snprintf(full, sizeof(full), "%s/%s", base_dir, exp);
    } else {
        snprintf(full, sizeof(full), "%s", exp);
    }
    free(exp);
    /* normpath */
    char norm[PATH_MAX];
    {
        char buf[PATH_MAX]; snprintf(buf, sizeof(buf), "%s", full);
        char *segs[256]; int n = 0;
        char *tok = strtok(buf, "/");
        while (tok) { if (tok[0]) segs[n++] = tok; tok = strtok(NULL, "/"); }
        char *out = norm;
        for (int k = 0; k < n; k++) {
            if (strcmp(segs[k], ".") == 0) continue;
            if (strcmp(segs[k], "..") == 0) { if (out > norm) { /* pop */ char *s = out - 1; while (out > norm && *s != '/') s--; if (*s == '/') s++; out = s; } continue; }
            if (out > norm) *out++ = '/';
            strcpy(out, segs[k]); out += strlen(segs[k]);
        }
        if (out == norm) { norm[0] = '/'; norm[1] = '\0'; } else *out = '\0';
    }
    if (file_tools_is_blocked_device_path(norm)) return true;
    /* Walk symlink hops (bounded). */
    char current[PATH_MAX];
    snprintf(current, sizeof(current), "%s", norm);
    char seen[20][PATH_MAX];
    int seen_n = 0;
    for (int iter = 0; iter < 20; iter++) {
        char linkbuf[PATH_MAX];
        ssize_t rl = readlink(current, linkbuf, sizeof(linkbuf) - 1);
        if (rl < 0) break;
        linkbuf[rl] = '\0';
        char target[PATH_MAX];
        if (linkbuf[0] == '/') snprintf(target, sizeof(target), "%s", linkbuf);
        else {
            char *slash = strrchr(current, '/');
            if (slash) { *slash = '\0'; snprintf(target, sizeof(target), "%s/%s", current, linkbuf); *slash = '/'; }
            else snprintf(target, sizeof(target), "%s", linkbuf);
        }
        /* normpath target */
        char tnorm[PATH_MAX];
        { char buf[PATH_MAX]; snprintf(buf, sizeof(buf), "%s", target); char *segs[256]; int n=0; char *tok=strtok(buf,"/"); while(tok){if(tok[0])segs[n++]=tok;tok=strtok(NULL,"/");} char *o=tnorm; for(int k=0;k<n;k++){ if(strcmp(segs[k],".")==0)continue; if(strcmp(segs[k],"..")==0){if(o>tnorm){char*s=o-1;while(o>tnorm&&*s!='/')s--;if(*s=='/')s++;o=s;}continue;} if(o>tnorm)*o++='/'; strcpy(o,segs[k]);o+=strlen(segs[k]);} if(o==tnorm){tnorm[0]='/';tnorm[1]='\0';}else*o='\0'; }
        if (file_tools_is_blocked_device_path(tnorm)) { return true; }
        /* cycle check */
        bool cyc = false;
        for (int k = 0; k < seen_n; k++) if (strcmp(seen[k], tnorm) == 0) { cyc = true; break; }
        if (cyc) break;
        if (seen_n < 20) snprintf(seen[seen_n++], sizeof(seen[0]), "%s", tnorm);
        snprintf(current, sizeof(current), "%s", tnorm);
    }
    char *rp = file_tools_realpath_dup(norm);
    if (rp) {
        bool blocked = file_tools_is_blocked_device_path(rp);
        free(rp);
        return blocked;
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * _sentinel_free_abs_cwd / _configured_terminal_cwd
 * --------------------------------------------------------------------------- */
/* PoP: _sentinel_free_abs_cwd @ tools/file_tools.py:_sentinel_free_abs_cwd */
/* Normalize a cwd candidate to an absolute, sentinel-free anchor. Caller frees. */
char *file_tools_sentinel_free_abs_cwd(const char *raw)
{
    if (!raw) return NULL;
    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s", raw);
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (int i = 0; g_terminal_cwd_sentinels[i]; i++) {
        if (strcmp(buf, g_terminal_cwd_sentinels[i]) == 0) return NULL;
    }
    char *exp = file_tools_expand_tilde(raw);
    if (exp[0] != '/') { free(exp); return NULL; }
    /* existence-agnostic: Python only checks isabs; we honor that contract. */
    return exp;
}

/* PoP: _configured_terminal_cwd @ tools/file_tools.py:_configured_terminal_cwd */
/* $TERMINAL_CWD only when it names a real directory anchor. Caller frees. */
char *file_tools_configured_terminal_cwd(void)
{
    const char *env = getenv("TERMINAL_CWD");
    if (!env) return NULL;
    return file_tools_sentinel_free_abs_cwd(env);
}

/* ---------------------------------------------------------------------------
 * Workspace root resolution (best-effort; C terminal env is single/shared).
 * --------------------------------------------------------------------------- */
/* PoP: _authoritative_workspace_root @ tools/file_tools.py:_authoritative_workspace_root */
/* Best-effort absolute workspace root for divergence checks. Caller frees. */
char *file_tools_authoritative_workspace_root(const char *task_id)
{
    (void)task_id;
    /* Live terminal cwd (single shared env in C) */
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        /* prefer $TERMINAL_CWD if it is a real absolute anchor */
    }
    char *term = file_tools_configured_terminal_cwd();
    if (term) {
        /* In C the live cwd is the shared env cwd; honor $TERMINAL_CWD which is
         * the configured worktree anchor when present. */
        return term;
    }
    return NULL;
}

/* PoP: _resolve_base_dir @ tools/file_tools.py:_resolve_base_dir */
/* Absolute base directory for resolving relative paths. Caller frees. */
char *file_tools_resolve_base_dir(const char *task_id)
{
    char *root = file_tools_authoritative_workspace_root(task_id);
    char base[PATH_MAX];
    if (root) {
        char *exp = file_tools_expand_tilde(root);
        if (exp[0] == '/') snprintf(base, sizeof(base), "%s", exp);
        else { char cwd[PATH_MAX]; getcwd(cwd, sizeof(cwd)); snprintf(base, sizeof(base), "%s/%s", cwd, exp); }
        free(exp); free(root);
    } else {
        char cwd[PATH_MAX];
        getcwd(cwd, sizeof(cwd));
        snprintf(base, sizeof(base), "%s", cwd);
    }
    char *rp = file_tools_realpath_dup(base);
    if (!rp) rp = strdup(base);
    return rp;
}

/* PoP: _resolve_path_for_task @ tools/file_tools.py:_resolve_path_for_task */
/* Resolve filepath against the task's absolute base directory. Caller frees. */
char *file_tools_resolve_path_for_task(const char *filepath, const char *task_id)
{
    if (!filepath) return strdup("");
    char *exp = file_tools_expand_tilde(filepath);
    if (exp[0] == '/') {
        char *rp = file_tools_realpath_dup(exp);
        if (!rp) rp = strdup(exp);
        free(exp);
        return rp;
    }
    char *base = file_tools_resolve_base_dir(task_id);
    char combined[PATH_MAX];
    snprintf(combined, sizeof(combined), "%s/%s", base ? base : "", exp);
    free(exp); free(base);
    char *rp = file_tools_realpath_dup(combined);
    if (!rp) rp = strdup(combined);
    return rp;
}

/* ---------------------------------------------------------------------------
 * _path_resolution_warning
 * --------------------------------------------------------------------------- */
/* PoP: _path_resolution_warning @ tools/file_tools.py:_path_resolution_warning */
/* Warn when a relative path resolved OUTSIDE the task's workspace root. Caller frees. */
char *file_tools_path_resolution_warning(const char *filepath, const char *resolved, const char *task_id)
{
    if (!filepath || !resolved) return NULL;
    char *exp = file_tools_expand_tilde(filepath);
    bool is_abs = (exp[0] == '/');
    free(exp);
    if (is_abs) return NULL;
    char *root = file_tools_authoritative_workspace_root(task_id);
    if (!root) return NULL;
    char *root_rp = file_tools_realpath_dup(root);
    char *resolved_rp = file_tools_realpath_dup(resolved);
    free(root);
    bool outside = true;
    if (root_rp && resolved_rp) {
        size_t rl = strlen(root_rp);
        outside = !(strncmp(resolved_rp, root_rp, rl) == 0 &&
                   (resolved_rp[rl] == '/' || resolved_rp[rl] == '\0'));
    }
    char *warn = NULL;
    if (outside) {
        size_t need = strlen(filepath) + strlen(resolved) + 256;
        warn = (char *)malloc(need);
        if (warn) {
            snprintf(warn, need,
                "Relative path %s resolved to %s, which is OUTSIDE the active workspace (%s). "
                "The edit will land in a different directory than the terminal's cwd. If this is "
                "not intended (e.g. a git-worktree session writing into the main checkout), pass "
                "an absolute path under the workspace instead.",
                filepath, resolved, root_rp ? root_rp : "");
        }
    }
    free(root_rp); free(resolved_rp);
    return warn;
}

/* ---------------------------------------------------------------------------
 * _check_sensitive_path
 * --------------------------------------------------------------------------- */
/* PoP: _check_sensitive_path @ tools/file_tools.py:_check_sensitive_path */
/* Return an error message if the path targets a sensitive system location. Caller frees. */
char *file_tools_check_sensitive_path(const char *filepath, const char *task_id)
{
    if (!filepath) return NULL;
    char *resolved = file_tools_resolve_path_for_task(filepath, task_id);
    const char *resolved_s = resolved ? resolved : filepath;
    char *norm_exp = file_tools_expand_tilde(filepath);
    char *norm_rp = file_tools_realpath_dup(norm_exp);
    const char *normalized = norm_rp ? norm_rp : norm_exp;

    char *err = NULL;
    for (int i = 0; g_sensitive_path_prefixes[i]; i++) {
        if (strncmp(resolved_s, g_sensitive_path_prefixes[i], strlen(g_sensitive_path_prefixes[i])) == 0 ||
            strncmp(normalized, g_sensitive_path_prefixes[i], strlen(g_sensitive_path_prefixes[i])) == 0) {
            size_t need = strlen(filepath) + 160;
            err = (char *)malloc(need);
            if (err) snprintf(err, need,
                "Refusing to write to sensitive system path: %s\n"
                "Use the terminal tool with sudo if you need to modify system files.", filepath);
            break;
        }
    }
    if (!err) {
        for (int i = 0; g_sensitive_exact_paths[i]; i++) {
            if (strcmp(resolved_s, g_sensitive_exact_paths[i]) == 0 ||
                strcmp(normalized, g_sensitive_exact_paths[i]) == 0) {
                size_t need = strlen(filepath) + 160;
                err = (char *)malloc(need);
                if (err) snprintf(err, need,
                    "Refusing to write to sensitive system path: %s\n"
                    "Use the terminal tool with sudo if you need to modify system files.", filepath);
                break;
            }
        }
    }
    /* Hermes config file is security-sensitive — refuse direct agent writes. */
    if (!err) {
        char cfg_path[PATH_MAX];
        snprintf(cfg_path, sizeof(cfg_path), "%s/.hermes/config.yaml", getenv("HOME") ? getenv("HOME") : "");
        char *cfg_rp = file_tools_realpath_dup(cfg_path);
        if (cfg_rp) {
            if (strcmp(resolved_s, cfg_rp) == 0 || strcmp(normalized, cfg_rp) == 0) {
                size_t need = strlen(filepath) + 200;
                err = (char *)malloc(need);
                if (err) snprintf(err, need,
                    "Refusing to write to Hermes config file: %s\n"
                    "Agent cannot modify security-sensitive configuration. "
                    "Edit ~/.hermes/config.yaml directly or use 'hermes config' instead.", filepath);
            }
            free(cfg_rp);
        }
    }
    free(norm_exp); free(norm_rp); free(resolved);
    return err;
}

/* ---------------------------------------------------------------------------
 * _check_cross_profile_path
 * --------------------------------------------------------------------------- */
/* PoP: _check_cross_profile_path @ tools/file_tools.py:_check_cross_profile_path */
/* Soft-guard warning when filepath lands in another profile's scope / a sandbox
 * or container mirror. Caller frees. */
char *file_tools_check_cross_profile_path(const char *filepath, const char *task_id)
{
    if (!filepath) return NULL;
    char *resolved = file_tools_resolve_path_for_task(filepath, task_id);
    const char *r = resolved ? resolved : filepath;

    char warn_buf[1024];
    if (file_is_cross_profile_target(r, warn_buf, sizeof(warn_buf)) && warn_buf[0]) {
        char *w = strdup(warn_buf);
        free(resolved);
        return w;
    }
    char *sw = get_sandbox_mirror_warning(r);
    if (sw) { free(resolved); return sw; }
    /* container mirror: C terminal backend has no per-task docker mirror prefix */
    char *cw = get_container_mirror_warning(r, NULL);
    if (cw) { free(resolved); return cw; }
    free(resolved);
    return NULL;
}

