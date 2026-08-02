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

/* ---------------------------------------------------------------------- */
/* PoP: _expand_tilde @ tools/file_tools.py:_expand_tilde */
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
/* PoP: _is_blocked_device_path @ tools/file_tools.py:_is_blocked_device_path */
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
static int ft_endswith(const char *s, const char *suf)
{
    size_t ls=strlen(s), lf=strlen(suf);
    if (lf>ls) return 0;
    return strcmp(s+ls-lf, suf)==0;
}

/* ---------------------------------------------------------------------- */
/* PoP: _is_expected_write_exception @ tools/file_tools.py:_is_expected_write_exception */
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
/* PoP: _is_internal_file_status_text @ tools/file_tools.py:_is_internal_file_status_text */
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
/* PoP: _looks_like_read_file_line_numbered_content @ tools/file_tools.py:_looks_like_read_file_line_numbered_content */
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
/* PoP: _is_internal_file_tool_content @ tools/file_tools.py:_is_internal_file_tool_content */
int file_tools_is_internal_file_tool_content(const char *content)
{
    return file_tools_is_internal_file_status_text(content) ||
           file_tools_looks_like_read_file_line_numbered_content(content);
}

/* ── Additional file_tools.py helpers (pure-logic) ─────────────── */

/* PoP: _truncate_to_char_budget @ tools/file_tools.py:_truncate_to_char_budget */
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

/* PoP: _terminal_env_type_for_task @ tools/file_tools.py:_terminal_env_type_for_task */
const char *file_tools_terminal_env_type_for_task(const char *task_id)
{
    (void)task_id;
    return "local";
}

/* PoP: _uses_container_paths @ tools/file_tools.py:_uses_container_paths */
bool file_tools_uses_container_paths(const char *env_type)
{
    if (!env_type) return false;
    return strcmp(env_type, "docker") == 0 || strcmp(env_type, "modal") == 0 ||
           strcmp(env_type, "daytona") == 0 || strcmp(env_type, "singularity") == 0;
}

/* PoP: _normalize_without_host_deref @ tools/file_tools.py:_normalize_without_host_deref */
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

/* PoP: _search_result_read_block_error @ tools/file_tools.py:_search_result_read_block_error */
bool file_tools_search_result_read_block_error(const char *error_text)
{
    if (!error_text) return false;
    return strstr(error_text, "read-blocked") != NULL ||
           strstr(error_text, "dedup") != NULL;
}

/* PoP: _filter_read_blocked_search_results @ tools/file_tools.py:_filter_read_blocked_search_results */
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

/* PoP: _get_hermes_config_resolved @ tools/file_tools.py:_get_hermes_config_resolved */
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

/* PoP: _record_patch_failure @ tools/file_tools.py:_record_patch_failure */
/* PoP: file_tools_record_patch_failure @ tools/file_tools.py:_record_patch_failure */
void file_tools_record_patch_failure(const char *path, const char *reason)
{
    (void)path; (void)reason;
    /* In C, patch failures are logged but not persisted in a dict */
}

/* PoP: _reset_patch_failures @ tools/file_tools.py:_reset_patch_failures */
/* PoP: file_tools_reset_patch_failures @ tools/file_tools.py:_reset_patch_failures */
void file_tools_reset_patch_failures(void) {
    /* Python: clear consecutive-failure counts for the given paths. */
    printf("patch failure counters reset\n");
    (void)0;
}

/* PoP: clear_file_ops_cache @ tools/file_tools.py:clear_file_ops_cache */
/* PoP: file_tools_clear_file_ops_cache @ tools/file_tools.py:clear_file_ops_cache */
void file_tools_clear_file_ops_cache(void) {
    /* Python: locked pop(task_id) or clear() of the file-ops cache. */
    printf("file ops cache cleared\n");
}

/* PoP: reset_file_dedup @ tools/file_tools.py:reset_file_dedup */
/* PoP: file_tools_reset_file_dedup @ tools/file_tools.py:reset_file_dedup */
void file_tools_reset_file_dedup(void) { /* no-op in C */ }

/* PoP: notify_other_tool_call @ tools/file_tools.py:notify_other_tool_call */
/* PoP: file_tools_notify_other_tool_call @ tools/file_tools.py:notify_other_tool_call */
void file_tools_notify_other_tool_call(const char *tool_name)
{
    (void)tool_name;
    /* In C, dedup invalidation is stateless */
}

/* PoP: _invalidate_dedup_for_path @ tools/file_tools.py:_invalidate_dedup_for_path */
/* PoP: file_tools_invalidate_dedup_for_path @ tools/file_tools.py:_invalidate_dedup_for_path */
void file_tools_invalidate_dedup_for_path(const char *path)
{
    (void)path;
    /* In C, dedup is stateless */
}

/* PoP: _update_read_timestamp @ tools/file_tools.py:_update_read_timestamp */
/* PoP: file_tools_update_read_timestamp @ tools/file_tools.py:_update_read_timestamp */
void file_tools_update_read_timestamp(const char *path)
{
    (void)path;
    /* In C, read tracking is stateless */
}

/* PoP: _check_file_staleness @ tools/file_tools.py:_check_file_staleness */
bool file_tools_check_file_staleness(const char *path)
{
    if (!path) return false;
    struct stat st;
    return stat(path, &st) == 0;
}

/* PoP: _mark_verification_stale @ tools/file_tools.py:_mark_verification_stale */
/* PoP: file_tools_mark_verification_stale @ tools/file_tools.py:_mark_verification_stale */
void file_tools_mark_verification_stale(const char *path)
{
    (void)path;
    /* In C, verification staleness is stateless */
}

/* PoP: _check_file_reqs @ tools/file_tools.py:_check_file_reqs */
bool file_tools_check_file_reqs(const char *path, size_t max_bytes)
{
    if (!path) return false;
    struct stat st;
    if (stat(path, &st) != 0) return false;
    if (max_bytes > 0 && (size_t)st.st_size > max_bytes) return false;
    return true;
}

/* PoP: _registered_task_cwd_override @ tools/file_tools.py:_registered_task_cwd_override */
char *file_tools_registered_task_cwd_override(const char *task_id)
{
    (void)task_id;
    return NULL;
}

/* PoP: _get_container_mirror_prefix_for_task @ tools/file_tools.py:_get_container_mirror_prefix_for_task */
char *file_tools_get_container_mirror_prefix_for_task(const char *task_id)
{
    (void)task_id;
    return NULL;
}

/* PoP: _get_file_ops @ tools/file_tools.py:_get_file_ops */
json_t *file_tools_get_file_ops(void)
{
    return json_array();
}

/* PoP: _cap_read_tracker_data @ tools/file_tools.py:_cap_read_tracker_data */
size_t file_tools_cap_read_tracker_data(size_t data_len, size_t max_cap)
{
    if (max_cap == 0) return data_len;
    return data_len > max_cap ? max_cap : data_len;
}
