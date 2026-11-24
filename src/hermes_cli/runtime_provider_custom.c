/*
 * runtime_provider_custom.c — C11 port of upstream
 * hermes-agent/hermes_cli/runtime_provider_custom.py.
 *
 * Gates (one port file, partitioned dispatch):
 *   w30_runtimepro_33ebc6_p1 — provider-spec normalisation, config scanning,
 *                              field lifting and the named-entry lookup.
 *   w30_runtimepro_33ebc6_p2 — identity recovery (bare "custom" -> "custom:<name>")
 *                              and the runtime-derivation decisions.
 *
 * PoP: w30_runtimepro_33ebc6_p1_normalize_custom_provider_name @ hermes_cli/runtime_provider_custom.py:_normalize_custom_provider_name
 * PoP: w30_runtimepro_33ebc6_p1_normalize_base_url_for_match @ hermes_cli/runtime_provider_custom.py:_normalize_base_url_for_match
 * PoP: w30_runtimepro_33ebc6_p1_clean @ hermes_cli/runtime_provider_custom.py:_clean
 * PoP: w30_runtimepro_33ebc6_p1_custom_provider_slug @ hermes_cli/providers.py:custom_provider_slug
 * PoP: w30_runtimepro_33ebc6_p1_custom_provider_aliases @ hermes_cli/providers.py:custom_provider_aliases
 * PoP: w30_runtimepro_33ebc6_p1_normalize_route_base_url @ hermes_cli/route_identity.py:normalize_route_base_url
 * PoP: w30_runtimepro_33ebc6_p1_base_url_hostname @ utils/urls.py:base_url_hostname
 * PoP: w30_runtimepro_33ebc6_p1_route_key @ hermes_cli/runtime_provider_custom.py:_route_key
 * PoP: w30_runtimepro_33ebc6_p1_entry_url @ hermes_cli/runtime_provider_custom.py:_entry_url
 * PoP: w30_runtimepro_33ebc6_p1_filter_capabilities @ hermes_cli/runtime_provider_custom.py:_filter_capabilities
 * PoP: w30_runtimepro_33ebc6_p1_is_provider_enabled @ hermes_cli/config_providers.py:is_provider_enabled
 * PoP: w30_runtimepro_33ebc6_p1_normalize_extra_headers @ hermes_cli/config_providers.py:normalize_extra_headers
 * PoP: w30_runtimepro_33ebc6_p1_canonical_api_mode @ hermes_cli/config_providers.py:_canonical_api_mode
 * PoP: w30_runtimepro_33ebc6_p1_parse_api_mode @ hermes_cli/runtime_provider.py:_parse_api_mode
 * PoP: w30_runtimepro_33ebc6_p1_shadowed_by_builtin @ hermes_cli/runtime_provider_custom.py:_shadowed_by_builtin
 * PoP: w30_runtimepro_33ebc6_p1_builtin_named_entries @ hermes_cli/runtime_provider_custom.py:builtin_named_entries
 * PoP: w30_runtimepro_33ebc6_p1_builtin_name_user_entry @ hermes_cli/runtime_provider_custom.py:builtin_name_user_entry
 * PoP: w30_runtimepro_33ebc6_p1_persisted_provider_id @ hermes_cli/runtime_provider_custom.py:persisted_provider_id
 * PoP: w30_runtimepro_33ebc6_p1_key_env_secret @ hermes_cli/runtime_provider_custom.py:_key_env_secret
 * PoP: w30_runtimepro_33ebc6_p1_model_cfg_key_env_for @ hermes_cli/runtime_provider_custom.py:_model_cfg_key_env_for
 * PoP: w30_runtimepro_33ebc6_p1_lift_model_capabilities @ hermes_cli/runtime_provider_custom.py:_lift_model_capabilities
 * PoP: w30_runtimepro_33ebc6_p1_lift_common_custom_fields @ hermes_cli/runtime_provider_custom.py:_lift_common_custom_fields
 * PoP: w30_runtimepro_33ebc6_p1_match_new_style_provider @ hermes_cli/runtime_provider_custom.py:_match_new_style_provider
 * PoP: w30_runtimepro_33ebc6_p1_match_legacy_custom_provider @ hermes_cli/runtime_provider_custom.py:_match_legacy_custom_provider
 * PoP: w30_runtimepro_33ebc6_p1_compatible_entry_path @ hermes_cli/runtime_provider_custom.py:_compatible_entry_path
 * PoP: w30_runtimepro_33ebc6_p1_select_named_custom_provider @ hermes_cli/runtime_provider_custom.py:_select_named_custom_provider
 * PoP: w30_runtimepro_33ebc6_p1_named_custom_provider_config_path @ hermes_cli/runtime_provider_custom.py:named_custom_provider_config_path
 * PoP: w30_runtimepro_33ebc6_p1_custom_provider_config_path_for_url @ hermes_cli/runtime_provider_custom.py:custom_provider_config_path_for_url
 * PoP: w30_runtimepro_33ebc6_p1_codex_model_provider_id @ hermes_cli/runtime_provider_custom.py:codex_model_provider_id
 * PoP: w30_runtimepro_33ebc6_p1_has_named_custom_provider @ hermes_cli/runtime_provider_custom.py:has_named_custom_provider
 * PoP: w30_runtimepro_33ebc6_p2_has_usable_secret @ hermes_cli/auth.py:has_usable_secret
 * PoP: w30_runtimepro_33ebc6_p2_entry_declares_credential @ agent/custom_entry_pool.py:entry_declares_credential
 * PoP: w30_runtimepro_33ebc6_p2_model_id_matches @ hermes_cli/runtime_provider_custom.py:_model_id_matches
 * PoP: w30_runtimepro_33ebc6_p2_entry_serves_model @ hermes_cli/runtime_provider_custom.py:_entry_serves_model
 * PoP: w30_runtimepro_33ebc6_p2_find_custom_identity @ hermes_cli/runtime_provider_custom.py:_find_custom_identity
 * PoP: w30_runtimepro_33ebc6_p2_find_custom_provider_identity @ hermes_cli/runtime_provider_custom.py:find_custom_provider_identity
 * PoP: w30_runtimepro_33ebc6_p2_find_custom_provider_identity_by_model @ hermes_cli/runtime_provider_custom.py:find_custom_provider_identity_by_model
 * PoP: w30_runtimepro_33ebc6_p2_routed_custom_identity @ hermes_cli/runtime_provider_custom.py:routed_custom_identity
 * PoP: w30_runtimepro_33ebc6_p2_empty_provider_identity @ hermes_cli/runtime_provider_custom.py:empty_provider_identity
 * PoP: w30_runtimepro_33ebc6_p2_endpoint_identity @ hermes_cli/runtime_provider_custom.py:_endpoint_identity
 * PoP: w30_runtimepro_33ebc6_p2_session_provider_identity @ hermes_cli/runtime_provider_custom.py:session_provider_identity
 * PoP: w30_runtimepro_33ebc6_p2_canonical_custom_identity @ hermes_cli/runtime_provider_custom.py:canonical_custom_identity
 * PoP: w30_runtimepro_33ebc6_p2_is_routable_provider @ hermes_cli/runtime_provider_custom.py:is_routable_provider
 * PoP: w30_runtimepro_33ebc6_p2_custom_provider_request_overrides @ hermes_cli/runtime_provider_custom.py:_custom_provider_request_overrides
 * PoP: w30_runtimepro_33ebc6_p2_apply_custom_provider_extras @ hermes_cli/runtime_provider_custom.py:_apply_custom_provider_extras
 * PoP: w30_runtimepro_33ebc6_p2_expand_direct_api_alias @ hermes_cli/runtime_provider_custom.py:expand_direct_api_alias
 * PoP: w30_runtimepro_33ebc6_p2_opencode_family_for_custom @ hermes_cli/runtime_provider_custom.py:_opencode_family_for_custom
 * PoP: w30_runtimepro_33ebc6_p2_entry_credential @ hermes_cli/runtime_provider_custom.py:_entry_credential
 * PoP: w30_runtimepro_33ebc6_p2_owning_entry_credential @ hermes_cli/runtime_provider_custom.py:_owning_entry_credential
 * PoP: w30_runtimepro_33ebc6_p2_owned_config_url_credential @ hermes_cli/runtime_provider_custom.py:owned_config_url_credential
 * PoP: w30_runtimepro_33ebc6_p2_resolve_direct_alias_runtime @ hermes_cli/runtime_provider_custom.py:_resolve_direct_alias_runtime
 * PoP: w30_runtimepro_33ebc6_p2_resolve_named_custom_runtime @ hermes_cli/runtime_provider_custom.py:_resolve_named_custom_runtime
 *
 * Seams (answers passed in, never fetched here): the config file itself
 * (``load_config``), the credential store (``get_secret_str`` ->
 * ``seams.secrets``), the provider registry (``resolve_provider`` ->
 * ``canonical``), the compatible-entry view of ``custom_providers:``, the
 * per-entry normalisation used for identity comparison, the credential pool,
 * the host-gated env key candidates, the plugin transport registry, the
 * managed local endpoint and the ``hermes_cli.models`` OpenCode helpers.
 *
 * Numeric stringification matches Python's ``str()`` for integral numbers;
 * libjson has one numeric type, so non-integral values are not stringified in
 * the fixtures (they would be indistinguishable from an int on the way in).
 */
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w30_runtimepro_33ebc6_p1.h"
#include "noc_w30_runtimepro_33ebc6_p2.h"

/* ───────────────────────────── small string helpers ───────────────────────── */

typedef struct {
    char *s;
    size_t n, cap;
} rp_sb;

static char *rp_str_dup_static(const char *s);
static char *rp_substr(const char *s, size_t n);

static void sb_init(rp_sb *b) {
    b->s = NULL;
    b->n = b->cap = 0;
}

static void sb_grow(rp_sb *b, size_t need) {
    size_t cap;
    char *p;
    if (b->n + need + 1 <= b->cap) return;
    cap = b->cap ? b->cap : 64;
    while (cap < b->n + need + 1) cap *= 2;
    p = (char *)realloc(b->s, cap);
    if (!p) return;
    b->s = p;
    b->cap = cap;
}

static void sb_addn(rp_sb *b, const char *s, size_t n) {
    if (!s || !n) return;
    sb_grow(b, n);
    if (b->n + n + 1 > b->cap) return;
    memcpy(b->s + b->n, s, n);
    b->n += n;
    b->s[b->n] = '\0';
}

static void sb_add(rp_sb *b, const char *s) {
    if (s) sb_addn(b, s, strlen(s));
}

static void sb_addc(rp_sb *b, char c) {
    sb_addn(b, &c, 1);
}

static char *sb_take(rp_sb *b) {
    char *out;
    if (!b->s) return rp_str_dup_static("");
    out = b->s;
    b->s = NULL;
    b->n = b->cap = 0;
    return out;
}

static char *rp_str_dup_static(const char *s) {
    return rp_substr(s ? s : "", s ? strlen(s) : 0);
}

static char *rp_substr(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* Python str.strip() over ASCII whitespace. */
static char *rp_strip(const char *s) {
    size_t n, a = 0, z;
    if (!s) return rp_str_dup_static("");
    n = strlen(s);
    while (a < n && isspace((unsigned char)s[a])) a++;
    z = n;
    while (z > a && isspace((unsigned char)s[z - 1])) z--;
    return rp_substr(s + a, z - a);
}

/* Lowercase in place (ASCII). */
static void rp_lower(char *s) {
    if (!s) return;
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* ───────────────────────── Python value semantics ─────────────────────────── */

static int rp_truthy(const json_t *v) {
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

static const char *rp_sv(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static const json_t *rp_get(const json_t *o, const char *key) {
    if (!o || o->type != JSON_OBJECT || !key) return NULL;
    return json_obj_get(o, key);
}

/* A seam answer may sit at the top level of the request or under "seams". */
static const json_t *rp_arg(const json_t *req, const char *key) {
    const json_t *v = rp_get(req, key);
    if (v) return v;
    return rp_get(rp_get(req, "seams"), key);
}

/* Python repr of a number: integral values print without a decimal point. */
static char *rp_num_repr(double d) {
    char buf[64];
    if (d == (double)(long long)d && fabs(d) < 1e16)
        snprintf(buf, sizeof buf, "%lld", (long long)d);
    else
        snprintf(buf, sizeof buf, "%.17g", d);
    return rp_str_dup_static(buf);
}

static void rp_repr_into(rp_sb *b, const json_t *v, int depth);

/* Python repr of a str: shortest quoting. */
static void rp_repr_string(rp_sb *b, const char *s) {
    char quote = '\'';
    const char *p;
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    sb_addc(b, quote);
    for (p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == (unsigned char)quote || c == '\\') {
            sb_addc(b, '\\');
            sb_addc(b, (char)c);
        } else if (c == '\n') {
            sb_add(b, "\\n");
        } else if (c == '\t') {
            sb_add(b, "\\t");
        } else if (c == '\r') {
            sb_add(b, "\\r");
        } else if (c < 0x20) {
            char t[8];
            snprintf(t, sizeof t, "\\x%02x", c);
            sb_add(b, t);
        } else {
            sb_addc(b, (char)c);
        }
    }
    sb_addc(b, quote);
}

static void rp_repr_into(rp_sb *b, const json_t *v, int depth) {
    size_t i, n;
    char *num;
    if (!v || depth > 6) {
        sb_add(b, "None");
        return;
    }
    switch (v->type) {
    case JSON_NULL:
        sb_add(b, "None");
        return;
    case JSON_BOOL:
        sb_add(b, v->bool_val ? "True" : "False");
        return;
    case JSON_NUMBER:
        num = rp_num_repr(v->num_val);
        sb_add(b, num);
        free(num);
        return;
    case JSON_STRING:
        rp_repr_string(b, v->str_val ? v->str_val : "");
        return;
    case JSON_ARRAY:
        sb_addc(b, '[');
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) sb_add(b, ", ");
            rp_repr_into(b, json_get(v, i), depth + 1);
        }
        sb_addc(b, ']');
        return;
    case JSON_OBJECT:
        sb_addc(b, '{');
        n = json_object_size(v);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(v, i);
            if (i) sb_add(b, ", ");
            rp_repr_string(b, k ? k : "");
            sb_add(b, ": ");
            rp_repr_into(b, json_object_get_at(v, i), depth + 1);
        }
        sb_addc(b, '}');
        return;
    }
}

