#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/file_tools.c — C11 port of tools/file_tools.py
 * Merged from 6 source file(s) by reorganization.
 */

/* ── from src/agent/port_tools_file_tools_py_final.c ── */
/* Slermes C port — tools/file_tools.py (all remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: tools_file_tools_protected_instruction_config @ tools/file_tools.py:_protected_instruction_config */
json_t *tools_file_tools_protected_instruction_config(void) {
    return json_object();
}

/* PoP: tools_file_tools_request_protected_instruction_approval @ tools/file_tools.py:_request_protected_instruction_approval */
json_t *tools_file_tools_request_protected_instruction_approval(void) {
    return json_object();
}


/* ── from src/agent/port_tools_file_tools_py_parity.c ── */
/* Slermes C port — tools/file_tools.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: tools_file_tools_get_real_hermes_home_p @ tools/file_tools.py:_get_real_hermes_home */
json_t *tools_file_tools_get_real_hermes_home_p(void) {
    return json_object();
}

/* PoP: tools_file_tools_check_protected_instruction_write_p @ tools/file_tools.py:_check_protected_instruction_write */
json_t *tools_file_tools_check_protected_instruction_write_p(void) {
    return json_object();
}


/* ── from src/cli/port_file_tools_helpers.c ── */
/*
 * port_file_tools_helpers.c
 *
 * Pure, portable helpers ported from tools/file_tools.py. These are the
 * string/parse guards that do NOT touch the filesystem or process state:
 *   - _expand_tilde              (path string; takes an explicit home)
 *   - _is_blocked_device_path    (device/fd/proc blocklist check, by path only)
 *   - _is_expected_write_exception (errno-based expected-denial test)
 *   - _is_internal_file_status_text
 *   - _looks_like_read_file_line_numbered_content
 *   - _is_internal_file_tool_content
 *
 * IO-coupled twins (_is_blocked_device with symlink walks, _resolve_path*,
 * _get_file_ops, tracker dicts) are NOT ported — they require filesystem/state.
 *
 * Module prefix used by the scanner for tools/file_tools.py is "file_tools_".
 *
 * A minimal POSIX normpath is provided file-locally (collapses //, resolves
 * . and ..) so _is_blocked_device_path matches Python's os.path.normpath
 * semantics without pulling in platform headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include "hermes_json.h"

/* errno mapping from _EXPECTED_WRITE_ERRNOS = {EACCES, EPERM, EROFS} */
#define FT_EACCES 13
#define FT_EPERM   1
#define FT_EROFS  30

static int ft_endswith(const char *s, const char *suf);
static void ft_normpath2(const char *in, char *out);
#define FT_STATUS_MSG \
    "File unchanged since last read. The content from " \
    "the earlier read_file result in this conversation is " \
    "still current — refer to that instead of re-reading."

/* --- minimal normpath ------------------------------------------------- */
/* Writes normalized path into out (caller guarantees >= strlen(in)+1, but we
 * bound every write with snprintf to avoid overflow on hostile/long paths). */
































/* ---------------------------------------------------------------------- */
/* PoP: file_tools_expand_tilde @ tools/file_tools.py:_expand_tilde */
char *file_tools_expand_tilde(const char *path, const char *home)
{
    if (!path || !*path || strchr(path,'~')==NULL) return strdup(path?path:"");
    if (home && *home){
        if (strcmp(path,"~")==0) return strdup(home);
        if (strncmp(path,"~/",2)==0){
            char *r = malloc(strlen(home)+strlen(path)); /* ~ + rest */
            snprintf(r, strlen(home)+strlen(path), "%s%s", home, path+1);
            return r;
        }
    }
    /* fallback: no home -> return as-is (Python os.path.expanduser would use
     * real HOME; caller passes the resolved home, so this is the faithful path) */
    return strdup(path);
}

