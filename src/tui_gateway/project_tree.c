/*
 * project_tree.c — C home for upstream Hermes tui_gateway/project_tree.py.
 *
 * PoP: w11_projecttre_cfc189_path_key      @ tui_gateway/project_tree.py:_path_key
 * PoP: w11_projecttre_cfc189_lane_key      @ tui_gateway/project_tree.py:_lane_key
 * PoP: w11_projecttre_cfc189_base_name     @ tui_gateway/project_tree.py:base_name
 * PoP: w11_projecttre_cfc189_project_node  @ tui_gateway/project_tree.py:_project_node
 * PoP: w11_projecttre_cfc189_place_by_heuristic @ tui_gateway/project_tree.py:_place_by_heuristic
 *
 * Pure path/lane/tree helpers ported verbatim. ASCII casefold is used for
 * Windows comparison keys; the live code additionally NFC-normalizes Unicode
 * segments (fixtures use ASCII, where NFC is the identity).
 */
#include "noc_w11_projecttre_cfc189.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define PT_MAX_ITEMS 256

static char *pt_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static char *pt_dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static const char *pt_s(const char *s) { return s ? s : ""; }
/* json_get_str() only works on objects; array elements are scalar nodes. */
static const char *pt_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* Python str.strip() for the ASCII whitespace corpus used here. */
static char *pt_strip(const char *s) {
    const char *a = pt_s(s);
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    return pt_dupn(a, (size_t)(b - a));
}
static void pt_rstrip_seps(char *buf) {
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '/' || buf[n - 1] == '\\')) buf[--n] = '\0';
}
static int pt_is_sep(char c) { return c == '/' || c == '\\'; }
static int pt_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY) return json_len(v) > 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) > 0;
    return 1;
}
static char *pt_lower(const char *s) {
    char *o = pt_dup(pt_s(s));
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* _segments / _comparison_segments (NFC skipped: identity on ASCII). */
static json_t *pt_split(const char *path, int lower) {
    json_t *arr = json_array();
    char *buf = pt_dup(pt_s(path));
    pt_rstrip_seps(buf);
    char *p = buf;
    while (*p) {
        if (pt_is_sep(*p)) { p++; continue; }
        char *start = p;
        while (*p && !pt_is_sep(*p)) p++;
        char *seg = pt_dupn(start, (size_t)(p - start));
        if (lower) for (char *q = seg; *q; q++) *q = (char)tolower((unsigned char)*q);
        json_append(arr, json_string(seg));
        free(seg);
    }
    free(buf);
    return arr;
}

bool w11_projecttre_cfc189_is_windows_path(const char *path) {
    char *v = pt_strip(path);
    int r = 0;
    size_t n = strlen(v);
    if (n >= 3 && isalpha((unsigned char)v[0]) && v[1] == ':' && pt_is_sep(v[2])) r = 1;
    if (!r && n >= 2 && ((v[0] == '\\' && v[1] == '\\') || (v[0] == '/' && v[1] == '/'))) r = 1;
    free(v);
    return r ? true : false;
}

json_t *w11_projecttre_cfc189_segments(const char *path) { return pt_split(path, 0); }

const char *w11_projecttre_cfc189_base_name(const char *path) {
    json_t *segs = pt_split(path, 0);
    size_t n = json_len(segs);
    char *out = n ? pt_dup(pt_jstr(json_get(segs, n - 1))) : pt_dup("");
    json_free(segs);
    return out;
}

const char *w11_projecttre_cfc189_path_key(const char *path) {
    int lower = w11_projecttre_cfc189_is_windows_path(path) ? 1 : 0;
    json_t *segs = pt_split(path, lower);
    size_t cap = 4, len = 0;
    for (size_t i = 0; i < json_len(segs); i++)
        cap += strlen(pt_jstr(json_get(segs, i))) + 1;
    char *out = (char *)malloc(cap);
    if (!out) { json_free(segs); return pt_dup(""); }
    out[0] = '\0';
    for (size_t i = 0; i < json_len(segs); i++) {
        if (i) out[len++] = '/';
        const char *s = pt_jstr(json_get(segs, i));
        size_t n = strlen(s);
        memcpy(out + len, s, n); len += n;
    }
    out[len] = '\0';
    json_free(segs);
    return out;
}

const char *w11_projecttre_cfc189_branch_lane_id(const char *repo_root, const char *branch) {
    char *b = pt_strip(branch);
    size_t n = strlen(pt_s(repo_root)) + strlen("::branch::") + strlen(b) + 1;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "%s::branch::%s", pt_s(repo_root), b);
    free(b);
    return out;
}

