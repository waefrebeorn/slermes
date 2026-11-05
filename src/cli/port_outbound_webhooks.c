/*
 * port_outbound_webhooks.c — pure helpers from agent/outbound_webhooks.py.
 */

#include "port_outbound_webhooks.h"
#include "hermes_json.h"
#include "../lib/libcrypto/crypto.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *ow_clone(const json_t *n)
{
    char *ser = json_serialize(n);
    json_t *copy;
    if (!ser) return NULL;
    copy = json_parse(ser, NULL);
    free(ser);
    return copy;
}

char *ow_serialize_payload(const char *event, const char *profile,
                           const char *delivery_id, const char *timestamp,
                           const char *extra_json)
{
    json_t *payload = json_object();
    json_t *extra = NULL;
    char *out;

    json_set(payload, "hook_event_name", json_string(event ? event : ""));
    json_set(payload, "profile", json_string(profile ? profile : ""));

    if (extra_json && extra_json[0]) {
        extra = json_parse(extra_json, NULL);
        if (extra && extra->type == JSON_OBJECT) {
            size_t i;
            for (i = 0; i < extra->c.count; i++) {
                if (extra->c.keys && extra->c.items)
                    json_set(payload, extra->c.keys[i],
                             ow_clone(extra->c.items[i]));
            }
        }
    }

    json_set(payload, "delivery_id", json_string(delivery_id ? delivery_id : ""));
    json_set(payload, "timestamp", json_string(timestamp ? timestamp : ""));

    out = json_serialize(payload);
    if (extra) json_free(extra);
    json_free(payload);
    return out;
}

static char *ow_hex(const unsigned char d[CRYPTO_SHA256_LEN])
{
    static const char *const H = "0123456789abcdef";
    char *buf = malloc(2 * CRYPTO_SHA256_LEN + 1);
    int k;
    if (!buf) return NULL;
    for (k = 0; k < CRYPTO_SHA256_LEN; k++) {
        buf[2 * k] = H[(d[k] >> 4) & 0xf];
        buf[2 * k + 1] = H[d[k] & 0xf];
    }
    buf[2 * CRYPTO_SHA256_LEN] = '\0';
    return buf;
}

char *ow_build_delivery_json(const char *event, const char *url,
                             const char *label, const char *secret, int timeout,
                             const char *body, const char *delivery_id)
{
    json_t *d = json_object();
    json_t *headers = json_object();
    const char *e = event ? event : "";
    const char *l = label ? label : "";
    char *out;

    json_set(headers, "Content-Type", json_string("application/json"));
    json_set(headers, "User-Agent",
             json_string("Hermes-Agent-Outbound-Webhook"));
    json_set(headers, "X-Hermes-Event", json_string(e));
    json_set(headers, "X-Hermes-Delivery",
             json_string(delivery_id ? delivery_id : ""));

    if (secret && secret[0]) {
        unsigned char digest[CRYPTO_SHA256_LEN];
        const char *b = body ? body : "";
        char sig[512];
        crypto_hmac_sha256((const unsigned char *)secret, strlen(secret),
                           (const unsigned char *)b, strlen(b), digest);
        snprintf(sig, sizeof(sig), "sha256=%s", ow_hex(digest));
        json_set(headers, "X-Hermes-Signature-256", json_string(sig));
    }

    json_set(d, "url", json_string(url ? url : ""));
    json_set(d, "label", json_string(l));
    json_set(d, "event", json_string(e));
    json_set(d, "body", json_string(body ? body : ""));
    json_set(d, "headers", headers); /* steals headers node */
    json_set(d, "timeout", json_int(timeout));

    out = json_serialize(d);
    json_free(d);
    return out;
}

/* ---- _parse_single_target ---- */

static const int OW_MAX_TIMEOUT = 60;

static int ow_in_list(const json_t *arr, const char *s)
{
    size_t i;
    if (!arr || arr->type != JSON_ARRAY || !s) return 0;
    for (i = 0; i < arr->c.count; i++) {
        const json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && it->str_val &&
            strcmp(it->str_val, s) == 0)
            return 1;
    }
    return 0;
}

static char *ow_trimdup(const char *s)
{
    const char *b, *e;
    char *o;
    size_t n;
    if (!s) return NULL;
    b = s;
    while (*b && isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    o = malloc(n + 1);
    if (o) {
        memcpy(o, b, n);
        o[n] = '\0';
    }
    return o;
}

/* Python-style int(text): optional sign + digits, leading/trailing whitespace
 * tolerated, anything else invalid. */
static int ow_parse_int_str(const char *s, long *out)
{
    const char *p = s;
    long v = 0;
    int neg = 0, nd = 0;
    if (!s) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (*p < '0' || *p > '9') return 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); nd = 1; p++; }
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '\0') return 0;
    if (!nd) return 0;
    *out = neg ? -v : v;
    return 1;
}

/* int(timeout_raw): number truncates toward zero; bool -> 1; string parsed
 * strictly; null/object/array invalid. Returns 0 on failure. */
