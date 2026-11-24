/*
 * update_cmd_fleet_checkout.c -- real C11 port of the checkout-ancestry reader
 * of upstream Hermes hermes_cli/update_cmd_fleet_checkout.py.
 *
 * PoP: w21_updatecmdf_df3d7d_checkout_contains @ hermes_cli/update_cmd_fleet_checkout.py:checkout_contains
 * PoP: w21_updatecmdf_df3d7d_stamped_sha @ hermes_cli/update_cmd_fleet_checkout.py:checkout_contains
 *
 * The two things the reader reaches outside itself are seams here: the build
 * identity dict (build_info.get_code_identity) and the git probe result
 * (update_custody.run_git). Everything the function decides is local.
 */
#include "hermes_json.h"
#include "noc_w21_updatecmdf_df3d7d.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *upd_dup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int upd_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static int upd_is_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* `str(value)` for the scalar stamps a build file carries. */
static char *upd_stamped_sha(const json_t *identity) {
    const json_t *v;
    char buf[64];

    if (!identity || identity->type != JSON_OBJECT) return upd_dup("");
    v = json_obj_get(identity, "sha");
    if (!upd_truthy(v)) return upd_dup("");          /* `... or ""` */
    switch (v->type) {
    case JSON_STRING:
        return upd_dup(v->str_val);
    case JSON_BOOL:
        return upd_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (upd_is_int(v)) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return upd_dup(buf);
    default:
        return upd_dup("");                          /* a container stamp is not a sha */
    }
}

int w21_updatecmdf_df3d7d_checkout_contains(const char *sha, const json_t *identity,
                                            const json_t *probe) {
    const char *want = sha ? sha : "";
    const char *source;

    /* `get_code_identity() or {}`: an absent or falsy identity reads as {}. */
    if (!upd_truthy(identity)) identity = NULL;
    else if (identity->type != JSON_OBJECT) {
        /* `identity.get(...)` on a truthy non-dict raises AttributeError
         * upstream; the reader's contract is fail-closed, so an
         * out-of-contract identity is not evidence of containment. */
        return 0;
    }
    source = identity ? json_get_str(identity, "source", "") : "";
    if (!strcmp(source, "build-file")) {
        char *stamped = upd_stamped_sha(identity);
        int hit = 0;
        if (stamped && stamped[0]) {
            size_t sl = strlen(stamped), wl = strlen(want);
            hit = !strcmp(stamped, want)
               || (wl <= sl && !memcmp(stamped, want, wl))       /* stamped.startswith(sha) */
               || (sl <= wl && !memcmp(stamped, want, sl));      /* sha.startswith(stamped) */
        }
        free(stamped);
        return hit;
    }
    /* `run_git(...)` -> result.returncode == 0; any raise is False. */
    if (!probe || probe->type != JSON_OBJECT) return 0;
    if (json_has(probe, "error")) {
        const json_t *err = json_obj_get(probe, "error");
        if (err && err->type != JSON_NULL) return 0;   /* the probe raised */
    }
    return json_get_num(probe, "returncode", 1) == 0.0 ? 1 : 0;
}