const char *w11_projecttre_cfc189_kanban_lane_id(const char *repo_root) {
    size_t n = strlen(pt_s(repo_root)) + strlen("::kanban") + 1;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "%s::kanban", pt_s(repo_root));
    return out;
}

const char *w11_projecttre_cfc189_lane_key(const char *path_or_lane) {
    const char *s = pt_s(path_or_lane);
    const char *mk = strstr(s, "::branch::");
    if (!mk) mk = strstr(s, "::kanban");
    if (!mk) return w11_projecttre_cfc189_path_key(s);
    size_t rootlen = (size_t)(mk - s);
    char *root = pt_dupn(s, rootlen);
    char *key = (char *)w11_projecttre_cfc189_path_key(root);
    free(root);
    size_t n = strlen(key) + strlen(mk) + 1;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "%s%s", key, mk);
    free(key);
    return out;
}

json_t *w11_projecttre_cfc189_kanban_worktree_dir(const char *path) {
    /* ^(.*[/\\]\.worktrees)[/\\]t_[0-9a-f]+[/\\]?$ */
    const char *s = pt_s(path);
    size_t n = strlen(s);
    if (n == 0) return json_null();
    size_t end = n;
    if (pt_is_sep(s[end - 1])) end--;
    if (end == 0) return json_null();
    size_t seg_start = end;
    while (seg_start > 0 && !pt_is_sep(s[seg_start - 1])) seg_start--;
    if (seg_start == 0 || end - seg_start < 3 ||
        s[seg_start] != 't' || s[seg_start + 1] != '_') return json_null();
    for (size_t i = seg_start + 2; i < end; i++)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
            return json_null();
    size_t before = seg_start - 1;
    if (!pt_is_sep(s[before])) return json_null();
    const char *needle = ".worktrees";
    size_t nl = strlen(needle);
    if (before < nl + 1) return json_null();
    if (strncmp(s + before - nl, needle, nl) != 0) return json_null();
    size_t wstart = before - nl;
    if (wstart == 0 || !pt_is_sep(s[wstart - 1])) return json_null();
    char *out = pt_dupn(s, wstart + nl);
    json_t *r = json_string(out);
    free(out);
    return r;
}

const char *w11_projecttre_cfc189_with_base_name(const char *path, const char *name) {
    char *buf = pt_dup(pt_s(path));
    pt_rstrip_seps(buf);
    const char *nm = pt_s(name);
    size_t pos = 0;
    for (size_t i = 0; buf[i]; i++) if (pt_is_sep(buf[i])) pos = i + 1;
    size_t n = pos + strlen(nm) + 1;
    char *out = (char *)malloc(n);
    if (out) {
        memcpy(out, buf, pos);
        strcpy(out + pos, nm);
        if (!*nm) pt_rstrip_seps(out);
    }
    free(buf);
    return out;
}

const char *w11_projecttre_cfc189_field(json_t *row, const char *key) {
    json_t *v = row ? json_obj_get(row, key) : NULL;
    if (!v || v->type != JSON_STRING) return pt_dup("");
    return pt_strip(v->str_val);
}

double w11_projecttre_cfc189_session_time(json_t *session) {
    json_t *v = session ? json_obj_get(session, "last_active") : NULL;
    double t = 0.0;
    if (pt_truthy(v)) t = json_get_num(session, "last_active", 0.0);
    else {
        v = session ? json_obj_get(session, "started_at") : NULL;
        if (pt_truthy(v)) t = json_get_num(session, "started_at", 0.0);
    }
    return t;
}

