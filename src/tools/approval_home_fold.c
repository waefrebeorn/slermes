/*
 * approval_home_fold.c — C port of upstream Hermes tools/approval_home_fold.py.
 *
 * PoP: w11_approvalho_875e9d_msys_drive_spellings @ tools/approval_home_fold.py:_msys_drive_spellings
 * PoP: w11_approvalho_875e9d_path_below @ tools/approval_home_fold.py:_path_below
 * PoP: w11_approvalho_875e9d_home_spellings @ tools/approval_home_fold.py:_home_spellings
 * PoP: w11_approvalho_875e9d_fold_home_prefixes @ tools/approval_home_fold.py:_fold_home_prefixes
 * PoP: w11_approvalho_875e9d_rewrite_user_home_ex @ tools/approval_home_fold.py:_rewrite_resolved_user_home
 *
 * Environment-dependent Python values are injected as explicit arguments (the
 * CRT setenv(name,"") pitfall), mirroring the port-kit's _ex seam convention.
 */
#include "noc_w11_approvalho_875e9d.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXP 64
#define PLEN 1024
#define CLEN 16384

static int is_sep(char c) { return c == '/' || c == '\\'; }
static int is_ws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
static int tail_ok(char c) {
    if (is_sep(c) || is_ws(c)) return 0;
    return !strchr("'\"`;|&<>()", (int)c);
}
static int lb_forbidden(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '.' || c == '~' || c == '$' || c == '}' || c == '/' || c == '\\' || c == '-';
}

static int components(const char *p, char out[MAXP][PLEN]) {
    int n = 0;
    if (!p) return 0;
    while (*p) {
        while (*p && is_sep(*p)) p++;
        if (!*p) break;
        const char *s = p;
        while (*p && !is_sep(*p)) p++;
        size_t len = (size_t)(p - s);
        if (n < MAXP && len < PLEN) { memcpy(out[n], s, len); out[n][len] = 0; n++; }
    }
    return n;
}

struct strlist { char items[MAXP * 4][PLEN]; int count; };
static void sl_add(struct strlist *l, const char *s) {
    for (int i = 0; i < l->count; i++) if (!strcmp(l->items[i], s)) return;
    if (l->count < MAXP * 4) { snprintf(l->items[l->count], PLEN, "%s", s ? s : ""); l->count++; }
}
static json_t *sl_json(const struct strlist *l) {
    json_t *a = json_array();
    for (int i = 0; i < l->count; i++) json_append(a, json_string(l->items[i]));
    return a;
}