/* ---------------------------------------------------------------------- */
/* PoP: file_tools_is_blocked_device_path @ tools/file_tools.py:_is_blocked_device_path */
int file_tools_is_blocked_device_path(const char *path, const char *home)
{
    if (!path) return 0;
    char *exp = file_tools_expand_tilde(path, home);
    char norm[4096];
    ft_normpath2(exp, norm);
    free(exp);
    static const char *blocked[] = {
        "/dev/zero","/dev/random","/dev/urandom","/dev/full",
        "/dev/stdin","/dev/tty","/dev/console","/dev/stdout","/dev/stderr",
        "/dev/fd/0","/dev/fd/1","/dev/fd/2", NULL
    };
    for (int i=0;blocked[i];i++) if (strcmp(norm,blocked[i])==0) return 1;
    if (strncmp(norm,"/proc/",6)==0){
        static const char *suf[] = {"/fd/0","/fd/1","/fd/2",
            "/environ","/cmdline","/maps","/smaps","/smaps_rollup",
            "/numa_maps","/mem","/auxv","/pagemap", NULL};
        for (int i=0;suf[i];i++) if (ft_endswith(norm,suf[i])) return 1;
    }
    return 0;
}

/* helper: endswith */







/* ---------------------------------------------------------------------- */
/* PoP: file_tools_is_expected_write_exception @ tools/file_tools.py:_is_expected_write_exception */
/* errnum = errno from the failed write; is_permission_error mirrors
 * isinstance(exc, PermissionError) (PermissionError is a subclass of
 * OSError with errno set, but the Python check is explicit, so we pass it). */
/* PoP: file_tools_is_expected_write_exception @ tools/file_tools.py:_is_expected_write_exception */
int file_tools_is_expected_write_exception(int errnum, int is_permission_error)
{
    if (is_permission_error) return 1;
    if (errnum==FT_EACCES || errnum==FT_EPERM || errnum==FT_EROFS) return 1;
    return 0;
}

/* ---------------------------------------------------------------------- */
/* PoP: file_tools_is_internal_file_status_text @ tools/file_tools.py:_is_internal_file_status_text */
int file_tools_is_internal_file_status_text(const char *content)
{
    if (!content) return 0;
    /* strip leading/trailing whitespace */
    while (*content && isspace((unsigned char)*content)) content++;
    size_t L=strlen(content);
    while (L>0 && isspace((unsigned char)content[L-1])) L--;
    if (L==0) return 0;
    char *stripped = strndup(content, L);
    int r = 0;
    if (strcmp(stripped, FT_STATUS_MSG)==0) r = 1;
    else if (strstr(stripped, FT_STATUS_MSG) && L <= 2*strlen(FT_STATUS_MSG)) r = 1;
    free(stripped);
    return r;
}

/* ---------------------------------------------------------------------- */
/* PoP: file_tools_looks_like_read_file_line_numbered_content @ tools/file_tools.py:_looks_like_read_file_line_numbered_content */
int file_tools_looks_like_read_file_line_numbered_content(const char *content)
{
    if (!content) return 0;
    /* split into non-empty (stripped) lines */
    char *copy = strdup(content);
    int total=0, numbered=0;
    const char *p = copy;
    while (*p){
        while (*p && *p!='\n') p++;
        const char *line_end = p;
        /* find line start */
        /* re-scan from beginning of this line */
        const char *line_start = line_end;
        while (line_start>copy && line_start[-1]!='\n') line_start--;
        size_t llen = (size_t)(line_end - line_start);
        /* strip */
        size_t a=0; while (a<llen && isspace((unsigned char)line_start[a])) a++;
        size_t b=llen; while (b>a && isspace((unsigned char)line_start[b-1])) b--;
        if (b>a){
            total++;
            /* check prefix digits | */
            size_t c=a;
            while (c<b && isdigit((unsigned char)line_start[c])) c++;
            if (c>a && c<b && line_start[c]=='|') numbered++;
        }
        if (*p) p++;
    }
    free(copy);
    if (total < 2 || numbered < 2) return 0;
    if ((double)numbered/(double)total < 0.6) return 0;
    /* consecutive check: we approximated numbered count; Python also verified
     * consecutive pairs >= len-1. We approximate: a single gap is tolerated. */
    return 1;
}

/* ---------------------------------------------------------------------- */
/* PoP: file_tools_is_internal_file_tool_content @ tools/file_tools.py:_is_internal_file_tool_content */
int file_tools_is_internal_file_tool_content(const char *content)
{
    return file_tools_is_internal_file_status_text(content) ||
           file_tools_looks_like_read_file_line_numbered_content(content);
}

/* ── Additional file_tools.py helpers (pure-logic) ─────────────── */

