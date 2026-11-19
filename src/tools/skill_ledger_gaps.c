#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/skill_ledger.py: 6 missing functions ── */

/* ── rest_revive3 seam infrastructure (sl) ───────────────────────────────
 * fn(sl_io_t *io, json_t *in); sl_seam records {ev,args,ret} (args
 * adopted), raise-specs model Python exceptions, NULL return == None. */
typedef struct sl_io {
    json_t *(*call)(struct sl_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} sl_io_t;

static int sl_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int sl_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static json_t *sl_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *sl_seam(sl_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static char *sl_strndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len]) len++;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

/* POSIX os.path.normpath (collapses //, ., .. ; preserves a leading "//"). */
static char *sl_normpath(const char *path) {
    if (!path || !path[0]) return strdup(".");
    int initial = 0;
    if (path[0] == '/') {
        initial = 1;
        if (path[1] == '/' && path[2] != '/') initial = 2;
    }
    /* component stack */
    char **comps = NULL; size_t n = 0, cap = 0;
    const char *p = path;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;
        int is_dotdot = (len == 2 && start[0] == '.' && start[1] == '.');
        int keep;
        if (!is_dotdot) keep = 1;
        else if (!initial && n == 0) keep = 1;
        else if (n > 0 && strcmp(comps[n-1], "..") == 0) keep = 1;
        else keep = 0;
        if (keep) {
            if (n == cap) { cap = cap ? cap * 2 : 8; comps = realloc(comps, cap * sizeof *comps); }
            comps[n++] = sl_strndup(start, len);
        } else if (n > 0) {
            free(comps[--n]);
        }
    }
    size_t total = (size_t)initial + 1;
    for (size_t i = 0; i < n; i++) total += strlen(comps[i]) + 1;
    char *out = malloc(total + 1);
    size_t o = 0;
    for (int i = 0; i < initial; i++) out[o++] = '/';
    for (size_t i = 0; i < n; i++) {
        if (i) out[o++] = '/';
        size_t l = strlen(comps[i]);
        memcpy(out + o, comps[i], l); o += l;
    }
    out[o] = '\0';
    for (size_t i = 0; i < n; i++) free(comps[i]);
    free(comps);
    if (o == 0) { out[0] = '.'; out[1] = '\0'; }
    return out;
}

/* Split a normalized path into anchor (leading slashes, possibly "") and the
 * component list. Returns component count; comps[] malloc'd. */
static size_t sl_components(const char *np, char **anchor, char ***comps) {
    size_t a = 0;
    while (np[a] == '/') a++;
    *anchor = sl_strndup(np, a);
    char **cs = NULL; size_t n = 0, cap = 0;
    const char *p = np + a;
    while (*p) {
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - start);
        if (len > 0) {
            if (n == cap) { cap = cap ? cap * 2 : 8; cs = realloc(cs, cap * sizeof *cs); }
            cs[n++] = sl_strndup(start, len);
        }
        while (*p == '/') p++;
    }
    *comps = cs;
    return n;
}

/* _rel_posix: normpath both, relative_to; malloc'd posix rel or NULL. */
static char *sl_rel_posix(const char *path, const char *root) {
    char *np = sl_normpath(path);
    char *nr = sl_normpath(root);
    char *ap, *ar; char **cp, **cr;
    size_t npn = sl_components(np, &ap, &cp);
    size_t nrn = sl_components(nr, &ar, &cr);
    char *result = NULL;
    if (strcmp(ap, ar) == 0 && nrn <= npn) {
        int ok = 1;
        for (size_t i = 0; i < nrn; i++) if (strcmp(cp[i], cr[i]) != 0) { ok = 0; break; }
        if (ok) {
            if (npn == nrn) result = strdup(".");
            else {
                size_t total = 1;
                for (size_t i = nrn; i < npn; i++) total += strlen(cp[i]) + 1;
                result = malloc(total + 1);
                size_t o = 0;
                for (size_t i = nrn; i < npn; i++) {
                    if (i > nrn) result[o++] = '/';
                    size_t l = strlen(cp[i]); memcpy(result + o, cp[i], l); o += l;
                }
                result[o] = '\0';
            }
        }
    }
    free(ap); free(ar);
    for (size_t i = 0; i < npn; i++) free(cp[i]);
    for (size_t i = 0; i < nrn; i++) free(cr[i]);
    free(cp); free(cr); free(np); free(nr);
    return result;
}

