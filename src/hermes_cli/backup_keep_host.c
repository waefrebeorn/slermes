/*
 * backup_keep_host.c - C home for upstream Hermes hermes_cli/backup_keep_host.py.
 * PoP: w15_backupkeep_a55382_host_env_key @ hermes_cli/backup_keep_host.py:host_env_key
 * PoP: w15_backupkeep_a55382__env_name @ hermes_cli/backup_keep_host.py:_env_name
 * PoP: w15_backupkeep_a55382_merge_env @ hermes_cli/backup_keep_host.py:merge_env
 * PoP: w15_backupkeep_a55382_merge_config @ hermes_cli/backup_keep_host.py:merge_config
 * PoP: w15_backupkeep_a55382_merge_auth @ hermes_cli/backup_keep_host.py:merge_auth
 *
 * Faithful ports of the pure per-member merge policy. Zip traversal, the auth
 * store lock and the s6 gateway pause bind live files/processes and stay in the
 * live Python owner.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_backupkeep_a55382.h"

/* POSIX dup() collision avoided: this helper is named dupstr. */
static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (r) memcpy(r, s, n + 1);
    return r;
}
static char *py_strip(const char *s) {
    if (!s) return dupstr("");
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *r = (char *)malloc(n + 1);
    if (r) { memcpy(r, a, n); r[n] = '\0'; }
    return r;
}
typedef struct { char **items; size_t n; } Lines;
static Lines split_lines(const char *s) {
    Lines L = { NULL, 0 };
    if (!s) return L;
    size_t cap = 8;
    L.items = (char **)malloc(cap * sizeof(char *));
    if (!L.items) return L;
    const char *p = s;
    while (*p) {
        const char *q = p;
        while (*q && *q != '\n' && *q != '\r') q++;
        size_t len = (size_t)(q - p);
        char *line = (char *)malloc(len + 1);
        if (line) { memcpy(line, p, len); line[len] = '\0'; }
        if (L.n == cap) { cap *= 2; L.items = (char **)realloc(L.items, cap * sizeof(char *)); }
        L.items[L.n++] = line;
        if (*q == '\r') { q++; if (*q == '\n') q++; }
        else if (*q == '\n') q++;
        p = q;
    }
    return L;
}
static void free_lines(Lines *L) {
    if (!L) return;
    for (size_t i = 0; i < L->n; i++) free(L->items[i]);
    free(L->items);
    L->items = NULL; L->n = 0;
}
static char *join_lines(char **items, size_t n) {
    size_t total = 1;
    for (size_t i = 0; i < n; i++) total += strlen(items[i]) + 1;
    char *r = (char *)malloc(total + 1);
    if (!r) return dupstr("");
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (i) r[k++] = '\n';
        size_t m = strlen(items[i]);
        memcpy(r + k, items[i], m);
        k += m;
    }
    r[k++] = '\n';
    r[k] = '\0';
    return r;
}

int w15_backupkeep_a55382_host_env_key(const char *name) {
    if (!name) return 0;
    return !strcmp(name, "API_SERVER_KEY")
        || !strncmp(name, "NOUS_", 5)
        || !strncmp(name, "HERMES_", 7)
        || !strncmp(name, "TOOL_GATEWAY_", 13);
}

struct json_t *w15_backupkeep_a55382_env_name(const char *line) {
    char *body = py_strip(line);
    if (!body[0] || body[0] == '#' || !strchr(body, '=')) {
        free(body); return json_string("");
    }
    const char *p = body;
    if (!strncmp(p, "export ", 7)) p += 7;
    const char *eq = strchr(p, '=');
    if (!eq) { free(body); return json_string(""); }
    size_t n = (size_t)(eq - p);
    char *tmp = (char *)malloc(n + 1);
    if (tmp) { memcpy(tmp, p, n); tmp[n] = '\0'; }
    char *r = py_strip(tmp ? tmp : "");
    json_t *out = json_string(r);
    free(tmp); free(body); free(r);
    return out;
}