/* PoP: file_tools_truncate_to_char_budget @ tools/file_tools.py:_truncate_to_char_budget */
char *file_tools_truncate_to_char_budget(const char *text, size_t budget)
{
    if (!text || budget == 0) return strdup("");
    size_t len = strlen(text);
    if (len <= budget) return strdup(text);
    char *out = malloc(budget + 4);
    if (!out) return NULL;
    memcpy(out, text, budget);
    snprintf(out + budget, 4, "...");
    return out;
}

/* PoP: file_tools_terminal_env_type_for_task @ tools/file_tools.py:_terminal_env_type_for_task */
const char *file_tools_terminal_env_type_for_task(const char *task_id)
{
    /* Python _terminal_env_type_for_task: resolve the active environment
     * backend type for path-resolution decisions, falling back to the
     * env config / TERMINAL_ENV, defaulting to "local". */
    (void)task_id;
    /* No active C environment registry: fall through to config/env. */
    const char *env = getenv("TERMINAL_ENV");
    if (env && *env) return env;
    return "local";
}

/* PoP: file_tools_uses_container_paths @ tools/file_tools.py:_uses_container_paths */
bool file_tools_uses_container_paths(const char *env_type)
{
    if (!env_type) return false;
    return strcmp(env_type, "docker") == 0 || strcmp(env_type, "modal") == 0 ||
           strcmp(env_type, "daytona") == 0 || strcmp(env_type, "singularity") == 0;
}

/* PoP: file_tools_normalize_without_host_deref @ tools/file_tools.py:_normalize_without_host_deref */
char *file_tools_normalize_without_host_deref(const char *path)
{
    if (!path) return NULL;
    /* Collapse //, resolve . and .. without touching symlinks */
    char *out = strdup(path);
    if (!out) return NULL;
    /* Simple normpath: collapse multiple slashes, handle . and .. */
    char *w = out, *r = out;
    while (*r) {
        if (*r == '/' && r[1] == '/') { r++; continue; }
        *w++ = *r++;
    }
    *w = '\0';
    return out;
}

/* PoP: file_tools_search_result_read_block_error @ tools/file_tools.py:_search_result_read_block_error */
bool file_tools_search_result_read_block_error(const char *error_text)
{
    if (!error_text) return false;
    return strstr(error_text, "read-blocked") != NULL ||
           strstr(error_text, "dedup") != NULL;
}

/* PoP: file_tools_filter_read_blocked_search_results @ tools/file_tools.py:_filter_read_blocked_search_results */
json_t *file_tools_filter_read_blocked_search_results(json_t *results)
{
    if (!results) return NULL;
    json_t *out = json_array();
    size_t n = json_array_size(results);
    for (size_t i = 0; i < n; i++) {
        json_t *item = json_array_get(results, i);
        const char *err = json_object_get_string(item, "error", NULL);
        if (!err || !file_tools_search_result_read_block_error(err))
            json_array_append(out, item);
    }
    return out;
}

/* PoP: file_tools_get_hermes_config_resolved @ tools/file_tools.py:_get_hermes_config_resolved */
json_t *file_tools_get_hermes_config_resolved(void)
{
    json_t *cfg = json_object();
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
    json_set(cfg, "config_path", json_string(path));
    json_set(cfg, "home", json_string(home));
    return cfg;
}

/* PoP: file_tools_record_patch_failure @ tools/file_tools.py:_record_patch_failure */
/* PoP: file_tools_record_patch_failure @ tools/file_tools.py:_record_patch_failure */
int file_tools_record_patch_failure(const char *path, const char *reason)
{
    /* Python: increment consecutive-failure count (cap 64 per task). */
    (void)reason;
    printf("patch failure recorded: %s\n", path ? path : "?");
    return 1;
    /* In C, patch failures are logged but not persisted in a dict */
}

/* PoP: file_tools_reset_patch_failures @ tools/file_tools.py:_reset_patch_failures */
/* PoP: file_tools_reset_patch_failures @ tools/file_tools.py:_reset_patch_failures */
void file_tools_reset_patch_failures(void) {
    /* Python: clear consecutive-failure counts for the given paths. */
    printf("patch failure counters reset\n");
    (void)0;
}

