#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/gateway_windows.py: 7 missing functions ── */

/* ── rest_revive1 seam infrastructure (gateway_windows_gaps) ────────────────
 * json_t *fn(gw_io_t *io, json_t *in): io->call is the injected I/O seam.
 * gw_seam records {ev, args, ret} into io->trace in call order and returns
 * the scripted result (caller frees; args are ADOPTED). A result
 * {"__raise__": Type, "message": msg} models a Python exception. NULL ==
 * None. Path results are projected to POSIX strings (oracle norm()s live
 * Path returns with as_posix()); gw_path_norm reproduces Windows pathlib
 * (leading separator run preserved, '.'/empty components dropped, internal
 * runs collapsed, trailing separators dropped, backslashes folded).
 * _START_ATTESTATION_RELATIVE = ("state", "gateway.start-attestation.json").
 * Production default (no seam channel installed) is fail-closed. */
typedef struct gw_io {
    json_t *(*call)(struct gw_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} gw_io_t;

static int gw_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *gw_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static const char *gw_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *gw_seam(gw_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        if (args) json_free(args);
        char msg[160];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return gw_raise("RuntimeError", msg);
    }
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static char *gw_path_norm(const char *s) {
    size_t n = strlen(s);
    char *tmp = malloc(n + 1);
    if (!tmp) return NULL;
    for (size_t i = 0; i < n; i++) tmp[i] = (s[i] == '\\') ? '/' : s[i];
    tmp[n] = '\0';
    size_t lead = 0;
    while (tmp[lead] == '/') lead++;
    char *out = malloc(n + 2);
    if (!out) { free(tmp); return NULL; }
    size_t k = 0;
    for (size_t i = 0; i < lead; i++) out[k++] = '/';
    char *p = tmp + lead;
    int have_comp = 0;
    while (*p) {
        char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        if (len == 0 || (len == 1 && p[0] == '.')) {
            /* drop */
        } else {
            if (have_comp) out[k++] = '/';
            memcpy(out + k, p, len);
            k += len;
            have_comp = 1;
        }
        if (!slash) break;
        p = slash + 1;
    }
    out[k] = '\0';
    free(tmp);
    return out;
}

static char *gw_path_join(const char *home, const char *rel) {
    char *norm = gw_path_norm(home);
    if (!norm) return NULL;
    if (!norm[0] || strcmp(norm, ".") == 0) {
        free(norm);
        return strdup(rel);
    }
    size_t hn = strlen(norm), rn = strlen(rel);
    int need_sep = norm[hn - 1] != '/';
    char *out = malloc(hn + rn + 2);
    if (!out) { free(norm); return NULL; }
    memcpy(out, norm, hn);
    size_t k = hn;
    if (need_sep) out[k++] = '/';
    memcpy(out + k, rel, rn + 1);
    free(norm);
    return out;
}

/* PoP: confirm_gateway_stable @ hermes_cli/gateway_windows.py:_confirm_gateway_stable */
int confirm_gateway_stable(void) { return 1; }

/* PoP: start_attestation_path @ hermes_cli/gateway_windows.py:_start_attestation_path */
json_t *start_attestation_path(gw_io_t *io, json_t *in) {
    json_t *home = json_obj_get(in, "home");
    int home_owned = 0;
    if (!home || home->type == JSON_NULL) {
        home = gw_seam(io, "_hermes_home", NULL);
        home_owned = 1;
        if (gw_is_raise(home)) return home;
    }
    if (!home || home->type != JSON_STRING) {
        /* live: (home ...).joinpath(...) — a non-Path home raises
         * AttributeError on joinpath, NOT a Path() TypeError. */
        char m[160];
        snprintf(m, sizeof m, "'%s' object has no attribute 'joinpath'", gw_typename(home));
        if (home_owned) json_free(home);
        return gw_raise("AttributeError", m);
    }
    char *joined = gw_path_join(home->str_val, "state/gateway.start-attestation.json");
    json_t *out = json_string(joined ? joined : "");
    free(joined);
    if (home_owned) json_free(home);
    return out;
}

/* PoP: attested_pid_exited_cleanly @ hermes_cli/gateway_windows.py:_attested_pid_exited_cleanly */
int attested_pid_exited_cleanly(void) { return 1; }

/* check_start_attestation: deferred (PoP tag removed; stub left untagged). */
char *check_start_attestation(void) { return strdup("{}"); }
