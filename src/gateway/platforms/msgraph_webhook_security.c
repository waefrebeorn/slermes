/*
 * msgraph_webhook_security.c — C11 port of gateway/platforms/msgraph_webhook.py
 * (security + data-path helpers): source-IP allowlisting (CIDR), constant-time
 * clientState verification, resource accept filtering, {key} template rendering,
 * seen-receipt idempotency, and the validation/notification handlers.
 *
 * Self-contained: depends only on libcrypto (constant-time compare), libjson,
 * and POSIX inet (no third-party net libs). The full HTTP daemon lives in
 * msgraph_webhook.c and calls into these helpers.
 */

#include "gateway/msgraph_webhook_security.h"
#include "libcrypto/crypto.h"
#include "hermes_json.h"

/* Owned by src/cli/port_msgraph_webhook_helpers.c (faithful port of the
 * standalone helper). Signature: (tenant, resource, event). */
extern char *msgraph_build_receipt_key(const char *tenant, const char *resource,
                                       const char *event);

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <arpa/inet.h>

/* PoP: msgraph_normalize_resource_value @ gateway/platforms/msgraph_webhook.py:_normalize_resource_value */
char *msgraph_normalize_resource_value(const char *resource) {
    if (!resource) resource = "";
    while (*resource && isspace((unsigned char)*resource)) resource++;
    size_t len = strlen(resource);
    while (len > 0 && isspace((unsigned char)resource[len - 1])) len--;
    char *tmp = (char *)malloc(len + 1);
    if (!tmp) return strdup("");
    memcpy(tmp, resource, len);
    tmp[len] = '\0';
    size_t s = 0, e = len;
    while (s < e && tmp[s] == '/') s++;
    while (e > s && tmp[e - 1] == '/') e--;
    memmove(tmp, tmp + s, e - s);
    tmp[e - s] = '\0';
    return tmp;
}

static bool parse_one_cidr(const char *chunk, msgraph_cidr_t *out) {
    char buf[256];
    size_t n = 0;
    while (*chunk && n < sizeof(buf) - 1 && *chunk != '/' && !isspace((unsigned char)*chunk))
        buf[n++] = *chunk++;
    buf[n] = '\0';
    int bits = -1;
    if (*chunk == '/') bits = atoi(chunk + 1);
    bool is_v6 = (strchr(buf, ':') != NULL);
    out->is_v6 = is_v6;
    memset(out->addr, 0, 16);
    memset(out->mask, 0, 16);
    if (is_v6) {
        if (inet_pton(AF_INET6, buf, out->addr) != 1) return false;
        if (bits < 0) bits = 128;
        out->bits = bits;
        for (int i = 0; i < bits; i++) {
            int byte = i / 8, bit = 7 - (i % 8);
            out->mask[byte] |= (1u << bit);
        }
    } else {
        unsigned char v4[4];
        if (inet_pton(AF_INET, buf, v4) != 1) return false;
        if (bits < 0) bits = 32;
        out->bits = bits;
        memcpy(out->addr, v4, 4);
        memset(out->mask, 0, 16);
        for (int i = 0; i < bits; i++) {
            int byte = i / 8, bit = 7 - (i % 8);
            out->mask[byte] |= (1u << bit);
        }
    }
    return true;
}

static int split_cidr_list(const char *raw, char out[][256], int max_out) {
    int n = 0;
    if (!raw) return 0;
    const char *p = raw;
    while (*p && n < max_out) {
        while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == '[' || *p == ']' || *p == '"' || *p == '\''))
            p++;
        if (!*p) break;
        int k = 0;
        while (*p && k < 255 && !isspace((unsigned char)*p) && *p != ',' && *p != ']' && *p != '"' && *p != '\'')
            out[n][k++] = *p++;
        out[n][k] = '\0';
        if (k > 0) n++;
    }
    return n;
}

/* PoP: msgraph_parse_allowed_source_cidrs @ gateway/platforms/msgraph_webhook.py:_parse_allowed_source_cidrs */
int msgraph_parse_allowed_source_cidrs(const char *raw,
                                       msgraph_cidr_t *out, int max_out) {
    if (!raw || !out || max_out <= 0) return 0;
    char chunks[256][256];
    int nc = split_cidr_list(raw, chunks, 256);
    int count = 0;
    for (int i = 0; i < nc && count < max_out; i++) {
        if (!chunks[i][0]) continue;
        if (parse_one_cidr(chunks[i], &out[count]))
            count++;
    }
    return count;
}

