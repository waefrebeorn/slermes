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
