/*
 * port_update_pause_record.c — see port_update_pause_record.h.
 */

#include "port_update_pause_record.h"

#include "libhash/hash.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 *  _install_key  (update_pause_record.py:_install_key)
 *
 *      hashlib.sha256(os.path.normcase(str(root)).encode("utf-8")).hexdigest()[:12]
 *
 *  os.path.normcase on Windows: lowercase, and '/' -> '\'. Everything else is
 *  unchanged (on POSIX normcase is the identity, so the lowercase would be
 *  wrong there -- this port follows the Windows behaviour the record uses).
 * =========================================================================== */

/* PoP: upr_install_key @ hermes_cli/update_pause_record.py:_install_key */
void upr_install_key(const char *root, char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    out[0] = '\0';
    if (!root)
        return;

    /* normcase: lowercase + forward slash -> backslash, into a malloc'd copy */
    size_t n = strlen(root);
    char *norm = (char *)malloc(n + 1);
    if (!norm)
        return;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)root[i];
        norm[i] = (ch == '/') ? '\\' : (char)tolower(ch);
    }
    norm[n] = '\0';

    char *hex = hash_sha256_hex((const unsigned char *)norm, n);
    free(norm);
    if (!hex)
        return;
    snprintf(out, outsz, "%.12s", hex);
    free(hex);
}

/* ===========================================================================
 *  _digest  (update_pause_record.py:_digest)
 *
 *      try: return hashlib.sha256(path.read_bytes()).hexdigest()
 *      except OSError: return None
 * =========================================================================== */

/* PoP: upr_digest_file @ hermes_cli/update_pause_record.py:_digest */
char *upr_digest_file(const char *path)
{
    if (!path)
        return NULL;
    return hash_sha256_file(path);   /* NULL on any read error */
}

/* ===========================================================================
 *  JSON truthiness -- Python's `or` chain in _has_work / bool().
 * =========================================================================== */

static int json_truthy(const json_t *v)
{
    if (!v)
        return 0;
    switch (v->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0;
    case JSON_STRING: return (v->str_val && *v->str_val) ? 1 : 0;
    case JSON_ARRAY: return v->c.count > 0;
    case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

/* ===========================================================================
 *  _has_work  (update_pause_record.py:_has_work)
 *
 *      bool(token.get("profiles") or any(u.get("argv") for u in unmapped)
 *           or token.get("services") or token.get("posix_units")
 *           or token.get("cold_start_if_installed") or token.get("cold_start_profiles"))
 * =========================================================================== */

/* PoP: upr_has_work @ hermes_cli/update_pause_record.py:_has_work */
int upr_has_work(const json_t *token)
{
    if (!token || token->type != JSON_OBJECT)
        return 0;
    if (json_truthy(json_obj_get(token, "profiles")))
        return 1;

    json_t *unmapped = json_obj_get(token, "unmapped");
    if (unmapped && unmapped->type == JSON_ARRAY) {
        for (size_t i = 0; i < unmapped->c.count; i++) {
            json_t *u = unmapped->c.items[i];
            if (u && u->type == JSON_OBJECT && json_truthy(json_obj_get(u, "argv")))
                return 1;
        }
    }

    return json_truthy(json_obj_get(token, "services"))
        || json_truthy(json_obj_get(token, "posix_units"))
        || json_truthy(json_obj_get(token, "cold_start_if_installed"))
        || json_truthy(json_obj_get(token, "cold_start_profiles"));
}

/* ===========================================================================
 *  _without  (update_pause_record.py:_without)
 *
 *      token["profiles"] = {p: pid for ... if str(pid) not in pids}
 *      token["unmapped"] = [u for u ... if str(u.get("pid")) not in pids]
 *      if "posix_units" in token: same filter
 *
 *  `pids` carries the already-stringified pids, matching Python's `str(pid)`.
 * =========================================================================== */

static int in_pids(const char *value, const char *const *pids, size_t n)
{
    if (!value)
        return 0;
    for (size_t i = 0; i < n; i++) {
        if (pids[i] && strcmp(value, pids[i]) == 0)
            return 1;
    }
    return 0;
}

/* A pid rendered the way Python's str() would render it: JSON numbers lose
 * their trailing ".0" (json_t keeps only a double). */
static char *pid_str(const json_t *v, char *buf, size_t bufsz)
{
    if (!v)
        return NULL;
    if (v->type == JSON_STRING && v->str_val)
        return v->str_val;
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) {
            snprintf(buf, bufsz, "%lld", (long long)d);
        } else {
            snprintf(buf, bufsz, "%.17g", d);
        }
        return buf;
    }
    if (v->type == JSON_BOOL)
        return v->bool_val ? "true" : "false";
    return NULL;
}