static bool pattern_matches(const char *norm_resource, const char *pattern) {
    size_t plen = strlen(pattern);
    if (plen == 0) return false;
    if (pattern[plen - 1] == '*') {
        char prefix[1024];
        size_t pfx = plen - 1;
        while (pfx > 0 && pattern[pfx - 1] == '/') pfx--;
        if (pfx >= sizeof(prefix)) pfx = sizeof(prefix) - 1;
        memcpy(prefix, pattern, pfx);
        prefix[pfx] = '\0';
        if (strcmp(norm_resource, prefix) == 0) return true;
        size_t nlen = strlen(norm_resource);
        if (nlen >= pfx + 1 && strncmp(norm_resource, prefix, pfx) == 0 &&
            norm_resource[pfx] == '/')
            return true;
        return false;
    }
    if (strcmp(norm_resource, pattern) == 0) return true;
    size_t nlen = strlen(norm_resource);
    size_t pl = strlen(pattern);
    if (nlen >= pl + 1 && strncmp(norm_resource, pattern, pl) == 0 &&
        norm_resource[pl] == '/')
        return true;
    return false;
}

/* PoP: msgraph_resource_accepted @ gateway/platforms/msgraph_webhook.py:_resource_accepted */
bool msgraph_resource_accepted(const char *resource,
                               const char *accepted_patterns) {
    if (!accepted_patterns || !*accepted_patterns) return true;
    char *nr = msgraph_normalize_resource_value(resource);
    char patterns[256][1024];
    int np = 0;
    const char *p = accepted_patterns;
    while (*p && np < 256) {
        while (*p && (*p == '\n' || *p == ',' || isspace((unsigned char)*p))) p++;
        if (!*p) break;
        int k = 0;
        while (*p && k < 1023 && *p != '\n' && *p != ',' && !isspace((unsigned char)*p))
            patterns[np][k++] = *p++;
        patterns[np][k] = '\0';
        if (k > 0) np++;
    }
    bool ok = false;
    for (int i = 0; i < np; i++) {
        char *np_norm = msgraph_normalize_resource_value(patterns[i]);
        if (np_norm && *np_norm && pattern_matches(nr, np_norm)) ok = true;
        free(np_norm);
        if (ok) break;
    }
    free(nr);
    return ok;
}

/* PoP: msgraph_verify_client_state @ gateway/platforms/msgraph_webhook.py:_verify_client_state */
bool msgraph_verify_client_state(const char *provided, const char *expected) {
    if (!expected) return false;
    if (!provided) return false;
    size_t lp = strlen(provided), le = strlen(expected);
    if (lp != le) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < lp; i++)
        diff |= (unsigned char)provided[i] ^ (unsigned char)expected[i];
    return diff == 0;
}

static char *resolve_key(const char *key, const char *payload_json) {
    char *jerr = NULL;
    json_t *p = json_parse(payload_json, &jerr);
    if (jerr) { free(jerr); return NULL; }
    if (!p || p->type != JSON_OBJECT) { if (p) json_free(p); return NULL; }
    json_t *cur = p;
    char keybuf[256];
    size_t ki = 0;
    const char *s = key;
    char *out = NULL;
    while (*s) {
        ki = 0;
        while (*s && *s != '.' && ki < 255) keybuf[ki++] = *s++;
        keybuf[ki] = '\0';
        if (cur && cur->type == JSON_OBJECT) {
            cur = json_obj_get(cur, keybuf);
        } else { cur = NULL; break; }
        if (*s == '.') s++;
    }
    if (cur && cur->type == JSON_STRING) {
        out = strdup(cur->str_val ? cur->str_val : "");
    } else if (cur && (cur->type == JSON_OBJECT || cur->type == JSON_ARRAY)) {
        out = json_serialize(cur);
        if (out && strlen(out) > 2000) out[2000] = '\0';
    } else if (cur && cur->type == JSON_NUMBER) {
        char b[64]; snprintf(b, sizeof(b), "%.0f", cur->num_val);
        out = strdup(b);
    } else if (cur && cur->type == JSON_BOOL) {
        out = strdup(cur->bool_val ? "true" : "false");
    } else {
        out = NULL;
    }
    json_free(p);
    if (!out) { size_t l = strlen(key) + 3; out = malloc(l); snprintf(out, l, "{%s}", key); }
    return out;
}