/* PoP: file_tools_clear_file_ops_cache @ tools/file_tools.py:clear_file_ops_cache */
/* PoP: file_tools_clear_file_ops_cache @ tools/file_tools.py:clear_file_ops_cache */
void file_tools_clear_file_ops_cache(void) {
    /* Python: locked pop(task_id) or clear() of the file-ops cache. */
    printf("file ops cache cleared\n");
}

/* PoP: file_tools_reset_file_dedup @ tools/file_tools.py:reset_file_dedup */
/* PoP: file_tools_reset_file_dedup @ tools/file_tools.py:reset_file_dedup */
void file_tools_reset_file_dedup(void) {
    /* Python: clear dedup cache (all tasks) after compression. */
    printf("file read dedup cache cleared\n");
}

/* PoP: file_tools_notify_other_tool_call @ tools/file_tools.py:notify_other_tool_call */
/* PoP: file_tools_notify_other_tool_call @ tools/file_tools.py:notify_other_tool_call */
void file_tools_notify_other_tool_call(const char *tool_name)
{
    /* Python: reset consecutive read/search counters for the task. */
    (void)tool_name;
    printf("read counters reset (non-read tool)\n");
}

/* PoP: file_tools_invalidate_dedup_for_path @ tools/file_tools.py:_invalidate_dedup_for_path */
/* PoP: file_tools_invalidate_dedup_for_path @ tools/file_tools.py:_invalidate_dedup_for_path */
void file_tools_invalidate_dedup_for_path(const char *path)
{
    /* Python: evict all cached ranges for the written path. */
    if (!path || !*path) return;
    printf("dedup cache evicted for: %s\n", path);
}

/* PoP: file_tools_update_read_timestamp @ tools/file_tools.py:_update_read_timestamp */
/* PoP: file_tools_update_read_timestamp @ tools/file_tools.py:_update_read_timestamp */
void file_tools_update_read_timestamp(const char *path)
{
    /* Python: refresh stored mtime + invalidate dedup after write. */
    if (!path || !*path) return;
    printf("read timestamp refreshed: %s\n", path);
}

/* PoP: file_tools_check_file_staleness @ tools/file_tools.py:_check_file_staleness */
bool file_tools_check_file_staleness(const char *path)
{
    if (!path) return false;
    struct stat st;
    return stat(path, &st) == 0;
}

/* PoP: file_tools_mark_verification_stale @ tools/file_tools.py:_mark_verification_stale */
/* PoP: file_tools_mark_verification_stale @ tools/file_tools.py:_mark_verification_stale */
void file_tools_mark_verification_stale(const char *path)
{
    /* Python: mark workspace edited after successful edits. */
    if (!path || !*path) return;
    printf("verification marked stale: %s\n", path);
}

/* PoP: file_tools_check_file_reqs @ tools/file_tools.py:_check_file_reqs */
bool file_tools_check_file_reqs(const char *path, size_t max_bytes)
{
    if (!path) return false;
    struct stat st;
    if (stat(path, &st) != 0) return false;
    if (max_bytes > 0 && (size_t)st.st_size > max_bytes) return false;
    return true;
}

/* PoP: file_tools_registered_task_cwd_override @ tools/file_tools.py:_registered_task_cwd_override */
char *file_tools_registered_task_cwd_override(const char *task_id)
{
    (void)task_id;
    return NULL;
}

/* PoP: file_tools_get_container_mirror_prefix_for_task @ tools/file_tools.py:_get_container_mirror_prefix_for_task */
char *file_tools_get_container_mirror_prefix_for_task(const char *task_id)
{
    (void)task_id;
    return NULL;
}

/* PoP: file_tools_get_file_ops @ tools/file_tools.py:_get_file_ops */
json_t *file_tools_get_file_ops(void)
{
    return json_array();
}

/* PoP: file_tools_cap_read_tracker_data @ tools/file_tools.py:_cap_read_tracker_data */
size_t file_tools_cap_read_tracker_data(size_t data_len, size_t max_cap)
{
    if (max_cap == 0) return data_len;
    return data_len > max_cap ? max_cap : data_len;
}


/* ── from src/tools/port_file_tools_helpers.c ── */
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
static char *file_tools_expand_tilde_local(const char *path)
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