static Lines host_lines(const char *text) {
    Lines all = split_lines(text), out = { NULL, 0 };
    size_t cap = 8;
    out.items = (char **)malloc(cap * sizeof(char *));
    if (!out.items) { free_lines(&all); return out; }
    for (size_t i = 0; i < all.n; i++) {
        char *nm = py_strip(all.items[i]);
        char *key = NULL;
        if (!(nm[0] == '#' || !strchr(nm, '='))) {
            const char *p = nm;
            if (!strncmp(p, "export ", 7)) p += 7;
            const char *eq = strchr(p, '=');
            if (eq) {
                char *tmp = (char *)malloc((size_t)(eq - p) + 1);
                if (tmp) {
                    size_t m = (size_t)(eq - p);
                    memcpy(tmp, p, m); tmp[m] = '\0';
                    key = py_strip(tmp); free(tmp);
                }
            }
        }
        if (key && w15_backupkeep_a55382_host_env_key(key)) {
            if (out.n == cap) { cap *= 2; out.items = (char **)realloc(out.items, cap * sizeof(char *)); }
            out.items[out.n++] = dupstr(all.items[i]);
        }
        free(key); free(nm);
    }
    free_lines(&all);
    return out;
}

struct json_t *w15_backupkeep_a55382_host_lines(const char *text) {
    Lines L = host_lines(text);
    json_t *arr = json_array();
    for (size_t i = 0; i < L.n; i++) json_append(arr, json_string(L.items[i]));
    free_lines(&L);
    return arr;
}

struct json_t *w15_backupkeep_a55382_merge_env(const char *archive_text, const char *host_text) {
    const char *a = archive_text ? archive_text : "";
    const char *h = host_text ? host_text : "";
    Lines ah = host_lines(a), hh = host_lines(h);
    int same = (ah.n == hh.n);
    if (same) for (size_t i = 0; i < ah.n; i++) if (strcmp(ah.items[i], hh.items[i])) { same = 0; break; }
    if (same) { free_lines(&ah); free_lines(&hh); return json_string(a); }
    free_lines(&ah); free_lines(&hh);

    Lines hl = split_lines(h);
    char **names = (char **)malloc((hl.n + 1) * sizeof(char *));
    char **vals = (char **)malloc((hl.n + 1) * sizeof(char *));
    int *used = (int *)calloc(hl.n + 1, sizeof(int));
    size_t m = 0;
    for (size_t i = 0; i < hl.n; i++) {
        json_t *nmn = w15_backupkeep_a55382_env_name(hl.items[i]);
        const char *nm = nmn->str_val;
        if (!w15_backupkeep_a55382_host_env_key(nm)) { json_free(nmn); continue; }
        int found = -1;
        for (size_t j = 0; j < m; j++) if (!strcmp(names[j], nm)) { found = (int)j; break; }
        if (found >= 0) { free(vals[found]); vals[found] = dupstr(hl.items[i]); json_free(nmn); }
        else { names[m] = dupstr(nm); vals[m] = dupstr(hl.items[i]); used[m] = 0; m++; json_free(nmn); }
    }
    free_lines(&hl);

    Lines al = split_lines(a);
    char **out = (char **)malloc((al.n + m + 1) * sizeof(char *));
    size_t on = 0;
    for (size_t i = 0; i < al.n; i++) {
        json_t *nmn = w15_backupkeep_a55382_env_name(al.items[i]);
        const char *nm = nmn->str_val;
        if (w15_backupkeep_a55382_host_env_key(nm)) {
            int found = -1;
            for (size_t j = 0; j < m; j++) if (!strcmp(names[j], nm)) { found = (int)j; break; }
            if (found >= 0) { out[on++] = vals[found]; used[found] = 1; }
        } else out[on++] = dupstr(al.items[i]);
        json_free(nmn);
    }
    for (size_t j = 0; j < m; j++) if (!used[j]) out[on++] = vals[j];
    char *joined = join_lines(out, on);
    json_t *res = json_string(joined);
    for (size_t i = 0; i < on; i++) {
        int is_val = 0;
        for (size_t j = 0; j < m; j++) if (out[i] == vals[j]) { is_val = 1; break; }
        if (!is_val) free(out[i]);
    }
    for (size_t j = 0; j < m; j++) free(vals[j]);
    free(names); free(vals); free(used); free(out); free(joined); free_lines(&al);
    return res;
}