/* Python str(value): no ``or ""`` folding. */
static char *rp_str_of(const json_t *v) {
    rp_sb b;
    char *out;
    if (!v) return rp_str_dup_static("None");
    switch (v->type) {
    case JSON_STRING:
        return rp_str_dup_static(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        return rp_num_repr(v->num_val);
    case JSON_BOOL:
        return rp_str_dup_static(v->bool_val ? "True" : "False");
    case JSON_NULL:
        return rp_str_dup_static("None");
    case JSON_ARRAY:
    case JSON_OBJECT:
        break;
    }
    sb_init(&b);
    rp_repr_into(&b, v, 0);
    out = sb_take(&b);
    return out;
}

/* Python str(value or ""). */
static char *rp_or_str(const json_t *v) {
    if (!rp_truthy(v)) return rp_str_dup_static("");
    return rp_str_of(v);
}

/* Python str(value or "").strip() */
static char *rp_clean(const json_t *v) {
    char *raw = rp_or_str(v);
    char *out = rp_strip(raw);
    free(raw);
    return out;
}

/* Python ``==`` over the shapes these values hold. */
static int rp_equal(const json_t *a, const json_t *b) {
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        double x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type != b->type) return 0;
    switch (a->type) {
    case JSON_STRING:
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    case JSON_BOOL:
        return a->bool_val == b->bool_val;
    case JSON_ARRAY:
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!rp_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    case JSON_OBJECT:
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !rp_equal(json_object_get_at(a, i), rp_get(b, k))) return 0;
        }
        return 1;
    default:
        return 0;
    }
}

static json_t *rp_jstr(const char *s) {
    return json_string(s ? s : "");
}

static json_t *rp_jbool(int v) {
    return json_bool(v ? true : false);
}

static void rp_set_str(json_t *obj, const char *key, const char *val) {
    json_set(obj, key, rp_jstr(val));
}

/* ──────────────────────── base_url / route normalisation ─────────────────── */

static int rp_valid_scheme(const char *s, size_t n) {
    size_t i;
    if (!n || !isalpha((unsigned char)s[0])) return 0;
    for (i = 1; i < n; i++) {
        char c = s[i];
        if (!(isalnum((unsigned char)c) || c == '+' || c == '-' || c == '.')) return 0;
    }
    return 1;
}

/* hermes_cli.route_identity.normalize_route_base_url */
static char *rp_route_norm(const char *raw_in) {
    const char *raw = raw_in ? raw_in : "";
    const char *p, *delim, *rest, *q, *hash;
    size_t scheme_len, net_len, path_len;
    char *netloc, *hostport, *host = NULL, *port_text = NULL, *userinfo = NULL;
    char *scheme, *path, *out;
    long port = -1;
    int has_port = 0, had_query = 0, i;
    rp_sb b;

    if (!*raw) return rp_str_dup_static("");
    for (p = raw; *p; p++)
        if ((unsigned char)*p <= 0x20) return rp_str_dup_static(raw);
    for (p = raw; *p && *p != '#'; p++)
        if (*p == '?') { had_query = 1; break; }

    delim = strstr(raw, "://");
    if (!delim) return rp_str_dup_static(raw);
    scheme_len = (size_t)(delim - raw);
    if (!rp_valid_scheme(raw, scheme_len)) return rp_str_dup_static(raw);

    scheme = rp_substr(raw, scheme_len);
    rp_lower(scheme);

    rest = delim + 3;
    net_len = strcspn(rest, "/?#");
    if (!net_len) {
        free(scheme);
        return rp_str_dup_static(raw);
    }
    netloc = rp_substr(rest, net_len);

    {
        char *at = strrchr(netloc, '@');
        hostport = at ? rp_str_dup_static(at + 1) : rp_str_dup_static(netloc);
        if (at) userinfo = rp_substr(netloc, (size_t)(at - netloc));
    }

    if (hostport[0] == '[') {
        char *close = strchr(hostport, ']');
        if (!close) goto bail_raw;
        host = rp_substr(hostport + 1, (size_t)(close - hostport - 1));
        if (close[1] == ':') port_text = rp_str_dup_static(close + 2);
        else if (close[1] != '\0') goto bail_raw;
    } else {
        char *colon = strrchr(hostport, ':');
        if (colon) {
            host = rp_substr(hostport, (size_t)(colon - hostport));
            port_text = rp_str_dup_static(colon + 1);
        } else {
            host = rp_str_dup_static(hostport);
        }
    }
    if (port_text && *port_text) {
        for (p = port_text; *p; p++)
            if (!isdigit((unsigned char)*p)) goto bail_raw;
        port = atol(port_text);
        if (port < 0 || port > 65535) goto bail_raw;
        has_port = 1;
    }

    /* hostname lowercasing, with a %zone preserved verbatim after the '%'. */
    {
        char *pct = strchr(host, '%');
        if (pct) {
            char *addr = rp_substr(host, (size_t)(pct - host));
            rp_sb hb;
            rp_lower(addr);
            sb_init(&hb);
            sb_add(&hb, addr);
            sb_add(&hb, pct);
            free(addr);
            free(host);
            host = sb_take(&hb);
        } else {
            rp_lower(host);
        }
    }

    if (hostport[0] == '[' || strchr(host, ':')) {
        rp_sb hb;
        sb_init(&hb);
        sb_addc(&hb, '[');
        sb_add(&hb, host);
        sb_addc(&hb, ']');
        free(host);
        host = sb_take(&hb);
    }

    sb_init(&b);
    sb_add(&b, scheme);
    sb_add(&b, "://");
    if (userinfo) {
        sb_add(&b, userinfo);
        sb_addc(&b, '@');
    }
    sb_add(&b, host);
    if (has_port && !((!strcmp(scheme, "http") && port == 80) || (!strcmp(scheme, "https") && port == 443))) {
        char pbuf[32];
        snprintf(pbuf, sizeof pbuf, ":%ld", port);
        sb_add(&b, pbuf);
    }

    rest += net_len;
    q = strpbrk(rest, "?#");
    path_len = q ? (size_t)(q - rest) : strlen(rest);
    path = rp_substr(rest, path_len);
    if (path_len && path[path_len - 1] == '/' && !had_query) path[path_len - 1] = '\0';
    sb_add(&b, path);

    if (q && *q == '?') {
        size_t qlen;
        const char *qp = q + 1;
        hash = strchr(qp, '#');
        qlen = hash ? (size_t)(hash - qp) : strlen(qp);
        if (qlen) {
            sb_addc(&b, '?');
            sb_addn(&b, qp, qlen);
        } else if (had_query) {
            sb_addc(&b, '?');
        }
    } else if (had_query) {
        sb_addc(&b, '?');
    }

    out = sb_take(&b);
    free(path);
    free(scheme);
    free(host);
    free(hostport);
    free(netloc);
    free(port_text);
    free(userinfo);
    return out;

bail_raw:
    free(scheme);
    free(host);
    free(hostport);
    free(netloc);
    free(port_text);
    free(userinfo);
    return rp_str_dup_static(raw);
    (void)i;
}

/* _route_key(v) = normalize_route_base_url(_clean(v)) */
static char *rp_route_key_text(const char *raw) {
    char *trimmed = rp_strip(raw ? raw : "");
    char *norm = rp_route_norm(trimmed);
    free(trimmed);
    return norm;
}

/* utils.urls.base_url_hostname */
static char *rp_hostname(const char *raw_in) {
    char *trimmed = rp_strip(raw_in ? raw_in : "");
    char *url, *netloc, *hostport, *host, *out, *p, *at, *colon, *close;
    const char *delim, *rest;
    size_t net_len;

    if (!*trimmed) return trimmed;
    if ((delim = strstr(trimmed, "://")) != NULL) {
        size_t sl = (size_t)(delim - trimmed);
        if (!rp_valid_scheme(trimmed, sl)) {
            free(trimmed);
            return rp_str_dup_static("");
        }
        url = rp_str_dup_static(delim + 3);
    } else {
        url = trimmed;
        trimmed = NULL;
    }
    if (!*url || url[0] == '/' || url[0] == '?' || url[0] == '#') {
        free(url);
        free(trimmed);
        return rp_str_dup_static("");
    }
    rest = url;
    net_len = strcspn(rest, "/?#");
    netloc = rp_substr(rest, net_len);
    at = strrchr(netloc, '@');
    hostport = (at ? at + 1 : netloc);
    if (*hostport == '[') {
        close = strchr(hostport, ']');
        host = rp_substr(hostport + 1, close ? (size_t)(close - hostport - 1) : strlen(hostport + 1));
    } else {
        colon = strchr(hostport, ':');
        host = colon ? rp_substr(hostport, (size_t)(colon - hostport)) : rp_str_dup_static(hostport);
    }
    rp_lower(host);
    for (p = host + strlen(host); p > host && p[-1] == '.'; p--) p[-1] = '\0';
    out = host;
    free(netloc);
    free(url);
    free(trimmed);
    return out;
}

/* ────────────────────────── provider identity helpers ─────────────────────── */

/* hermes_cli.providers.custom_provider_slug */
static char *rp_slug(const char *display_name, const char *provider_key) {
    char *key = rp_strip(provider_key ? provider_key : "");
    char *name = rp_strip(display_name ? display_name : "");
    const char *identity;
    char *norm, *out;
    rp_sb b;

    identity = *key ? key : name;
    norm = rp_str_dup_static(identity);
    rp_lower(norm);
    for (char *c = norm; *c; c++) if (*c == ' ') *c = '-';
    sb_init(&b);
    if (!strncmp(norm, "custom:", 7)) {
        sb_add(&b, norm);
    } else {
        sb_add(&b, "custom:");
        sb_add(&b, norm);
    }
    out = sb_take(&b);
    free(norm);
    free(key);
    free(name);
    return out;
}

#define RP_ALIAS_MAX 16

typedef struct {
    char *items[RP_ALIAS_MAX];
    int n;
} rp_alias_set;

static void rp_alias_add(rp_alias_set *set, const char *value) {
    int i;
    if (!value || !*value || set->n >= RP_ALIAS_MAX) return;
    for (i = 0; i < set->n; i++)
        if (!strcmp(set->items[i], value)) return;
    set->items[set->n++] = rp_str_dup_static(value);
}

/* hermes_cli.providers.custom_provider_aliases */
static void rp_aliases(const char *display_name, const char *provider_key, rp_alias_set *set) {
    const char *values[2];
    int i;
    set->n = 0;
    values[0] = display_name;
    values[1] = provider_key;
    for (i = 0; i < 2; i++) {
        char *raw = rp_strip(values[i] ? values[i] : "");
        char *normalized, *slug;
        rp_lower(raw);
        if (*raw) {
            normalized = rp_str_dup_static(raw);
            for (char *c = normalized; *c; c++) if (*c == ' ') *c = '-';
            rp_alias_add(set, raw);
            rp_alias_add(set, normalized);
            slug = rp_slug(normalized, "");
            rp_alias_add(set, slug);
            free(slug);
            if (!strncmp(normalized, "custom:", 7)) {
                char *suffix = rp_str_dup_static(normalized + 7);
                rp_sb b;
                if (*suffix) {
                    rp_alias_add(set, suffix);
                    sb_init(&b);
                    sb_add(&b, "custom:");
                    sb_add(&b, normalized);
                    {
                        char *combined = sb_take(&b);
                        rp_alias_add(set, combined);
                        free(combined);
                    }
                }
                free(suffix);
            }
            free(normalized);
        }
        free(raw);
    }
}

static int rp_aliases_has(const char *display_name, const char *provider_key, const char *needle) {
    rp_alias_set set;
    int i, hit = 0;
    rp_aliases(display_name, provider_key, &set);
    for (i = 0; i < set.n; i++)
        if (!strcmp(set.items[i], needle)) { hit = 1; break; }
    for (i = 0; i < set.n; i++) free(set.items[i]);
    return hit;
}

/* hermes_cli.config_providers.is_provider_enabled */
static int rp_enabled(const json_t *entry) {
    const json_t *flag;
    if (!entry || entry->type != JSON_OBJECT) return 1;
    flag = rp_get(entry, "enabled");
    if (!flag) return 1;
    if (flag->type == JSON_BOOL) return flag->bool_val ? 1 : 0;
    if (flag->type == JSON_STRING) {
        char *s = rp_strip(flag->str_val);
        int off;
        rp_lower(s);
        off = !strcmp(s, "false") || !strcmp(s, "0") || !strcmp(s, "no") || !strcmp(s, "off");
        free(s);
        return !off;
    }
    return rp_truthy(flag);
}