struct json_t *w11_approvalho_875e9d_msys_drive_spellings(const char *path) {
    json_t *out = json_array();
    if (!path || !((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) ||
        path[1] != ':' || !is_sep(path[2]) || !path[3]) return out;
    char rest[PLEN];
    snprintf(rest, sizeof(rest), "%s", path + 3);
    char buf[PLEN];
    snprintf(buf, sizeof(buf), "/%c/%s", (char)tolower((unsigned char)path[0]), rest);
    json_append(out, json_string(buf));
    snprintf(buf, sizeof(buf), "/%c/%s", (char)toupper((unsigned char)path[0]), rest);
    json_append(out, json_string(buf));
    return out;
}

struct json_t *w11_approvalho_875e9d_path_below(const char *root, const char *path, int win32) {
    char rp[MAXP][PLEN], pp[MAXP][PLEN];
    int rn = components(root, rp), pn = components(path, pp);
    if (rn == 0 || pn <= rn) return json_null();
    for (int i = 0; i < rn; i++) {
        int eq;
        if (win32) {
            char a[PLEN], b[PLEN];
            snprintf(a, sizeof(a), "%s", rp[i]); snprintf(b, sizeof(b), "%s", pp[i]);
            for (char *x = a; *x; x++) *x = (char)tolower((unsigned char)*x);
            for (char *x = b; *x; x++) *x = (char)tolower((unsigned char)*x);
            eq = !strcmp(a, b);
        } else eq = !strcmp(rp[i], pp[i]);
        if (!eq) return json_null();
    }
    char out[PLEN] = "";
    for (int i = rn; i < pn; i++) {
        if (i > rn) strncat(out, "/", sizeof(out) - strlen(out) - 1);
        strncat(out, pp[i], sizeof(out) - strlen(out) - 1);
    }
    return json_string(out);
}

struct json_t *w11_approvalho_875e9d_home_spellings(const struct json_t *paths, const char *command,
        const char *home_env, const char *expand_home, const char *local_appdata,
        const char *roaming, const char *profile, int win32) {
    struct strlist absolute = {{ {0} }, 0};
    if (paths && paths->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(paths); i++) {
            json_t *e = json_get(paths, i);
            const char *p = (e && e->type == JSON_STRING) ? e->str_val : "";
            sl_add(&absolute, p);
            json_t *sp = w11_approvalho_875e9d_msys_drive_spellings(p);
            for (size_t j = 0; j < json_len(sp); j++) {
                json_t *s = json_get(sp, j);
                if (s && s->type == JSON_STRING) sl_add(&absolute, s->str_val);
            }
            json_free(sp);
        }
    }
    const char *cmdbuf = command ? command : "";
    if (!strchr(cmdbuf, '~') && !strchr(cmdbuf, '$') && !strchr(cmdbuf, '%')) return sl_json(&absolute);
    const char *home = (home_env && *home_env) ? home_env : (expand_home ? expand_home : "");
    const char *names[4] = {"HOME", "LOCALAPPDATA", "APPDATA", "USERPROFILE"};
    const char *roots[4] = {home, local_appdata, roaming, profile};
    const char *prefixes[3] = {"$", "${", "%"};
    struct strlist relative = {{ {0} }, 0};
    char prefix[PLEN], rootbuf[PLEN];
    for (int k = -1; k < 4; k++) {
        if (k == -1) { snprintf(prefix, sizeof(prefix), "~"); snprintf(rootbuf, sizeof(rootbuf), "%s", home); }
        else {
            const char *n = names[k];
            for (int f = 0; f < 3; f++) {
                if (f == 0) snprintf(prefix, sizeof(prefix), "$%s", n);
                else if (f == 1) snprintf(prefix, sizeof(prefix), "${%s}", n);
                else snprintf(prefix, sizeof(prefix), "%%%s%%", n);
                snprintf(rootbuf, sizeof(rootbuf), "%s", roots[k] ? roots[k] : "");
                /* fall through to candidate loop via helper below */
                struct strlist spelled = {{ {0} }, 0};
                sl_add(&spelled, rootbuf);
                json_t *sp = w11_approvalho_875e9d_msys_drive_spellings(rootbuf);
                for (size_t z = 0; z < json_len(sp); z++) {
                    json_t *s = json_get(sp, z);
                    if (s && s->type == JSON_STRING) sl_add(&spelled, s->str_val);
                }
                json_free(sp);
                for (int a = 0; a < spelled.count; a++) {
                    for (int c = 0; c < absolute.count; c++) {
                        json_t *tail = w11_approvalho_875e9d_path_below(spelled.items[a], absolute.items[c], win32);
                        if (tail && tail->type == JSON_STRING) {
                            char item[PLEN];
                            snprintf(item, sizeof(item), "%s/%s", prefix, tail->str_val);
                            sl_add(&relative, item);
                        }
                        json_free(tail);
                    }
                }
            }
            continue;
        }
        struct strlist spelled = {{ {0} }, 0};
        sl_add(&spelled, rootbuf);
        json_t *sp = w11_approvalho_875e9d_msys_drive_spellings(rootbuf);
        for (size_t z = 0; z < json_len(sp); z++) {
            json_t *s = json_get(sp, z);
            if (s && s->type == JSON_STRING) sl_add(&spelled, s->str_val);
        }
        json_free(sp);
        for (int a = 0; a < spelled.count; a++) {
            for (int c = 0; c < absolute.count; c++) {
                json_t *tail = w11_approvalho_875e9d_path_below(spelled.items[a], absolute.items[c], win32);
                if (tail && tail->type == JSON_STRING) {
                    char item[PLEN];
                    snprintf(item, sizeof(item), "%s/%s", prefix, tail->str_val);
                    sl_add(&relative, item);
                }
                json_free(tail);
            }
        }
    }
    for (int i = 0; i < relative.count; i++) sl_add(&absolute, relative.items[i]);
    return sl_json(&absolute);
}