static int sl_is_within(const char *root, const char *path) {
    char *r = sl_rel_posix(path, root);
    int within = r != NULL;
    free(r);
    return within;
}

/* PurePosixPath.name: last parsed component (drop "."/empty, keep "..");
 * "" when there is no component. */
static char *sl_path_name(const char *path) {
    char *last = NULL;
    const char *p = path ? path : "";
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;
        free(last);
        last = sl_strndup(start, len);
    }
    return last ? last : strdup("");
}

/* PurePosixPath.parent: drop the last parsed component, keep the anchor
 * ( "/" or "//" ) and any ".."; "." when no component remains. */
static char *sl_path_parent(const char *path) {
    const char *p = path ? path : "";
    int anchor = 0;
    if (p[0] == '/') { anchor = 1; if (p[1] == '/' && p[2] != '/') anchor = 2; }
    char **comps = NULL; size_t n = 0, cap = 0;
    const char *q = p;
    while (*q) {
        while (*q == '/') q++;
        if (!*q) break;
        const char *start = q;
        while (*q && *q != '/') q++;
        size_t len = (size_t)(q - start);
        if (len == 1 && start[0] == '.') continue;
        if (n == cap) { cap = cap ? cap*2 : 8; comps = realloc(comps, cap*sizeof *comps); }
        comps[n++] = sl_strndup(start, len);
    }
    size_t pn = n > 0 ? n - 1 : 0;
    size_t total = (size_t)anchor + 1;
    for (size_t i = 0; i < pn; i++) total += strlen(comps[i]) + 1;
    char *result = malloc(total + 2);
    size_t o = 0;
    for (int i = 0; i < anchor; i++) result[o++] = '/';
    for (size_t i = 0; i < pn; i++) {
        if (i) result[o++] = '/';
        size_t l = strlen(comps[i]); memcpy(result+o, comps[i], l); o += l;
    }
    result[o] = '\0';
    if (o == 0) { result[0] = '.'; result[1] = '\0'; }
    for (size_t i = 0; i < n; i++) free(comps[i]);
    free(comps);
    return result;
}

/* joinpath: base + parts (relative, non-empty), single slashes. */
static char *sl_joinpath(const char *base, char **parts, size_t nparts) {
    size_t total = strlen(base) + 1;
    for (size_t i = 0; i < nparts; i++) total += strlen(parts[i]) + 1;
    char *out = malloc(total + 2);
    size_t o = 0;
    size_t bl = strlen(base);
    memcpy(out, base, bl); o = bl;
    for (size_t i = 0; i < nparts; i++) {
        if (!parts[i][0]) continue;
        if (o > 0 && out[o-1] != '/') out[o++] = '/';
        size_t l = strlen(parts[i]); memcpy(out + o, parts[i], l); o += l;
    }
    out[o] = '\0';
    if (o == 0) { out[0] = '.'; out[1] = '\0'; }
    return out;
}

/* _ARCHIVE_TS_SUFFIX_RE = ^(.+)-\d{14}$ ; group(1) or name. */
static char *sl_strip_ts(const char *name) {
    size_t n = strlen(name);
    if (n >= 16 && name[n-15] == '-') {
        int digits = 1;
        for (size_t i = n - 14; i < n; i++) if (name[i] < '0' || name[i] > '9') { digits = 0; break; }
        if (digits) return sl_strndup(name, n - 15);
    }
    return strdup(name);
}

static int sl_non_package_top(const char *seg) {
    return strcmp(seg, ".curator_backups") == 0 || strcmp(seg, ".hub") == 0 ||
           strcmp(seg, ".archive") == 0 || strcmp(seg, ".locks") == 0;
}

/* get_hermes_home() seam -> home string (owned). */
static char *sl_get_home(sl_io_t *io) {
    json_t *r = sl_seam(io, "get_hermes_home", json_object());
    if (sl_is_raise(r)) { json_free(r); return NULL; }
    char *s = strdup(r && r->type == JSON_STRING && r->str_val ? r->str_val : "");
    json_free(r);
    return s;
}