/* hermes_cli.auth.has_usable_secret */
static int rp_usable_secret(const json_t *value) {
    char *cleaned;
    size_t len;
    int ok;
    if (!value || value->type != JSON_STRING) return 0;
    cleaned = rp_strip(value->str_val);
    len = strlen(cleaned);
    ok = len >= 4;
    if (ok) {
        static const char *const ph[] = {"*", "**", "***", "changeme", "your_api_key",
                                         "your_api_key_here", "your-api-key", "placeholder",
                                         "example", "dummy", "null", "none"};
        size_t i;
        char *lower = rp_str_dup_static(cleaned);
        rp_lower(lower);
        for (i = 0; i < sizeof ph / sizeof ph[0]; i++)
            if (!strcmp(lower, ph[i])) { ok = 0; break; }
        if (ok) {
            /* _is_placeholder_shape */
            size_t ll = strlen(lower);
            if (ll >= 9 && !strncmp(lower, "your_", 5) && !strcmp(lower + ll - 5, "_here")) ok = 0;
        }
        if (ok) {
            static const char *const prefixes[] = {"sk-", "ghp_", "hf_"};
            size_t i2;
            for (i2 = 0; i2 < 3; i2++) {
                size_t pl = strlen(prefixes[i2]);
                if (!strncmp(lower, prefixes[i2], pl)) {
                    const char *tail = lower + pl;
                    if (*tail) {
                        const char *c;
                        int allx = 1;
                        for (c = tail; *c; c++) if (*c != 'x') { allx = 0; break; }
                        if (allx) { ok = 0; break; }
                    }
                }
            }
        }
        if (ok) {
            rp_sb b;
            const char *c;
            int allx = 1;
            sb_init(&b);
            for (c = lower; *c; c++)
                if (*c != ' ' && *c != '-' && *c != '_') sb_addc(&b, *c);
            if (b.n) {
                char *c2;
                for (c2 = b.s; *c2; c2++) if (*c2 != 'x') { allx = 0; break; }
                if (allx) ok = 0;
            } else {
                ok = ok; /* empty stripped form: not a placeholder */
            }
            free(b.s);
        }
        free(lower);
    }
    free(cleaned);
    return ok;
}

/* agent/custom_entry_pool.py:entry_declares_credential */
static int rp_declares_credential(const json_t *entry) {
    static const char *const keys[] = {"api_key", "key_env", "api_key_env", "key_cmd"};
    size_t i;
    if (!entry || entry->type != JSON_OBJECT) return 0;
    if (rp_get(entry, "declares_credential")) return rp_truthy(rp_get(entry, "declares_credential"));
    for (i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        char *v = rp_clean(rp_get(entry, keys[i]));
        int hit = *v != '\0';
        free(v);
        if (hit) return 1;
    }
    return 0;
}

/* ─────────────────────────── p1: pure accessors ──────────────────────────── */

static json_t *p1_normalize_custom_provider_name(const json_t *req) {
    char *s = rp_strip(rp_sv(rp_get(req, "value")));
    char *out;
    rp_lower(s);
    for (char *c = s; *c; c++) if (*c == ' ') *c = '-';
    out = rp_str_dup_static(s);
    free(s);
    {
        json_t *j = rp_jstr(out);
        free(out);
        return j;
    }
}

static json_t *p1_normalize_base_url_for_match(const json_t *req) {
    char *s = rp_clean(rp_get(req, "value"));
    size_t n = strlen(s);
    json_t *j;
    while (n && s[n - 1] == '/') s[--n] = '\0';
    rp_lower(s);
    j = rp_jstr(s);
    free(s);
    return j;
}

static json_t *p1_clean(const json_t *req) {
    char *s = rp_clean(rp_get(req, "value"));
    json_t *j = rp_jstr(s);
    free(s);
    return j;
}

static json_t *p1_custom_provider_slug(const json_t *req) {
    char *display = rp_or_str(rp_get(req, "display_name"));
    char *key = rp_or_str(rp_get(req, "provider_key"));
    char *slug = rp_slug(display, key);
    json_t *j = rp_jstr(slug);
    free(display);
    free(key);
    free(slug);
    return j;
}

static int rp_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *p1_custom_provider_aliases(const json_t *req) {
    char *display = rp_or_str(rp_get(req, "display_name"));
    char *key = rp_or_str(rp_get(req, "provider_key"));
    rp_alias_set set;
    const char *sorted[RP_ALIAS_MAX];
    json_t *arr = json_array();
    int i;

    rp_aliases(display, key, &set);
    for (i = 0; i < set.n; i++) sorted[i] = set.items[i];
    qsort(sorted, (size_t)set.n, sizeof sorted[0], rp_cmp_str);
    for (i = 0; i < set.n; i++) json_append(arr, rp_jstr(sorted[i]));
    /* Free only after the array is built: freeing inside the append loop left
     * the serialiser reading released memory once the fixture grew past a few
     * dozen lines (the case printed garbage only in a full-file run). */
    for (i = 0; i < set.n; i++) free(set.items[i]);
    free(display);
    free(key);
    return arr;
}

static json_t *p1_normalize_route_base_url(const json_t *req) {
    char *v = rp_or_str(rp_get(req, "value"));
    char *norm = rp_route_norm(v);
    json_t *j = rp_jstr(norm);
    free(v);
    free(norm);
    return j;
}

static json_t *p1_base_url_hostname(const json_t *req) {
    char *v = rp_or_str(rp_get(req, "value"));
    char *host = rp_hostname(v);
    json_t *j = rp_jstr(host);
    free(v);
    free(host);
    return j;
}

static json_t *p1_route_key(const json_t *req) {
    char *v = rp_clean(rp_get(req, "value"));
    char *norm = rp_route_norm(v);
    json_t *j = rp_jstr(norm);
    free(v);
    free(norm);
    return j;
}

static json_t *p1_entry_url(const json_t *req) {
    const json_t *entry = rp_arg(req, "entry");
    const json_t *v;
    if (!entry || entry->type != JSON_OBJECT) return rp_jstr("");
    v = rp_get(entry, "api");
    if (!rp_truthy(v)) v = rp_get(entry, "url");
    if (!rp_truthy(v)) v = rp_get(entry, "base_url");
    if (!rp_truthy(v)) return rp_jstr("");
    return json_copy(v);
}

static json_t *p1_filter_capabilities(const json_t *req) {
    const json_t *value = rp_get(req, "value");
    json_t *out = json_object();
    size_t i, n;
    if (!value || value->type != JSON_OBJECT) return out;
    n = json_object_size(value);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(value, i);
        const json_t *v = json_object_get_at(value, i);
        if (k && v && v->type == JSON_BOOL) json_set(out, k, json_copy(v));
    }
    return out;
}

static json_t *p1_is_provider_enabled(const json_t *req) {
    return rp_jbool(rp_enabled(rp_arg(req, "entry")));
}

static json_t *p1_normalize_extra_headers(const json_t *req) {
    const json_t *value = rp_get(req, "value");
    json_t *out = json_object();
    size_t i, n;
    if (!value || value->type != JSON_OBJECT || json_object_size(value) == 0) return out;
    n = json_object_size(value);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(value, i);
        const json_t *v = json_object_get_at(value, i);
        char *sv;
        if (!k || !v || v->type == JSON_NULL) continue;
        sv = rp_str_of(v);
        json_set(out, k, rp_jstr(sv));
        free(sv);
    }
    return out;
}

/* hermes_cli.config_providers._API_MODE_ALIASES */
static const char *rp_api_mode_alias(const char *norm) {
    static const char *const alias[][2] = {
        {"openai", "chat_completions"},
        {"openai_chat", "chat_completions"},
        {"openai-chat", "chat_completions"},
        {"chat-completions", "chat_completions"},
        {"chatcompletions", "chat_completions"},
        {"responses", "codex_responses"},
        {"openai_responses", "codex_responses"},
        {"openai-responses", "codex_responses"},
        {"anthropic", "anthropic_messages"},
        {"anthropic-messages", "anthropic_messages"},
        {"messages", "anthropic_messages"},
        {"bedrock", "bedrock_converse"},
        {"bedrock-converse", "bedrock_converse"},
    };
    size_t i;
    for (i = 0; i < sizeof alias / sizeof alias[0]; i++)
        if (!strcmp(norm, alias[i][0])) return alias[i][1];
    return NULL;
}

static json_t *p1_canonical_api_mode(const json_t *req) {
    const json_t *v = rp_get(req, "value");
    char *cleaned = rp_strip(v && v->type == JSON_STRING ? v->str_val : "");
    char *lower = rp_str_dup_static(cleaned);
    const char *mapped;
    json_t *out;
    rp_lower(lower);
    mapped = rp_api_mode_alias(lower);
    out = rp_jstr(mapped ? mapped : cleaned);
    free(cleaned);
    free(lower);
    return out;
}

/* hermes_cli/runtime_provider.py:_parse_api_mode, registry as a seam. */
static json_t *p1_parse_api_mode(const json_t *req) {
    static const char *const valid[] = {"chat_completions", "codex_responses", "anthropic_messages",
                                        "bedrock_converse", "codex_app_server"};
    const json_t *v = rp_get(req, "value");
    const json_t *registered = rp_arg(req, "registered");
    char *norm = NULL, *lower;
    const char *mapped;
    json_t *out = json_null();
    size_t i, n;

    if (v && v->type == JSON_STRING) {
        char *cleaned = rp_strip(v->str_val);
        lower = rp_str_dup_static(cleaned);
        rp_lower(lower);
        mapped = rp_api_mode_alias(lower);
        norm = rp_str_dup_static(mapped ? mapped : lower);
        free(lower);
        free(cleaned);
    }
    if (!norm || !*norm) {
        free(norm);
        return out;
    }
    for (i = 0; i < sizeof valid / sizeof valid[0]; i++) {
        if (!strcmp(norm, valid[i])) {
            json_free(out);
            out = rp_jstr(norm);
            free(norm);
            return out;
        }
    }
    if (registered && registered->type == JSON_ARRAY) {
        n = json_len(registered);
        for (i = 0; i < n; i++) {
            const json_t *item = json_get(registered, i);
            if (item && item->type == JSON_STRING && !strcmp(norm, item->str_val ? item->str_val : "")) {
                json_free(out);
                out = rp_jstr(norm);
                free(norm);
                return out;
            }
        }
    }
    free(norm);
    return out;
}

/* ─────────────────── p1: config lookup and field lifting ──────────────────── */

/* The provider registry's answer for ``resolve_provider(requested_norm)``. Returns
 * 1 when the canonical name shadows the raw one; sets *resolved to 0 on AuthError. */
static int rp_shadowed(const char *requested_norm, const json_t *req, char **canonical_out) {
    const json_t *canonical = rp_arg(req, "canonical");
    int auth_error = rp_truthy(rp_arg(req, "auth_error"));
    char *canon = NULL, *lower;

    *canonical_out = NULL;
    if (!strcmp(requested_norm, "custom") || !strncmp(requested_norm, "custom:", 7)) return 0;
    if (auth_error) return 0;
    canon = rp_or_str(canonical);
    lower = rp_strip(canon);
    rp_lower(lower);
    *canonical_out = lower;
    free(canon);
    return !strcmp(lower, requested_norm);
}

static json_t *p1_shadowed_by_builtin(const json_t *req) {
    char *norm = rp_or_str(rp_get(req, "requested_norm"));
    char *canonical = NULL;
    json_t *out = rp_jbool(rp_shadowed(norm, req, &canonical));
    free(norm);
    free(canonical);
    return out;
}

static json_t *p1_builtin_named_entries(const json_t *req) {
    char *norm = rp_or_str(rp_get(req, "requested_norm"));
    const json_t *providers = rp_get(req, "providers");
    json_t *out = json_object();
    size_t i, n;
    if (providers && providers->type == JSON_OBJECT) {
        n = json_object_size(providers);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(providers, i);
            char *cand, *lower;
            if (!k) continue;
            cand = rp_strip(k);
            lower = rp_str_dup_static(cand);
            rp_lower(lower);
            for (char *c = lower; *c; c++) if (*c == ' ') *c = '-';
            if (!strcmp(lower, norm)) json_set(out, k, json_copy(json_object_get_at(providers, i)));
            free(cand);
            free(lower);
        }
    }
    free(norm);
    return out;
}

static json_t *p1_builtin_name_user_entry(const json_t *req) {
    char *name = rp_or_str(rp_get(req, "name"));
    char *wanted = rp_strip(name);
    const json_t *providers = rp_get(req, "providers");
    char *canonical = NULL;
    json_t *out = json_null();
    size_t i, n;
    int shadowed;

    rp_lower(wanted);
    for (char *c = wanted; *c; c++) if (*c == ' ') *c = '-';
    shadowed = rp_shadowed(wanted, req, &canonical);
    if (!*wanted || !strcmp(wanted, "auto") || !providers || providers->type != JSON_OBJECT || !shadowed)
        goto done;
    n = json_object_size(providers);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(providers, i);
        const json_t *v = json_object_get_at(providers, i);
        char *cand, *lower;
        if (!k) continue;
        cand = rp_strip(k);
        lower = rp_str_dup_static(cand);
        rp_lower(lower);
        for (char *c = lower; *c; c++) if (*c == ' ') *c = '-';
        if (!strcmp(lower, wanted) && v && v->type == JSON_OBJECT && rp_enabled(v)) {
            const json_t *url = rp_get(v, "api");
            if (!rp_truthy(url)) url = rp_get(v, "url");
            if (!rp_truthy(url)) url = rp_get(v, "base_url");
            if (rp_truthy(url)) {
                json_free(out);
                out = rp_jstr(k);
                free(cand);
                free(lower);
                goto done;
            }
        }
        free(cand);
        free(lower);
    }
done:
    free(name);
    free(wanted);
    free(canonical);
    return out;
}

static json_t *p1_persisted_provider_id(const json_t *req) {
    const json_t *provider = rp_get(req, "provider");
    json_t *sub = json_copy(req);
    json_t *found;
    json_t *out;
    json_set(sub, "name", provider ? json_copy(provider) : json_null());
    found = p1_builtin_name_user_entry(sub);
    json_free(sub);
    if (!found || found->type == JSON_NULL) {
        json_free(found);
        return provider ? json_copy(provider) : json_null();
    }
    {
        char *owner = rp_str_dup_static(rp_sv(found));
        char *slug = rp_slug(owner, owner);
        out = rp_jstr(slug);
        free(owner);
        free(slug);
    }
    json_free(found);
    return out;
}

static char *rp_secret(const json_t *req, const char *name) {
    const json_t *secrets = rp_arg(req, "secrets");
    const json_t *v = rp_get(secrets, name);
    char *raw = rp_or_str(v);
    char *out = rp_strip(raw);
    free(raw);
    return out;
}

static char *rp_key_env_secret(const json_t *entry, const json_t *req) {
    const json_t *ke = rp_get(entry, "key_env");
    char *key_env, *value;
    if (!rp_truthy(ke)) ke = rp_get(entry, "api_key_env");
    key_env = rp_clean(ke);
    if (!*key_env) {
        free(key_env);
        return rp_str_dup_static("");
    }
    value = rp_secret(req, key_env);
    free(key_env);
    return value;
}

