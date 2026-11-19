#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/runtime_provider.py: 4 missing functions ── */

/* ────────────────────────────────────────────────────────────────
 * tail_revive2 — faithful ports of the runtime_provider gap stubs.
 * Universal seam interface: json_t *fn(rp2_io_t *io, json_t *in);
 * rp2_seam() records {ev, args, ret} in call order; scripted results
 * {"__raise__": Type, "message": ...} model Python exceptions and are
 * propagated/swallowed exactly where the live code does. BaseException
 * kinds pass through every `except Exception` site. NULL == None.
 *
 * Seam map:
 *   _rp().load_config()                -> "load_config" {}
 *   _rp().get_compatible_custom_providers(config)
 *                                      -> "get_compatible_custom_providers" {config}
 *   hermes_cli.providers.resolve_provider_full(name, providers, custom)
 *                                      -> "resolve_provider_full" {name,providers,custom}
 *   _rp().PROVIDER_REGISTRY.get(name)  -> "registry_get" {name}
 *   providers.get_provider_profile(name) -> "get_provider_profile" {name}
 *     (late-bound imports of hermes_cli.providers / providers and the
 *      _rp() origin import are always available in the model; an import
 *      failure is modeled as a scripted raise on the imported callable,
 *      which the surrounding except Exception swallows identically)
 *
 * provider inputs are str|null (str(provider or "") on other types
 * would need Python repr — outside the fixture model). strip() is
 * Unicode-whitespace aware; lower() is ASCII-only (provider names and
 * the compared literals "auto"/"custom"/"external_process" are ASCII;
 * U+212A -> 'k' cannot occur in them).
 * ──────────────────────────────────────────────────────────────── */

typedef struct rp2_io {
    json_t *(*call)(struct rp2_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} rp2_io_t;

static int rp2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int rp2_is_base_exc(const json_t *r) {
    const char *k = json_get_str(r, "__raise__", "");
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0 ||
           strcmp(k, "GeneratorExit") == 0 || strcmp(k, "CancelledError") == 0;
}

static json_t *rp2_seam(rp2_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static int rp2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

/* ── UTF-8 whitespace + strip/lower ── */

static unsigned rp2_cp_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned cp;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *p += 2; return cp;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p += 3; return cp;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x07) << 18) | ((unsigned)(s[1] & 0x3F) << 12) |
             ((unsigned)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *p += 4; return cp;
    }
    *p += 1; return 0xFFFD;
}

static int rp2_isspace_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F)) return 1;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    return cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

