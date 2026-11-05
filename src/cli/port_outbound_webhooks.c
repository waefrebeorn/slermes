/*
 * port_outbound_webhooks.c — pure helpers from agent/outbound_webhooks.py.
 */

#include "port_outbound_webhooks.h"
#include "hermes_json.h"
#include "../lib/libcrypto/crypto.h"

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
