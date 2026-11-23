/*
 * control_resolve.c — C home for upstream Hermes gateway/control_resolve.py.
 *
 * PoP: w9_controlreso_e0962d_plain @ gateway/control_resolve.py:_plain_spelling
 * PoP: w9_controlreso_e0962d_requested @ gateway/control_resolve.py:_requested_home
 * PoP: w9_controlreso_e0962d_constants @ gateway/control_resolve.py:RESOLVE_VERB/RESOLVE_PROTOCOL
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w9_controlreso_e0962d.h"

/* Windows spelling check: no '.'/'..', repeated separator or trailing separator.
 * Mirrors os.path.splitdrive + re.split(r"[\\/]", rest) on os.name == "nt". */
static bool plain_spelling(const char *home) {
    if (!home || !*home) return false;
    size_t i = 0;
    if (((home[0] >= 'A' && home[0] <= 'Z') || (home[0] >= 'a' && home[0] <= 'z')) && home[1] == ':') i = 2;
    const char *rest = home + i;
    if (*rest != '\\' && *rest != '/') return false; /* parts[0] == "" */
    const char *p = rest + 1;
    for (;;) {
        const char *start = p;
        while (*p && *p != '\\' && *p != '/') p++;
        size_t len = (size_t)(p - start);
        if (len == 0 || (len == 1 && start[0] == '.') || (len == 2 && start[0] == '.' && start[1] == '.')) return false;
        if (!*p) break;
        p++;
    }
    return true;
}

/* ntpath.isabs(): splitdrive(home)[1] must start with a backslash. */
static bool isabs_windows(const char *home) {
    if (!home || !*home) return false;
    size_t i = 0;
    if (((home[0] >= 'A' && home[0] <= 'Z') || (home[0] >= 'a' && home[0] <= 'z')) && home[1] == ':') i = 2;
    return home[i] == '\\';
}

static bool integral_num(const json_t *v) {
    if (!v || v->type != JSON_NUMBER) return false;
    long long x = (long long)v->num_val;
    return (double)x == v->num_val;
}

static bool allowed_envelope_key(const char *k) {
    return !strcmp(k, "protocol") || !strcmp(k, "verb") || !strcmp(k, "id") || !strcmp(k, "params");
}

json_t *w9_controlreso_e0962d_plain(const struct json_t *in) {
    return json_bool(plain_spelling(json_get_str(in, "home", "")));
}

json_t *w9_controlreso_e0962d_requested(const struct json_t *in) {
    bool ok = true;
    if (!in || in->type != JSON_OBJECT) ok = false;
    if (ok) {
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !allowed_envelope_key(k)) { ok = false; break; }
        }
    }
    const json_t *proto = ok ? json_obj_get(in, "protocol") : NULL;
    if (!proto || !integral_num(proto) || proto->num_val != 1.0) ok = false;
    const json_t *params = ok ? json_obj_get(in, "params") : NULL;
    if (!params || params->type != JSON_OBJECT) ok = false;
    const json_t *idv = ok ? json_obj_get(in, "id") : NULL;
    if (idv) {
        bool id_ok = (idv->type == JSON_NUMBER && integral_num(idv))
                  || (idv->type == JSON_STRING && idv->str_val && strlen(idv->str_val) <= 128);
        if (!id_ok) ok = false;
    }
    const char *home = NULL;
    if (ok) {
        if (json_object_size(params) != 1 || !json_obj_get(params, "home")) ok = false;
        else {
            const json_t *hv = json_obj_get(params, "home");
            if (!hv || hv->type != JSON_STRING || !hv->str_val) ok = false;
            else {
                home = hv->str_val;
                size_t n = strlen(home);
                if (n == 0 || n > 4096 || !isabs_windows(home) || !plain_spelling(home)) ok = false;
            }
        }
    }
    json_t *out = json_object();
    json_set(out, "ok", json_bool(ok));
    json_set(out, "home", (ok && home) ? json_string(home) : json_null());
    return out;
}

json_t *w9_controlreso_e0962d_constants(void) {
    json_t *out = json_object();
    json_set(out, "verb", json_string("resolve-profile"));
    json_set(out, "protocol", json_number(1.0));
    return out;
}
