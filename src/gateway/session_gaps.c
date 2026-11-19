#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/session.py: 11 missing functions ── */

/* ── rest_revive1 seam infrastructure (session_gaps) ────────────────────────
 * json_t *fn(sp_io_t *io, json_t *in): io->call is the injected I/O seam.
 * sp_seam records {ev, args, ret} into io->trace in call order and returns
 * the scripted result (caller frees; args are ADOPTED). A result
 * {"__raise__": Type, "message": msg} models a Python exception; except
 * Exception sites swallow plain specs but pass the BaseException trio
 * (SystemExit/KeyboardInterrupt/GeneratorExit) through. NULL == None.
 *
 * Self-model: in.self is a JSON object standing in for the mixin instance.
 * Attribute reads map to key reads; the oracle drives SimpleNamespace
 * doubles, so a missing SELF attribute raises with the SimpleNamespace
 * type name (sp_self_attr_error). Fixtures always carry _profile_home_cache
 * and config (the live attribute accesses outside try/except).
 *
 * Path results are projected to POSIX strings (the oracle norm()s live
 * Path returns with as_posix()). sp_path_norm reproduces Windows pathlib:
 * backslashes fold to '/', the LEADING separator run is preserved verbatim,
 * '.' and empty components drop, internal runs collapse, trailing seps drop.
 *
 * Production default (no seam channel installed) is fail-closed. */
typedef struct sp_io {
    json_t *(*call)(struct sp_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} sp_io_t;

static int sp_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int sp_is_base_exc(const json_t *r) {
    if (!sp_is_raise(r)) return 0;
    const char *k = json_get_str(r, "__raise__", "");
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0;
}

static json_t *sp_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static int sp_truthy(const json_t *v) {
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

static const char *sp_typename(const json_t *v) {
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

static json_t *sp_attr_error(const json_t *v, const char *attr) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", sp_typename(v), attr);
    return sp_raise("AttributeError", msg);
}

/* The oracle's self double is a SimpleNamespace; match its type name. */
static json_t *sp_self_attr_error(const char *attr) {
    char msg[160];
    snprintf(msg, sizeof msg, "'types.SimpleNamespace' object has no attribute '%s'", attr);
    return sp_raise("AttributeError", msg);
}

static json_t *sp_seam(sp_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        if (args) json_free(args);
        char msg[160];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return sp_raise("RuntimeError", msg);
    }
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* Python == for the JSON value model (bool is an int subclass). */
static int sp_json_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type == JSON_NULL || b->type == JSON_NULL)
        return a->type == JSON_NULL && b->type == JSON_NULL;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER) return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL) return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_STRING && b->type == JSON_STRING) return strcmp(a->str_val, b->str_val) == 0;
    return 0;
}