static json_t *p1_key_env_secret(const json_t *req) {
    char *value = rp_key_env_secret(rp_arg(req, "entry"), req);
    json_t *j = rp_jstr(value);
    free(value);
    return j;
}

static json_t *p1_model_cfg_key_env_for(const json_t *req) {
    const json_t *model_cfg = rp_arg(req, "model_cfg");
    char *cfg_base = rp_clean(rp_get(model_cfg, "base_url"));
    char *base = rp_clean(rp_get(req, "base_url"));
    size_t n;
    json_t *out;

    for (n = strlen(cfg_base); n && cfg_base[n - 1] == '/'; n--) cfg_base[n - 1] = '\0';
    for (n = strlen(base); n && base[n - 1] == '/'; n--) base[n - 1] = '\0';
    if (!*cfg_base || strcmp(cfg_base, base)) {
        out = rp_jstr("");
    } else {
        char *value = rp_key_env_secret(model_cfg, req);
        out = rp_jstr(value);
        free(value);
    }
    free(cfg_base);
    free(base);
    return out;
}

static json_t *rp_capabilities(const json_t *entry, const json_t *model) {
    json_t *caps = json_object();
    const json_t *entry_caps = rp_get(entry, "capabilities");
    const json_t *models = rp_get(entry, "models");
    size_t i, n;

    if (entry_caps && entry_caps->type == JSON_OBJECT) {
        n = json_object_size(entry_caps);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(entry_caps, i);
            const json_t *v = json_object_get_at(entry_caps, i);
            if (k && v && v->type == JSON_BOOL) json_set(caps, k, json_copy(v));
        }
    }
    if (model && model->type == JSON_STRING && models && models->type == JSON_OBJECT) {
        const json_t *mc = rp_get(models, model->str_val);
        if (mc && mc->type == JSON_OBJECT) {
            n = json_object_size(mc);
            for (i = 0; i < n; i++) {
                const char *k = json_object_get_key_at(mc, i);
                const json_t *v = json_object_get_at(mc, i);
                if (k && v && v->type == JSON_BOOL) json_set(caps, k, json_copy(v));
            }
        }
    }
    return caps;
}

static json_t *p1_lift_model_capabilities(const json_t *req) {
    json_t *caps = rp_capabilities(rp_arg(req, "entry"), rp_get(req, "model"));
    if (json_object_size(caps) == 0) {
        json_free(caps);
        return json_null();
    }
    return caps;
}

/* _lift_common_custom_fields, as a merge into an optional starting object. */
static void rp_lift_common(const json_t *entry, json_t *result, const char *provider_key,
                           const char *key_env, const char *api_mode, const json_t *req) {
    const json_t *extra_body = rp_get(entry, "extra_body");
    json_t *headers;
    json_t *caps;
    (void)req;

    if (key_env && *key_env) rp_set_str(result, "key_env", key_env);
    if (provider_key && *provider_key) rp_set_str(result, "provider_key", provider_key);
    if (extra_body && extra_body->type == JSON_OBJECT) json_set(result, "extra_body", json_copy(extra_body));

    headers = json_object();
    {
        const json_t *raw = rp_get(entry, "extra_headers");
        if (raw && raw->type == JSON_OBJECT) {
            size_t i, n = json_object_size(raw);
            for (i = 0; i < n; i++) {
                const char *k = json_object_get_key_at(raw, i);
                const json_t *v = json_object_get_at(raw, i);
                char *sv;
                if (!k || !v || v->type == JSON_NULL) continue;
                sv = rp_str_of(v);
                json_set(headers, k, rp_jstr(sv));
                free(sv);
            }
        }
    }
    if (json_object_size(headers) > 0) json_set(result, "extra_headers", headers);
    else json_free(headers);

    if (api_mode && *api_mode) rp_set_str(result, "api_mode", api_mode);

    caps = rp_capabilities(entry, NULL);
    if (json_object_size(caps) > 0) json_set(result, "capabilities", caps);
    else json_free(caps);
}

static json_t *p1_lift_common_custom_fields(const json_t *req) {
    const json_t *entry = rp_arg(req, "entry");
    const json_t *base = rp_get(req, "result");
    json_t *result = (base && base->type == JSON_OBJECT) ? json_copy(base) : json_object();
    char *provider_key = rp_or_str(rp_get(req, "provider_key"));
    char *key_env = rp_or_str(rp_get(req, "key_env"));
    char *api_mode = rp_or_str(rp_get(req, "api_mode"));

    rp_lift_common(entry, result, provider_key, key_env, api_mode, req);
    free(provider_key);
    free(key_env);
    free(api_mode);
    return result;
}

/* ─────────────────────── p1: entry matching (new/legacy) ─────────────────── */

static char *rp_entry_url_text(const json_t *entry) {
    const json_t *v;
    if (!entry || entry->type != JSON_OBJECT) return rp_str_dup_static("");
    v = rp_get(entry, "api");
    if (!rp_truthy(v)) v = rp_get(entry, "url");
    if (!rp_truthy(v)) v = rp_get(entry, "base_url");
    if (!rp_truthy(v)) return rp_str_dup_static("");
    return rp_or_str(v);
}

static json_t *p1_match_new_style_provider(const json_t *req) {
    char *norm = rp_or_str(rp_get(req, "requested_norm"));
    const json_t *providers = rp_get(req, "providers");
    const json_t *rc = rp_get(req, "resolve_credentials");
    int resolve = rc ? rp_truthy(rc) : 1;
    json_t *out = json_null();
    size_t i, n;

    if (!providers || providers->type != JSON_OBJECT) goto done;
    n = json_object_size(providers);
    for (i = 0; i < n; i++) {
        const char *ep_name = json_object_get_key_at(providers, i);
        const json_t *entry = json_object_get_at(providers, i);
        char *display, *base_url, *key_env, *api_key = NULL;
        json_t *result;

        if (!entry || entry->type != JSON_OBJECT || !rp_enabled(entry)) continue;
        display = rp_or_str(rp_get(entry, "name"));
        if (!*display) {
            free(display);
            display = rp_str_dup_static(ep_name ? ep_name : "");
        }
        if (!rp_aliases_has(display, ep_name ? ep_name : "", norm)) {
            free(display);
            continue;
        }
        base_url = rp_entry_url_text(entry);
        if (!*base_url) {
            free(display);
            free(base_url);
            continue;
        }
        {
            const json_t *ke = rp_get(entry, "key_env");
            if (!rp_truthy(ke)) ke = rp_get(entry, "api_key_env");
            key_env = rp_clean(ke);
        }
        api_key = (resolve && *key_env) ? rp_secret(req, key_env) : rp_str_dup_static("");
        if (!*api_key) {
            char *raw = rp_clean(rp_get(entry, "api_key"));
            free(api_key);
            api_key = raw;
        }
        result = json_object();
        {
            const json_t *name = rp_get(entry, "name");
            char *trimmed_base = rp_strip(base_url);
            const json_t *default_model = rp_get(entry, "default_model");
            json_set(result, "name", name ? json_copy(name) : rp_jstr(ep_name ? ep_name : ""));
            json_set(result, "base_url", rp_jstr(trimmed_base));
            json_set(result, "api_key", rp_jstr(api_key));
            json_set(result, "model", default_model ? json_copy(default_model) : rp_jstr(""));
            free(trimmed_base);
        }
        {
            char *key_cmd = rp_clean(rp_get(entry, "key_cmd"));
            if (*key_cmd) rp_set_str(result, "key_cmd", key_cmd);
            free(key_cmd);
        }
        {
            const json_t *raw_mode = rp_get(entry, "api_mode");
            char *mode_value = NULL;
            json_t *parsed;
            if (!rp_truthy(raw_mode)) raw_mode = rp_get(entry, "transport");
            /* rp._parse_api_mode(x or y) */
            {
                json_t *sub = json_object();
                json_t *mode_out;
                json_set(sub, "value", raw_mode ? json_copy(raw_mode) : json_null());
                json_set(sub, "registered", rp_arg(req, "registered") ? json_copy(rp_arg(req, "registered")) : json_null());
                mode_out = p1_parse_api_mode(sub);
                json_free(sub);
                parsed = mode_out;
            }
            if (parsed && parsed->type == JSON_STRING) mode_value = rp_str_dup_static(parsed->str_val);
            json_free(parsed);
            rp_lift_common(entry, result, ep_name ? ep_name : "", key_env,
                           mode_value ? mode_value : "", req);
            free(mode_value);
        }
        json_free(out);
        out = json_object();
        json_set(out, "entry", result);
        json_set(out, "key", rp_jstr(ep_name ? ep_name : ""));
        free(display);
        free(base_url);
        free(key_env);
        free(api_key);
        goto done;
    }
done:
    free(norm);
    return out;
}

static json_t *p1_match_legacy_custom_provider(const json_t *req) {
    char *norm = rp_or_str(rp_get(req, "requested_norm"));
    const json_t *list = rp_get(req, "custom_providers");
    json_t *out = json_null();
    size_t i, n;

    if (!list || list->type != JSON_ARRAY) goto done;
    n = json_len(list);
    for (i = 0; i < n; i++) {
        const json_t *entry = json_get(list, i);
        const json_t *name = rp_get(entry, "name");
        const json_t *base = rp_get(entry, "base_url");
        char *provider_key, *key_env, *model_name, *api_mode = NULL;
        json_t *result;

        if (!name || name->type != JSON_STRING) continue;
        if (!base || base->type != JSON_STRING) continue;
        provider_key = rp_clean(rp_get(entry, "provider_key"));
        if (!rp_aliases_has(name->str_val, provider_key, norm)) {
            free(provider_key);
            continue;
        }
        result = json_object();
        {
            char *trimmed_name = rp_strip(name->str_val);
            char *trimmed_base = rp_strip(base->str_val);
            char *api_key = rp_clean(rp_get(entry, "api_key"));
            json_set(result, "name", rp_jstr(trimmed_name));
            json_set(result, "base_url", rp_jstr(trimmed_base));
            json_set(result, "api_key", rp_jstr(api_key));
            free(trimmed_name);
            free(trimmed_base);
            free(api_key);
        }
        model_name = rp_clean(rp_get(entry, "model"));
        if (*model_name) rp_set_str(result, "model", model_name);
        free(model_name);

        key_env = rp_clean(rp_get(entry, "key_env"));
        {
            json_t *sub = json_object();
            json_t *parsed;
            json_set(sub, "value", rp_get(entry, "api_mode") ? json_copy(rp_get(entry, "api_mode")) : json_null());
            json_set(sub, "registered", rp_arg(req, "registered") ? json_copy(rp_arg(req, "registered")) : json_null());
            parsed = p1_parse_api_mode(sub);
            json_free(sub);
            if (parsed && parsed->type == JSON_STRING) api_mode = rp_str_dup_static(parsed->str_val);
            json_free(parsed);
        }
        rp_lift_common(entry, result, provider_key, key_env, api_mode ? api_mode : "", req);
        json_free(out);
        out = json_object();
        json_set(out, "entry", result);
        json_set(out, "raw", json_copy(entry));
        free(provider_key);
        free(key_env);
        free(api_mode);
        goto done;
    }
done:
    free(norm);
    return out;
}

/* _compatible_entry_path — the legacy view's normaliser is a seam
 * (``normalized`` is the per-raw-entry answer, index-aligned with ``custom_providers``). */
static json_t *p1_compatible_entry_path(const json_t *req) {
    const json_t *config = rp_get(req, "config");
    const json_t *entry = rp_arg(req, "entry");
    const json_t *providers = rp_get(config, "providers");
    const json_t *key = rp_get(entry, "provider_key");
    json_t *out = json_null();

    if (rp_truthy(key) && providers && providers->type == JSON_OBJECT) {
        char *key_text = rp_str_of(key);
        size_t i, n = json_object_size(providers);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(providers, i);
            char *trimmed = rp_strip(k);
            if (!strcmp(trimmed, key_text)) {
                out = json_array();
                json_append(out, rp_jstr("providers"));
                json_append(out, rp_jstr(k));
                free(trimmed);
                free(key_text);
                return out;
            }
            free(trimmed);
        }
        free(key_text);
        return out;
    }
    {
        const json_t *legacy = rp_get(config, "custom_providers");
        const json_t *normalized = rp_arg(req, "normalized");
        size_t i, n = (legacy && legacy->type == JSON_ARRAY) ? json_len(legacy) : 0;
        for (i = 0; i < n; i++) {
            const json_t *norm = (normalized && normalized->type == JSON_ARRAY) ? json_get(normalized, i) : NULL;
            if (rp_equal(norm, entry)) {
                out = json_array();
                json_append(out, rp_jstr("custom_providers"));
                json_append(out, json_number((double)i));
                return out;
            }
        }
    }
    return out;
}