/* PoP: upr_without @ hermes_cli/update_pause_record.py:_without */
json_t *upr_without(json_t *token, const char *const *pids, size_t n)
{
    if (!token || token->type != JSON_OBJECT)
        return token;

    /* profiles: Python assigns unconditionally (`token.get("profiles") or {}`)
     * so a token without the key gains an empty object. */
    json_t *profiles = json_obj_get(token, "profiles");
    json_t *kept = json_object();
    if (profiles && profiles->type == JSON_OBJECT) {
        for (size_t i = 0; i < profiles->c.count; i++) {
            char buf[64];
            char *s = pid_str(profiles->c.items[i], buf, sizeof(buf));
            if (!in_pids(s, pids, n))
                json_set(kept, profiles->c.keys[i], json_copy(profiles->c.items[i]));
        }
    }
    json_obj_del(token, "profiles");
    json_set(token, "profiles", kept);

    /* unmapped: same, always assigned. */
    json_t *unmapped = json_obj_get(token, "unmapped");
    json_t *kept_u = json_array();
    if (unmapped && unmapped->type == JSON_ARRAY) {
        for (size_t i = 0; i < unmapped->c.count; i++) {
            json_t *u = unmapped->c.items[i];
            char buf[64];
            char *s = NULL;
            if (u && u->type == JSON_OBJECT)
                s = pid_str(json_obj_get(u, "pid"), buf, sizeof(buf));
            if (!in_pids(s, pids, n))
                json_append(kept_u, json_copy(u));
        }
    }
    json_obj_del(token, "unmapped");
    json_set(token, "unmapped", kept_u);

    /* posix_units: only when the key is present. */
    if (json_obj_get(token, "posix_units")) {
        json_t *units = json_obj_get(token, "posix_units");
        if (units && units->type == JSON_ARRAY) {
            json_t *kept_p = json_array();
            for (size_t i = 0; i < units->c.count; i++) {
                json_t *u = units->c.items[i];
                char buf[64];
                char *s = NULL;
                if (u && u->type == JSON_OBJECT)
                    s = pid_str(json_obj_get(u, "pid"), buf, sizeof(buf));
                if (!in_pids(s, pids, n))
                    json_append(kept_p, json_copy(u));
            }
            json_obj_del(token, "posix_units");
            json_set(token, "posix_units", kept_p);
        }
    }

    return token;
}

/* ===========================================================================
 *  Path suffixes. Python's Path.with_name / with_suffix, which work on the
 *  whole name rather than the extension.
 * =========================================================================== */

/* PoP: upr_accepted_path @ hermes_cli/update_pause_record.py:_accepted_path */
void upr_accepted_path(const char *path, char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    out[0] = '\0';
    if (!path)
        return;
    snprintf(out, outsz, "%s.accepted", path);
}

/* PoP: upr_retired_path @ hermes_cli/update_pause_record.py:_retired_path */
void upr_retired_path(const char *path, char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    out[0] = '\0';
    if (!path) {
        return;
    }
    /* Path.with_suffix(".retired") replaces the last dot-suffix, or appends
     * when there is none; a leading dot is part of the stem, not a suffix. */
    const char *slash = strrchr(path, '/');
    const char *slash2 = strrchr(path, '\\');
    if (slash2 && (!slash || slash2 > slash))
        slash = slash2;
    const char *name = slash ? slash + 1 : path;
    const char *dot = strrchr(name, '.');
    if (dot && dot != name) {
        size_t stem = (size_t)(dot - path);
        snprintf(out, outsz, "%.*s.retired", (int)stem, path);
    } else {
        snprintf(out, outsz, "%s.retired", path);
    }
}