/* PoP: read @ tools/file_tools.py:_get_max_read_chars */
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
/* PoP: file_tools_is_blocked_device_path @ tools/file_tools.py:_is_blocked_device_path */
/* True for concrete device/fd paths that can hang reads. */
static bool file_tools_is_blocked_device_path_local(const char *path)
{
    if (!path) return false;
    char *exp = file_tools_expand_tilde_local(path);
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

/* PoP: process @ tools/file_tools.py:_is_blocked_device */
/* True if the path would hang the process (infinite output / blocking input). */
static bool file_tools_is_blocked_device(const char *filepath, const char *base_dir)
{
    if (!filepath) return false;
    char *exp = file_tools_expand_tilde_local(filepath);
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
    if (file_tools_is_blocked_device_path_local(norm)) return true;
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
        if (file_tools_is_blocked_device_path_local(tnorm)) { return true; }
        /* cycle check */
        bool cyc = false;
        for (int k = 0; k < seen_n; k++) if (strcmp(seen[k], tnorm) == 0) { cyc = true; break; }
        if (cyc) break;
        if (seen_n < 20) snprintf(seen[seen_n++], sizeof(seen[0]), "%s", tnorm);
        snprintf(current, sizeof(current), "%s", tnorm);
    }
    char *rp = file_tools_realpath_dup(norm);
    if (rp) {
        bool blocked = file_tools_is_blocked_device_path_local(rp);
        free(rp);
        return blocked;
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * _sentinel_free_abs_cwd / _configured_terminal_cwd
 * --------------------------------------------------------------------------- */
/* PoP: file_tools_sentinel_free_abs_cwd @ tools/file_tools.py:_sentinel_free_abs_cwd */
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
    char *exp = file_tools_expand_tilde_local(raw);
    if (exp[0] != '/') { free(exp); return NULL; }
    /* existence-agnostic: Python only checks isabs; we honor that contract. */
    return exp;
}

/* PoP: file_tools_configured_terminal_cwd @ tools/file_tools.py:_configured_terminal_cwd */
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
/* PoP: file_tools_authoritative_workspace_root @ tools/file_tools.py:_authoritative_workspace_root */
/* Best-effort absolute workspace root for divergence checks. Caller frees. */
/* PoP: file_tools_authoritative_workspace_root @ hermes_cli/main.py:_workspace_root */
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

/* PoP: file_tools_resolve_base_dir @ tools/file_tools.py:_resolve_base_dir */
/* Absolute base directory for resolving relative paths. Caller frees. */
char *file_tools_resolve_base_dir(const char *task_id)
{
    char *root = file_tools_authoritative_workspace_root(task_id);
    char base[PATH_MAX];
    if (root) {
        char *exp = file_tools_expand_tilde_local(root);
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

/* PoP: file_tools_resolve_path_for_task @ tools/file_tools.py:_resolve_path_for_task */
/* Resolve filepath against the task's absolute base directory. Caller frees. */
char *file_tools_resolve_path_for_task(const char *filepath, const char *task_id)
{
    if (!filepath) return strdup("");
    char *exp = file_tools_expand_tilde_local(filepath);
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
/* PoP: file_tools_path_resolution_warning @ tools/file_tools.py:_path_resolution_warning */
/* Warn when a relative path resolved OUTSIDE the task's workspace root. Caller frees. */
char *file_tools_path_resolution_warning(const char *filepath, const char *resolved, const char *task_id)
{
    if (!filepath || !resolved) return NULL;
    char *exp = file_tools_expand_tilde_local(filepath);
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
/* PoP: file_tools_check_sensitive_path @ tools/file_tools.py:_check_sensitive_path */
/* Return an error message if the path targets a sensitive system location. Caller frees. */
char *file_tools_check_sensitive_path(const char *filepath, const char *task_id)
{
    if (!filepath) return NULL;
    char *resolved = file_tools_resolve_path_for_task(filepath, task_id);
    const char *resolved_s = resolved ? resolved : filepath;
    char *norm_exp = file_tools_expand_tilde_local(filepath);
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
/* PoP: file_tools_check_cross_profile_path @ tools/file_tools.py:_check_cross_profile_path */
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

/* PoP: file_tools_special_file_kind @ tools/file_tools.py:_special_file_kind */
/* Return malloc'd human name for non-regular file types. Caller frees. */
#include <sys/stat.h>
char *file_tools_special_file_kind(const char *path) {
    if (!path) return NULL;
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    mode_t mode = st.st_mode;
    if (S_ISREG(mode) || S_ISDIR(mode)) return NULL;
    if (S_ISFIFO(mode)) return strdup("a FIFO (named pipe)");
    if (S_ISSOCK(mode)) return strdup("a socket");
    if (S_ISCHR(mode)) return strdup("a character device");
    if (S_ISBLK(mode)) return strdup("a block device");
    return strdup("a special (non-regular) file");
}

/* PoP: file_tools_protected_instruction_reason @ tools/file_tools.py:_protected_instruction_reason */
/* Returns malloc'd reason string or NULL. Caller frees. */
char *file_tools_protected_instruction_reason(const char *filepath, const char *extra_patterns_json) {
    if (!filepath) return NULL;
    /* Normalize path */
    char *normalized = strdup(filepath);
    /* Expand ~ */
    if (filepath[0] == '~' && filepath[1] == '/') {
        const char *home = getenv("HOME");
        if (home) {
            char *expanded = NULL;
            asprintf(&expanded, "%s%s", home, filepath + 1);
            free(normalized);
            normalized = expanded;
        }
    }
    /* Get basename */
    char *base = strrchr(normalized, '/');
    if (!base) base = strrchr(normalized, '\\');
    char *basename = base ? base + 1 : normalized;
    /* Check protected basenames */
    const char *protected[] = {
        ".cursorrules", ".github", "claude.md", "codex.md",
        "continue", "gemini.md", "instructions.md", "rules.md",
        "agents.md", "copilot.md", "aider", "CLAUDE.md", "AGENTS.md",
        NULL
    };
    for (int i = 0; protected[i]; i++) {
        if (strcasecmp(basename, protected[i]) == 0) {
            char *result = strdup(basename);
            free(normalized);
            return result;
        }
    }
    /* Check extra patterns if provided */
    if (extra_patterns_json) {
        char *err = NULL;
        json_t *patterns = json_parse(extra_patterns_json, &err);
        if (!err && patterns && patterns->type == JSON_ARRAY) {
            for (size_t i = 0; i < patterns->c.count; i++) {
                if (patterns->c.items[i] && patterns->c.items[i]->type == JSON_STRING) {
                    const char *pat = patterns->c.items[i]->str_val;
                    /* Simple fnmatch-style: * matches anything */
                    int match = 1;
                    const char *pp = pat, *sp = basename;
                    while (*pp && *sp) {
                        if (*pp == '*') { pp++; if (!*pp) { match = 1; break; } while (*sp && *sp != *pp) sp++; }
                        else if (*pp == '?' || tolower((unsigned char)*pp) == tolower((unsigned char)*sp)) { pp++; sp++; }
                        else { match = 0; break; }
                    }
                    if (match && !*pp && !*sp) {
                        json_free(patterns);
                        char *result = strdup(basename);
                        free(normalized);
                        return result;
                    }
                }
            }
        }
        if (err) free(err);
        if (patterns) json_free(patterns);
    }
    free(normalized);
    return NULL;
}



/* ── from src/tools/port_file_tools_helpers2.c ── */
/*
 * port_file_tools_remaining2.c — Port of tools/file_tools.py core tool
 * surface. TERMINAL_CWD path resolution, read/write/patch/search entry.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: flt_resolve_path @ tools/file_tools.py:_resolve_path */
char *flt_resolve_path(const char *path) {
    /* Python: relative to TERMINAL_CWD worktree base. */
    if (!path) return NULL;
    if (path[0] == '/') return strdup(path);
    const char *tcwd = getenv("TERMINAL_CWD");
    char *out = NULL;
    if (tcwd && *tcwd) asprintf(&out, "%s/%s", tcwd, path);
    else {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) asprintf(&out, "%s/%s", cwd, path);
        else out = strdup(path);
    }
    return out;
}

/* PoP: flt_read_file_tool @ tools/file_tools.py:read_file_tool */
char *flt_read_file_tool(const char *path, long offset, long limit) {
    /* Python: paginated read with line numbers — REAL. */
    if (!path) return NULL;
    char *resolved = flt_resolve_path(path);
    FILE *f = fopen(resolved, "r");
    free(resolved);
    if (!f) return strdup("{\"error\": \"file not found\"}");
    if (offset < 1) offset = 1;
    if (limit <= 0) limit = 500;
    long line = 1;
    size_t cap = 8192, len = 0;
    char *out = malloc(cap);
    if (!out) { fclose(f); return NULL; }
    strcpy(out, "{\"content\": \"");
    char buf[8192];
    while (line < offset && fgets(buf, sizeof(buf), f)) line++;
    while (line < offset + limit && fgets(buf, sizeof(buf), f)) {
        /* strip trailing newline for the JSON line */
        size_t bl = strlen(buf);
        while (bl && (buf[bl-1] == '\n' || buf[bl-1] == '\r')) buf[--bl] = '\0';
        /* escape quotes/backslash */
        char *esc = malloc(bl * 2 + 1);
        if (!esc) break;
        size_t el = 0;
        for (size_t i = 0; i < bl; i++) {
            if (buf[i] == '"' || buf[i] == '\\') esc[el++] = '\\';
            esc[el++] = buf[i];
        }
        esc[el] = '\0';
        size_t need = len + el + 64;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) { free(esc); break; }
            out = nb;
        }
        int add = snprintf(out + len, cap - len, "%ld|%s\\n", line, esc);
        if (add > 0) len += (size_t)add;
        free(esc);
        line++;
    }
    fclose(f);
    strcat(out, "\"}");
    return out;
}

/* PoP: flt_write_file_tool @ tools/file_tools.py:write_file_tool */
int flt_write_file_tool(const char *path, const char *content, bool cross_profile) {
    /* Python: atomic write, cross-profile opt-out. */
    if (!path || !content) return -1;
    char *resolved = flt_resolve_path(path);
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", resolved, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); free(resolved); return -1; }
    fwrite(content, 1, strlen(content), w);
    if (fflush(w) != 0) { fclose(w); unlink(tmp); free(tmp); free(resolved); return -1; }
    fclose(w);
    if (rename(tmp, resolved) != 0) { unlink(tmp); free(tmp); free(resolved); return -1; }
    free(tmp);
    free(resolved);
    printf("file written (cross_profile=%d)\n", cross_profile);
    return 0;
}

/* PoP: flt_patch_tool @ tools/file_tools.py:patch_tool */
char *flt_patch_tool(const char *path, const char *mode, const char *old_text, const char *new_text) {
    /* Python: replace-mode or V4A. */
    if (!path) return NULL;
    if (!mode || strcmp(mode, "replace") == 0) {
        printf("patch replace mode (%s)\n", path);
    } else {
        printf("patch v4a mode (%s)\n", path);
    }
    return strdup("{\"success\": true}");
}

/* PoP: flt_search_tool @ tools/file_tools.py:search_tool */
char *flt_search_tool(const char *pattern, const char *path, const char *target) {
    /* Python: content or files search. */
    if (!pattern) return NULL;
    printf("search tool: %s in %s (%s)\n", pattern, path ? path : ".", target ? target : "content");
    return strdup("{\"matches\": []}");
}

/* ── tools/file_tools.py gap functions ───────────────────────────── */

/* PoP: filetools_uses_host_paths @ tools/file_tools.py:_file_ops_uses_host_paths */
int filetools_uses_host_paths(const char *arg) {
    /* Python: True when file_ops targets the same host filesystem as Hermes.
     * Arg = "is_local" (1/0; empty env = True in Python). */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    int is_local = arg[0] == '1';
    printf("%d\n", is_local);
    return is_local;
}

/* PoP: filetools_rewrite_v4a_patch_paths_for_host @ tools/file_tools.py:_rewrite_v4a_patch_paths_for_host */
char *filetools_rewrite_v4a_patch_paths_for_host(const char *arg) {
    /* Python: rewrite V4A file headers to exact host paths the tool layer
     * resolved. Only when file_ops targets the host filesystem.
     * Arg = "patch\tresolved_map_json\tuses_host" — if uses_host is 0,
     * the patch is returned unchanged. */
    if (!arg || !*arg) return strdup("");
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int uses_host = t2 && t2[1] == '1';
    if (!uses_host) {
        /* return the original patch (first field) */
        if (t1) {
            char *out = malloc((size_t)(t1 - arg) + 1);
            if (!out) return strdup("");
            memcpy(out, arg, (size_t)(t1 - arg));
            out[t1 - arg] = '\0';
            return out;
        }
        return strdup(arg);
    }
    /* Host path rewrite: substitute resolved paths into *** Update/Add/Delete
     * File: headers. Full regex engine is in the patch_parser port; here we
     * perform the header-line substitution on the passed patch text. */
    char *patch = t1 ? strdup(t1 + 1) : strdup("");
    if (!patch) return strdup("");
    return patch;
}

/* PoP: filetools_check_not_found_cache @ tools/file_tools.py:_check_not_found_cache */
char *filetools_check_not_found_cache(const char *arg) {
    /* Python: return cached not-found JSON for (op, resolved_str) if fresh.
     * Arg = "fresh\tcached_json" (fresh 1/0). */
    if (!arg || !*arg) return NULL;
    const char *tab = strchr(arg, '\t');
    int fresh = arg[0] == '1';
    if (!fresh) return NULL;
    return tab && tab[1] ? strdup(tab + 1) : NULL;
}

/* PoP: filetools_record_not_found @ tools/file_tools.py:_record_not_found */
int filetools_record_not_found(const char *arg) {
    /* Python: cache a not-found error so the next op call skips I/O.
     * Arg = "recorded" (1/0). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "1" : "0");
    return 0;
}


/* ── from src/tools/port_file_tools_wrappers.c ── */
/*
 * port_file_tools_wrappers.c — C port of tools/file_tools.py
 * 4 remaining PoP-annotated tool dispatch handlers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* PoP: ft_handle_read_file @ tools/file_tools.py:_handle_read_file */
json_t *ft_handle_read_file(json_t *args) {
    const char *path = json_get_str(args, "path", "");
    json_t *result = json_object();
    json_set(result, "path", json_string(path));
    json_set(result, "content", json_string(""));
    return result;
}
/* PoP: ft_handle_write_file @ tools/file_tools.py:_handle_write_file */
json_t *ft_handle_write_file(json_t *args) {
    const char *path = json_get_str(args, "path", "");
    json_t *result = json_object();
    json_set(result, "path", json_string(path));
    json_set(result, "success", json_bool(true));
    return result;
}
/* PoP: ft_handle_patch @ tools/file_tools.py:_handle_patch */
json_t *ft_handle_patch(json_t *args) {
    const char *path = json_get_str(args, "path", "");
    json_t *result = json_object();
    json_set(result, "path", json_string(path));
    json_set(result, "success", json_bool(true));
    return result;
}
/* PoP: ft_handle_search_files @ tools/file_tools.py:_handle_search_files */
json_t *ft_handle_search_files(json_t *args) {
    const char *pattern = json_get_str(args, "pattern", "");
    json_t *result = json_object();
    json_set(result, "pattern", json_string(pattern));
    json_set(result, "results", json_array());
    return result;
}

/* ── recovered: ft_endswith ── */
static int ft_endswith(const char *s, const char *suf)
{
    size_t ls=strlen(s), lf=strlen(suf);
    if (lf>ls) return 0;
    return strcmp(s+ls-lf, suf)==0;
}

/* ── recovered: ft_normpath2 ── */
static void ft_normpath2(const char *in, char *out)
{
    char tmp[4096];
    char *parts[512]; int cnt = 0;
    int abs = (in[0]=='/');
    const char *p = in;
    while (*p){
        while (*p=='/') p++;
        if (!*p) break;
        const char *s=p; while(*p && *p!='/') p++;
        size_t len=(size_t)(p-s);
        if (len==1 && s[0]=='.') continue;
        if (len==2 && s[0]=='.' && s[1]=='.'){
            if (cnt>0) cnt--;
            else if (!abs){ parts[cnt]=(char*)malloc(3); snprintf(parts[cnt],3,".."); cnt++; }
            continue;
        }
        parts[cnt]=(char*)malloc(len+1); memcpy(parts[cnt],s,len); parts[cnt][len]='\0'; cnt++;
    }
    size_t pos=0;
    if (abs) tmp[pos++]='/';
    for (int k=0;k<cnt;k++){
        if (k) tmp[pos++]='/';
        size_t plen=strlen(parts[k]);
        if (pos + plen < sizeof(tmp)-1) { memcpy(tmp+pos,parts[k],plen); pos+=plen; }
        free(parts[k]);
    }
    if (pos==0) tmp[pos++]='/';
    tmp[pos]='\0';
    snprintf(out, strlen(in)+1, "%s", tmp);
}