double w11_projecttre_cfc189_last_active(json_t *sessions) {
    double m = 0.0;
    if (!sessions || sessions->type != JSON_ARRAY) return 0.0;
    for (size_t i = 0; i < json_len(sessions); i++) {
        double t = w11_projecttre_cfc189_session_time(json_get(sessions, i));
        if (t > m) m = t;
    }
    return m;
}

json_t *w11_projecttre_cfc189_placement(const char *repo_root, const char *lane_key,
                                         const char *lane_label, const char *lane_path,
                                         bool is_main, bool is_kanban, bool is_git) {
    json_t *o = json_object();
    json_set(o, "repo_key", json_string(pt_s(repo_root)));
    const char *bn = w11_projecttre_cfc189_base_name(repo_root);
    json_set(o, "repo_label", json_string(*bn ? bn : pt_s(repo_root)));
    free((void *)bn);
    json_set(o, "lane_key", json_string(pt_s(lane_key)));
    json_set(o, "lane_label", json_string(pt_s(lane_label)));
    json_set(o, "lane_path", json_string(pt_s(lane_path)));
    json_set(o, "is_main", json_bool(is_main));
    json_set(o, "is_kanban", json_bool(is_kanban));
    json_set(o, "is_git", json_bool(is_git));
    return o;
}

json_t *w11_projecttre_cfc189_trunk_placement(const char *repo_root, const char *branch) {
    char *b = pt_strip(branch);
    const char *label = *b ? b : "main";
    const char *lk = w11_projecttre_cfc189_branch_lane_id(repo_root, label);
    json_t *r = w11_projecttre_cfc189_placement(repo_root, lk, label, repo_root, true, false, true);
    free((void *)lk); free(b);
    return r;
}

json_t *w11_projecttre_cfc189_kanban_placement(const char *repo_root, const char *kanban_dir) {
    const char *lk = w11_projecttre_cfc189_kanban_lane_id(repo_root);
    json_t *r = w11_projecttre_cfc189_placement(repo_root, lk, "kanban", kanban_dir, false, true, true);
    free((void *)lk);
    return r;
}

json_t *w11_projecttre_cfc189_repo_node(const char *root, const char *label) {
    json_t *o = json_object();
    json_set(o, "id", json_string(pt_s(root)));
    json_set(o, "label", json_string(pt_s(label)));
    json_set(o, "path", json_string(pt_s(root)));
    json_set(o, "groups", json_array());
    json_set(o, "sessionCount", json_number(0));
    return o;
}

json_t *w11_projecttre_cfc189_project_node(const char *pid, const char *label, json_t *path,
                                            json_t *repos, double session_count,
                                            double last_active, json_t *previews,
                                            json_t *rows, json_t *flags) {
    json_t *o = json_object();
    json_set(o, "id", json_string(pt_s(pid)));
    json_set(o, "label", json_string(pt_s(label)));
    json_set(o, "path", path && path->type == JSON_STRING ? json_copy(path) : json_null());
    json_set(o, "color", json_null());
    json_set(o, "icon", json_null());
    json_set(o, "isAuto", json_bool(false));
    json_set(o, "isNoProject", json_bool(false));
    json_set(o, "sessionCount", json_number(session_count));
    json_set(o, "lastActive", json_number(last_active));
    double tokens = 0.0, cost = 0.0;
    json_t *ids = json_array();
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rows); i++) {
            json_t *s = json_get(rows, i);
            tokens += json_get_num(s, "input_tokens", 0.0) + json_get_num(s, "output_tokens", 0.0);
            json_t *a = json_obj_get(s, "actual_cost_usd");
            if (pt_truthy(a)) cost += json_get_num(s, "actual_cost_usd", 0.0);
            else cost += json_get_num(s, "estimated_cost_usd", 0.0);
            json_t *id = json_obj_get(s, "id");
            if (pt_truthy(id) && id->type == JSON_STRING) json_append(ids, json_copy(id));
        }
    }
    json_set(o, "totalTokens", json_number(tokens));
    json_set(o, "totalCostUsd", json_number(cost));
    json_set(o, "repos", repos ? json_copy(repos) : json_array());
    json_set(o, "previewSessions", previews ? json_copy(previews) : json_array());
    json_set(o, "sessionIds", ids);
    if (flags && flags->type == JSON_OBJECT)
        for (size_t i = 0; i < json_object_size(flags); i++)
            json_set(o, json_object_get_key_at(flags, i),
                     json_copy(json_object_get_at(flags, i)));
    return o;
}

