/*
 * worktree_ops.c — C home for upstream Hermes hermes_cli/worktree_ops.py.
 *
 * PoP: w9_worktreeops_2fb4e8_normalize_git_bash_path @ hermes_cli/worktree_ops.py:_normalize_git_bash_path
 * PoP: w9_worktreeops_2fb4e8_prune_tier_hours @ hermes_cli/worktree_ops.py:prune_tier_hours
 * PoP: w9_worktreeops_2fb4e8_path_is_within_root @ hermes_cli/worktree_ops.py:_path_is_within_root
 *
 * Only the pure path/string decision layers are ported; the git subprocess and
 * filesystem layers stay in Python and are never called by these exports.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w9_worktreeops_2fb4e8.h"

struct json_t *w9_worktreeops_2fb4e8_normalize_git_bash_path(const char *p) {
    if (!p || !*p) return json_string(p ? p : "");
    /* Regex: ^/(?:(?:cygdrive|mnt)/)?([a-zA-Z])/(.*)$ */
    const char *s = p;
    if (*s != '/') return json_string(p);
    s++;
    if (!strncmp(s, "cygdrive/", 9)) s += 9;
    else if (!strncmp(s, "mnt/", 4)) s += 4;
    if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))) return json_string(p);
    if (s[1] != '/') return json_string(p);
    const char *rest = s + 2;
    char out[8192];
    size_t n = 0;
    out[n++] = (char)toupper((unsigned char)*s);
    out[n++] = ':';
    out[n++] = '\\';
    for (const char *q = rest; *q && n + 1 < sizeof(out); q++)
        out[n++] = (*q == '/') ? '\\' : *q;
    out[n] = '\0';
    return json_string(out);
}

struct json_t *w9_worktreeops_2fb4e8_prune_tier_hours(const char *name,
                                                     double max_age_hours) {
    int auto_tree = name && (!strncmp(name, "hermes-", 7) ||
                             !strncmp(name, "subagent-", 9));
    return json_number(auto_tree ? max_age_hours : max_age_hours * 3);
}

/* Windows PurePath comparison: case-insensitive, separators interchangeable,
 * trailing separators ignored.  relative_to succeeds on equality or on a
 * component-boundary prefix. */
static size_t norm_path(const char *in, char *out, size_t cap) {
    size_t n = 0;
    for (const char *p = in ? in : ""; *p && n + 1 < cap; p++) {
        char c = *p;
        if (c == '\\') c = '/';
        out[n++] = (char)tolower((unsigned char)c);
    }
    while (n > 1 && out[n - 1] == '/') n--;
    out[n] = '\0';
    return n;
}

struct json_t *w9_worktreeops_2fb4e8_path_is_within_root(const char *path,
                                                        const char *root) {
    char a[4096], b[4096];
    norm_path(path, a, sizeof(a));
    norm_path(root, b, sizeof(b));
    size_t la = strlen(a), lb = strlen(b);
    int ok;
    if (la < lb) ok = 0;
    else if (!memcmp(a, b, lb)) ok = (la == lb) || a[lb] == '/';
    else ok = 0;
    return json_bool(ok ? true : false);
}

void hermes_cli_worktree_ops_port_pending(void) { (void)0; }