/* ===========================================================================
 *  identity_is_live  (update_pause_record.py:identity_is_live)
 *
 *      update_lock.incarnation_live(ident.get("pid") or 0, ident.get("ct") or None)
 *      is not False
 *
 *  Unprovable counts as live: only an explicit "not live" verdict is death.
 * =========================================================================== */

/* PoP: upr_identity_is_live @ hermes_cli/update_pause_record.py:identity_is_live */
int upr_identity_is_live(const json_t *ident, upr_incarnation_fn fn)
{
    if (!fn)
        return 1;                    /* no probe: unprovable, so live */
    long long pid = 0;
    const char *ct = NULL;
    if (ident && ident->type == JSON_OBJECT) {
        json_t *p = json_obj_get(ident, "pid");
        if (p && p->type == JSON_NUMBER)
            pid = (long long)p->num_val;
        else if (p && p->type == JSON_STRING && p->str_val)
            pid = strtoll(p->str_val, NULL, 10);
        ct = json_get_str(ident, "ct", NULL);
    }
    return fn(pid, ct) != 0 ? 1 : 0;
}

/* ===========================================================================
 *  JSON equality -- Python's `x not in list` / `b not in baselines` compare by
 *  VALUE, so these are deep comparisons, not pointer identity.
 * =========================================================================== */

static int json_equal(const json_t *a, const json_t *b)
{
    if (a == b)
        return 1;
    if (!a || !b || a->type != b->type)
        return 0;
    switch (a->type) {
    case JSON_NULL: return 1;
    case JSON_BOOL: return a->bool_val == b->bool_val;
    case JSON_NUMBER: return a->num_val == b->num_val;
    case JSON_STRING: return a->str_val && b->str_val
                             && strcmp(a->str_val, b->str_val) == 0;
    case JSON_ARRAY:
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++) {
            if (!json_equal(a->c.items[i], b->c.items[i])) return 0;
        }
        return 1;
    case JSON_OBJECT:
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++) {
            const char *key = a->c.keys[i];
            const json_t *other = json_obj_get(b, key);
            if (!other || !json_equal(a->c.items[i], other)) return 0;
        }
        return 1;
    }
    return 0;
}

static int arr_contains(const json_t *arr, const json_t *needle)
{
    if (!arr || arr->type != JSON_ARRAY || !needle)
        return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        if (json_equal(arr->c.items[i], needle))
            return 1;
    }
    return 0;
}

/* ===========================================================================
 *  _holder  (update_pause_record.py:_holder)
 *
 *      held_by = body.get("owner") if src.suffix == ".json" else body.get("claimer")
 *      return held_by if isinstance(held_by, dict) else UNOWNED
 * =========================================================================== */

/* PoP: upr_holder @ hermes_cli/update_pause_record.py:_holder */
json_t *upr_holder(const char *src, const json_t *body)
{
    json_t *held = NULL;
    if (body && body->type == JSON_OBJECT) {
        int is_record = 0;
        if (src) {
            const char *dot = strrchr(src, '.');
            is_record = (dot && strcmp(dot, ".json") == 0);
        }
        held = json_obj_get(body, is_record ? "owner" : "claimer");
        if (held && held->type != JSON_OBJECT)
            held = NULL;
    }
    if (held)
        return held;
    /* UNOWNED = {"pid": 0, "ct": None} -- a fresh copy per call. */
    json_t *unowned = json_object();
    if (!unowned)
        return NULL;
    json_set(unowned, "pid", json_number(0));
    json_set(unowned, "ct", json_null());
    return unowned;
}

/* ===========================================================================
 *  split_draining  (update_pause_record.py:split_draining)
 *
 *      live = _live_pids(token)
 *      draining = {"profiles": {p: pid for ... if str(pid) in live},
 *                  "unmapped": [...], "posix_units": [...]}
 *      _without(token, live)
 *      return draining
 * =========================================================================== */