/* _select_named_custom_provider */
static json_t *rp_selected(const json_t *req) {
    char *requested = rp_or_str(rp_get(req, "requested_provider"));
    char *norm = rp_strip(requested);
    const json_t *config = rp_get(req, "config");
    const json_t *rc = rp_get(req, "resolve_credentials");
    int resolve = rc ? rp_truthy(rc) : 1;
    int shadowed = 0;
    json_t *out = json_null();

    rp_lower(norm);
    for (char *c = norm; *c; c++) if (*c == ' ') *c = '-';
    if (!*norm || !strcmp(norm, "auto")) goto done;
    {
        char *canonical = NULL;
        shadowed = rp_shadowed(norm, req, &canonical);
        free(canonical);
    }
    if (!config || config->type == JSON_NULL) config = rp_arg(req, "load_config");
    {
        const json_t *providers = rp_get(config, "providers");
        json_t *sub, *matched;
        const json_t *effective = providers;

        if (shadowed && providers && providers->type == JSON_OBJECT) {
            sub = json_object();
            json_set(sub, "requested_norm", rp_jstr(norm));
            json_set(sub, "providers", json_copy(providers));
            effective = p1_builtin_named_entries(sub);
            json_free(sub);
        } else if (providers && providers->type == JSON_OBJECT) {
            effective = providers;
        } else {
            effective = NULL;
        }
        sub = json_object();
        json_set(sub, "requested_norm", rp_jstr(norm));
        json_set(sub, "providers", effective ? json_copy(effective) : json_null());
        json_set(sub, "resolve_credentials", rp_jbool(resolve));
        json_set(sub, "secrets", rp_arg(req, "secrets") ? json_copy(rp_arg(req, "secrets")) : json_object());
        json_set(sub, "registered", rp_arg(req, "registered") ? json_copy(rp_arg(req, "registered")) : json_null());
        matched = p1_match_new_style_provider(sub);
        json_free(sub);
        if (effective != providers) json_free((json_t *)effective);
        if (matched && matched->type == JSON_OBJECT) {
            out = json_object();
            json_set(out, "entry", json_copy(rp_get(matched, "entry")));
            {
                json_t *path = json_array();
                json_append(path, rp_jstr("providers"));
                json_append(path, json_copy(rp_get(matched, "key")));
                json_set(out, "path", path);
            }
            json_free(matched);
            goto done;
        }
        json_free(matched);
    }
    if (shadowed) goto done;
    {
        const json_t *legacy_dict = rp_get(config, "custom_providers");
        const json_t *compatible;
        json_t *matched;
        if (legacy_dict && legacy_dict->type == JSON_OBJECT) goto done;
        compatible = rp_arg(req, "compatible");
        if (!rp_truthy(compatible)) compatible = rp_arg(req, "compatible");
        if (!rp_truthy(compatible)) goto done;
        {
            json_t *sub = json_object();
            json_set(sub, "requested_norm", rp_jstr(norm));
            json_set(sub, "custom_providers", json_copy(compatible));
            json_set(sub, "secrets", rp_arg(req, "secrets") ? json_copy(rp_arg(req, "secrets")) : json_object());
            json_set(sub, "registered", rp_arg(req, "registered") ? json_copy(rp_arg(req, "registered")) : json_null());
            matched = p1_match_legacy_custom_provider(sub);
            json_free(sub);
        }
        if (matched && matched->type == JSON_OBJECT) {
            json_t *path_req = json_object();
            json_t *path;
            json_set(path_req, "config", config ? json_copy(config) : json_object());
            json_set(path_req, "entry", json_copy(rp_get(matched, "raw")));
            json_set(path_req, "normalized", rp_arg(req, "normalized") ? json_copy(rp_arg(req, "normalized")) : json_null());
            path = p1_compatible_entry_path(path_req);
            json_free(path_req);
            out = json_object();
            json_set(out, "entry", json_copy(rp_get(matched, "entry")));
            json_set(out, "path", path ? path : json_null());
            json_free(matched);
        } else {
            json_free(matched);
        }
    }
done:
    free(requested);
    free(norm);
    return out;
}

static json_t *p1_select_named_custom_provider(const json_t *req) {
    return rp_selected(req);
}

/* The named-entry answer: the seam's when the fixture supplies one, else this
 * module's own config scan for that name. *hold keeps the scan alive. */
static const json_t *rp_named_or_scan(const json_t *req, const char *name, json_t **hold) {
    const json_t *entry = rp_arg(req, "named_entry");
    *hold = NULL;
    if (entry) return entry;
    {
        json_t *sub = json_object();
        json_t *sel;
        json_set(sub, "requested_provider", rp_jstr(name ? name : ""));
        json_set(sub, "config", rp_get(req, "config") ? json_copy(rp_get(req, "config")) : json_null());
        json_set(sub, "compatible", rp_arg(req, "compatible") ? json_copy(rp_arg(req, "compatible")) : json_null());
        json_set(sub, "secrets", rp_arg(req, "secrets") ? json_copy(rp_arg(req, "secrets")) : json_object());
        json_set(sub, "canonical", rp_arg(req, "canonical") ? json_copy(rp_arg(req, "canonical")) : json_null());
        json_set(sub, "auth_error", rp_arg(req, "auth_error") ? json_copy(rp_arg(req, "auth_error")) : json_null());
        json_set(sub, "registered", rp_arg(req, "registered") ? json_copy(rp_arg(req, "registered")) : json_null());
        json_set(sub, "normalized", rp_arg(req, "normalized") ? json_copy(rp_arg(req, "normalized")) : json_null());
        sel = rp_selected(sub);
        json_free(sub);
        *hold = sel;
        return rp_get(sel, "entry");
    }
}

static json_t *p1_named_custom_provider_config_path(const json_t *req) {
    json_t *sel = rp_selected(req);
    json_t *out = json_null();
    if (sel && sel->type == JSON_OBJECT) {
        const json_t *path = rp_get(sel, "path");
        out = path ? json_copy(path) : json_null();
    }
    json_free(sel);
    return out;
}

static json_t *p1_custom_provider_config_path_for_url(const json_t *req) {
    char *url = rp_or_str(rp_get(req, "base_url"));
    const json_t *config = rp_get(req, "config");
    const json_t *legacy = rp_get(config, "custom_providers");
    const json_t *compatible = rp_arg(req, "compatible");
    json_t *out = json_null();
    size_t strip_n, i, n;

    strip_n = strlen(url);
    while (strip_n && url[strip_n - 1] == '/') url[--strip_n] = '\0';
    if (!*url || (legacy && legacy->type == JSON_OBJECT)) goto done;
    if (!compatible) compatible = rp_arg(req, "compatible");
    if (!compatible || compatible->type != JSON_ARRAY) goto done;
    n = json_len(compatible);
    for (i = 0; i < n; i++) {
        const json_t *entry = json_get(compatible, i);
        char *base;
        if (!entry || entry->type != JSON_OBJECT) continue;
        base = rp_or_str(rp_get(entry, "base_url"));
        {
            size_t bn = strlen(base);
            while (bn && base[bn - 1] == '/') base[--bn] = '\0';
        }
        if (!strcmp(base, url)) {
            json_t *path_req = json_object();
            json_set(path_req, "config", config ? json_copy(config) : json_object());
            json_set(path_req, "entry", json_copy(entry));
            json_set(path_req, "normalized", rp_arg(req, "normalized") ? json_copy(rp_arg(req, "normalized")) : json_null());
            out = p1_compatible_entry_path(path_req);
            json_free(path_req);
            free(base);
            goto done;
        }
        free(base);
    }
done:
    free(url);
    return out;
}

static json_t *p1_codex_model_provider_id(const json_t *req) {
    char *requested = rp_or_str(rp_get(req, "requested_provider"));
    char *norm = rp_strip(requested);
    json_t *hold = NULL;
    const json_t *entry;
    json_t *out = json_null();
    rp_lower(norm);
    for (char *c = norm; *c; c++) if (*c == ' ') *c = '-';
    if (!*norm || !strcmp(norm, "custom")) goto done;
    entry = rp_named_or_scan(req, requested, &hold);
    if (!rp_truthy(entry)) { json_free(hold); goto done; }
    {
        char *name = rp_or_str(rp_get(entry, "name"));
        char *key = rp_or_str(rp_get(entry, "provider_key"));
        char *slug = rp_slug(name, key);
        if (!strncmp(slug, "custom:", 7) && slug[7]) out = rp_jstr(slug + 7);
        free(name);
        free(key);
        free(slug);
    }
    json_free(hold);
done:
    free(requested);
    free(norm);
    return out;
}

static json_t *p1_has_named_custom_provider(const json_t *req) {
    char *requested = rp_or_str(rp_get(req, "requested_provider"));
    json_t *hold = NULL;
    int found;
    found = rp_truthy(rp_named_or_scan(req, requested, &hold));
    json_free(hold);
    free(requested);
    return rp_jbool(found);
}

/* ───────────────────── p2: identity-recovery predicates ──────────────────── */

static json_t *p2_has_usable_secret(const json_t *req) {
    return rp_jbool(rp_usable_secret(rp_get(req, "value")));
}

static json_t *p2_entry_declares_credential(const json_t *req) {
    return rp_jbool(rp_declares_credential(rp_arg(req, "entry")));
}

static json_t *p2_model_id_matches(const json_t *req) {
    const json_t *value = rp_get(req, "value");
    char *target = rp_or_str(rp_get(req, "target"));
    int hit = 0;
    if (value && value->type == JSON_STRING) {
        char *trimmed = rp_strip(value->str_val);
        rp_lower(trimmed);
        hit = !strcmp(trimmed, target);
        free(trimmed);
    }
    free(target);
    return rp_jbool(hit);
}

static json_t *p2_entry_serves_model(const json_t *req) {
    const json_t *entry = rp_arg(req, "entry");
    char *target = rp_or_str(rp_get(req, "model"));
    int hit = 0;
    size_t i, n;

    rp_lower(target);
    {
        char *trimmed = rp_strip(target);
        free(target);
        target = trimmed;
    }
    if (!entry || entry->type != JSON_OBJECT) {
        free(target);
        return rp_jbool(0);
    }
    {
        static const char *const keys[] = {"model", "default_model"};
        for (i = 0; i < 2 && !hit; i++) {
            const json_t *v = rp_get(entry, keys[i]);
            if (v && v->type == JSON_STRING) {
                char *t = rp_strip(v->str_val);
                rp_lower(t);
                if (!strcmp(t, target)) hit = 1;
                free(t);
            }
        }
    }
    if (!hit) {
        const json_t *models = rp_get(entry, "models");
        if (models && models->type == JSON_OBJECT) {
            n = json_object_size(models);
            for (i = 0; i < n && !hit; i++) {
                const char *k = json_object_get_key_at(models, i);
                char *t = rp_strip(k);
                rp_lower(t);
                if (!strcmp(t, target)) hit = 1;
                free(t);
            }
        } else if (models && models->type == JSON_ARRAY) {
            n = json_len(models);
            for (i = 0; i < n && !hit; i++) {
                const json_t *item = json_get(models, i);
                const json_t *value = NULL;
                if (item && item->type == JSON_OBJECT) {
                    value = rp_get(item, "id");
                    if (!rp_truthy(value)) value = rp_get(item, "name");
                } else if (item) {
                    value = item;
                }
                if (value && value->type == JSON_STRING) {
                    char *t = rp_strip(value->str_val);
                    rp_lower(t);
                    if (!strcmp(t, target)) hit = 1;
                    free(t);
                }
            }
        }
    }
    free(target);
    return rp_jbool(hit);
}

/* matches() for _find_custom_identity: mode "url" | "model". */
static int rp_entry_matches(const json_t *entry, const char *mode, const char *target) {
    if (!strcmp(mode, "url")) {
        char *url = rp_entry_url_text(entry);
        char *norm = rp_route_norm(url);
        int hit = !strcmp(norm, target);
        free(url);
        free(norm);
        return hit;
    }
    {
        json_t *sub = json_object();
        json_t *out;
        int hit;
        json_set(sub, "entry", entry ? json_copy(entry) : json_null());
        json_set(sub, "model", rp_jstr(target));
        out = p2_entry_serves_model(sub);
        hit = rp_truthy(out);
        json_free(sub);
        json_free(out);
        return hit;
    }
}

static char *rp_find_identity(const json_t *req, const char *mode, const char *target_in,
                              const char *prefer_in) {
    const json_t *config = rp_get(req, "config");
    const json_t *providers = rp_get(config, "providers");
    const json_t *compatible = rp_arg(req, "compatible");
    char *target = rp_str_dup_static(target_in ? target_in : "");
    char *prefer = prefer_in ? rp_str_dup_static(prefer_in) : NULL;
    char *first = NULL;
    rp_alias_set owned;
    size_t i, n;

    if (!config || config->type == JSON_NULL) config = rp_arg(req, "load_config");
    if (!compatible) compatible = rp_arg(req, "compatible");
    owned.n = 0;
    providers = rp_get(config, "providers");
    if (providers && providers->type == JSON_OBJECT) {
        n = json_object_size(providers);
        for (i = 0; i < n; i++) {
            const char *ep_name = json_object_get_key_at(providers, i);
            const json_t *entry = json_object_get_at(providers, i);
            char *slug, *url;
            if (!ep_name || !entry || entry->type != JSON_OBJECT || !rp_enabled(entry)) continue;
            slug = rp_slug(ep_name, ep_name);
            url = rp_entry_url_text(entry);
            if (*url) rp_alias_add(&owned, slug);
            free(url);
            if (rp_entry_matches(entry, mode, target)) {
                if (!prefer || !strcmp(slug, prefer)) {
                    char *out = rp_str_dup_static(slug);
                    free(slug);
                    free(target);
                    free(prefer);
                    for (i = 0; i < (size_t)owned.n; i++) free(owned.items[i]);
                    return out;
                }
                (void)slug;
                if (!first) first = rp_str_dup_static(slug);
            }
            free(slug);
        }
    }
    if (compatible && compatible->type == JSON_ARRAY) {
        n = json_len(compatible);
        for (i = 0; i < n; i++) {
            const json_t *entry = json_get(compatible, i);
            const json_t *name = rp_get(entry, "name");
            char *slug, *key;
            int is_owned, skip;
            if (!name || name->type != JSON_STRING) continue;
            {
                char *trimmed = rp_strip(name->str_val);
                skip = !*trimmed;
                free(trimmed);
            }
            if (skip || !rp_entry_matches(entry, mode, target)) continue;
            key = rp_or_str(rp_get(entry, "provider_key"));
            slug = rp_slug(name->str_val, key);
            is_owned = 0;
            for (size_t j = 0; j < (size_t)owned.n; j++)
                if (!strcmp(owned.items[j], slug)) { is_owned = 1; break; }
            if (is_owned && !*key) {
                free(slug);
                free(key);
                continue;
            }
            free(key);
            if (!prefer || !strcmp(slug, prefer)) {
                char *out = slug;
                free(target);
                free(prefer);
                free(first);
                for (size_t j = 0; j < (size_t)owned.n; j++) free(owned.items[j]);
                return out;
            }
            if (!first) first = rp_str_dup_static(slug);
            free(slug);
        }
    }
    for (i = 0; i < (size_t)owned.n; i++) free(owned.items[i]);
    free(target);
    free(prefer);
    return first;
}