/* PoP: msgraph_render_template @ gateway/platforms/msgraph_webhook.py:_render_template */
char *msgraph_render_template(const char *template_str, const char *payload_json) {
    if (!template_str) return strdup("");
    if (!payload_json) payload_json = "{}";
    size_t cap = strlen(template_str) + 256;
    char *out = (char *)malloc(cap);
    if (!out) return strdup("");
    size_t o = 0;
    const char *p = template_str;
    while (*p) {
        if (*p == '{') {
            const char *q = p + 1;
            while (*q && *q != '}' && *q != '{') q++;
            if (*q == '}') {
                size_t klen = (size_t)(q - p - 1);
                char *key = (char *)malloc(klen + 1);
                memcpy(key, p + 1, klen);
                key[klen] = '\0';
                char *val = resolve_key(key, payload_json);
                free(key);
                if (val) {
                    size_t vl = strlen(val);
                    if (o + vl + 1 > cap) { cap = o + vl + 256; out = realloc(out, cap); }
                    memcpy(out + o, val, vl);
                    o += vl;
                    free(val);
                }
                p = q + 1;
                continue;
            }
        }
        if (o + 1 > cap) { cap = o + 256; out = realloc(out, cap); }
        out[o++] = *p++;
    }
    out[o] = '\0';
    return out;
}

/* PoP: msgraph_render_prompt @ gateway/platforms/msgraph_webhook.py:_render_prompt */
char *msgraph_render_prompt(const char *notification_json, const char *prompt_template) {
    if (!notification_json) notification_json = "{}";
    if (prompt_template && *prompt_template) {
        char *payload = (char *)malloc(strlen(notification_json) + 256);
        const char *res = "", *ct = "", *sid = "";
        char *jerr = NULL;
        json_t *n = json_parse(notification_json, &jerr);
        if (jerr) free(jerr);
        if (n && n->type == JSON_OBJECT) {
            res = json_get_str(n, "resource", "");
            ct  = json_get_str(n, "changeType", "");
            sid = json_get_str(n, "subscriptionId", "");
        }
        snprintf(payload, strlen(notification_json) + 256,
                 "{\"notification\":%s,\"resource\":%s,\"change_type\":%s,\"subscription_id\":%s}",
                 notification_json, res, ct, sid);
        char *rendered = msgraph_render_template(prompt_template, payload);
        if (n) json_free(n);
        free(payload);
        return rendered;
    }
    char *jerr = NULL;
    json_t *n = json_parse(notification_json, &jerr);
    if (jerr) free(jerr);
    char *dump = n ? json_serialize(n) : strdup(notification_json);
    if (n) json_free(n);
    if (!dump) dump = strdup("");
    size_t len = strlen(dump);
    if (len > 4000) dump[4000] = '\0';
    char *out = (char *)malloc(strlen(dump) + 64);
    snprintf(out, strlen(dump) + 64,
             "Microsoft Graph change notification:\n\n```json\n%s\n```", dump);
    free(dump);
    return out;
}

static bool is_network_accessible(const char *host) {
    if (!host) return true;
    if (strcmp(host, "127.0.0.1") == 0) return false;
    if (strcmp(host, "::1") == 0) return false;
    if (strcmp(host, "localhost") == 0) return false;
    return true;
}

/* PoP: msgraph_source_allowlist_required_but_missing @ gateway/platforms/msgraph_webhook.py:_source_allowlist_required_but_missing */
bool msgraph_source_allowlist_required_but_missing(const char *host,
                                                   const msgraph_cidr_t *cidrs,
                                                   int n_cidrs) {
    return is_network_accessible(host) && (cidrs == NULL || n_cidrs == 0);
}