/* Python: pattern.sub(lambda m: replacement + m.group("tail")... ) for each path, longest first. */
struct json_t *w11_approvalho_875e9d_fold_home_prefixes(const char *command, const struct json_t *paths,
        const char *replacement, int win32) {
    static char out[CLEN];
    snprintf(out, sizeof(out), "%s", command ? command : "");
    struct strlist sorted = {{ {0} }, 0};
    if (paths && paths->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(paths); i++) {
            json_t *e = json_get(paths, i);
            if (e && e->type == JSON_STRING && e->str_val && *e->str_val) sl_add(&sorted, e->str_val);
        }
    }
    /* longest first (dict.fromkeys(sorted(..., key=len, reverse=True))) */
    for (int i = 1; i < sorted.count; i++)
        for (int j = i; j > 0 && strlen(sorted.items[j]) > strlen(sorted.items[j - 1]); j--) {
            char tmp[PLEN]; snprintf(tmp, sizeof(tmp), "%s", sorted.items[j]);
            snprintf(sorted.items[j], PLEN, "%s", sorted.items[j - 1]);
            snprintf(sorted.items[j - 1], PLEN, "%s", tmp);
        }
    const char *rep = replacement ? replacement : "";
    for (int pi = 0; pi < sorted.count; pi++) {
        char comps[MAXP][PLEN];
        int nc = components(sorted.items[pi], comps);
        if (nc == 0) continue;
        int single_drive = (nc == 1 && strlen(comps[0]) == 2 &&
                            ((comps[0][0] >= 'a' && comps[0][0] <= 'z') || (comps[0][0] >= 'A' && comps[0][0] <= 'Z')) &&
                            comps[0][1] == ':');
        if (single_drive) continue;
        int posix_single = (nc == 1 && !win32 && sorted.items[pi][0] == '/');
        char cur[CLEN];
        snprintf(cur, sizeof(cur), "%s", out);
        char res[CLEN]; res[0] = 0;
        size_t len = strlen(cur);
        size_t i = 0, last = 0;
        while (i < len) {
            int ok = 0;
            if (posix_single && i > 0 && lb_forbidden((unsigned char)cur[i - 1])) { i++; continue; }
            size_t j = i;
            if (posix_single) {
                while (j < len && cur[j] == '/') j++;
                if (j < len && cur[j] == '\\' && j + 1 < len && cur[j + 1] == '/') { j += 2; while (j < len && cur[j] == '/') j++; }
                if (j == i) goto no_match;
            } else {
                while (j < len && is_sep(cur[j])) j++;
            }
            size_t k = j;
            size_t cl = strlen(comps[0]);
            if (len - k < cl || strncmp(cur + k, comps[0], cl)) goto no_match;
            k += cl;
            for (int ci = 1; ci < nc; ci++) {
                size_t ns = 0;
                while (k < len && is_sep(cur[k])) { k++; ns++; }
                if (!ns) goto no_match;
                cl = strlen(comps[ci]);
                if (len - k < cl || strncmp(cur + k, comps[ci], cl)) goto no_match;
                k += cl;
            }
            /* mandatory tail: one or more (sep + allowed*) */
            size_t tail_start = k;
            int groups = 0;
            while (k < len) {
                if (posix_single) {
                    if (cur[k] == '/' || (cur[k] == '\\' && k + 1 < len && cur[k + 1] == '/')) {
                        k += (cur[k] == '\\') ? 2 : 1;
                    } else break;
                } else {
                    if (!is_sep(cur[k])) break;
                    k++;
                }
                while (k < len && tail_ok(cur[k])) k++;
                groups++;
            }
            if (!groups) goto no_match;
            strncat(res, cur + last, i - last);
            strncat(res, rep, sizeof(res) - strlen(res) - 1);
            for (size_t t = tail_start; t < k && strlen(res) + 2 < sizeof(res); t++) {
                if (cur[t] == '\\' && t + 1 < k && cur[t + 1] == '/') { strncat(res, "/", 1); t++; }
                else strncat(res, cur[t] == '\\' ? "/" : &cur[t], 1);
            }
            last = k;
            i = k;
            continue;
        no_match:
            i++;
        }
        strncat(res, cur + last, sizeof(res) - strlen(res) - 1);
        snprintf(out, sizeof(out), "%s", res);
    }
    return json_string(out);
}

struct json_t *w11_approvalho_875e9d_rewrite_user_home_ex(const char *command, const char *expand_home,
        const char *real_home, const char *home_env, int win32) {
    json_t *paths = json_array();
    json_append(paths, json_string(expand_home ? expand_home : ""));
    json_append(paths, json_string(real_home ? real_home : ""));
    json_append(paths, json_string(home_env ? home_env : ""));
    struct strlist all = {{ {0} }, 0};
    for (size_t i = 0; i < json_len(paths); i++) {
        json_t *e = json_get(paths, i);
        const char *p = (e && e->type == JSON_STRING) ? e->str_val : "";
        sl_add(&all, p);
    }
    for (int i = 0; i < all.count; i++) {
        json_t *sp = w11_approvalho_875e9d_msys_drive_spellings(all.items[i]);
        for (size_t j = 0; j < json_len(sp); j++) {
            json_t *s = json_get(sp, j);
            if (s && s->type == JSON_STRING) json_append(paths, json_string(s->str_val));
        }
        json_free(sp);
    }
    json_t *out = w11_approvalho_875e9d_fold_home_prefixes(command, paths, "~", win32);
    json_free(paths);
    return out;
}