static json_t *p2_find_custom_identity(const json_t *req) {
    char *mode = rp_or_str(rp_get(req, "mode"));
    const json_t *target_j = rp_get(req, "target");
    char *target = NULL, *prefer = NULL, *found;
    const json_t *prefer_j = rp_get(req, "prefer");
    json_t *out;

    if (!strcmp(mode, "url")) {
        char *raw = rp_or_str(target_j);
        target = rp_route_key_text(raw);
        free(raw);
    } else {
        char *raw = rp_or_str(target_j);
        char *trimmed = rp_strip(raw);
        rp_lower(trimmed);
        free(raw);
        target = trimmed;
    }
    if (rp_truthy(prefer_j)) prefer = rp_str_of(prefer_j);
    found = rp_find_identity(req, mode, target, prefer);
    if (found && *found) out = rp_jstr(found);
    else out = json_null();
    free(found);
    free(target);
    free(prefer);
    free(mode);
    return out;
}

static json_t *p2_find_custom_provider_identity(const json_t *req) {
    char *raw = rp_or_str(rp_get(req, "base_url"));
    char *target = rp_route_key_text(raw);
    char *found;
    json_t *out;
    if (!*target) {
        free(raw);
        free(target);
        return json_null();
    }
    found = rp_find_identity(req, "url", target, NULL);
    out = (found && *found) ? rp_jstr(found) : json_null();
    free(raw);
    free(target);
    free(found);
    return out;
}

static json_t *p2_find_custom_provider_identity_by_model(const json_t *req) {
    char *raw = rp_or_str(rp_get(req, "model"));
    char *target = rp_strip(raw);
    char *found;
    json_t *out;
    rp_lower(target);
    if (!*target) {
        free(raw);
        free(target);
        return json_null();
    }
    found = rp_find_identity(req, "model", target, NULL);
    out = (found && *found) ? rp_jstr(found) : json_null();
    free(raw);
    free(target);
    free(found);
    return out;
}

static json_t *p2_routed_custom_identity(const json_t *req) {
    char *raw_url = rp_or_str(rp_get(req, "base_url"));
    char *url = rp_route_key_text(raw_url);
    char *raw_model = rp_or_str(rp_get(req, "model"));
    char *target = rp_strip(raw_model);
    const json_t *held_j = rp_get(req, "held");
    char *held = rp_truthy(held_j) ? rp_str_of(held_j) : NULL;
    char *found = NULL, *first = NULL;
    json_t *out = json_null();

    rp_lower(target);
    if (*target) {
        /* find: on_url(entry) and entry_serves_model(entry, target), prefer=held */
        const json_t *config = rp_get(req, "config");
        const json_t *compatible = rp_arg(req, "compatible");
        const json_t *providers;
        size_t i, n;
        rp_alias_set owned;
        int matched = 0;

        if (!config) config = rp_arg(req, "load_config");
        if (!compatible) compatible = rp_arg(req, "compatible");
        owned.n = 0;
        providers = rp_get(config, "providers");
        if (providers && providers->type == JSON_OBJECT) {
            n = json_object_size(providers);
            for (i = 0; i < n; i++) {
                const char *ep_name = json_object_get_key_at(providers, i);
                const json_t *entry = json_object_get_at(providers, i);
                char *slug, *url_text;
                int on_url, serves;
                if (!ep_name || !entry || entry->type != JSON_OBJECT || !rp_enabled(entry)) continue;
                slug = rp_slug(ep_name, ep_name);
                url_text = rp_entry_url_text(entry);
                if (*url_text) rp_alias_add(&owned, slug);
                {
                    char *norm = rp_route_norm(url_text);
                    on_url = !*url ? 1 : !strcmp(norm, url);
                    free(norm);
                }
                serves = rp_entry_matches(entry, "model", target);
                free(url_text);
                if (on_url && serves) {
                    matched = 1;
                    if (!held || !strcmp(slug, held)) {
                        if (!found) found = rp_str_dup_static(slug);
                    } else if (!first) {
                        first = rp_str_dup_static(slug);
                    }
                }
                free(slug);
            }
        }
        if (!found && compatible && compatible->type == JSON_ARRAY) {
            n = json_len(compatible);
            for (i = 0; i < n && !found; i++) {
                const json_t *entry = json_get(compatible, i);
                const json_t *name = rp_get(entry, "name");
                char *slug, *key, *url_text;
                int on_url, is_owned = 0;
                if (!name || name->type != JSON_STRING) continue;
                {
                    char *trimmed = rp_strip(name->str_val);
                    int empty = !*trimmed;
                    free(trimmed);
                    if (empty) continue;
                }
                if (!rp_entry_matches(entry, "model", target)) continue;
                url_text = rp_entry_url_text(entry);
                {
                    char *norm = rp_route_norm(url_text);
                    on_url = !*url ? 1 : !strcmp(norm, url);
                    free(norm);
                }
                free(url_text);
                if (!on_url) continue;
                key = rp_or_str(rp_get(entry, "provider_key"));
                slug = rp_slug(name->str_val, key);
                for (size_t j = 0; j < (size_t)owned.n; j++)
                    if (!strcmp(owned.items[j], slug)) { is_owned = 1; break; }
                if (is_owned && !*key) {
                    free(slug);
                    free(key);
                    continue;
                }
                free(key);
                if (!held || !strcmp(slug, held)) found = slug;
                else { if (!first) first = rp_str_dup_static(slug); free(slug); }
            }
        }
        for (i = 0; i < (size_t)owned.n; i++) free(owned.items[i]);
        if (matched && !found) found = first ? rp_str_dup_static(first) : NULL;
    }
    if (!found && *url) {
        char *f2 = rp_find_identity(req, "url", url, NULL);
        if (f2 && *f2) found = f2;
        else free(f2);
    }
    if (found && *found) out = rp_jstr(found);
    free(found);
    free(first);
    free(held);
    free(raw_url);
    free(url);
    free(raw_model);
    free(target);
    return out;
}

static json_t *p2_empty_provider_identity(const json_t *req) {
    char *raw_model = rp_or_str(rp_get(req, "model"));
    char *target = rp_strip(raw_model);
    json_t *out = json_null();
    rp_lower(target);
    if (*target) {
        json_t *found = p2_find_custom_provider_identity_by_model(req);
        if (found && found->type == JSON_STRING) out = json_copy(found);
        json_free(found);
        if (out->type == JSON_STRING) goto done;
    }
    {
        char *raw_url = rp_or_str(rp_get(req, "base_url"));
        char *url = rp_route_key_text(raw_url);
        if (*url) {
            char *found = rp_find_identity(req, "url", url, NULL);
            if (found && *found) {
                json_free(out);
                out = rp_jstr(found);
            }
            free(found);
        }
        free(raw_url);
        free(url);
    }
done:
    free(raw_model);
    free(target);
    return out;
}

static json_t *p2_endpoint_identity(const json_t *req) {
    json_t *identity = p2_find_custom_provider_identity(req);
    json_t *out = json_null();
    if (identity && identity->type == JSON_STRING) {
        out = json_copy(identity);
        json_free(identity);
        return out;
    }
    json_free(identity);
    {
        const json_t *endpoint = rp_arg(req, "managed_endpoint");
        const json_t *ep_base = rp_get(endpoint, "base_url");
        if (rp_truthy(endpoint) && ep_base) {
            char *raw = rp_or_str(rp_get(req, "base_url"));
            char *a = rp_route_key_text(raw);
            char *eb = rp_or_str(ep_base);
            char *b2 = rp_route_key_text(eb);
            if (!strcmp(a, b2) && *a) out = rp_jstr("llamacpp");
            free(raw);
            free(a);
            free(eb);
            free(b2);
        }
    }
    return out;
}

/* ───────────────────── p2: session / canonical identity ──────────────────── */

static json_t *p2_canonical_custom_identity(const json_t *req);

static json_t *p2_session_provider_identity(const json_t *req) {
    char *provider = rp_clean(rp_get(req, "provider"));
    char *lower = rp_str_dup_static(provider);
    json_t *out;
    rp_lower(lower);
    if (strcmp(lower, "custom")) {
        out = rp_jstr(provider);
        free(provider);
        free(lower);
        return out;
    }
    free(lower);
    {
        char *requested = rp_clean(rp_get(req, "requested_provider"));
        char *raw_url = rp_or_str(rp_get(req, "base_url"));
        char *url = rp_route_key_text(raw_url);
        char *req_lower = rp_str_dup_static(requested);
        int skip;

        rp_lower(req_lower);
        skip = !*req_lower || !strcmp(req_lower, "custom") || !strcmp(req_lower, "auto");
        free(req_lower);

        if (*url && !skip) {
            json_t *sel_req = json_object();
            json_t *sel;
            json_set(sel_req, "requested_provider", rp_jstr(requested));
            json_set(sel_req, "config", rp_get(req, "config") ? json_copy(rp_get(req, "config")) : json_null());
            json_set(sel_req, "compatible", rp_arg(req, "compatible") ? json_copy(rp_arg(req, "compatible")) : json_null());
            json_set(sel_req, "secrets", rp_arg(req, "secrets") ? json_copy(rp_arg(req, "secrets")) : json_object());
            json_set(sel_req, "resolve_credentials", rp_jbool(0));
            json_set(sel_req, "canonical", rp_arg(req, "canonical") ? json_copy(rp_arg(req, "canonical")) : json_null());
            json_set(sel_req, "auth_error", rp_arg(req, "auth_error") ? json_copy(rp_arg(req, "auth_error")) : json_null());
            json_set(sel_req, "registered", rp_arg(req, "registered") ? json_copy(rp_arg(req, "registered")) : json_null());
            json_set(sel_req, "normalized", rp_arg(req, "normalized") ? json_copy(rp_arg(req, "normalized")) : json_null());
            sel = rp_selected(sel_req);
            json_free(sel_req);
            if (sel && sel->type == JSON_OBJECT) {
                const json_t *entry = rp_get(sel, "entry");
                char *entry_url = rp_entry_url_text(entry);
                char *norm = rp_route_norm(entry_url);
                int same = !strcmp(norm, url);
                free(entry_url);
                free(norm);
                json_free(sel);
                if (same) {
                    out = rp_jstr(requested);
                    free(requested);
                    free(raw_url);
                    free(url);
                    return out;
                }
            } else {
                json_free(sel);
            }
        }
        free(requested);
        if (*url) {
            json_t *identity;
            char *found = NULL;
            json_t *ep_req = json_object();
            json_set(ep_req, "base_url", rp_jstr(raw_url));
            json_set(ep_req, "config", rp_get(req, "config") ? json_copy(rp_get(req, "config")) : json_null());
            json_set(ep_req, "compatible", rp_arg(req, "compatible") ? json_copy(rp_arg(req, "compatible")) : json_null());
            json_set(ep_req, "managed_endpoint", rp_arg(req, "managed_endpoint") ? json_copy(rp_arg(req, "managed_endpoint")) : json_null());
            identity = p2_endpoint_identity(ep_req);
            json_free(ep_req);
            if (identity && identity->type == JSON_STRING) found = rp_str_dup_static(identity->str_val);
            json_free(identity);
            out = rp_jstr(found ? found : provider);
            free(found);
            free(raw_url);
            free(url);
            free(provider);
            return out;
        }
        free(raw_url);
        free(url);
        {
            json_t *canon_req = json_object();
            json_t *canon;
            char *identified = NULL;
            const json_t *model = rp_get(req, "model");
            json_set(canon_req, "base_url", json_null());
            json_set(canon_req, "config_provider", json_null());
            json_set(canon_req, "model", model ? json_copy(model) : json_null());
            json_set(canon_req, "requested", json_null());
            json_set(canon_req, "config", rp_get(req, "config") ? json_copy(rp_get(req, "config")) : json_null());
            json_set(canon_req, "compatible", rp_arg(req, "compatible") ? json_copy(rp_arg(req, "compatible")) : json_null());
            json_set(canon_req, "canonical", rp_arg(req, "canonical") ? json_copy(rp_arg(req, "canonical")) : json_null());
            json_set(canon_req, "auth_error", rp_arg(req, "auth_error") ? json_copy(rp_arg(req, "auth_error")) : json_null());
            json_set(canon_req, "registered", rp_arg(req, "registered") ? json_copy(rp_arg(req, "registered")) : json_null());
            canon = p2_canonical_custom_identity(canon_req);
            json_free(canon_req);
            if (canon && canon->type == JSON_STRING) identified = rp_str_dup_static(canon->str_val);
            json_free(canon);
            out = rp_jstr(identified ? identified : provider);
            free(identified);
            free(provider);
            return out;
        }
    }
}