/* _skills_dir() = get_hermes_home()/"skills". NULL on raise (propagated). */
static char *sl_skills_dir(sl_io_t *io, json_t **err) {
    *err = NULL;
    json_t *r = sl_seam(io, "get_hermes_home", json_object());
    if (sl_is_raise(r)) { *err = r; return NULL; }
    const char *home = r && r->type == JSON_STRING && r->str_val ? r->str_val : "";
    char *skills = sl_joinpath(home, NULL, 0);
    /* append "skills" */
    size_t hl = strlen(skills);
    char *out = malloc(hl + 8);
    snprintf(out, hl + 8, "%s%sskills", skills, (hl > 0 && skills[hl-1] == '/') ? "" : "/");
    free(skills);
    json_free(r);
    return out;
}

/* _package_rel(root): posix rel under skills, minus non-package tops. */
static char *sl_package_rel(sl_io_t *io, const char *root, json_t **err) {
    *err = NULL;
    char *skills = sl_skills_dir(io, err);
    if (!skills) return NULL;
    char *rel = sl_rel_posix(root, skills);
    free(skills);
    if (!rel) return NULL;
    /* strip("/") */
    char *p = rel;
    while (*p == '/') p++;
    size_t l = strlen(p);
    while (l > 0 && p[l-1] == '/') l--;
    char *stripped = sl_strndup(p, l);
    free(rel);
    if (!stripped[0]) { free(stripped); return NULL; }
    /* first segment */
    char *slash = strchr(stripped, '/');
    char *first = slash ? sl_strndup(stripped, (size_t)(slash - stripped)) : strdup(stripped);
    int bad = sl_non_package_top(first);
    free(first);
    if (bad) { free(stripped); return NULL; }
    return stripped;
}

/* PoP: strip_archive_timestamp @ tools/skill_ledger.py:_strip_archive_timestamp */
json_t *strip_archive_timestamp(void *io, json_t *in) {
    (void)io;
    const char *name = json_get_str(in, "name", "");
    char *r = sl_strip_ts(name);
    json_t *out = json_string(r);
    free(r);
    return out;
}

/* PoP: package_rel @ tools/skill_ledger.py:_package_rel */
json_t *package_rel(sl_io_t *io, json_t *in) {
    const char *root = json_get_str(in, "root", "");
    json_t *err = NULL;
    char *r = sl_package_rel(io, root, &err);
    if (err) return err;
    json_t *out = r ? json_string(r) : json_null();
    free(r);
    return out;
}

/* _skill_md_parents(before): parents of items whose path name == SKILL.md. */
static char **sl_skill_md_parents(const json_t *before, size_t *n) {
    *n = 0;
    if (!before || before->type != JSON_ARRAY) return NULL;
    char **out = NULL; size_t cap = 0;
    for (size_t i = 0; i < json_len(before); i++) {
        const json_t *item = json_get(before, i);
        const json_t *pn = json_obj_get(item, "path");
        char *path = pn ? (pn->type == JSON_STRING ? strdup(pn->str_val ? pn->str_val : "") : strdup("")) : strdup("");
        char *name = sl_path_name(path);
        if (strcmp(name, "SKILL.md") == 0) {
            char *parent = sl_path_parent(path);
            if (*n == cap) { cap = cap ? cap * 2 : 8; out = realloc(out, cap * sizeof *out); }
            out[(*n)++] = parent;
        }
        free(name); free(path);
    }
    return out;
}