static int ow_int_value(const json_t *n, long *out)
{
    if (!n) return 0;
    switch (n->type) {
    case JSON_NUMBER: *out = (long)n->num_val; return 1;
    case JSON_BOOL: *out = n->bool_val ? 1 : 0; return 1;
    case JSON_STRING:
        return ow_parse_int_str(n->str_val ? n->str_val : "", out);
    default: return 0;
    }
}

char *ow_parse_single_target(const char *raw_json, const char *valid_hooks_json,
                             const char *tool_scoped_json, const char *env_secret,
                             int default_timeout)
{
    json_t *raw = raw_json ? json_parse(raw_json, NULL) : NULL;
    json_t *valid = valid_hooks_json ? json_parse(valid_hooks_json, NULL) : NULL;
    json_t *tsc = tool_scoped_json ? json_parse(tool_scoped_json, NULL) : NULL;
    char *out = NULL;
    json_t *target = NULL;
    const json_t *urln, *evr, *mat, *traw, *nam, *secenv, *sec;
    char *url = NULL, *name = NULL;
    const char *secret;
    json_t *events = NULL;
    long timeout;
    int has_timeout = 0, evcount = 0;

    if (!raw || raw->type != JSON_OBJECT) goto done;

    urln = json_obj_get(raw, "url");
    if (!urln || urln->type != JSON_STRING) goto done;
    url = ow_trimdup(urln->str_val ? urln->str_val : "");
    if (!url || url[0] == '\0') goto done;
    {
        /* strip && lowercase startswith http:// or https:// */
        char lo[1024]; size_t i = 0;
        for (i = 0; i + 1 < sizeof(lo) && url[i]; i++)
            lo[i] = (char)tolower((unsigned char)url[i]);
        lo[i] = '\0';
        if (strncmp(lo, "http://", 7) != 0 && strncmp(lo, "https://", 8) != 0)
            goto done;
    }

    evr = json_obj_get(raw, "events");
    if (!evr || evr->type != JSON_ARRAY || evr->c.count == 0) goto done;
    events = json_array();
    {
        size_t i;
        for (i = 0; i < evr->c.count; i++) {
            const json_t *it = evr->c.items[i];
            if (it && it->type == JSON_STRING && it->str_val &&
                ow_in_list(valid, it->str_val))
                json_array_append(events, json_string(it->str_val));
        }
        evcount = events->c.count;
    }
    if (evcount == 0) { json_free(events); events = NULL; goto done; }

    has_timeout = 0;
    traw = json_obj_get(raw, "timeout");
    if (traw) has_timeout = ow_int_value(traw, &timeout);
    if (!has_timeout) timeout = default_timeout;
    if (timeout < 1) timeout = 1;
    if (timeout > OW_MAX_TIMEOUT) timeout = OW_MAX_TIMEOUT;

    nam = json_obj_get(raw, "name");
    if (nam && nam->type == JSON_STRING)
        name = ow_trimdup(nam->str_val ? nam->str_val : "");
    if (!name) name = strdup("");

    /* matcher: non-str -> None; str but no tool-scoped event -> None */
    mat = json_obj_get(raw, "matcher");
    if (mat && mat->type != JSON_STRING) mat = NULL;
    if (mat) {
        size_t i; int scoped = 0;
        if (events)
            for (i = 0; i < (size_t)evcount; i++) {
                const json_t *it = events->c.items[i];
                if (it && it->str_val && ow_in_list(tsc, it->str_val)) { scoped = 1; break; }
            }
        if (!scoped) mat = NULL;
    }

    /* secret_env (preferred) else inline secret */
    secret = NULL;
    secenv = json_obj_get(raw, "secret_env");
    if (secenv && secenv->type == JSON_STRING) {
        char *se = ow_trimdup(secenv->str_val ? secenv->str_val : "");
        int nonempty = se && se[0];
        if (nonempty) {
            secret = (env_secret && env_secret[0]) ? env_secret : NULL;
        } else {
            sec = json_obj_get(raw, "secret");
            secret = (sec && sec->type == JSON_STRING && sec->str_val && sec->str_val[0])
                     ? sec->str_val : NULL;
        }
        free(se);
    } else {
        sec = json_obj_get(raw, "secret");
        secret = (sec && sec->type == JSON_STRING && sec->str_val && sec->str_val[0])
                 ? sec->str_val : NULL;
    }

    target = json_object();
    json_set(target, "url", json_string(url));
    json_set(target, "events", events); /* steals */
    events = NULL;
    json_set(target, "name", json_string(name));
    if (secret) json_set(target, "secret", json_string(secret));
    if (mat) {
        json_set(target, "matcher", json_string(mat->str_val ? mat->str_val : ""));
    }
    json_set(target, "timeout", json_int((int)timeout));
    out = json_serialize(target);

done:
    free(url);
    free(name);
    if (target) json_free(target);
    if (events) json_free(events);
    if (raw) json_free(raw);
    if (valid) json_free(valid);
    if (tsc) json_free(tsc);
    return out;
}