/* PoP: upr_split_draining @ hermes_cli/update_pause_record.py:split_draining */
json_t *upr_split_draining(json_t *token, upr_incarnation_fn fn)
{
    json_t *live = upr_live_pids(token, fn);
    json_t *draining = json_object();
    if (!draining) { json_free(live); return NULL; }

    /* The live pid set, as strings -- Python's `str(pid) in live`. */
    const char **live_s = NULL;
    size_t n_live = 0;
    if (live && live->type == JSON_ARRAY) {
        n_live = live->c.count;
        live_s = (const char **)calloc(n_live ? n_live : 1, sizeof(char *));
        for (size_t i = 0; i < n_live; i++)
            live_s[i] = (live->c.items[i] && live->c.items[i]->type == JSON_STRING)
                            ? live->c.items[i]->str_val : NULL;
    }

    json_t *profiles = json_object();
    json_t *srcp = (token && token->type == JSON_OBJECT)
                       ? json_obj_get(token, "profiles") : NULL;
    if (srcp && srcp->type == JSON_OBJECT) {
        for (size_t i = 0; i < srcp->c.count; i++) {
            char buf[64];
            char *s = pid_str(srcp->c.items[i], buf, sizeof(buf));
            if (in_pids(s, live_s, n_live))
                json_set(profiles, srcp->c.keys[i], json_copy(srcp->c.items[i]));
        }
    }
    json_set(draining, "profiles", profiles);

    const char *keys[2] = {"unmapped", "posix_units"};
    for (size_t kk = 0; kk < 2; kk++) {
        json_t *kept = json_array();
        json_t *src = (token && token->type == JSON_OBJECT)
                          ? json_obj_get(token, keys[kk]) : NULL;
        if (src && src->type == JSON_ARRAY) {
            for (size_t i = 0; i < src->c.count; i++) {
                json_t *u = src->c.items[i];
                char buf[64];
                char *s = (u && u->type == JSON_OBJECT)
                              ? pid_str(json_obj_get(u, "pid"), buf, sizeof(buf))
                              : NULL;
                if (in_pids(s, live_s, n_live))
                    json_append(kept, json_copy(u));
            }
        }
        json_set(draining, keys[kk], kept);
    }

    /* _without(token, live) -- the same filter as split_draining's tail. */
    upr_without(token, live_s, n_live);

    free(live_s);
    json_free(live);
    return draining;
}

/* ===========================================================================
 *  merge_into  (update_pause_record.py:merge_into)
 *
 *      Fold an orphaned pause into this update's token: baselines, absorbed
 *      ids, profiles, unmapped, identities, stop records, cold start, POSIX
 *      units and services.
 * =========================================================================== */

static void merge_str_list(json_t *token, const char *key,
                           const json_t *adopted, int with_expected)
{
    json_t *dst = json_obj_get(token, key);
    if (!dst) {
        dst = json_array();
        json_set(token, key, dst);
    }
    json_t *src = (adopted && adopted->type == JSON_OBJECT)
                      ? json_obj_get(adopted, key) : NULL;
    if (!src || src->type != JSON_ARRAY)
        return;
    for (size_t i = 0; i < src->c.count; i++) {
        if (!arr_contains(dst, src->c.items[i]))
            json_append(dst, json_copy(src->c.items[i]));
    }
    (void)with_expected;
}