json_t *w11_projecttre_cfc189_lane_sort_key(json_t *group) {
    json_t *arr = json_array();
    const char *label = group ? json_get_str(group, "label", "") : "";
    char *low = pt_lower(label);
    int trunk = 0;
    json_t *im = group ? json_obj_get(group, "isMain") : NULL;
    if (pt_truthy(im) && (!strcmp(low, "main") || !strcmp(low, "master") ||
                          !strcmp(low, "trunk") || !strcmp(low, "develop"))) trunk = 1;
    json_append(arr, json_number(trunk ? 0 : 1));
    json_t *ik = group ? json_obj_get(group, "isKanban") : NULL;
    json_append(arr, json_number(pt_truthy(ik) ? 1 : 0));
    json_t *sessions = group ? json_obj_get(group, "sessions") : NULL;
    json_append(arr, json_number(-w11_projecttre_cfc189_last_active(sessions)));
    json_append(arr, json_string(low));
    free(low);
    return arr;
}

json_t *w11_projecttre_cfc189_disambiguate_labels(json_t *items) {
    if (!items || items->type != JSON_ARRAY) return items;
    size_t n = json_len(items);
    if (n > PT_MAX_ITEMS) n = PT_MAX_ITEMS;
    int done[PT_MAX_ITEMS] = {0};
    for (size_t i = 0; i < n; i++) {
        if (done[i]) continue;
        json_t *a = json_get(items, i);
        const char *lbl = json_get_str(a, "label", "");
        size_t bucket[PT_MAX_ITEMS], bn = 0;
        for (size_t j = i; j < n; j++) {
            json_t *b = json_get(items, j);
            if (!strcmp(json_get_str(b, "label", ""), lbl)) bucket[bn++] = j;
        }
        for (size_t k = 0; k < bn; k++) done[bucket[k]] = 1;
        size_t pathed[PT_MAX_ITEMS], pn = 0;
        for (size_t k = 0; k < bn; k++) {
            json_t *b = json_get(items, bucket[k]);
            if (pt_truthy(json_obj_get(b, "path"))) pathed[pn++] = k;
        }
        if (pn < 2) continue;
        json_t *parents[PT_MAX_ITEMS];
        size_t maxlen = 0;
        for (size_t k = 0; k < pn; k++) {
            const char *p = json_get_str(json_get(items, bucket[pathed[k]]), "path", "");
            json_t *segs = pt_split(p, 0);
            size_t sn = json_len(segs);
            json_t *par = json_array();
            for (size_t q = 0; q + 1 < sn; q++) json_append(par, json_copy(json_get(segs, q)));
            parents[k] = par;
            json_free(segs);
            if (sn > maxlen) maxlen = sn - 1;
        }
        for (size_t depth = 1; depth <= maxlen; depth++) {
            for (size_t k = 0; k < pn; k++) {
                json_t *item = json_get(items, bucket[pathed[k]]);
                const char *p = json_get_str(item, "path", "");
                const char *bn2 = w11_projecttre_cfc189_base_name(p);
                const char *use = *bn2 ? bn2 : p;
                json_t *par = parents[k];
                size_t plen = json_len(par);
                size_t start = plen > depth ? plen - depth : 0;
                size_t cap = 2, len = 0;
                for (size_t q = start; q < plen; q++)
                    cap += strlen(pt_jstr(json_get(par, q))) + 1;
                cap += strlen(use) + 2;
                char *label = (char *)malloc(cap);
                if (label) {
                    label[0] = '\0';
                    for (size_t q = start; q < plen; q++) {
                        if (len) label[len++] = '/';
                        const char *seg = pt_jstr(json_get(par, q));
                        size_t sl = strlen(seg);
                        memcpy(label + len, seg, sl); len += sl;
                    }
                    if (len) label[len++] = '/';
                    strcpy(label + len, use);
                    json_set(item, "label", json_string(label));
                    free(label);
                }
                free((void *)bn2);
            }
            int uniq = 1;
            for (size_t a2 = 0; a2 < pn && uniq; a2++)
                for (size_t b2 = a2 + 1; b2 < pn; b2++)
                    if (!strcmp(json_get_str(json_get(items, bucket[pathed[a2]]), "label", ""),
                                json_get_str(json_get(items, bucket[pathed[b2]]), "label", ""))) {
                        uniq = 0; break;
                    }
            if (uniq) break;
        }
        for (size_t k = 0; k < pn; k++) json_free(parents[k]);
    }
    return json_copy(items);
}

