/*
 * web_git_util.c - web-git path / status classification helpers.
 *
 * Split from port_web_git.c (soul.md: self-contained modules). Path
 * resolution and git-status letter classification. No file-scope state.
 */
#include "web_git_util.h"
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


/* Trim a single trailing newline in place; returns the (possibly shortened)
 * string. Safe to call on the git_out buffers. */
/* PoP: trim_nl @ hermes_cli/web_git.py:_trim_nl */ char *trim_nl(char *s) {
    if (!s) return s;
    size_t L = strlen(s);
    while (L > 0 && (s[L-1] == '\n' || s[L-1] == '\r')) s[--L] = '\0';
    return s;
}


/* PoP: is_dir @ hermes_cli/web_git.py:_is_dir */ bool is_dir(const char *cwd) {
    struct stat st;
    if (stat(cwd, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}


/* ── porcelain-v2 -z walking ──────────────────────────────────────────── */

/* Resolve "old => new" / "dir/{old => new}/f" to the NEW path (Python
 * resolve_rename_path). Caller frees. */
/* PoP: resolve_rename_path @ hermes_cli/web_git.py:resolve_rename_path */ char *resolve_rename_path(const char *raw) {
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


/* PoP: entry_staged @ hermes_cli/web_git.py:_entry_staged */ bool entry_staged(const char *tag, const char *xy) {
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


/* PoP: status_letter @ hermes_cli/web_git.py:_status_letter */ char status_letter(const char *tag, const char *xy) {
    char t = tag ? tag[0] : '\0';
    if (t == '?') return '?';
    if (t == 'u') return 'U';
    char code = xy && xy[0] != '.' ? xy[0] : (xy && strlen(xy) > 1 ? xy[1] : '.');
    if (code == '.') code = 'M';
    return (char)toupper((unsigned char)code);
}


/* Adapter so walk_entries (entry_cb with void* ud) can classify into an array. */ void classify_cb_adapter(const char *tag, const char *xy, const char *path, void *ud) {
    json_t *arr = (json_t *)ud;
    json_append(arr, classify(tag, xy, path));
}
