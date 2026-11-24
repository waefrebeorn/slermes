/*
 * base_auto_tts.c - C11 port of upstream gateway/platforms/base_auto_tts.py
 * (gate w24_baseautott_534578).
 *
 * PoP: w24_baseautott_534578_synthesize @ gateway/platforms/base_auto_tts.py:synthesize_auto_tts
 * PoP: w24_baseautott_534578_profile_scope @ gateway/platforms/base_auto_tts.py:_profile_scope
 * PoP: w24_baseautott_534578_constants @ gateway/platforms/base_auto_tts.py:_synthesize
 *
 * What is ported is the decision a voice reply's synthesis makes after the
 * text reply already went out: whether synthesis runs at all, which path it
 * requests, and which of the tool's reported files are kept (truthy and
 * present).  The secrets scope, the TTS tool call and the filesystem stay
 * seams: the scope is a callable-or-not flag, the tool's JSON answer is an
 * input, and "exists" is the caller's list of present paths.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_baseautott_534578.h"

/* Python truthiness of a JSON value. */
static int at_truthy(const json_t *v) {
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

/* Path(path).exists() -- the caller reports which paths are present. */
static int at_exists(const json_t *existing, const char *path) {
    size_t i, n = (existing && existing->type == JSON_ARRAY) ? json_len(existing) : 0;
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(existing, i);
        if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, path) == 0) return 1;
    }
    return 0;
}

/* [str(p) for p in raw if p and Path(p).exists()], with raw =
 * tts.get("file_paths") or [tts.get("file_path")].
 * Returns 0 when a reported path is not a str (Path() raises TypeError, so the
 * whole _synthesize call is lost and the caller keeps no paths). */
static int at_collect(json_t *paths, const json_t *tts, const json_t *existing) {
    const json_t *raw = json_obj_get(tts, "file_paths");
    const json_t *success = json_obj_get(tts, "success");
    size_t i, n;

    if (success && !at_truthy(success)) return 1;   /* .get("success", True) falsy */
    if (!at_truthy(raw)) {
        const json_t *fp = json_obj_get(tts, "file_path");
        if (!at_truthy(fp)) return 1;
        if (fp->type != JSON_STRING) return 0;
        if (at_exists(existing, fp->str_val ? fp->str_val : ""))
            json_append(paths, json_string(fp->str_val ? fp->str_val : ""));
        return 1;
    }
    if (raw->type != JSON_ARRAY) return 0;          /* iterating a container: seam */
    n = json_len(raw);
    for (i = 0; i < n; i++) {
        const json_t *p = json_get(raw, i);
        if (!at_truthy(p)) continue;
        if (p->type != JSON_STRING) return 0;
        if (at_exists(existing, p->str_val ? p->str_val : ""))
            json_append(paths, json_string(p->str_val ? p->str_val : ""));
    }
    return 1;
}

json_t *w24_baseautott_534578_synthesize(const json_t *args) {
    const json_t *requirements = json_obj_get(args, "requirements");
    const json_t *clean = json_obj_get(args, "clean_text");
    const json_t *opath = json_obj_get(args, "output_path");
    const json_t *tts = json_obj_get(args, "tts");
    const json_t *existing = json_obj_get(args, "existing");
    json_t *out = json_object(), *paths = json_array();
    json_t *requested = json_null(), *error = json_null();

    if (!at_truthy(requirements)) {
        /* check_tts_requirements() false: nothing attempted, nothing requested */
    } else if (!at_truthy(clean)) {
        error = json_string(W24_BASEAUTOTT_534578_EMPTY_TEXT);
    } else {
        if (opath && opath->type == JSON_STRING) requested = json_string(opath->str_val);
        if (tts && tts->type == JSON_OBJECT) {
            if (!at_collect(paths, tts, existing)) {
                json_free(paths);
                paths = json_array();           /* the raise discards the batch */
                error = json_string("raised");  /* Path(non-str) TypeError seam */
            }
        }
    }

    json_set(out, "paths", paths);
    json_set(out, "requested_path", requested);
    json_set(out, "error", error);
    return out;
}

json_t *w24_baseautott_534578_profile_scope(const json_t *args) {
    const json_t *source = json_obj_get(args, "source");
    int resolver = json_get_bool(args, "resolver", 0);
    int noncallable = json_get_bool(args, "noncallable", 0);
    if (!resolver || noncallable || !source || source->type == JSON_NULL)
        return json_string("nullcontext");
    return json_string("resolve");
}

json_t *w24_baseautott_534578_constants(void) {
    json_t *o = json_object();
    json_set(o, "empty_text", json_string(W24_BASEAUTOTT_534578_EMPTY_TEXT));
    json_set(o, "default_success", json_bool(true));
    return o;
}