static const char *const HOST_KEYS[] = {
    "model","providers","fallback_providers","fallback","auxiliary","nous","auth",
    "web","image_gen","video_gen","tts","stt","dashboard","webapp","cron",
    "max_concurrent_sessions","max_live_sessions", NULL
};

struct json_t *w15_backupkeep_a55382_merge_config(const struct json_t *a, const struct json_t *h) {
    json_t *merged = (a && a->type == JSON_OBJECT) ? json_copy(a) : json_object();
    if (h && h->type == JSON_OBJECT) {
        for (int i = 0; HOST_KEYS[i]; i++) {
            json_t *v = json_obj_get(h, HOST_KEYS[i]);
            if (v) json_set(merged, HOST_KEYS[i], json_copy(v));
            else json_obj_del(merged, HOST_KEYS[i]);
        }
    }
    return merged;
}

struct json_t *w15_backupkeep_a55382_merge_auth(const struct json_t *a, const struct json_t *h) {
    json_t *m = (a && a->type == JSON_OBJECT) ? json_copy(a) : json_object();
    static const char *const sections[] = { "providers", "credential_pool", NULL };
    for (int s = 0; sections[s]; s++) {
        json_t *raw = (h && h->type == JSON_OBJECT) ? json_obj_get(h, sections[s]) : NULL;
        int ours = raw && raw->type == JSON_OBJECT;
        json_t *theirs = json_obj_get(m, sections[s]);
        if (!theirs || theirs->type != JSON_OBJECT) {
            if (!ours || !json_obj_get(raw, "nous")) continue;
            json_set(m, sections[s], json_object());
            theirs = json_obj_get(m, sections[s]);
        }
        json_obj_del(theirs, "nous");
        if (ours) {
            json_t *nous = json_obj_get(raw, "nous");
            if (nous) json_set(theirs, "nous", json_copy(nous));
        }
    }
    json_t *mav = json_obj_get(m, "active_provider");
    json_t *hav = (h && h->type == JSON_OBJECT) ? json_obj_get(h, "active_provider") : NULL;
    int mv = mav && mav->type == JSON_STRING && !strcmp(mav->str_val, "nous");
    int hv = hav && hav->type == JSON_STRING && !strcmp(hav->str_val, "nous");
    if (mv || hv) json_set(m, "active_provider", hav ? json_copy(hav) : json_null());
    return m;
}

struct json_t *w15_backupkeep_a55382_home_parts(const struct json_t *parts) {
    if (parts && parts->type == JSON_ARRAY && json_len(parts) >= 3) {
        json_t *f = json_get(parts, 0);
        if (f && f->type == JSON_STRING && !strcmp(f->str_val, "profiles")) {
            json_t *out = json_array();
            for (size_t i = 2; i < json_len(parts); i++) json_append(out, json_copy(json_get(parts, i)));
            return out;
        }
    }
    return parts ? json_copy(parts) : json_array();
}

static int part_is(const json_t *parts, size_t idx, const char *v) {
    json_t *p = json_get(parts, idx);
    return p && p->type == JSON_STRING && !strcmp(p->str_val, v);
}
int w15_backupkeep_a55382_is_auth_store(const struct json_t *parts) {
    if (!parts || parts->type != JSON_ARRAY) return 0;
    size_t n = json_len(parts);
    if (n == 1) return part_is(parts, 0, "auth.json");
    if (n == 3) return part_is(parts, 0, "profiles") && part_is(parts, 2, "auth.json");
    return 0;
}