static json_t *p2_canonical_custom_identity(const json_t *req) {
    const json_t *base_url_j = rp_get(req, "base_url");
    json_t *identity = json_null();
    char *requested_str = NULL;

    if (rp_truthy(base_url_j)) {
        json_t *sess_req = json_object();
        json_t *sess;
        json_set(sess_req, "provider", rp_jstr("custom"));
        json_set(sess_req, "requested_provider", rp_get(req, "requested") ? json_copy(rp_get(req, "requested")) : json_null());
        json_set(sess_req, "base_url", json_copy(base_url_j));
        json_set(sess_req, "model", json_null());
        json_set(sess_req, "config", rp_get(req, "config") ? json_copy(rp_get(req, "config")) : json_null());
        json_set(sess_req, "compatible", rp_arg(req, "compatible") ? json_copy(rp_arg(req, "compatible")) : json_null());
        json_set(sess_req, "managed_endpoint", rp_arg(req, "managed_endpoint") ? json_copy(rp_arg(req, "managed_endpoint")) : json_null());
        json_set(sess_req, "canonical", rp_arg(req, "canonical") ? json_copy(rp_arg(req, "canonical")) : json_null());
        json_set(sess_req, "auth_error", rp_arg(req, "auth_error") ? json_copy(rp_arg(req, "auth_error")) : json_null());
        json_set(sess_req, "registered", rp_arg(req, "registered") ? json_copy(rp_arg(req, "registered")) : json_null());
        json_set(sess_req, "normalized", rp_arg(req, "normalized") ? json_copy(rp_arg(req, "normalized")) : json_null());
        sess = p2_session_provider_identity(sess_req);
        json_free(sess_req);
        json_free(identity);
        identity = sess ? sess : json_null();
    } else {
        json_free(identity);
        identity = rp_jstr("custom");
    }
    if (!strcmp(rp_sv(identity), "custom")) {
        const json_t *model_j = rp_get(req, "model");
        if (rp_truthy(model_j)) {
            json_t *by_model = p2_find_custom_provider_identity_by_model(req);
            if (by_model && by_model->type == JSON_STRING) {
                json_free(identity);
                identity = by_model;
                return identity;
            }
            json_free(by_model);
        }
        {
            char *candidate = rp_clean(rp_get(req, "config_provider"));
            if (!*candidate) {
                char *mcfg = rp_clean(rp_get(rp_arg(req, "model_cfg"), "provider"));
                free(candidate);
                candidate = mcfg;
            }
            if (!*candidate) {
                const json_t *env = rp_arg(req, "env");
                char *env_value = rp_or_str(rp_get(env, "HERMES_INFERENCE_PROVIDER"));
                char *trimmed = rp_strip(env_value);
                free(env_value);
                free(candidate);
                candidate = trimmed;
            }
            {
                char *norm = rp_str_dup_static(candidate);
                rp_lower(norm);
                for (char *c = norm; *c; c++) if (*c == ' ') *c = '-';
                if (!*norm || !strcmp(norm, "custom") || !strcmp(norm, "auto") || !strcmp(norm, "openrouter")) {
                    free(candidate);
                    free(norm);
                    json_free(identity);
                    return json_null();
                }
                {
                    json_t *hold = NULL;
                    const json_t *entry = rp_named_or_scan(req, candidate, &hold);
                    if (!rp_truthy(entry)) {
                        json_free(hold);
                        free(candidate);
                        free(norm);
                        json_free(identity);
                        return json_null();
                    }
                    {
                        char *entry_base = rp_or_str(rp_get(entry, "base_url"));
                        const json_t *sub = req;
                        json_t *sub_req = json_object();
                        json_t *found;
                        (void)sub;
                        json_set(sub_req, "base_url", rp_jstr(entry_base));
                        json_set(sub_req, "config", rp_get(req, "config") ? json_copy(rp_get(req, "config")) : json_null());
                        json_set(sub_req, "compatible", rp_arg(req, "compatible") ? json_copy(rp_arg(req, "compatible")) : json_null());
                        found = p2_find_custom_provider_identity(sub_req);
                        json_free(sub_req);
                        free(entry_base);
                        if (found && found->type == JSON_STRING) {
                            json_free(identity);
                            identity = found;
                        } else {
                            char *slug = rp_slug(norm, "");
                            json_free(found);
                            json_free(identity);
                            identity = rp_jstr(slug);
                            free(slug);
                        }
                        json_free(hold);
                    }
                }
                free(candidate);
                free(norm);
            }
        }
    }
    (void)requested_str;
    return identity;
}

static json_t *p2_is_routable_provider(const json_t *req) {
    char *name = rp_or_str(rp_get(req, "provider"));
    char *trimmed = rp_strip(name);
    char *lower;
    json_t *out;
    rp_lower(trimmed);
    if (!*trimmed) {
        free(name);
        free(trimmed);
        return rp_jbool(1);
    }
    if (!strcmp(trimmed, "auto")) {
        free(name);
        free(trimmed);
        return rp_jbool(1);
    }
    if (!strcmp(trimmed, "custom")) {
        free(name);
        free(trimmed);
        return rp_jbool(0);
    }
    out = rp_jbool(rp_truthy(rp_arg(req, "resolved")));
    free(name);
    free(trimmed);
    (void)lower;
    return out;
}

/* ─────────────────── p2: extras, alias and credential decisions ─────────── */

static json_t *p2_custom_provider_request_overrides(const json_t *req) {
    const json_t *cp = rp_get(req, "custom_provider");
    const json_t *extra = rp_get(cp, "extra_body");
    json_t *out = json_object();
    if (!extra || extra->type != JSON_OBJECT || json_object_size(extra) == 0) return out;
    json_set(out, "extra_body", json_copy(extra));
    return out;
}

static void rp_apply_extras(const json_t *cp, const json_t *target_model, json_t *result) {
    const json_t *model_name = NULL;
    json_t *caps;
    if (rp_truthy(target_model)) model_name = target_model;
    else model_name = rp_get(cp, "model");
    if (rp_truthy(model_name)) json_set(result, "model", json_copy(model_name));
    caps = rp_capabilities(cp, model_name);
    if (json_object_size(caps) > 0) json_set(result, "capabilities", caps);
    else json_free(caps);
    {
        const json_t *headers = rp_get(cp, "extra_headers");
        if (rp_truthy(headers)) json_set(result, "extra_headers", json_copy(headers));
    }
    {
        const json_t *extra = rp_get(cp, "extra_body");
        if (extra && extra->type == JSON_OBJECT && json_object_size(extra) > 0) {
            json_t *merged = json_object();
            const json_t *existing = rp_get(result, "request_overrides");
            size_t i, n;
            if (existing && existing->type == JSON_OBJECT) {
                n = json_object_size(existing);
                for (i = 0; i < n; i++)
                    json_set(merged, json_object_get_key_at(existing, i),
                             json_copy(json_object_get_at(existing, i)));
            }
            json_set(merged, "extra_body", json_copy(extra));
            json_set(result, "request_overrides", merged);
        }
    }
}

static json_t *p2_apply_custom_provider_extras(const json_t *req) {
    const json_t *base = rp_get(req, "result");
    json_t *result = (base && base->type == JSON_OBJECT) ? json_copy(base) : json_object();
    rp_apply_extras(rp_get(req, "custom_provider"), rp_get(req, "target_model"), result);
    return result;
}

static json_t *p2_expand_direct_api_alias(const json_t *req) {
    const json_t *provider = rp_get(req, "provider");
    const json_t *existing = rp_get(req, "existing_base");
    const json_t *named = rp_arg(req, "named_entry");
    json_t *out = json_array();

    if (!rp_truthy(provider)) {
        json_append(out, provider ? json_copy(provider) : json_null());
        json_append(out, existing ? json_copy(existing) : json_null());
        return out;
    }
    {
        char *raw = rp_or_str(provider);
        char *norm = rp_strip(raw);
        int is_openai;
        rp_lower(norm);
        is_openai = !strcmp(norm, "openai");
        free(norm);
        free(raw);
        if (!is_openai || (named && named->type != JSON_NULL)) {
            json_append(out, json_copy(provider));
            json_append(out, existing ? json_copy(existing) : json_null());
            return out;
        }
    }
    {
        char *base = rp_or_str(existing);
        char *trimmed = rp_strip(base);
        json_append(out, rp_jstr("custom"));
        if (*trimmed) {
            json_append(out, rp_jstr(trimmed));
        } else {
            char *env = rp_secret(req, "OPENAI_BASE_URL");
            char *env_trimmed = rp_strip(env);
            size_t n = strlen(env_trimmed);
            while (n && env_trimmed[n - 1] == '/') env_trimmed[--n] = '\0';
            json_append(out, rp_jstr(*env_trimmed ? env_trimmed : "https://api.openai.com/v1"));
            free(env);
            free(env_trimmed);
        }
        free(base);
        free(trimmed);
    }
    return out;
}

static json_t *p2_opencode_family_for_custom(const json_t *req) {
    const json_t *family = rp_arg(req, "family");
    if (rp_truthy(family)) return json_copy(family);
    {
        char *raw = rp_or_str(rp_get(req, "base_url"));
        char *host = rp_hostname(raw);
        json_t *out = json_null();
        if (!strcmp(host, "opencode.ai")) {
            char *lower = rp_str_dup_static(raw);
            rp_lower(lower);
            out = rp_jstr(strstr(lower, "/zen/go") ? "opencode-go" : "opencode-zen");
            free(lower);
        }
        free(raw);
        free(host);
        return out;
    }
}

static json_t *rp_entry_credential(const json_t *entry, const char *name, const json_t *req,
                                   int key_cmd_buildable) {
    char *key_cmd = rp_clean(rp_get(entry, "key_cmd"));
    json_t *out = NULL;
    if (*key_cmd && key_cmd_buildable) {
        json_t *tp = json_object();
        json_set(tp, "__token_provider__", rp_jstr(key_cmd));
        free(key_cmd);
        return tp;
    }
    free(key_cmd);
    {
        char *api_key = rp_clean(rp_get(entry, "api_key"));
        json_t *probe = json_string(api_key);
        if (rp_usable_secret(probe)) {
            json_free(probe);
            out = rp_jstr(api_key);
            free(api_key);
            return out;
        }
        json_free(probe);
        free(api_key);
    }
    {
        char *value = rp_key_env_secret(entry, req);
        json_t *probe = json_string(value);
        if (rp_usable_secret(probe)) out = rp_jstr(value);
        else out = rp_jstr("");
        json_free(probe);
        free(value);
        (void)name;
        return out;
    }
}

static json_t *p2_entry_credential(const json_t *req) {
    const json_t *entry = rp_arg(req, "entry");
    char *name = rp_or_str(rp_get(req, "name"));
    json_t *out = rp_entry_credential(entry, name, req, rp_truthy(rp_arg(req, "key_cmd_buildable")));
    free(name);
    return out;
}

static json_t *rp_owning_credential(const json_t *req) {
    const json_t *entry = rp_arg(req, "entry");
    json_t *out = json_object();
    if (!rp_truthy(entry) || !rp_declares_credential(entry)) {
        json_set(out, "declared", rp_jbool(0));
        json_set(out, "key", rp_jstr(""));
        return out;
    }
    {
        char *name = rp_or_str(rp_get(entry, "name"));
        char *label = *name ? rp_str_dup_static(name) : rp_str_dup_static("custom");
        json_t *key = rp_entry_credential(entry, label, req, rp_truthy(rp_arg(req, "key_cmd_buildable")));
        json_set(out, "declared", rp_jbool(1));
        json_set(out, "key", key);
        free(name);
        free(label);
    }
    return out;
}

static json_t *p2_owning_entry_credential(const json_t *req) {
    return rp_owning_credential(req);
}

static json_t *p2_owned_config_url_credential(const json_t *req) {
    char *cfg_url = rp_clean(rp_arg(req, "config_base_url"));
    char *base = rp_clean(rp_get(req, "base_url"));
    size_t n;
    json_t *out;

    for (n = strlen(base); n && base[n - 1] == '/'; n--) base[n - 1] = '\0';
    if (!*cfg_url || strcmp(cfg_url, base)) {
        out = json_object();
        json_set(out, "declared", rp_jbool(0));
        json_set(out, "key", rp_jstr(""));
    } else {
        out = rp_owning_credential(req);
    }
    free(cfg_url);
    free(base);
    return out;
}

/* _custom_runtime: api_mode || detected || "chat_completions"; key || placeholder. */
static void rp_runtime_into(json_t *out, const char *api_mode, const char *detected,
                            const char *base_url, const char *api_key, const char *source,
                            const char *requested_provider) {
    const char *mode = (api_mode && *api_mode) ? api_mode
                       : (detected && *detected) ? detected : "chat_completions";
    rp_set_str(out, "provider", "custom");
    rp_set_str(out, "api_mode", mode);
    rp_set_str(out, "base_url", base_url ? base_url : "");
    rp_set_str(out, "api_key", (api_key && *api_key) ? api_key : "no-key-required");
    rp_set_str(out, "source", source ? source : "");
    if (requested_provider) rp_set_str(out, "requested_provider", requested_provider);
}

static json_t *p2_resolve_direct_alias_runtime(const json_t *req) {
    char *raw_base = rp_or_str(rp_get(req, "explicit_base_url"));
    char *base_url = rp_strip(raw_base);
    size_t n = strlen(base_url);
    char *explicit_key = rp_clean(rp_get(req, "explicit_api_key"));
    const json_t *pool = rp_arg(req, "pool");
    json_t *out;

    while (n && base_url[n - 1] == '/') base_url[--n] = '\0';

    {
        json_t *probe = json_string(explicit_key);
        int usable = rp_usable_secret(probe);
        json_free(probe);
        if (!usable) {
            if (rp_truthy(pool)) {
                out = json_copy(pool);
                rp_set_str(out, "source", "direct-alias");
                free(raw_base);
                free(base_url);
                free(explicit_key);
                return out;
            }
        }
    }
    {
        /* _owning_entry_credential(rp, base_url): the owning entry's own credential,
         * so its declaration decides whether host-gated env keys may stand in. */
        json_t *own = rp_owning_credential(req);
        int declared = rp_truthy(rp_get(own, "declared"));
        char *entry_key = rp_or_str(rp_get(own, "key"));
        char *api_key = NULL;
        json_free(own);

        {
            json_t *probe = json_string(explicit_key);
            if (rp_usable_secret(probe)) api_key = rp_str_dup_static(explicit_key);
            json_free(probe);
        }
        if (!api_key) {
            json_t *sub = json_object();
            json_set(sub, "model_cfg", rp_arg(req, "model_cfg") ? json_copy(rp_arg(req, "model_cfg")) : json_object());
            json_set(sub, "base_url", rp_jstr(base_url));
            json_set(sub, "secrets", rp_arg(req, "secrets") ? json_copy(rp_arg(req, "secrets")) : json_object());
            {
                json_t *moved = p1_model_cfg_key_env_for(sub);
                char *cand = rp_or_str(moved);
                json_free(moved);
                json_free(sub);
                {
                    json_t *probe = json_string(cand);
                    if (rp_usable_secret(probe)) api_key = rp_str_dup_static(cand);
                    json_free(probe);
                }
                free(cand);
            }
        }
        if (!api_key && !declared) {
            const json_t *gated = rp_arg(req, "host_gated");
            if (gated && gated->type == JSON_ARRAY) {
                size_t i, gn = json_len(gated);
                for (i = 0; i < gn && !api_key; i++) {
                    const json_t *item = json_get(gated, i);
                    json_t *probe;
                    if (!item || item->type != JSON_STRING) continue;
                    probe = json_copy(item);
                    if (rp_usable_secret(probe)) api_key = rp_str_dup_static(item->str_val);
                    json_free(probe);
                }
            }
        }
        if (!api_key && *entry_key) api_key = rp_str_dup_static(entry_key);
        out = json_object();
        rp_runtime_into(out, NULL, rp_sv(rp_arg(req, "detect_mode")), base_url,
                        api_key ? api_key : "", "direct-alias",
                        rp_sv(rp_get(req, "requested_provider")));
        free(api_key);
        free(entry_key);
    }
    free(raw_base);
    free(base_url);
    free(explicit_key);
    return out;
}