/* PoP: upr_merge_into @ hermes_cli/update_pause_record.py:merge_into */
json_t *upr_merge_into(json_t *token, const json_t *adopted)
{
    if (!token || token->type != JSON_OBJECT)
        return token;
    if (!adopted || adopted->type != JSON_OBJECT)
        return token;

    json_set(token, "resume_needed", json_bool(1));

    /* baselines: extend with the adopted ones not already present (by value) */
    json_t *baselines = json_obj_get(token, "baselines");
    if (!baselines) {
        baselines = json_array();
        json_set(token, "baselines", baselines);
    }
    json_t *a_base = json_obj_get(adopted, "baselines");
    if (a_base && a_base->type == JSON_ARRAY) {
        for (size_t i = 0; i < a_base->c.count; i++) {
            if (!arr_contains(baselines, a_base->c.items[i]))
                json_append(baselines, json_copy(a_base->c.items[i]));
        }
    }

    /* absorbed: adopted pause_id plus every id it absorbed */
    json_t *absorbed = json_obj_get(token, "absorbed");
    if (!absorbed) {
        absorbed = json_array();
        json_set(token, "absorbed", absorbed);
    }
    json_t *seed[16];
    size_t n_seed = 0;
    json_t *apid = json_obj_get(adopted, "pause_id");
    if (apid && apid->type == JSON_STRING) seed[n_seed++] = apid;
    json_t *a_abs = json_obj_get(adopted, "absorbed");
    if (a_abs && a_abs->type == JSON_ARRAY) {
        for (size_t i = 0; i < a_abs->c.count && n_seed < 16; i++)
            seed[n_seed++] = a_abs->c.items[i];
    }
    for (size_t i = 0; i < n_seed; i++) {
        if (!arr_contains(absorbed, seed[i]))
            json_append(absorbed, json_copy(seed[i]));
    }

    /* profiles: setdefault per name */
    json_t *profiles = json_obj_get(token, "profiles");
    if (!profiles) {
        profiles = json_object();
        json_set(token, "profiles", profiles);
    }
    json_t *a_prof = json_obj_get(adopted, "profiles");
    if (a_prof && a_prof->type == JSON_OBJECT) {
        for (size_t i = 0; i < a_prof->c.count; i++) {
            if (!json_obj_get(profiles, a_prof->c.keys[i]))
                json_set(profiles, a_prof->c.keys[i],
                         json_copy(a_prof->c.items[i]));
        }
    }

    /* unmapped */
    json_t *unmapped = json_obj_get(token, "unmapped");
    if (!unmapped) {
        unmapped = json_array();
        json_set(token, "unmapped", unmapped);
    }
    json_t *a_unm = json_obj_get(adopted, "unmapped");
    if (a_unm && a_unm->type == JSON_ARRAY) {
        for (size_t i = 0; i < a_unm->c.count; i++) {
            if (!arr_contains(unmapped, a_unm->c.items[i]))
                json_append(unmapped, json_copy(a_unm->c.items[i]));
        }
    }

    /* identities: setdefault per pid */
    json_t *idents = json_obj_get(token, "identities");
    if (!idents) {
        idents = json_object();
        json_set(token, "identities", idents);
    }
    json_t *a_id = json_obj_get(adopted, "identities");
    if (a_id && a_id->type == JSON_OBJECT) {
        for (size_t i = 0; i < a_id->c.count; i++) {
            if (!json_obj_get(idents, a_id->c.keys[i]))
                json_set(idents, a_id->c.keys[i], json_copy(a_id->c.items[i]));
        }
    }

    /* stop record: only when either side carries one */
    if (json_obj_get(adopted, "stop_requested")
        || json_obj_get(token, "stop_requested")) {
        json_t *req = json_obj_get(adopted, "stop_requested");
        json_t *sent = json_obj_get(adopted, "stop_sent");
        if (!req) {                        /* no stop record: all count as asked */
            /* Python: list(adopted["identities"]) -- the dict's KEYS. */
            json_t *ids = json_obj_get(adopted, "identities");
            json_t *keys = json_array();
            if (ids && ids->type == JSON_OBJECT) {
                for (size_t i = 0; i < ids->c.count; i++)
                    json_append(keys, json_string(ids->c.keys[i]));
            }
            req = sent = keys;
        }
        json_t *treq = json_obj_get(token, "stop_requested");
        json_t *tsent = json_obj_get(token, "stop_sent");
        json_t *u_req = json_array(), *u_sent = json_array();
        if (treq && treq->type == JSON_ARRAY) {
            for (size_t i = 0; i < treq->c.count; i++)
                json_append(u_req, json_copy(treq->c.items[i]));
        }
        if (req && req->type == JSON_ARRAY) {
            for (size_t i = 0; i < req->c.count; i++)
                json_append(u_req, json_copy(req->c.items[i]));
        }
        if (tsent && tsent->type == JSON_ARRAY) {
            for (size_t i = 0; i < tsent->c.count; i++)
                json_append(u_sent, json_copy(tsent->c.items[i]));
        }
        if (sent && sent->type == JSON_ARRAY) {
            for (size_t i = 0; i < sent->c.count; i++)
                json_append(u_sent, json_copy(sent->c.items[i]));
        }
        json_obj_del(token, "stop_requested");
        json_set(token, "stop_requested", u_req);
        json_obj_del(token, "stop_sent");
        json_set(token, "stop_sent", u_sent);
    }

    /* cold start */
    json_t *cold = json_obj_get(token, "cold_start_profiles");
    if (!cold) {
        cold = json_object();
        json_set(token, "cold_start_profiles", cold);
    }
    json_t *a_cold = json_obj_get(adopted, "cold_start_profiles");
    if (a_cold && a_cold->type == JSON_OBJECT) {
        for (size_t i = 0; i < a_cold->c.count; i++) {
            if (!json_obj_get(cold, a_cold->c.keys[i]))
                json_set(cold, a_cold->c.keys[i], json_copy(a_cold->c.items[i]));
        }
    }
    if (json_truthy(json_obj_get(adopted, "cold_start_if_installed"))) {
        json_set(token, "cold_start_if_installed", json_bool(1));
        json_t *att = json_obj_get(adopted, "attested_generation");
        if (att && !json_obj_get(token, "attested_generation"))
            json_set(token, "attested_generation", json_copy(att));
    }

    /* POSIX platform + units */
    json_t *platform = json_obj_get(adopted, "platform");
    if (platform && platform->type == JSON_STRING) {
        json_set(token, "platform", json_copy(platform));
        json_t *units = json_obj_get(token, "posix_units");
        if (!units) {
            units = json_array();
            json_set(token, "posix_units", units);
        }
        json_t *a_units = json_obj_get(adopted, "posix_units");
        if (a_units && a_units->type == JSON_ARRAY) {
            for (size_t i = 0; i < a_units->c.count; i++) {
                if (!arr_contains(units, a_units->c.items[i]))
                    json_append(units, json_copy(a_units->c.items[i]));
            }
        }
    }

    /* services: `token.setdefault("services", [])` runs unconditionally, so an
     * empty list appears even when neither side has services. */
    json_t *a_svc = json_obj_get(adopted, "services");
    int have_services = (a_svc && a_svc->type == JSON_ARRAY && a_svc->c.count > 0);
    merge_str_list(token, "services", adopted, 0);
    if (have_services) {
        json_t *expected = json_obj_get(token, "expected_services");
        if (!expected) {
            expected = json_array();
            json_set(token, "expected_services", expected);
        }
        json_t *svc = json_obj_get(token, "services");
        if (svc && svc->type == JSON_ARRAY) {
            for (size_t i = 0; i < svc->c.count; i++) {
                if (!arr_contains(expected, svc->c.items[i]))
                    json_append(expected, json_copy(svc->c.items[i]));
            }
        }
        if (!json_obj_get(token, "restarted_services")) {
            json_set(token, "restarted_services", json_array());
        }
        json_t *sp = json_obj_get(token, "service_profiles");
        if (!sp) {
            sp = json_object();
            json_set(token, "service_profiles", sp);
        }
        json_t *a_sp = json_obj_get(adopted, "service_profiles");
        if (a_sp && a_sp->type == JSON_OBJECT) {
            for (size_t i = 0; i < a_sp->c.count; i++)
                json_set(sp, a_sp->c.keys[i], json_copy(a_sp->c.items[i]));
        }
    }

    return token;
}

/* ===========================================================================
 *  _live_pids  (update_pause_record.py:_live_pids)
 *
 *      {str(pid) for pid, ct in (token.get("identities") or {}).items()
 *       if identity_is_live({"pid": pid, "ct": ct})}
 * =========================================================================== */

/* PoP: upr_live_pids @ hermes_cli/update_pause_record.py:_live_pids */
json_t *upr_live_pids(const json_t *token, upr_incarnation_fn fn)
{
    json_t *out = json_array();
    if (!out)
        return NULL;
    json_t *idents = (token && token->type == JSON_OBJECT)
                         ? json_obj_get(token, "identities") : NULL;
    if (!idents || idents->type != JSON_OBJECT)
        return out;
    for (size_t i = 0; i < idents->c.count; i++) {
        json_t *entry = json_object();
        if (!entry)
            continue;
        json_set(entry, "pid", json_string(idents->c.keys[i]));
        json_set(entry, "ct", idents->c.items[i] ? json_copy(idents->c.items[i])
                                                 : json_null());
        if (upr_identity_is_live(entry, fn))
            json_append(out, json_string(idents->c.keys[i]));
        json_free(entry);
    }
    return out;
}