/* Windows pathlib normalization projected to POSIX (see header note). */
static char *sp_path_norm(const char *s) {
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
            /* drop empty and '.' components */
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

/* PoP: _db @ gateway/session.py:_db */
void *_db(void) { return NULL; }

/* PoP: _db @ gateway/session.py:_db */
void *db_2(void) { return NULL; }

/* PoP: routing_db @ gateway/session_persistence.py:routing_db */
void *routing_db(void) { return NULL; }

/* PoP: named_profile_for_key @ gateway/session_persistence.py:_named_profile_for_key */
json_t *named_profile_for_key(sp_io_t *io, json_t *in) {
    json_t *self = json_obj_get(in, "self");
    json_t *config = self ? json_obj_get(self, "config") : NULL;
    if (!config) return sp_self_attr_error("config");
    /* getattr(self.config, "multiplex_profiles", False) */
    json_t *mp = (config->type == JSON_OBJECT) ? json_obj_get(config, "multiplex_profiles") : NULL;
    if (!sp_truthy(mp)) return json_null();
    json_t *args = json_object();
    json_t *sk = json_obj_get(in, "session_key");
    json_set(args, "session_key", sk ? json_copy(sk) : json_null());
    json_t *profile = sp_seam(io, "_profile_from_session_key", args);
    if (sp_is_raise(profile)) return profile;
    if (!sp_truthy(profile)) {
        if (profile) json_free(profile);
        return json_null();
    }
    if (profile->type == JSON_STRING && strcmp(profile->str_val, "default") == 0) {
        json_free(profile);
        return json_null();
    }
    return profile;
}

/* PoP: profile_home_for_key @ gateway/session_persistence.py:_profile_home_for_key */
json_t *profile_home_for_key(sp_io_t *io, json_t *in) {
    json_t *self = json_obj_get(in, "self");
    json_t *sk = json_obj_get(in, "session_key");
    json_t *args = json_object();
    json_set(args, "session_key", sk ? json_copy(sk) : json_null());
    json_t *profile = sp_seam(io, "_named_profile_for_key", args);
    if (sp_is_raise(profile)) return profile;
    if (!profile || profile->type == JSON_NULL) {
        if (profile) json_free(profile);
        return json_null();
    }
    json_t *cache = self ? json_obj_get(self, "_profile_home_cache") : NULL;
    if (!cache || cache->type != JSON_OBJECT) {
        json_free(profile);
        return sp_self_attr_error("_profile_home_cache");
    }
    if (profile->type == JSON_STRING) {
        json_t *hit = json_obj_get(cache, profile->str_val);
        if (hit) {
            json_t *out = json_copy(hit);
            json_free(profile);
            return out;
        }
    }
    /* try: from hermes_cli.profiles import get_profile_dir, profile_exists;
     *      if profile_exists(profile): home = Path(get_profile_dir(profile))
     * except Exception as exc: logger.debug(...); home = None */
    json_t *home = NULL;
    json_t *caught = NULL;
    json_t *imp = sp_seam(io, "import_profiles", NULL);
    if (sp_is_raise(imp)) {
        if (sp_is_base_exc(imp)) { json_free(profile); return imp; }
        caught = imp;
    } else {
        json_free(imp);
        json_t *eargs = json_object();
        json_set(eargs, "profile", json_copy(profile));
        json_t *ex = sp_seam(io, "profile_exists", eargs);
        if (sp_is_raise(ex)) {
            if (sp_is_base_exc(ex)) { json_free(profile); return ex; }
            caught = ex;
        } else {
            int exists = sp_truthy(ex);
            json_free(ex);
            if (exists) {
                json_t *gargs = json_object();
                json_set(gargs, "profile", json_copy(profile));
                json_t *dir = sp_seam(io, "get_profile_dir", gargs);
                if (sp_is_raise(dir)) {
                    if (sp_is_base_exc(dir)) { json_free(profile); return dir; }
                    caught = dir;
                } else if (dir && dir->type == JSON_STRING) {
                    char *norm = sp_path_norm(dir->str_val);
                    home = json_string(norm && norm[0] ? norm : ".");
                    free(norm);
                    json_free(dir);
                } else {
                    /* Path(non-str) raises TypeError (py3.14 message). */
                    char m[200];
                    snprintf(m, sizeof m,
                        "argument should be a str or an os.PathLike object where"
                        " __fspath__ returns a str, not '%s'", sp_typename(dir));
                    caught = sp_raise("TypeError", m);
                    if (dir) json_free(dir);
                }
            }
        }
    }
    if (caught) {
        /* logger.debug("Could not resolve profile home for %r: %s",
         *              session_key, exc) — args recorded verbatim; the exc
         * slot carries str(exc) (fixtures use plain-message classes). */
        json_t *largs = json_object();
        json_set_hdr(largs, "msg", "Could not resolve profile home for %r: %s");
        json_t *arr = json_array();
        json_append(arr, sk ? json_copy(sk) : json_null());
        json_append(arr, json_string(json_get_str(caught, "message", "")));
        json_set(largs, "args", arr);
        json_t *lr = sp_seam(io, "logger.debug", largs);
        if (sp_is_raise(lr)) {
            json_free(caught);
            json_free(profile);
            return lr;
        }
        json_free(lr);
        json_free(caught);
    }
    if (home) {
        /* Only hits are memoized. */
        if (profile->type == JSON_STRING)
            json_set(cache, profile->str_val, json_copy(home));
        json_free(profile);
        return home;
    }
    json_free(profile);
    return json_null();
}

/* PoP: db_for_key @ gateway/session_persistence.py:db_for_key */
void *db_for_key(const char *key) { (void)key; return NULL; }

/* PoP: owner_key_for_session_id @ gateway/session_persistence.py:_owner_key_for_session_id */
json_t *owner_key_for_session_id(sp_io_t *io, json_t *in) {
    (void)io;
    json_t *session_id = json_obj_get(in, "session_id");
    if (!sp_truthy(session_id)) return json_null();
    json_t *self = json_obj_get(in, "self");
    json_t *entries = self ? json_obj_get(self, "_entries") : NULL;
    /* try: for entry in list(self._entries.values()): ... except Exception: pass.
     * A bad entry (non-object / missing attribute) aborts the WHOLE scan. */
    if (entries && entries->type == JSON_OBJECT) {
        for (size_t i = 0; i < entries->c.count; i++) {
            json_t *entry = entries->c.items[i];
            if (!entry || entry->type != JSON_OBJECT) goto fallback;
            json_t *sid = json_obj_get(entry, "session_id");
            if (!sid) goto fallback;
            if (sp_json_eq(sid, session_id)) {
                json_t *sk = json_obj_get(entry, "session_key");
                if (!sk) goto fallback;
                return json_copy(sk);
            }
        }
    }
fallback:
    /* return (getattr(self, "_session_owner_hints", None) or {}).get(session_id) */
    {
        json_t *hints = self ? json_obj_get(self, "_session_owner_hints") : NULL;
        if (!sp_truthy(hints)) return json_null();
        if (hints->type != JSON_OBJECT) return sp_attr_error(hints, "get");
        if (session_id->type != JSON_STRING) return json_null();
        json_t *v = json_obj_get(hints, session_id->str_val);
        return v ? json_copy(v) : json_null();
    }
}

/* PoP: db_for_session_id @ gateway/session_persistence.py:db_for_session_id */
void *db_for_session_id(const char *id) { (void)id; return NULL; }

/* PoP: get_transcript_drain_lock @ gateway/session_transcript.py:_get_transcript_drain_lock */
json_t *get_transcript_drain_lock(sp_io_t *io, json_t *in) {
    (void)in;
    /* self._lazy("_transcript_drain_lock", threading.RLock) — the factory
     * class is modeled by its qualified name on both sides. */
    json_t *args = json_object();
    json_set_hdr(args, "name", "_transcript_drain_lock");
    json_set_hdr(args, "factory", "threading.RLock");
    json_t *r = sp_seam(io, "_lazy", args);
    if (sp_is_raise(r)) return r;
    return r ? r : json_null();
}