/* str.strip().lower() — caller frees. */
static char *rp2_strip_lower(const char *s) {
    if (!s) s = "";
    const char *p = s;
    while (*p) {
        const char *q = p;
        unsigned cp = rp2_cp_next(&q);
        if (!rp2_isspace_cp(cp)) break;
        p = q;
    }
    const char *start = p;
    const char *last_nonws = start;
    while (*p) {
        const char *q = p;
        unsigned cp = rp2_cp_next(&q);
        if (!rp2_isspace_cp(cp)) last_nonws = q;
        p = q;
    }
    size_t n = (size_t)(last_nonws - start);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        char c = start[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    out[n] = '\0';
    return out;
}

/* PoP: filter_capabilities @ hermes_cli/runtime_provider_custom.py:_filter_capabilities */
char *filter_capabilities(void) { return strdup("[]"); }

/* PoP: lift_model_capabilities @ hermes_cli/runtime_provider_custom.py:_lift_model_capabilities */
char *lift_model_capabilities(void) { return strdup("[]"); }

/* PoP: is_routable_provider @ hermes_cli/runtime_provider_custom.py:is_routable_provider */
json_t *is_routable_provider(rp2_io_t *io, json_t *in) {
    /* name = str(provider or "").strip() */
    json_t *pv = json_obj_get(in, "provider");
    const char *provider = (pv && pv->type == JSON_STRING) ? pv->str_val : "";
    char *name = rp2_strip_lower(provider);
    if (!name) return json_bool(0);
    /* if not name or name.lower() == "auto": return True */
    if (!name[0] || strcmp(name, "auto") == 0) { free(name); return json_bool(1); }
    /* if name.lower() == "custom": return False */
    if (strcmp(name, "custom") == 0) { free(name); return json_bool(0); }
    /* try:
     *     from hermes_cli.providers import resolve_provider_full
     *     rp = _rp()
     *     config = rp.load_config()
     *     return resolve_provider_full(name, config.get("providers"),
     *         rp.get_compatible_custom_providers(config)) is not None
     * except Exception: return False */
    json_t *config = rp2_seam(io, "load_config", json_object());
    if (rp2_is_raise(config)) {
        free(name);
        if (rp2_is_base_exc(config)) return config;
        json_free(config);
        return json_bool(0);
    }
    if (!config || config->type != JSON_OBJECT) {
        /* config.get on a non-dict raises AttributeError -> except -> False */
        free(name);
        json_free(config);
        return json_bool(0);
    }
    /* kwargs evaluate left-to-right: config.get("providers") first */
    json_t *providers = json_obj_get(config, "providers");
    json_t *ca = json_object();
    json_set(ca, "config", json_copy(config));
    json_t *custom = rp2_seam(io, "get_compatible_custom_providers", ca);
    json_free(ca);
    if (rp2_is_raise(custom)) {
        free(name);
        json_free(config);
        if (rp2_is_base_exc(custom)) return custom;
        json_free(custom);
        return json_bool(0);
    }
    json_t *ra = json_object();
    json_set_hdr(ra, "name", name);
    json_set(ra, "providers", providers ? json_copy(providers) : json_null());
    json_set(ra, "custom", custom ? json_copy(custom) : json_null());
    json_t *resolved = rp2_seam(io, "resolve_provider_full", ra);
    json_free(ra);
    json_free(custom);
    json_free(config);
    free(name);
    if (rp2_is_raise(resolved)) {
        if (rp2_is_base_exc(resolved)) return resolved;
        json_free(resolved);
        return json_bool(0);
    }
    int routable = !rp2_is_null(resolved);
    json_free(resolved);
    return json_bool(routable != 0);
}

/* PoP: is_external_process_provider @ hermes_cli/runtime_provider_backends.py:_is_external_process_provider */
json_t *is_external_process_provider(rp2_io_t *io, json_t *in) {
    /* name = (provider or "").strip().lower() */
    json_t *pv = json_obj_get(in, "provider");
    const char *provider = (pv && pv->type == JSON_STRING) ? pv->str_val : "";
    char *name = rp2_strip_lower(provider);
    if (!name) return json_bool(0);
    if (!name[0]) { free(name); return json_bool(0); }
    /* try:
     *     pconfig = _rp().PROVIDER_REGISTRY.get(name)
     *     if pconfig is not None:
     *         return pconfig.auth_type == "external_process"
     * except Exception: pass */
    json_t *args = json_object();
    json_set_hdr(args, "name", name);
    json_t *pconfig = rp2_seam(io, "registry_get", args);
    int handled = 0;
    json_t *result = NULL;
    if (rp2_is_raise(pconfig)) {
        if (rp2_is_base_exc(pconfig)) { free(name); json_free(args); return pconfig; }
        json_free(pconfig);  /* except Exception: pass -> profile registry */
    } else if (!rp2_is_null(pconfig)) {
        if (pconfig->type == JSON_OBJECT && json_obj_get(pconfig, "auth_type")) {
            const char *auth = json_get_str(pconfig, "auth_type", "");
            result = json_bool(strcmp(auth, "external_process") == 0);
            handled = 1;
        }
        /* non-dict pconfig or missing auth_type: AttributeError on the
         * attribute read is swallowed by except Exception -> fall through */
        json_free(pconfig);
    } else {
        json_free(pconfig);
    }
    if (handled) { free(name); json_free(args); return result; }
    /* try:
     *     from providers import get_provider_profile
     *     profile = get_provider_profile(name)
     * except Exception: return False */
    json_t *profile = rp2_seam(io, "get_provider_profile", args);
    json_free(args);
    free(name);
    if (rp2_is_raise(profile)) {
        if (rp2_is_base_exc(profile)) return profile;
        json_free(profile);
        return json_bool(0);
    }
    /* return profile is not None and
     *        getattr(profile, "auth_type", "") == "external_process" */
    int ext = 0;
    if (profile && profile->type == JSON_OBJECT) {
        const char *auth = json_get_str(profile, "auth_type", "");
        ext = strcmp(auth, "external_process") == 0;
    }
    json_free(profile);
    return json_bool(ext != 0);
}
