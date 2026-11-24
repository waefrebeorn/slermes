/*
 * tui_worktree_resume.c — C port of upstream Hermes hermes_cli/tui_worktree_resume.py.
 *
 * PoP: w18_tuiworktre_fbaffe_key @ hermes_cli/tui_worktree_resume.py:_key
 * PoP: w18_tuiworktre_fbaffe_recorded @ hermes_cli/tui_worktree_resume.py:_recorded_tree_for
 *
 * _key() is os.path.normcase(os.path.realpath(str(path))): on Windows that is a
 * backslash-normalized, lowercased, dot-segment-collapsed absolute path. The
 * filesystem reads behind _read()/Path.exists() are explicit seams (`trees`,
 * `existing`). Everything else mirrors _recorded_tree_for verbatim, including
 * its "first dict entry only" early return.
 */
#include "noc_w18_tuiworktre_fbaffe.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Normalize a path the way Windows normcase(realpath(path)) does.
 * Result is lowercased with '\\' separators and '.'/'..' collapsed. */
static void path_key(const char *path, char *out, size_t cap) {
    char tmp[4096];
    size_t n = 0;
    if (!path) path = "";
    for (const char *p = path; *p && n + 1 < sizeof(tmp); p++)
        tmp[n++] = (*p == '/') ? '\\' : *p;
    tmp[n] = '\0';

    char segs[128][256];
    int cnt = 0;
    const char *p = tmp;
    char drv[3] = {0, 0, 0};
    int rooted = 0;
    if (*p == '\\') { rooted = 1; p++; }
    else if (isalpha((unsigned char)p[0]) && p[1] == ':') {
        drv[0] = (char)tolower((unsigned char)p[0]);
        drv[1] = ':';
        p += 2;
        if (*p == '\\') { rooted = 1; p++; }
    }
    while (*p) {
        const char *s = p;
        while (*p && *p != '\\') p++;
        size_t l = (size_t)(p - s);
        if (*p == '\\') p++;
        if (l == 0) continue;
        if (l == 1 && s[0] == '.') continue;
        if (l == 2 && s[0] == '.' && s[1] == '.') { if (cnt > 0) cnt--; continue; }
        if (l >= 256 || cnt >= 128) continue;
        memcpy(segs[cnt], s, l);
        segs[cnt][l] = '\0';
        for (size_t k = 0; k < l; k++)
            segs[cnt][k] = (char)tolower((unsigned char)segs[cnt][k]);
        cnt++;
    }
    size_t o = 0;
    if (drv[0]) { out[o++] = drv[0]; out[o++] = ':'; }
    if (rooted) out[o++] = '\\';
    for (int i = 0; i < cnt; i++) {
        if (o > 0 && out[o - 1] != '\\' && o + 1 < cap) out[o++] = '\\';
        size_t l = strlen(segs[i]);
        if (o + l >= cap) break;
        memcpy(out + o, segs[i], l);
        o += l;
    }
    out[o] = '\0';
}

static char key_buf[4096];
const char *w18_tuiworktre_fbaffe_key(const char *path) {
    path_key(path ? path : "", key_buf, sizeof(key_buf));
    return key_buf;
}

/* str(Path.parent): strip trailing separators, drop the last component. */
static void parent_of(const char *p, char *out, size_t cap) {
    size_t l = strlen(p);
    while (l > 0 && p[l - 1] == '\\') l--;
    int found = 0;
    size_t idx = 0;
    for (size_t i = 0; i < l; i++) if (p[i] == '\\') { idx = i; found = 1; }
    if (!found) { snprintf(out, cap, "%s", p); return; }
    if (idx <= 2 && l > 1 && p[1] == ':') {
        snprintf(out, cap, "%.*s\\", (int)idx, p);
        return;
    }
    if (idx == 0) { snprintf(out, cap, "\\"); return; }
    snprintf(out, cap, "%.*s", (int)idx, p);
}

static int exists_key(const json_t *existing, const char *key) {
    if (!json_is_array(existing)) return 0;
    for (size_t i = 0; i < json_len(existing); i++) {
        json_t *e = json_get(existing, i);
        if (e && e->type == JSON_STRING && !strcmp(e->str_val, key)) return 1;
    }
    return 0;
}

json_t *w18_tuiworktre_fbaffe_recorded(const char *cwd, const json_t *trees,
                                       const json_t *existing) {
    char cand[4096];
    path_key(cwd ? cwd : "", cand, sizeof(cand));
    for (int depth = 0; depth < 64; depth++) {
        char key[4096];
        path_key(cand, key, sizeof(key));
        json_t *entry = json_is_object(trees) ? json_obj_get(trees, key) : NULL;
        if (!entry || entry->type != JSON_OBJECT) {
            char parent[4096];
            parent_of(cand, parent, sizeof(parent));
            if (!strcmp(parent, cand)) break;
            strcpy(cand, parent);
            continue;
        }
        /* First dict entry wins: a non-matching one returns None immediately. */
        const char *repo = json_get_str(entry, "repo_root", "");
        const char *tree = json_get_str(entry, "path", "");
        const char *branch = json_get_str(entry, "branch", "");
        if (!branch || !*branch) return json_null();

        char raw[8200], k[4096];
        snprintf(raw, sizeof(raw), "%s/.git", repo ? repo : "");
        path_key(raw, k, sizeof(k));
        if (!exists_key(existing, k)) return json_null();

        char tkey[4096], tparent[4096];
        path_key(tree ? tree : "", tkey, sizeof(tkey));
        parent_of(tkey, tparent, sizeof(tparent));

        snprintf(raw, sizeof(raw), "%s/.worktrees", repo ? repo : "");
        path_key(raw, k, sizeof(k));
        if (strcmp(tparent, k) != 0) return json_null();
        if (strcmp(tkey, key) != 0) return json_null();

        json_t *out = json_object();
        json_set(out, "path", json_string(tree ? tree : ""));
        json_set(out, "branch", json_string(branch));
        json_set(out, "repo_root", json_string(repo ? repo : ""));
        return out;
    }
    return json_null();
}