/* PoP: package_prefixes @ tools/skill_ledger.py:package_prefixes */
json_t *package_prefixes(sl_io_t *io, json_t *in) {
    const json_t *rootn = json_obj_get(in, "root");
    int has_root = rootn && rootn->type != JSON_NULL;
    const char *root = has_root ? (rootn->type == JSON_STRING ? rootn->str_val : "") : NULL;
    const json_t *skilln = json_obj_get(in, "skill");
    int has_skill = skilln && skilln->type == JSON_STRING && skilln->str_val && skilln->str_val[0];
    const char *skill = has_skill ? skilln->str_val : NULL;
    const json_t *before = json_obj_get(in, "before");

    /* candidates list (strings or NULL), order preserved */
    char **cand = NULL; size_t nc = 0, cc = 0;
    #define PUSH_CAND(s) do { if (nc == cc) { cc = cc ? cc*2 : 8; cand = realloc(cand, cc*sizeof *cand); } cand[nc++] = (s); } while (0)
    json_t *err = NULL;
    if (has_root) {
        char *pr = sl_package_rel(io, root, &err);
        if (err) { for (size_t i=0;i<nc;i++) free(cand[i]); free(cand); return err; }
        PUSH_CAND(pr);
    } else PUSH_CAND(NULL);
    size_t nparents = 0;
    char **parents = sl_skill_md_parents(before, &nparents);
    for (size_t i = 0; i < nparents; i++) {
        char *pr = sl_package_rel(io, parents[i], &err);
        if (err) { for (size_t k=0;k<nc;k++) free(cand[k]); free(cand); for (size_t k=0;k<nparents;k++) free(parents[k]); free(parents); return err; }
        PUSH_CAND(pr);
    }
    for (size_t i = 0; i < nparents; i++) free(parents[i]);
    free(parents);
    PUSH_CAND(has_skill ? strdup(skill) : NULL);
    PUSH_CAND(has_skill ? sl_strip_ts(skill) : NULL);
    #undef PUSH_CAND

    /* dict.fromkeys over (c or "").strip("/") filtered truthy */
    json_t *out = json_array();
    char **seen = NULL; size_t ns = 0, cs = 0;
    for (size_t i = 0; i < nc; i++) {
        const char *c = cand[i] ? cand[i] : "";
        while (*c == '/') c++;
        size_t l = strlen(c);
        while (l > 0 && c[l-1] == '/') l--;
        if (l == 0) continue;
        char *p = sl_strndup(c, l);
        int dup = 0;
        for (size_t k = 0; k < ns; k++) if (strcmp(seen[k], p) == 0) { dup = 1; break; }
        if (!dup) {
            if (ns == cs) { cs = cs ? cs*2 : 8; seen = realloc(seen, cs*sizeof *seen); }
            seen[ns++] = p;
            json_append(out, json_string(p));
        } else free(p);
    }
    for (size_t i = 0; i < nc; i++) free(cand[i]);
    free(cand);
    for (size_t i = 0; i < ns; i++) free(seen[i]);
    free(seen);
    return out;
}

