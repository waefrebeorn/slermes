/*
 * local_pythonpath.c — C11 port of upstream Hermes tools/environments/local_pythonpath.py
 * (gate w26_localpytho_6351f3).
 * STATUS: PORTED — behavior verified by tests/oracle_w26_localpytho_6351f3.py.
 * PoP: w26_localpytho_6351f3_same_path @ tools/environments/local_pythonpath.py:_same_path
 * PoP: w26_localpytho_6351f3_strip_pythonpath @ tools/environments/local_pythonpath.py:_strip_hermes_owned_pythonpath
 * PoP: w26_localpytho_6351f3_strip_all @ tools/environments/local_pythonpath.py:_strip_hermes_owned_pythonpath_and_runtime_markers
 * PoP: w26_localpytho_6351f3_constants @ tools/environments/local_pythonpath.py:_ACTIVE_VENV_MARKER_VARS
 *
 * Seams: site-packages discovery, the validated runtime venv, and the repo-root
 * aliases all need a filesystem, so they arrive as the fixed `owned_paths` list.
 * What is ported is the provenance decision: normcased-part path equality and
 * the exact entry filtering it drives.  An entry is removed only when it is a
 * non-empty exact spelling of an owned path; descendants, empty components and
 * every unrelated entry are preserved byte-for-byte.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_localpytho_6351f3.h"

#define LP_MAX_PARTS 64
#define LP_MAX_PART 512

static const char *const LP_MARKERS[3] = {"VIRTUAL_ENV", "CONDA_PREFIX", "PYTHONHOME"};

static int lp_is_sep(char c) { return c == '/' || c == '\\'; }

/* os.path (ntpath) Path.parts, spelled as normcase(part): drive/root tokens then
 * components, '.' dropped, '..' kept.  Returns the part count. */