static int rp_is_llamacpp_alias(const char *norm) {
    return !strcmp(norm, "llamacpp") || !strcmp(norm, "llama.cpp") || !strcmp(norm, "llama-cpp");
}

static json_t *p2_resolve_named_custom_runtime(const json_t *req) {
    char *requested = rp_or_str(rp_get(req, "requested_provider"));
    char *requested_norm = rp_strip(requested);
    const json_t *explicit_base_j = rp_get(req, "explicit_base_url");
    char *explicit_base = rp_or_str(explicit_base_j);
    char *explicit_key = rp_clean(rp_get(req, "explicit_api_key"));
    json_t *out = json_null();
    json_t *custom_provider = NULL;

    rp_lower(requested_norm);
    if (rp_is_llamacpp_alias(requested_norm) && !*explicit_base) {
        const json_t *named = rp_get(req, "custom_provider");
        if (rp_truthy(named)) custom_provider = json_copy(named);
        else {
            const json_t *endpoint = rp_arg(req, "managed_endpoint");
            if (rp_truthy(endpoint)) {
                char *ep_key = rp_or_str(rp_get(endpoint, "api_key"));
                char *ep_base = rp_or_str(rp_get(endpoint, "base_url"));
                const char *key = *explicit_key ? explicit_key : (*ep_key ? ep_key : "no-key-required");
                json_t *rt = json_object();
                rp_runtime_into(rt, "chat_completions", NULL, ep_base, key, "local-runtime", requested);
                out = rt;
                free(ep_key);
                free(ep_base);
            } else {
                json_t *err = json_object();
                json_set(err, "error", rp_jstr("local_runtime_unavailable"));
                json_set(err, "retryable",
                         rp_jbool(rp_truthy(rp_get(rp_get(rp_get(req, "config"), "local_runtime"), "enabled"))));
                out = err;
            }
            goto done;
        }
    }
    if (*requested_norm && strcmp(requested_norm, "custom") && rp_truthy(rp_arg(req, "resolves_to_custom"))) {
        free(requested_norm);
        requested_norm = rp_str_dup_static("custom");
    }
    if (!strcmp(requested_norm, "custom") && *explicit_base) {
        json_t *direct = p2_resolve_direct_alias_runtime(req);
        out = direct;
        goto done;
    }
    if (!custom_provider) {
        json_t *sel_req = json_object();
        json_t *sel;
        json_set(sel_req, "requested_provider", rp_jstr(requested));
        json_set(sel_req, "config", rp_get(req, "config") ? json_copy(rp_get(req, "config")) : json_null());
        json_set(sel_req, "compatible", rp_arg(req, "compatible") ? json_copy(rp_arg(req, "compatible")) : json_null());
        json_set(sel_req, "secrets", rp_arg(req, "secrets") ? json_copy(rp_arg(req, "secrets")) : json_object());
        json_set(sel_req, "canonical", rp_arg(req, "canonical") ? json_copy(rp_arg(req, "canonical")) : json_null());
        json_set(sel_req, "auth_error", rp_arg(req, "auth_error") ? json_copy(rp_arg(req, "auth_error")) : json_null());
        json_set(sel_req, "registered", rp_arg(req, "registered") ? json_copy(rp_arg(req, "registered")) : json_null());
        json_set(sel_req, "normalized", rp_arg(req, "normalized") ? json_copy(rp_arg(req, "normalized")) : json_null());
        sel = rp_selected(sel_req);
        json_free(sel_req);
        if (sel && sel->type == JSON_OBJECT) custom_provider = json_copy(rp_get(sel, "entry"));
        json_free(sel);
    }
    if (!rp_truthy(custom_provider)) goto done;
    {
        char *cp_base = rp_or_str(rp_get(custom_provider, "base_url"));
        char *base_url;
        size_t n;
        if (*explicit_base) base_url = rp_str_dup_static(explicit_base);
        else base_url = rp_str_dup_static(cp_base);
        n = strlen(base_url);
        while (n && base_url[n - 1] == '/') base_url[--n] = '\0';
        free(cp_base);
        if (!*base_url) {
            free(base_url);
            goto done;
        }
        {
            const json_t *pool = rp_arg(req, "pool");
            int pool_hit = 0;
            json_t *probe = json_string(explicit_key);
            int usable = rp_usable_secret(probe);
            json_free(probe);
            if (!usable && rp_truthy(pool)) pool_hit = 1;
            if (pool_hit) {
                out = json_copy(pool);
                rp_apply_extras(custom_provider, rp_get(req, "target_model"), out);
                free(base_url);
                goto done;
            }
        }
        {
            char *api_key = NULL;
            char *cp_name = rp_or_str(rp_get(custom_provider, "name"));
            char *label = *cp_name ? rp_str_dup_static(cp_name) : rp_str_dup_static(requested);
            json_t *probe = json_string(explicit_key);

            if (rp_usable_secret(probe)) api_key = rp_str_dup_static(explicit_key);
            json_free(probe);
            if (!api_key) {
                json_t *cred = rp_entry_credential(custom_provider, label, req,
                                                   rp_truthy(rp_arg(req, "key_cmd_buildable")));
                if (cred && cred->type == JSON_STRING && *(cred->str_val ? cred->str_val : ""))
                    api_key = rp_str_dup_static(cred->str_val);
                json_free(cred);
            }
            if (!api_key && !rp_declares_credential(custom_provider)) {
                const json_t *gated = rp_arg(req, "host_gated");
                if (gated && gated->type == JSON_ARRAY) {
                    size_t i, gn = json_len(gated);
                    for (i = 0; i < gn && !api_key; i++) {
                        const json_t *item = json_get(gated, i);
                        json_t *pr;
                        if (!item || item->type != JSON_STRING) continue;
                        pr = json_copy(item);
                        if (rp_usable_secret(pr)) api_key = rp_str_dup_static(item->str_val);
                        json_free(pr);
                    }
                }
            }
            {
                char *cp_mode = rp_or_str(rp_get(custom_provider, "api_mode"));
                char *source = NULL;
                rp_sb sb;
                json_t *rt = json_object();
                sb_init(&sb);
                sb_add(&sb, "custom_provider:");
                sb_add(&sb, *cp_name ? cp_name : requested);
                source = sb_take(&sb);
                rp_runtime_into(rt, cp_mode, rp_sv(rp_arg(req, "detect_mode")), base_url,
                                api_key ? api_key : "", source, requested);
                rp_apply_extras(custom_provider, rp_get(req, "target_model"), rt);
                out = rt;
                free(source);
                free(cp_mode);
            }
            free(api_key);
            free(label);
            free(cp_name);
        }
        /* OpenCode-family re-derivation, when the entry pins no api_mode. */
        {
            const json_t *family = rp_arg(req, "family");
            const json_t *cp_mode = rp_get(custom_provider, "api_mode");
            if (rp_truthy(family) && !rp_truthy(cp_mode) && out && out->type == JSON_OBJECT) {
                const json_t *target_model = rp_get(req, "target_model");
                const json_t *cp_model = rp_get(custom_provider, "model");
                const json_t *mcfg = rp_arg(req, "model_cfg");
                char *effective = NULL;
                if (rp_truthy(target_model)) effective = rp_str_of(target_model);
                else if (rp_truthy(cp_model)) effective = rp_str_of(cp_model);
                else {
                    char *def = rp_or_str(rp_get(mcfg, "default"));
                    effective = rp_strip(def);
                    free(def);
                }
                {
                    char *trimmed = rp_strip(effective);
                    free(effective);
                    effective = trimmed;
                }
                if (*effective) {
                    const json_t *mode = rp_arg(req, "opencode_api_mode");
                    if (mode) rp_set_str(out, "api_mode", rp_sv(mode));
                }
                {
                    const json_t *norm_base = rp_arg(req, "opencode_base_url");
                    if (norm_base) rp_set_str(out, "base_url", rp_sv(norm_base));
                }
                free(effective);
            }
        }
        free(base_url);
    }
done:
    json_free(custom_provider);
    free(requested);
    free(requested_norm);
    free(explicit_base);
    free(explicit_key);
    return out;
}

/* ─────────────────────────────── dispatchers ─────────────────────────────── */

typedef json_t *(*rp_handler)(const json_t *req);

typedef struct {
    const char *name;
    rp_handler fn;
} rp_entry;

static const rp_entry P1_TABLE[] = {
    {"normalize_custom_provider_name", p1_normalize_custom_provider_name},
    {"normalize_base_url_for_match", p1_normalize_base_url_for_match},
    {"clean", p1_clean},
    {"custom_provider_slug", p1_custom_provider_slug},
    {"custom_provider_aliases", p1_custom_provider_aliases},
    {"normalize_route_base_url", p1_normalize_route_base_url},
    {"base_url_hostname", p1_base_url_hostname},
    {"route_key", p1_route_key},
    {"entry_url", p1_entry_url},
    {"filter_capabilities", p1_filter_capabilities},
    {"is_provider_enabled", p1_is_provider_enabled},
    {"normalize_extra_headers", p1_normalize_extra_headers},
    {"canonical_api_mode", p1_canonical_api_mode},
    {"parse_api_mode", p1_parse_api_mode},
    {"shadowed_by_builtin", p1_shadowed_by_builtin},
    {"builtin_named_entries", p1_builtin_named_entries},
    {"builtin_name_user_entry", p1_builtin_name_user_entry},
    {"persisted_provider_id", p1_persisted_provider_id},
    {"key_env_secret", p1_key_env_secret},
    {"model_cfg_key_env_for", p1_model_cfg_key_env_for},
    {"lift_model_capabilities", p1_lift_model_capabilities},
    {"lift_common_custom_fields", p1_lift_common_custom_fields},
    {"match_new_style_provider", p1_match_new_style_provider},
    {"match_legacy_custom_provider", p1_match_legacy_custom_provider},
    {"compatible_entry_path", p1_compatible_entry_path},
    {"select_named_custom_provider", p1_select_named_custom_provider},
    {"named_custom_provider_config_path", p1_named_custom_provider_config_path},
    {"custom_provider_config_path_for_url", p1_custom_provider_config_path_for_url},
    {"codex_model_provider_id", p1_codex_model_provider_id},
    {"has_named_custom_provider", p1_has_named_custom_provider},
};

static const rp_entry P2_TABLE[] = {
    {"has_usable_secret", p2_has_usable_secret},
    {"entry_declares_credential", p2_entry_declares_credential},
    {"model_id_matches", p2_model_id_matches},
    {"entry_serves_model", p2_entry_serves_model},
    {"find_custom_identity", p2_find_custom_identity},
    {"find_custom_provider_identity", p2_find_custom_provider_identity},
    {"find_custom_provider_identity_by_model", p2_find_custom_provider_identity_by_model},
    {"routed_custom_identity", p2_routed_custom_identity},
    {"empty_provider_identity", p2_empty_provider_identity},
    {"endpoint_identity", p2_endpoint_identity},
    {"session_provider_identity", p2_session_provider_identity},
    {"canonical_custom_identity", p2_canonical_custom_identity},
    {"is_routable_provider", p2_is_routable_provider},
    {"custom_provider_request_overrides", p2_custom_provider_request_overrides},
    {"apply_custom_provider_extras", p2_apply_custom_provider_extras},
    {"expand_direct_api_alias", p2_expand_direct_api_alias},
    {"opencode_family_for_custom", p2_opencode_family_for_custom},
    {"entry_credential", p2_entry_credential},
    {"owning_entry_credential", p2_owning_entry_credential},
    {"owned_config_url_credential", p2_owned_config_url_credential},
    {"resolve_direct_alias_runtime", p2_resolve_direct_alias_runtime},
    {"resolve_named_custom_runtime", p2_resolve_named_custom_runtime},
};

static json_t *rp_dispatch(const rp_entry *table, size_t count, const json_t *req, int *known) {
    const char *func;
    size_t i;
    *known = 0;
    if (!req || req->type != JSON_OBJECT) return json_null();
    func = json_get_str(req, "func", "");
    for (i = 0; i < count; i++) {
        if (!strcmp(table[i].name, func)) {
            *known = 1;
            return table[i].fn(req);
        }
    }
    return json_null();
}

json_t *w30_runtimepro_33ebc6_p1_dispatch(const json_t *req, int *known) {
    return rp_dispatch(P1_TABLE, sizeof P1_TABLE / sizeof P1_TABLE[0], req, known);
}

json_t *w30_runtimepro_33ebc6_p2_dispatch(const json_t *req, int *known) {
    return rp_dispatch(P2_TABLE, sizeof P2_TABLE / sizeof P2_TABLE[0], req, known);
}

size_t w30_runtimepro_33ebc6_p1_count(void) {
    return sizeof P1_TABLE / sizeof P1_TABLE[0];
}

size_t w30_runtimepro_33ebc6_p2_count(void) {
    return sizeof P2_TABLE / sizeof P2_TABLE[0];
}