json_t *w11_projecttre_cfc189_stamp_profile(json_t *projects, const char *profile) {
    if (!projects || projects->type != JSON_ARRAY) return projects;
    for (size_t i = 0; i < json_len(projects); i++) {
        json_t *p = json_get(projects, i);
        json_t *repos = json_obj_get(p, "repos");
        if (repos && repos->type == JSON_ARRAY)
            for (size_t r = 0; r < json_len(repos); r++) {
                json_t *groups = json_obj_get(json_get(repos, r), "groups");
                if (!groups || groups->type != JSON_ARRAY) continue;
                for (size_t g = 0; g < json_len(groups); g++) {
                    json_t *ss = json_obj_get(json_get(groups, g), "sessions");
                    if (!ss || ss->type != JSON_ARRAY) continue;
                    for (size_t s = 0; s < json_len(ss); s++)
                        json_set(json_get(ss, s), "profile", json_string(pt_s(profile)));
                }
            }
        json_t *pre = json_obj_get(p, "previewSessions");
        if (pre && pre->type == JSON_ARRAY)
            for (size_t s = 0; s < json_len(pre); s++)
                json_set(json_get(pre, s), "profile", json_string(pt_s(profile)));
    }
    return json_copy(projects);
}

json_t *w11_projecttre_cfc189_place_by_heuristic(const char *path) {
    const char *base = w11_projecttre_cfc189_base_name(path);
    if (!*base) { free((void *)base); return json_null(); }
    json_t *kw = w11_projecttre_cfc189_kanban_worktree_dir(path);
    if (kw && kw->type == JSON_STRING) {
        const char *kdir = kw->str_val;
        const char *root = w11_projecttre_cfc189_with_base_name(kdir, "");
        const char *lk = w11_projecttre_cfc189_kanban_lane_id(root);
        json_t *r = w11_projecttre_cfc189_placement(root, lk, "kanban", kdir, false, true, true);
        free((void *)root); free((void *)lk); json_free(kw); free((void *)base);
        return r;
    }
    json_free(kw);
    const char *hit = NULL;
    for (const char *p = strstr(base, "-wt-"); p; p = strstr(p + 1, "-wt-"))
        if (p > base && p[4]) hit = p;
    if (hit) {
        char *g1 = pt_dupn(base, (size_t)(hit - base));
        char *g2 = pt_dup(hit + 4);
        const char *root = w11_projecttre_cfc189_with_base_name(path, g1);
        json_t *r = w11_projecttre_cfc189_placement(root, path, g2, path, false, false, true);
        free((void *)root); free(g1); free(g2); free((void *)base);
        return r;
    }
    const char *lk = w11_projecttre_cfc189_branch_lane_id(path, "main");
    json_t *r = w11_projecttre_cfc189_placement(path, lk, base, path, true, false, false);
    free((void *)lk); free((void *)base);
    return r;
}

json_t *w11_projecttre_cfc189_constants(void) {
    json_t *o = json_object();
    json_set(o, "DEFAULT_BRANCH_LABEL", json_string("main"));
    json_set(o, "NO_PROJECT_ID", json_string("__no_project__"));
    json_set(o, "NO_PROJECT_LABEL", json_string("Home"));
    return o;
}