static int lp_parts(const char *s, char out[][LP_MAX_PART], int cap) {
    int n = 0;
    size_t i = 0, L = strlen(s);
    char tok[LP_MAX_PART];

    if (L >= 2 && ((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z')) && s[1] == ':') {
        size_t k = 0;
        tok[k++] = (char)tolower((unsigned char)s[0]);
        tok[k++] = ':';
        i = 2;
        if (i < L && lp_is_sep(s[i])) {
            tok[k++] = '\\';
            while (i < L && lp_is_sep(s[i])) i++;
        }
        tok[k] = '\0';
        if (n < cap) snprintf(out[n], LP_MAX_PART, "%s", tok);
        n++;
    } else if (L >= 2 && lp_is_sep(s[0]) && lp_is_sep(s[1])) {
        size_t k = 0;
        while (i < L && lp_is_sep(s[i])) i++;
        tok[k++] = '\\';
        tok[k++] = '\\';
        while (i < L && !lp_is_sep(s[i]) && k + 3 < LP_MAX_PART) tok[k++] = (char)tolower((unsigned char)s[i++]);
        while (i < L && lp_is_sep(s[i])) i++;
        tok[k++] = '\\';
        while (i < L && !lp_is_sep(s[i]) && k + 2 < LP_MAX_PART) tok[k++] = (char)tolower((unsigned char)s[i++]);
        tok[k++] = '\\';
        tok[k] = '\0';
        if (n < cap) snprintf(out[n], LP_MAX_PART, "%s", tok);
        n++;
        while (i < L && lp_is_sep(s[i])) i++;
    } else if (L >= 1 && lp_is_sep(s[0])) {
        if (n < cap) snprintf(out[n], LP_MAX_PART, "\\");
        n++;
        while (i < L && lp_is_sep(s[i])) i++;
    }

    while (i < L) {
        size_t k = 0;
        while (i < L && lp_is_sep(s[i])) i++;
        if (i >= L) break;
        while (i < L && !lp_is_sep(s[i])) {
            if (k + 1 < LP_MAX_PART) tok[k++] = (char)tolower((unsigned char)s[i]);
            i++;
        }
        tok[k] = '\0';
        if (!strcmp(tok, ".")) continue;
        if (n < cap) snprintf(out[n], LP_MAX_PART, "%s", tok);
        n++;
    }
    return n;
}

static int lp_same(const char *a, const char *b) {
    static char pa[LP_MAX_PARTS][LP_MAX_PART], pb[LP_MAX_PARTS][LP_MAX_PART];
    int na, nb, i;
    if (!a || !b) return 0;
    na = lp_parts(a, pa, LP_MAX_PARTS);
    nb = lp_parts(b, pb, LP_MAX_PARTS);
    if (na > LP_MAX_PARTS || nb > LP_MAX_PARTS || na != nb) return 0;
    for (i = 0; i < na; i++)
        if (strcmp(pa[i], pb[i])) return 0;
    return 1;
}

/* Python truthiness of a JSON value. */
static int lp_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Remove Hermes-owned PYTHONPATH entries. 0 = ok, 1/2 = env or PYTHONPATH is
 * not the mapping/string live expects (AttributeError there). */
static int lp_strip(const json_t *env, const json_t *owned, const char *sep,
                    int markers, json_t **out) {
    json_t *dst;
    const json_t *pp;
    char d;
    size_t cnt = 1, i, L, j;
    char **entries;
    int *stripped;
    size_t kept = 0;

    *out = NULL;
    if (env && env->type != JSON_OBJECT) return 1;
    dst = env ? json_copy(env) : json_object();
    pp = env ? json_obj_get(env, "PYTHONPATH") : NULL;
    if (!lp_truthy(pp)) {
        if (markers) {
            size_t m;
            for (m = 0; m < sizeof(LP_MARKERS) / sizeof(LP_MARKERS[0]); m++)
                json_obj_del(dst, LP_MARKERS[m]);
        }
        *out = dst;
        return 0;
    }
    if (pp->type != JSON_STRING) {
        json_free(dst);
        return 2;
    }

    d = (sep && sep[0]) ? sep[0] : ';';
    L = strlen(pp->str_val ? pp->str_val : "");
    for (i = 0; i < L; i++)
        if (pp->str_val[i] == d) cnt++;
    entries = (char **)calloc(cnt, sizeof(char *));
    stripped = (int *)calloc(cnt, sizeof(int));
    if (!entries || !stripped) {
        free(entries);
        free(stripped);
        json_free(dst);
        return 3;
    }
    for (i = 0, j = 0; j < cnt; j++) {
        size_t start = i, len;
        while (i < L && pp->str_val[i] != d) i++;
        len = i - start;
        entries[j] = (char *)malloc(len + 1);
        if (entries[j]) {
            memcpy(entries[j], pp->str_val + start, len);
            entries[j][len] = '\0';
        }
        if (i < L) i++;
    }

    for (j = 0; j < cnt; j++) {
        size_t k;
        if (!entries[j] || !entries[j][0]) continue;
        for (k = 0; k < json_len(owned); k++) {
            const json_t *o = json_get(owned, k);
            if (o && o->type == JSON_STRING && o->str_val && lp_same(entries[j], o->str_val)) {
                stripped[j] = 1;
                break;
            }
        }
    }
    for (j = 0; j < cnt; j++)
        if (!stripped[j]) kept++;

    if (kept) {
        size_t total = 2, pos = 0;
        char *joined;
        for (j = 0; j < cnt; j++) {
            if (stripped[j]) continue;
            total += strlen(entries[j] ? entries[j] : "") + 1;
        }
        joined = (char *)malloc(total);
        if (!joined) {
            for (j = 0; j < cnt; j++) free(entries[j]);
            free(entries);
            free(stripped);
            json_free(dst);
            return 3;
        }
        joined[0] = '\0';
        {
            size_t nout = 0;
            for (j = 0; j < cnt; j++) {
                size_t el;
                if (stripped[j]) continue;
                el = strlen(entries[j] ? entries[j] : "");
                if (nout++) joined[pos++] = d;
                memcpy(joined + pos, entries[j] ? entries[j] : "", el);
                pos += el;
            }
        }
        joined[pos] = '\0';
        json_set(dst, "PYTHONPATH", json_string(joined));
        free(joined);
    } else {
        json_obj_del(dst, "PYTHONPATH");
    }

    for (j = 0; j < cnt; j++) free(entries[j]);
    free(entries);
    free(stripped);

    if (markers) {
        for (j = 0; j < sizeof(LP_MARKERS) / sizeof(LP_MARKERS[0]); j++)
            json_obj_del(dst, LP_MARKERS[j]);
    }
    *out = dst;
    return 0;
}

json_t *w26_localpytho_6351f3_same_path(const char *left, const char *right) {
    json_t *o;
    if (!left || !right) {
        o = json_object();
        json_set(o, "error", json_string("TypeError"));
        return o;
    }
    return json_bool(lp_same(left, right) ? 1 : 0);
}

json_t *w26_localpytho_6351f3_strip_pythonpath(const json_t *env,
                                              const json_t *owned_paths,
                                              const char *sep) {
    json_t *env_out = NULL, *res;
    int rc = lp_strip(env, owned_paths, sep, 0, &env_out);
    if (rc == 1 || rc == 2) {
        res = json_object();
        json_set(res, "error", json_string("AttributeError"));
        return res;
    }
    if (rc != 0) return json_null();
    res = json_object();
    json_set(res, "env", env_out);
    return res;
}

json_t *w26_localpytho_6351f3_strip_all(const json_t *env,
                                       const json_t *owned_paths,
                                       const char *sep) {
    json_t *env_out = NULL, *res;
    int rc = lp_strip(env, owned_paths, sep, 1, &env_out);
    if (rc == 1 || rc == 2) {
        res = json_object();
        json_set(res, "error", json_string("AttributeError"));
        return res;
    }
    if (rc != 0) return json_null();
    res = json_object();
    json_set(res, "env", env_out);
    return res;
}

json_t *w26_localpytho_6351f3_constants(void) {
    json_t *o = json_object(), *m = json_array();
    size_t i;
    for (i = 0; i < sizeof(LP_MARKERS) / sizeof(LP_MARKERS[0]); i++)
        json_append(m, json_string(LP_MARKERS[i]));
    json_set(o, "markers", m);
    json_set(o, "is_windows", json_bool(1));
    json_set(o, "pathsep", json_string(";"));
    return o;
}
