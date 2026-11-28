/*
 * hermes_constants_scratch_stuck.c — real C11 port of upstream
 * hermes_constants_scratch_stuck.py (gate w5_hermesconst_b6148b).
 *
 * PoP: hcsstuck_is_permission_error @ hermes_constants_scratch_stuck.py:is_permission_error
 * PoP: hcsstuck_accessible_ex @ hermes_constants_scratch_stuck.py:_accessible
 * PoP: hcsstuck_still_stuck @ hermes_constants_scratch_stuck.py:_still_stuck
 * PoP: hcsstuck_shlex_quote @ hermes_constants_scratch_stuck.py:removal_hint
 *
 * Filesystem/process inputs (errno, euid, lstat fingerprint, os.access) are
 * injected at the gate; the decision logic is reproduced exactly.
 */
#include "noc_w5_hermesconst_b6148b.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

#define HCSSTUCK_EACCES 13
#define HCSSTUCK_EPERM  1
#define HCSSTUCK_RECHECK_SECONDS 604800.0

int hcsstuck_is_permission_error(int is_oserror, long long err) {
    return is_oserror && (err == HCSSTUCK_EACCES || err == HCSSTUCK_EPERM);
}

int hcsstuck_accessible_ex(int lexists, int access_ok) {
    return (!lexists) || access_ok;
}

static int json_equal(const json_t *a, const json_t *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return a->str_val && b->str_val && !strcmp(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++)
                if (!json_equal(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++) {
                const char *k = a->c.keys ? a->c.keys[i] : NULL;
                if (!k) return 0;
                const json_t *bv = json_obj_get(b, k);
                if (!bv || !json_equal(a->c.items[i], bv)) return 0;
            }
            return 1;
    }
    return 0;
}

static double py_since(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0.0;
    if (v->type == JSON_BOOL)   return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) return (v->str_val && *v->str_val) ? strtod(v->str_val, NULL) : 0.0;
    return 0.0;
}

int hcsstuck_still_stuck(const char *rec_json, const char *fp_json,
                         long long current_uid, double now,
                         const char *access_json) {
    json_t *rec = json_parse(rec_json ? rec_json : "null", NULL);
    json_t *fp  = json_parse(fp_json ? fp_json : "null", NULL);
    json_t *acc = json_parse(access_json ? access_json : "[]", NULL);
    if (!rec || rec->type != JSON_OBJECT) {
        json_free(rec); json_free(fp); json_free(acc);
        return 0;
    }
    json_t *uid = json_obj_get(rec, "uid");
    int uid_ok = uid && uid->type == JSON_NUMBER && (long long)uid->num_val == current_uid;
    double since = py_since(json_obj_get(rec, "since"));
    int within = (now - since) < HCSSTUCK_RECHECK_SECONDS;
    int fp_ok = json_equal(json_obj_get(rec, "fp"), fp);
    json_t *blockers = json_obj_get(rec, "blockers");
    int blockers_ok = blockers && blockers->type == JSON_ARRAY && blockers->c.count > 0;
    int not_all = 0;
    if (blockers_ok) {
        for (size_t i = 0; i < blockers->c.count; i++) {
            json_t *b = blockers->c.items[i];
            if (!b || b->type != JSON_STRING) continue;
            int ok = 0;
            if (acc && acc->type == JSON_ARRAY && i < acc->c.count && acc->c.items[i])
                ok = (acc->c.items[i]->type == JSON_BOOL && acc->c.items[i]->bool_val);
            if (!ok) { not_all = 1; break; }
        }
    }
    int result = uid_ok && within && fp_ok && blockers_ok && not_all;
    json_free(rec); json_free(fp); json_free(acc);
    return result;
}

static int shlex_safe(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return 1;
    /* python's _find_unsafe is compiled with re.ASCII, so a non-ascii char IS unsafe:
     * shlex.quote("café") returns 'café' quoted. Do not widen this to >= 0x80. */
    return c && strchr("_@%+=:,./-", (int)c) != NULL;
}
char *hcsstuck_shlex_quote(const char *s) {
    if (!s || !*s) {
        char *q = (char *)malloc(3);
        if (q) { q[0] = q[1] = '\''; q[2] = '\0'; }
        return q;
    }
    int all_safe = 1;
    for (const char *p = s; *p; p++) if (!shlex_safe((unsigned char)*p)) { all_safe = 0; break; }
    if (all_safe) return strdup(s);
    size_t n = 2;
    for (const char *p = s; *p; p++) n += (*p == '\'') ? 5u : 1u;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t i = 0;
    out[i++] = '\'';
    for (const char *p = s; *p; p++) {
        if (*p == '\'') { memcpy(out + i, "'\"'\"'", 5); i += 5; }
        else out[i++] = *p;
    }
    out[i++] = '\'';
    out[i] = '\0';
    return out;
}
