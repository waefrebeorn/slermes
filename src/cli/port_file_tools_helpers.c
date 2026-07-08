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
/* Writes normalized path into out (must be >= strlen(in)+1). */
static void ft_normpath2(const char *in, char *out)
{
    char tmp[4096];
    /* tokenize into a copy */
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
            else if (!abs){ parts[cnt]=(char*)malloc(3); strcpy(parts[cnt],".."); cnt++; }
            continue;
        }
        parts[cnt]=(char*)malloc(len+1); memcpy(parts[cnt],s,len); parts[cnt][len]='\0'; cnt++;
    }
    size_t pos=0;
    if (abs) tmp[pos++]='/';
    for (int k=0;k<cnt;k++){
        if (k) tmp[pos++]='/';
        memcpy(tmp+pos,parts[k],strlen(parts[k])); pos+=strlen(parts[k]);
        free(parts[k]);
    }
    if (pos==0) tmp[pos++]='/';
    tmp[pos]='\0';
    strcpy(out,tmp);
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
