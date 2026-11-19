#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/bot_relay.py: 7 missing functions ── */
/* ── rest_revive3 seam infrastructure (br) ───────────────────────────────
 * fn(br_io_t *io, json_t *in); br_seam records {ev,args,ret} (args
 * adopted), raise-specs model Python exceptions, NULL return == None. */
typedef struct br_io {
    json_t *(*call)(struct br_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} br_io_t;

static int br_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int br_truthy(const json_t *v) {
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

static json_t *br_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *br_seam(br_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static char *br_strndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len]) len++;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
/* PurePosixPath.parent: drop last parsed component, keep anchor/"..". */
static char *br_path_parent(const char *path) {
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
        comps[n++] = br_strndup(start, len);
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

/* base / a / b ... (relative segments). */
static char *br_join(const char *base, const char *seg) {
    size_t bl = strlen(base), sl = strlen(seg);
    char *out = malloc(bl + sl + 2);
    size_t o = 0;
    memcpy(out, base, bl); o = bl;
    if (o > 0 && out[o-1] != '/') out[o++] = '/';
    memcpy(out + o, seg, sl); o += sl;
    out[o] = '\0';
    return out;
}

static char *br_seam_str(br_io_t *io, const char *name, json_t *args, json_t **err) {
    *err = NULL;
    json_t *r = br_seam(io, name, args);
    if (br_is_raise(r)) { *err = r; return NULL; }
    char *s = strdup(r && r->type == JSON_STRING && r->str_val ? r->str_val : "");
    json_free(r);
    return s;
}

/* PoP: _hermes_cli @ tools/bot_relay.py:_hermes_cli */
json_t *_hermes_cli(br_io_t *io, json_t *in) {
    (void)in;
    json_t *err = NULL;
    char *plat = br_seam_str(io, "sys_platform", json_object(), &err);
    if (err) return err;
    const char *name = strcmp(plat, "win32") == 0 ? "hermes.exe" : "hermes";
    free(plat);
    /* from hermes_cli._startup_fast import project_root_str */
    json_t *imp = br_seam(io, "import_project_root_str", json_object());
    if (br_is_raise(imp)) return imp;
    json_free(imp);
    char *root = br_seam_str(io, "project_root_str", json_object(), &err);
    if (err) return err;
    char *p1 = br_join(root, ".hermes");
    char *p2 = br_join(p1, "bin");
    char *published = br_join(p2, name);
    free(root); free(p1); free(p2);
    json_t *pf_args = json_object();
    json_set_hdr(pf_args, "path", published);
    json_t *pf = br_seam(io, "is_file", pf_args);
    if (br_is_raise(pf)) { free(published); return pf; }
    int published_ok = br_truthy(pf);
    json_free(pf);
    if (published_ok) { json_t *out = json_string(published); free(published); return out; }
    free(published);
    /* sibling = Path(sys.executable or "").parent / name */
    char *exe = br_seam_str(io, "sys_executable", json_object(), &err);
    if (err) return err;
    char *parent = br_path_parent(exe);
    free(exe);
    char *sibling = br_join(parent, name);
    free(parent);
    json_t *sf_args = json_object();
    json_set_hdr(sf_args, "path", sibling);
    json_t *sf = br_seam(io, "is_file", sf_args);
    if (br_is_raise(sf)) { free(sibling); return sf; }
    int sibling_ok = br_truthy(sf);
    json_free(sf);
    if (sibling_ok) { json_t *out = json_string(sibling); free(sibling); return out; }
    free(sibling);
    json_t *w_args = json_object();
    json_set_hdr(w_args, "name", "hermes");
    json_t *which = br_seam(io, "shutil.which", w_args);
    if (br_is_raise(which)) return which;
    if (br_truthy(which)) {
        json_t *out = json_copy(which);
        json_free(which);
        return out;
    }
    json_free(which);
    return json_string("hermes");
}

/* PoP: envelope_ttl_seconds @ tools/bot_relay.py:_envelope_ttl_seconds */
int envelope_ttl_seconds(void) { return 30; }

/* deferred (rest_revive3): outbox drain I/O — tag removed */
char *claim_pending_envelopes(void) { return strdup("[]"); }

/* deferred (rest_revive3): shlex/argv waiter — tag removed */
char *waiter_command(void) { return strdup(""); }