/* PoP: fill_snapshot_from_curator_backup @ tools/skill_ledger.py:fill_snapshot_from_curator_backup */
json_t *fill_snapshot_from_curator_backup(sl_io_t *io, json_t *in) {
    const json_t *rootn = json_obj_get(in, "root");
    int has_root = rootn && rootn->type != JSON_NULL;
    const char *root = has_root ? (rootn->type == JSON_STRING ? rootn->str_val : "") : NULL;
    const json_t *existing = json_obj_get(in, "existing");
    const json_t *skilln = json_obj_get(in, "skill");

    /* out = list(existing or []) */
    json_t *out = json_array();
    if (existing && existing->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(existing); i++) json_append(out, json_copy(json_get(existing, i)));

    /* prefixes = package_prefixes(root, skill, out) */
    json_t *pp_in = json_object();
    json_set(pp_in, "root", rootn ? json_copy(rootn) : json_null());
    json_set(pp_in, "skill", skilln ? json_copy(skilln) : json_null());
    json_set(pp_in, "before", json_copy(out));
    json_t *prefixes = package_prefixes(io, pp_in);
    json_free(pp_in);
    if (sl_is_raise(prefixes)) { json_free(out); return prefixes; }
    if (json_len(prefixes) == 0) { json_free(prefixes); return out; }

    /* extra = _read_package_files_from_latest_backup(prefixes) */
    json_t *rp_args = json_object();
    json_set(rp_args, "prefixes", json_copy(prefixes));
    json_free(prefixes);
    json_t *extra = sl_seam(io, "_read_package_files_from_latest_backup", rp_args);
    if (sl_is_raise(extra)) { json_free(extra); return out; }  /* except Exception -> return out */
    if (!extra || extra->type != JSON_OBJECT || extra->c.count == 0) { json_free(extra); return out; }

    /* skills = _skills_dir() */
    json_t *err = NULL;
    char *skills = sl_skills_dir(io, &err);
    if (err) { json_free(extra); json_free(out); return err; }

    /* dest_root + pkg_names */
    char *dest_root_name = has_root ? sl_path_name(root) : NULL;
    char *dest_root_stripped = has_root ? sl_strip_ts(dest_root_name) : NULL;

    /* have = {_rel_posix(str(i.get("path","")), skills) for i in out} minus None */
    char **have = NULL; size_t nh = 0, ch = 0;
    for (size_t i = 0; i < json_len(out); i++) {
        const json_t *item = json_get(out, i);
        const json_t *pn = json_obj_get(item, "path");
        char *path = pn ? (pn->type == JSON_STRING ? strdup(pn->str_val ? pn->str_val : "") : strdup("")) : strdup("");
        char *rel = sl_rel_posix(path, skills);
        free(path);
        if (rel) {
            if (nh == ch) { ch = ch ? ch*2 : 8; have = realloc(have, ch*sizeof *have); }
            have[nh++] = rel;
        }
    }

    for (size_t ei = 0; ei < extra->c.count; ei++) {
        const char *rel = extra->c.keys[ei];
        const json_t *data = extra->c.items[ei];
        /* parts = rel.split("/") */
        char **parts = NULL; size_t np = 0, cp = 0;
        const char *p = rel;
        while (1) {
            const char *slash = strchr(p, '/');
            size_t len = slash ? (size_t)(slash - p) : strlen(p);
            if (np == cp) { cp = cp ? cp*2 : 8; parts = realloc(parts, cp*sizeof *parts); }
            parts[np++] = sl_strndup(p, len);
            if (!slash) break;
            p = slash + 1;
        }
        size_t start = 0;
        if (has_root && np > 0 &&
            (strcmp(parts[0], dest_root_name) == 0 || strcmp(parts[0], dest_root_stripped) == 0))
            start = 1;
        if (start >= np) { for (size_t k=0;k<np;k++) free(parts[k]); free(parts); continue; }
        /* dest = (dest_root or skills).joinpath(*parts[start:]) */
        const char *base = has_root ? root : skills;
        char *dest = sl_joinpath(base, parts + start, np - start);
        for (size_t k=0;k<np;k++) free(parts[k]); free(parts);
        /* if not _is_within(skills, dest) or not _is_within(get_hermes_home(), dest): continue */
        if (!sl_is_within(skills, dest)) { free(dest); continue; }
        char *home = sl_get_home(io);
        if (!home) { free(dest); continue; }
        int within_home = sl_is_within(home, dest);
        free(home);
        if (!within_home) { free(dest); continue; }
        /* rel_key = _rel_posix(dest, skills) */
        char *rel_key = sl_rel_posix(dest, skills);
        if (!rel_key) { free(dest); continue; }
        int already = 0;
        for (size_t k = 0; k < nh; k++) if (strcmp(have[k], rel_key) == 0) { already = 1; break; }
        if (already) { free(rel_key); free(dest); continue; }
        /* out.append({"path": str(dest), "sha256": _store_blob(data)}) */
        json_t *sb_args = json_object();
        json_set(sb_args, "data", json_copy(data));
        json_t *hash = sl_seam(io, "_store_blob", sb_args);
        if (sl_is_raise(hash)) { json_free(hash); free(rel_key); free(dest); continue; }
        json_t *entry = json_object();
        json_set_hdr(entry, "path", dest);
        json_set(entry, "sha256", hash ? hash : json_null());
        json_append(out, entry);
        if (nh == ch) { ch = ch ? ch*2 : 8; have = realloc(have, ch*sizeof *have); }
        have[nh++] = rel_key;
        free(dest);
    }
    for (size_t i = 0; i < nh; i++) free(have[i]);
    free(have);
    free(skills); free(dest_root_name); free(dest_root_stripped);
    json_free(extra);
    return out;
}

/* PoP: latest_skills_tarball @ tools/skill_ledger.py:latest_skills_tarball */
char *latest_skills_tarball(void) { return strdup(""); }

/* deferred (rest_revive3): tarfile I/O port pending — tag removed */
char *read_package_files_from_latest_backup(void) { return strdup("[]"); }
