/*
 * restart_watcher_env.c — faithful C11 port of the detached restart shell
 * line and PYTHONPATH rewrite in gateway/restart_watcher_env.py
 * (gate w14_restartwat_1fbad5).
 * PoP: w14_restartwat_1fbad5_detached_restart_shell @ gateway/restart_watcher_env.py:detached_restart_shell
 * PoP: w14_restartwat_1fbad5_keep_importable @ gateway/restart_watcher_env.py:keep_running_checkout_importable
 */
#include "noc_w14_restartwat_1fbad5.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *buf; size_t cap, n; } SB;
static void sb_init(SB *s) {
    s->cap = 256; s->n = 0; s->buf = (char *)malloc(s->cap); s->buf[0] = '\0';
}
static void sb_add(SB *s, const char *x) {
    size_t l = strlen(x);
    while (s->n + l + 1 > s->cap) { s->cap *= 2; s->buf = (char *)realloc(s->buf, s->cap); }
    memcpy(s->buf + s->n, x, l);
    s->n += l; s->buf[s->n] = '\0'; /* always NUL at the current length */
}
static char *sb_take(SB *s) { return s->buf; }

/* str(Path(p)) on Windows: forward slashes become backslashes, redundant
 * separators and "." components are collapsed, empty becomes ".". */
static char *py_path_str(const char *p) {
    if (!p || !*p) return strdup(".");
    char *s = strdup(p);
    for (char *q = s; *q; ++q) if (*q == '/') *q = '\\';

    char drive[3] = {0};
    size_t i = 0;
    if (s[0] && s[1] == ':' && isalpha((unsigned char)s[0])) {
        drive[0] = s[0]; drive[1] = ':'; drive[2] = '\0';
        i = 2;
    }
    char anchor[3] = {0};
    if (s[i] == '\\') {
        size_t j = i;
        while (s[j] == '\\') j++;
        if (!drive[0] && j - i >= 2) { anchor[0] = '\\'; anchor[1] = '\\'; anchor[2] = '\0'; }
        else { anchor[0] = '\\'; anchor[1] = '\0'; }
        i = j;
    }

    SB out; sb_init(&out);
    sb_add(&out, drive);
    sb_add(&out, anchor);
    int have = 0;
    while (i <= strlen(s)) {
        size_t start = i;
        while (s[i] && s[i] != '\\') i++;
        size_t l = i - start;
        if (l == 1 && s[start] == '.') { /* skip "." component */ }
        else if (l > 0) {
            if (have) sb_add(&out, "\\");
            char seg[4096];
            size_t cp = l < sizeof(seg) - 1 ? l : sizeof(seg) - 1;
            memcpy(seg, s + start, cp); seg[cp] = '\0';
            sb_add(&out, seg);
            have = 1;
        }
        if (!s[i]) break;
        i++;
    }
    free(s);
    if (!have && !drive[0] && !anchor[0]) { free(out.buf); return strdup("."); }
    return sb_take(&out);
}

/* shlex.quote(s) for POSIX shells. */
static char *shlex_quote(const char *s) {
    if (!*s) return strdup("''");
    int safe = 1;
    for (const char *p = s; *p; ++p) {
        if (!(isalnum((unsigned char)*p) || strchr("_@%+=:,./-", *p))) { safe = 0; break; }
    }
    if (safe) return strdup(s);
    SB out; sb_init(&out);
    sb_add(&out, "'");
    for (const char *p = s; *p; ++p) {
        if (*p == '\'') sb_add(&out, "'\"'\"'");
        else { char c[2] = {*p, '\0'}; sb_add(&out, c); }
    }
    sb_add(&out, "'");
    return sb_take(&out);
}

json_t *w14_restartwat_1fbad5_detached_restart_shell(const char *cmd,
                                                     const char *current_pid,
                                                     double restart_after_s,
                                                     const char *log_path) {
    const char *c = cmd ? cmd : "";
    const char *pid = current_pid ? current_pid : "";
    SB out; sb_init(&out);

    char wait[1024];
    snprintf(wait, sizeof(wait),
             "deadline=$(( $(date +%%s) + %lld )); "
             "while kill -0 %s 2>/dev/null && [ $(date +%%s) -lt $deadline ]; do sleep 0.2; done; ",
             (long long)(int)restart_after_s, pid);
    sb_add(&out, wait);

    if (log_path == NULL) {
        char tail[4096];
        snprintf(tail, sizeof(tail), "%s gateway restart", c);
        sb_add(&out, tail);
    } else {
        char *path_str = py_path_str(log_path);
        char *log_arg = shlex_quote(path_str);
        free(path_str);
        char part[8192];
        snprintf(part, sizeof(part),
                 "if : >> %s 2>/dev/null; then %s gateway restart >> %s 2>&1; "
                 "echo \"detached restart exited with status $?\" >> %s; "
                 "else %s gateway restart; fi",
                 log_arg, c, log_arg, log_arg, c);
        sb_add(&out, part);
        free(log_arg);
    }
    json_t *ret = json_string(out.buf);
    free(out.buf);
    return ret;
}

/* os.pathsep on the Windows host running the live code is ';'. */
json_t *w14_restartwat_1fbad5_keep_importable(const char *pythonpath,
                                              const char *root) {
    const char *pp = pythonpath ? pythonpath : "";
    if (!root || !*root) return json_string(pp);

    int present = 0;
    const char *start = pp;
    for (const char *p = pp;; ++p) {
        if (*p == ';' || *p == '\0') {
            size_t l = (size_t)(p - start);
            if (l && strlen(root) == l && !strncmp(start, root, l)) { present = 1; break; }
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    if (present) return json_string(pp);

    SB out; sb_init(&out);
    sb_add(&out, root);
    start = pp;
    for (const char *p = pp;; ++p) {
        if (*p == ';' || *p == '\0') {
            size_t l = (size_t)(p - start);
            if (l) {
                char seg[4096];
                size_t cp = l < sizeof(seg) - 1 ? l : sizeof(seg) - 1;
                memcpy(seg, start, cp); seg[cp] = '\0'; /* NUL at new length */
                sb_add(&out, ";");
                sb_add(&out, seg);
            }
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    json_t *ret = json_string(out.buf);
    free(out.buf);
    return ret;
}