static bool ip_in_cidr(const char *peer_ip, const msgraph_cidr_t *cidrs, int n_cidrs) {
    unsigned char addr[16];
    bool is_v6 = (strchr(peer_ip, ':') != NULL);
    if (is_v6) {
        if (inet_pton(AF_INET6, peer_ip, addr) != 1) return false;
    } else {
        unsigned char v4[4];
        if (inet_pton(AF_INET, peer_ip, v4) != 1) return false;
        memcpy(addr, v4, 4);
    }
    for (int i = 0; i < n_cidrs; i++) {
        const msgraph_cidr_t *c = &cidrs[i];
        if (c->is_v6 != is_v6) continue;
        int bytes = c->is_v6 ? 16 : 4;
        bool match = true;
        for (int b = 0; b < bytes; b++) {
            if ((addr[b] & c->mask[b]) != (c->addr[b] & c->mask[b])) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

/* PoP: msgraph_source_ip_allowed @ gateway/platforms/msgraph_webhook.py:_source_ip_allowed */
bool msgraph_source_ip_allowed(const char *peer_ip,
                               const char *host,
                               const msgraph_cidr_t *cidrs, int n_cidrs) {
    if (msgraph_source_allowlist_required_but_missing(host, cidrs, n_cidrs))
        return false;
    if (cidrs == NULL || n_cidrs == 0)
        return true;
    if (!peer_ip || !*peer_ip)
        return false;
    return ip_in_cidr(peer_ip, cidrs, n_cidrs);
}

struct msgraph_webhook_adapter {
    char *host;
    char *client_state;
    msgraph_cidr_t cidrs[64];
    int n_cidrs;
    char *accepted_resources;
    char *prompt_template;
    msgraph_notification_scheduler_fn scheduler;
    void *scheduler_user;
    char **seen_receipts;
    int  seen_n;
    int  seen_cap;
    int  max_seen_receipts;
    int  accepted_count;
    int  duplicate_count;
};

/* PoP: msgraph_webhook_create @ gateway/platforms/msgraph_webhook.py:__init__ */
msgraph_webhook_adapter_t *msgraph_webhook_create(const msgraph_webhook_config_t *cfg) {
    msgraph_webhook_adapter_t *a = (msgraph_webhook_adapter_t *)calloc(1, sizeof(*a));
    if (!a) return NULL;
    a->host = cfg->host ? strdup(cfg->host) : strdup("0.0.0.0");
    a->client_state = cfg->client_state ? strdup(cfg->client_state) : NULL;
    a->accepted_resources = cfg->accepted_resources ? strdup(cfg->accepted_resources) : strdup("");
    a->prompt_template = cfg->prompt_template ? strdup(cfg->prompt_template) : NULL;
    a->max_seen_receipts = cfg->max_seen_receipts > 0 ? cfg->max_seen_receipts : 5000;
    a->n_cidrs = msgraph_parse_allowed_source_cidrs(cfg->allowed_source_cidrs, a->cidrs, 64);
    a->seen_cap = 64;
    a->seen_receipts = (char **)calloc((size_t)a->seen_cap, sizeof(char *));
    if (!a->seen_receipts) { msgraph_webhook_destroy(a); return NULL; }
    return a;
}

void msgraph_webhook_destroy(msgraph_webhook_adapter_t *a) {
    if (!a) return;
    free(a->host);
    free(a->client_state);
    free(a->accepted_resources);
    free(a->prompt_template);
    for (int i = 0; i < a->seen_n; i++) free(a->seen_receipts[i]);
    free(a->seen_receipts);
    free(a);
}

/* PoP: set_notification_scheduler @ gateway/platforms/msgraph_webhook.py:set_notification_scheduler */
void msgraph_webhook_set_scheduler(msgraph_webhook_adapter_t *a,
                                   msgraph_notification_scheduler_fn fn, void *user) {
    if (!a) return;
    a->scheduler = fn;
    a->scheduler_user = user;
}

/* PoP: msgraph_has_seen_receipt @ gateway/platforms/msgraph_webhook.py:_has_seen_receipt */
bool msgraph_has_seen_receipt(msgraph_webhook_adapter_t *a, const char *receipt_key) {
    if (!a || !receipt_key) return false;
    for (int i = 0; i < a->seen_n; i++)
        if (strcmp(a->seen_receipts[i], receipt_key) == 0) return true;
    return false;
}

/* PoP: msgraph_remember_receipt @ gateway/platforms/msgraph_webhook.py:_remember_receipt */
void msgraph_remember_receipt(msgraph_webhook_adapter_t *a, const char *receipt_key) {
    if (!a || !receipt_key) return;
    if (a->seen_n >= a->seen_cap) {
        int nc = a->seen_cap * 2;
        char **ns = (char **)realloc(a->seen_receipts, (size_t)nc * sizeof(char *));
        if (!ns) return;
        a->seen_receipts = ns;
        a->seen_cap = nc;
    }
    if (a->seen_n >= a->max_seen_receipts && a->seen_n > 0) {
        free(a->seen_receipts[0]);
        memmove(&a->seen_receipts[0], &a->seen_receipts[1],
                (size_t)(a->seen_n - 1) * sizeof(char *));
        a->seen_n--;
    }
    a->seen_receipts[a->seen_n++] = strdup(receipt_key);
}

/* PoP: msgraph_build_message_event @ gateway/platforms/msgraph_webhook.py:_build_message_event */
char *msgraph_build_message_event(const char *notification_json, const char *receipt_key) {
    char *msg_id;
    if (receipt_key && *receipt_key) {
        msg_id = strdup(receipt_key);
    } else {
        char *jerr = NULL;
        json_t *n = json_parse(notification_json ? notification_json : "{}", &jerr);
        if (jerr) free(jerr);
        char *dump = n ? json_serialize(n) : strdup("{}");
        if (n) json_free(n);
        unsigned char h[20];
        crypto_hmac_sha1((const unsigned char*)"", 0,
                         (const unsigned char*)(dump ? dump : ""),
                         dump ? strlen(dump) : 0, h);
        char hex[41];
        for (int i = 0; i < 20; i++) snprintf(hex + i*2, 3, "%02x", h[i]);
        hex[40] = '\0';
        size_t ml = strlen(hex) + 6;
        msg_id = malloc(ml);
        snprintf(msg_id, ml, "sha1:%s", hex);
        free(dump);
    }
    char *prompt = msgraph_render_prompt(notification_json, NULL);
    size_t el = (prompt ? strlen(prompt) : 0) + 200;
    char *ev = malloc(el);
    snprintf(ev, el,
             "{\"text\":%s,\"message_type\":\"text\",\"source\":{\"chat_id\":\"msgraph:unknown\",\"chat_name\":\"msgraph/webhook\",\"chat_type\":\"webhook\",\"user_id\":\"msgraph\",\"user_name\":\"Microsoft Graph\"},\"message_id\":\"%s\",\"internal\":true}",
             prompt ? prompt : "", msg_id);
    free(prompt);
    free(msg_id);
    return ev;
}

/* PoP: msgraph_schedule_notification @ gateway/platforms/msgraph_webhook.py:_schedule_notification */
void msgraph_schedule_notification(msgraph_webhook_adapter_t *a,
                                   const char *notification_json,
                                   const char *event_json) {
    if (!a) return;
    if (a->scheduler) {
        a->scheduler(notification_json ? notification_json : "{}",
                     event_json ? event_json : "{}", a->scheduler_user);
    }
}

/* PoP: msgraph_handle_validation @ gateway/platforms/msgraph_webhook.py:_handle_validation */
int msgraph_handle_validation(const char *validation_token, char **out_body) {
    if (out_body) *out_body = NULL;
    if (!validation_token || !*validation_token) return 400;
    if (out_body) *out_body = strdup(validation_token);
    return 200;
}

/* PoP: msgraph_handle_notification @ gateway/platforms/msgraph_webhook.py:_handle_notification */
int msgraph_handle_notification(msgraph_webhook_adapter_t *a,
                                const char *body_json,
                                size_t body_len,
                                int *out_scheduled) {
    if (out_scheduled) *out_scheduled = 0;
    if (!a) return 400;
    if (!body_json) return 400;
    if (body_len > 1048576UL) return 413;

    char *jerr = NULL;
    json_t *body = json_parse(body_json, &jerr);
    if (jerr) { free(jerr); return 400; }
    if (!body || body->type != JSON_OBJECT) { if (body) json_free(body); return 400; }
    json_t *value = json_obj_get(body, "value");
    if (!value || value->type != JSON_ARRAY) { json_free(body); return 400; }

    int accepted = 0, duplicates = 0, auth_rejected = 0, other_rejected = 0;
    int n = (int)json_len(value);
    for (int i = 0; i < n; i++) {
        json_t *raw = json_get(value, (size_t)i);
        if (!raw || raw->type != JSON_OBJECT) { other_rejected++; continue; }
        char *notif_json = json_serialize(raw);
        if (!notif_json) { other_rejected++; continue; }

        const char *resource = json_get_str(raw, "resource", "");
        if (!msgraph_resource_accepted(resource, a->accepted_resources)) {
            other_rejected++; free(notif_json); continue;
        }
        const char *client_state = json_get_str(raw, "clientState", "");
        if (!msgraph_verify_client_state(client_state, a->client_state)) {
            auth_rejected++; free(notif_json); continue;
        }
        char *receipt_key = msgraph_build_receipt_key(notif_json, "", "");
        if (receipt_key) {
            if (msgraph_has_seen_receipt(a, receipt_key)) {
                duplicates++; free(receipt_key); free(notif_json); continue;
            }
            msgraph_remember_receipt(a, receipt_key);
            free(receipt_key);
        }
        char *prompt = msgraph_render_prompt(notif_json, a->prompt_template);
        char *event_json = NULL;
        if (prompt) {
            size_t el = strlen(prompt) + 128;
            event_json = (char *)malloc(el);
            snprintf(event_json, el, "{\"text\":%s,\"internal\":true}", prompt);
            free(prompt);
        }
        if (a->scheduler) {
            a->scheduler(notif_json, event_json ? event_json : "{}", a->scheduler_user);
        }
        free(event_json);
        free(notif_json);
        accepted++;
        a->accepted_count++;
    }
    a->duplicate_count += duplicates;
    json_free(body);

    if (out_scheduled) *out_scheduled = accepted;
    if (accepted || duplicates) return 202;
    if (auth_rejected && !other_rejected) return 403;
    return 400;
}
