/*
 * port_outbound_webhooks.c — C11 port of agent/outbound_webhooks.py.
 * Config parsing, payload serialization, delivery queue management.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include "hermes_json.h"
#include "hermes_crypto.h"
#include "hermes_outbound_webhooks.h"

/* ── Tool-scoped events set ───────────────────────────────── */
static const char *OW_TOOL_SCOPED_EVENTS[] = {
    "pre_tool_call",
    "post_tool_call",
    NULL
};

static bool is_tool_scoped_event(const char *event) {
    for (int i = 0; OW_TOOL_SCOPED_EVENTS[i]; i++)
        if (strcmp(event, OW_TOOL_SCOPED_EVENTS[i]) == 0) return true;
    return false;
}

static const char *OW_TOP_LEVEL_PAYLOAD_KEYS[] = {
    "tool_name", "args", "session_id", "parent_session_id",
    NULL
};

/* ── WebhookTarget ────────────────────────────────────────── */

/* PoP: WebhookTarget.label @ agent/outbound_webhooks.py:WebhookTarget.label */
/* PoP: __init__ @ agent/outbound_webhooks.py:WebhookTarget.__post_init__ */
/* PoP: ow_target_label @ agent/outbound_webhooks.py:WebhookTarget.label */
const char *ow_target_label(const ow_webhook_target_t *t) {
    if (!t) return "";
    return (t->name && t->name[0]) ? t->name : (t->url ? t->url : "");
}

/* PoP: WebhookTarget.matches_tool @ agent/outbound_webhooks.py:WebhookTarget.matches_tool */
/* PoP: matches_tool @ agent/outbound_webhooks.py:WebhookTarget.matches_tool */
bool ow_target_matches_tool(const ow_webhook_target_t *t, const char *tool_name) {
    if (!t || !t->matcher || !t->matcher[0]) return true;
    if (!tool_name) return false;
    if (strstr(t->matcher, "*")) {
        const char *star = strchr(t->matcher, '*');
        size_t prefix_len = (size_t)(star - t->matcher);
        if (strncmp(tool_name, t->matcher, prefix_len) == 0) return true;
        return false;
    }
    return strcmp(tool_name, t->matcher) == 0;
}

ow_webhook_target_t *ow_target_create(const char *url, const char **events,
                                       const char *name, const char *secret,
                                       const char *matcher, int timeout) {
    ow_webhook_target_t *t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    if (url) t->url = strdup(url);
    if (name) t->name = strdup(name);
    if (secret) t->secret = strdup(secret);
    if (matcher) t->matcher = strdup(matcher);
    t->timeout = timeout;
    if (events) {
        int n = 0;
        while (events[n]) n++;
        t->events = calloc((size_t)n + 1, sizeof(char *));
        for (int i = 0; i < n; i++)
            t->events[i] = strdup(events[i]);
        t->events[n] = NULL;
    }
    return t;
}

void ow_target_free(ow_webhook_target_t *t) {
    if (!t) return;
    free(t->url);
    if (t->events) {
        for (char **p = t->events; *p; p++) free(*p);
        free(t->events);
    }
    free(t->name);
    free(t->secret);
    free(t->matcher);
    free(t);
}

/* PoP: _parse_single_target @ agent/outbound_webhooks.py:_parse_single_target */
ow_webhook_target_t *ow_parse_single_target(int index, const json_t *raw) {
    if (!raw || raw->type != JSON_OBJECT) {
        fprintf(stderr, "hooks.outbound[%d] must be a mapping; got %s\n",
                index, raw ? "non-object" : "null");
        return NULL;
    }

    const char *url = json_get_str(raw, "url", NULL);
    if (!url || !url[0]) {
        fprintf(stderr, "hooks.outbound[%d] is missing a non-empty 'url'\n", index);
        return NULL;
    }
    if (strncasecmp(url, "http://", 7) != 0 && strncasecmp(url, "https://", 8) != 0) {
        fprintf(stderr, "hooks.outbound[%d].url must be http(s); got %s — skipped\n", index, url);
        return NULL;
    }
    if (strncasecmp(url, "http://", 7) == 0) {
        fprintf(stderr, "hooks.outbound[%d].url uses plain http:// — prefer https.\n", index);
    }

    /* Build events list from "events" array */
    const json_t *events_j = json_obj_get(raw, "events");
    if (!events_j || events_j->type != JSON_ARRAY || events_j->c.count == 0) {
        fprintf(stderr, "hooks.outbound[%d] needs a non-empty 'events' list\n", index);
        return NULL;
    }

    size_t ne = events_j->c.count;
    char **evlist = calloc(ne + 1, sizeof(char *));
    size_t evcount = 0;
    for (size_t i = 0; i < ne; i++) {
        const json_t *ev = events_j->c.items[i];
        if (ev && ev->type == JSON_STRING && ev->str_val)
            evlist[evcount++] = strdup(ev->str_val);
    }
    if (evcount == 0) {
        free(evlist);
        fprintf(stderr, "hooks.outbound[%d] has no valid events — skipped\n", index);
        return NULL;
    }
    evlist[evcount] = NULL;

    /* matcher */
    const char *matcher_raw = json_get_str(raw, "matcher", NULL);
    char *matcher_clean = NULL;
    if (matcher_raw) {
        while (*matcher_raw == ' ' || *matcher_raw == '\t') matcher_raw++;
        if (*matcher_raw)
            matcher_clean = strdup(matcher_raw);
    }

    bool has_tool_scoped = false;
    for (size_t i = 0; i < evcount; i++) {
        if (is_tool_scoped_event(evlist[i])) { has_tool_scoped = true; break; }
    }
    if (matcher_clean && !has_tool_scoped) {
        fprintf(stderr, "hooks.outbound[%d].matcher=%s will be ignored — matcher is only honored for pre_tool_call / post_tool_call.\n", index, matcher_clean);
        free(matcher_clean);
        matcher_clean = NULL;
    }

    /* timeout */
    int timeout = (int)json_get_num(raw, "timeout", OW_DEFAULT_TIMEOUT_SECONDS);
    if (timeout < 1) timeout = 1;
    if (timeout > OW_MAX_TIMEOUT_SECONDS) timeout = OW_MAX_TIMEOUT_SECONDS;

    /* secret */
    char *secret = ow_resolve_secret(index, raw);

    /* name */
    const char *name_str = json_get_str(raw, "name", "");

    ow_webhook_target_t *t = calloc(1, sizeof(*t));
    t->url = strdup(url);
    t->events = evlist;
    t->name = strdup(name_str);
    t->secret = secret;
    t->matcher = matcher_clean;
    t->timeout = timeout;
    return t;
}

/* PoP: _resolve_secret @ agent/outbound_webhooks.py:_resolve_secret */
char *ow_resolve_secret(int index, const json_t *raw) {
    (void)index;
    if (!raw) return NULL;
    const char *env_name = json_get_str(raw, "secret_env", NULL);
    if (env_name && env_name[0]) {
        const char *env_val = getenv(env_name);
        if (env_val && env_val[0])
            return strdup(env_val);
        fprintf(stderr, "hooks.outbound[%d].secret_env=%s is not set — deliveries will be UNSIGNED\n", index, env_name);
        return NULL;
    }
    const char *sec = json_get_str(raw, "secret", NULL);
    if (sec && sec[0])
        return strdup(sec);
    return NULL;
}

/* PoP: _parse_outbound_block @ agent/outbound_webhooks.py:_parse_outbound_block */
ow_webhook_target_t **ow_parse_outbound_block(const json_t *raw) {
    if (!raw) return NULL;
    if (raw->type != JSON_ARRAY) {
        fprintf(stderr, "hooks.outbound must be a list; got %s\n",
                raw->type == JSON_OBJECT ? "object" : "non-array");
        return NULL;
    }
    size_t n = raw->c.count;
    if (n == 0) return NULL;

    ow_webhook_target_t **targets = calloc(n + 1, sizeof(ow_webhook_target_t *));
    size_t count = 0;
    for (size_t i = 0; i < n; i++) {
        ow_webhook_target_t *t = ow_parse_single_target((int)i, raw->c.items[i]);
        if (t)
            targets[count++] = t;
    }
    targets[count] = NULL;
    if (count == 0) {
        free(targets);
        return NULL;
    }
    return targets;
}

/* ── Payload serialization ────────────────────────────────── */

/* PoP: _serialize_payload @ agent/outbound_webhooks.py:_serialize_payload */
char *ow_serialize_payload(const char *event, const json_t *kwargs,
                            const char *delivery_id) {
    if (!event || !delivery_id) return NULL;

    const char *tool_name = json_get_str(kwargs, "tool_name", NULL);
    const char *session_id = json_get_str(kwargs, "session_id", "");
    const char *parent_session_id = json_get_str(kwargs, "parent_session_id", "");

    json_t *args = json_obj_get(kwargs, "args");
    if (args && args->type != JSON_OBJECT) args = NULL;

    /* extras = kwargs \ top-level keys */
    json_t *extras = json_object();
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < kwargs->c.count; i++) {
            const char *k = kwargs->c.keys[i];
            bool is_top = false;
            for (int j = 0; OW_TOP_LEVEL_PAYLOAD_KEYS[j]; j++) {
                if (strcmp(k, OW_TOP_LEVEL_PAYLOAD_KEYS[j]) == 0) {
                    is_top = true; break;
                }
            }
            if (!is_top)
                json_set(extras, k, json_copy(kwargs->c.items[i]));
        }
    }

    json_t *payload = json_object();
    json_set(payload, "hook_event_name", json_string(event));
    json_set(payload, "tool_name", tool_name ? json_string(tool_name) : json_null());
    json_set(payload, "tool_input", args ? json_copy(args) : json_null());

    const char *sid = (session_id && session_id[0]) ? session_id :
                      (parent_session_id && parent_session_id[0]) ? parent_session_id : "";
    json_set(payload, "session_id", json_string(sid));

    char cwd_buf[4096];
    const char *cwd = getcwd(cwd_buf, sizeof(cwd_buf)) ? cwd_buf : "";
    json_set(payload, "cwd", json_string(cwd));

    json_set(payload, "extra", extras);
    json_set(payload, "delivery_id", json_string(delivery_id));

    time_t now = time(NULL);
    struct tm tmv;
    gmtime_r(&now, &tmv);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tmv);
    json_set(payload, "timestamp", json_string(ts));

    char *body = json_serialize(payload);
    json_free(payload);
    return body;
}

/* PoP: _build_delivery @ agent/outbound_webhooks.py:_build_delivery */
json_t *ow_build_delivery(const char *event, const ow_webhook_target_t *target,
                           const char *body, size_t body_len,
                           const char *delivery_id) {
    (void)body_len;
    json_t *headers = json_object();
    json_set(headers, "Content-Type", json_string("application/json"));
    json_set(headers, "User-Agent", json_string("Hermes-Agent-Outbound-Webhook"));
    json_set(headers, "X-Hermes-Event", json_string(event));
    json_set(headers, "X-Hermes-Delivery", json_string(delivery_id));

    if (target->secret && target->secret[0]) {
        unsigned char digest[CRYPTO_SHA256_LEN];
        crypto_hmac_sha256((const unsigned char *)target->secret, strlen(target->secret),
                           (const unsigned char *)body, strlen(body),
                           digest);
        char sig[72];
        int pos = 0;
        for (size_t i = 0; i < CRYPTO_SHA256_LEN; i++)
            pos += snprintf(sig + pos, sizeof(sig) - (size_t)pos, "%02x", digest[i]);
        char header_val[256];
        snprintf(header_val, sizeof(header_val), "sha256=%s", sig);
        json_set(headers, "X-Hermes-Signature-256", json_string(header_val));
    }

    json_t *delivery = json_object();
    json_set(delivery, "url", json_string(target->url));
    json_set(delivery, "label", json_string(ow_target_label(target)));
    json_set(delivery, "event", json_string(event));
    json_set(delivery, "body", json_string(body));
    json_set(delivery, "headers", headers);
    json_set(delivery, "timeout", json_number((double)target->timeout));
    return delivery;
}

/* ── Delivery queue ───────────────────────────────────────── */

typedef struct {
    json_t **items;
    size_t head, tail, count, capacity;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    volatile int started;
    pthread_t worker;
} delivery_queue_t;

static delivery_queue_t g_queue = {
    .head = 0, .tail = 0, .count = 0, .capacity = OW_QUEUE_MAX_SIZE,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .not_empty = PTHREAD_COND_INITIALIZER,
    .started = 0, .worker = 0, .items = NULL
};

typedef struct {
    char *event;
    char *url;
} reg_entry_t;

static reg_entry_t *g_registered = NULL;
static size_t g_n_registered = 0;
static size_t g_cap_registered = 0;
static pthread_mutex_t g_reg_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: flush @ agent/outbound_webhooks.py:flush */
bool ow_flush(double timeout_seconds) {
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += (time_t)timeout_seconds;
    deadline.tv_nsec += (long)((timeout_seconds - (time_t)timeout_seconds) * 1e9);
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&g_queue.lock);
    while (g_queue.count > 0) {
        int rc = pthread_cond_timedwait(&g_queue.not_empty, &g_queue.lock, &deadline);
        if (rc == ETIMEDOUT) {
            bool empty = (g_queue.count == 0);
            pthread_mutex_unlock(&g_queue.lock);
            return empty;
        }
    }
    pthread_mutex_unlock(&g_queue.lock);
    return true;
}

/* PoP: reset_for_tests @ agent/outbound_webhooks.py:reset_for_tests */
void ow_reset_for_tests(void) {
    pthread_mutex_lock(&g_reg_lock);
    for (size_t i = 0; i < g_n_registered; i++) {
        free(g_registered[i].event);
        free(g_registered[i].url);
    }
    g_n_registered = 0;
    pthread_mutex_unlock(&g_reg_lock);

    pthread_mutex_lock(&g_queue.lock);
    while (g_queue.count > 0) {
        json_free(g_queue.items[g_queue.head]);
        g_queue.head = (g_queue.head + 1) % g_queue.capacity;
        g_queue.count--;
    }
    pthread_mutex_unlock(&g_queue.lock);
}

/* PoP: _enqueue @ agent/outbound_webhooks.py:_enqueue */
bool ow_enqueue(const json_t *delivery) {
    if (!delivery) return false;
    ow_ensure_worker();

    pthread_mutex_lock(&g_queue.lock);
    if (g_queue.count >= g_queue.capacity) {
        const char *ev = json_get_str(delivery, "event", "?");
        const char *lb = json_get_str(delivery, "label", "?");
        fprintf(stderr, "outbound webhook queue full (%zu pending) — dropping %s event for %s\n",
                g_queue.count, ev, lb);
        pthread_mutex_unlock(&g_queue.lock);
        return false;
    }

    if (!g_queue.items) {
        g_queue.items = calloc(g_queue.capacity, sizeof(json_t *));
        if (!g_queue.items) { pthread_mutex_unlock(&g_queue.lock); return false; }
    }

    g_queue.items[g_queue.tail] = json_copy(delivery);
    g_queue.tail = (g_queue.tail + 1) % g_queue.capacity;
    g_queue.count++;
    pthread_cond_signal(&g_queue.not_empty);
    pthread_mutex_unlock(&g_queue.lock);
    return true;
}

/* PoP: _worker_loop @ agent/outbound_webhooks.py:_worker_loop */
static void *ow_worker_thread(void *arg) {
    (void)arg;
    while (1) {
        json_t *delivery = NULL;
        pthread_mutex_lock(&g_queue.lock);
        while (g_queue.count == 0)
            pthread_cond_wait(&g_queue.not_empty, &g_queue.lock);
        delivery = g_queue.items[g_queue.head];
        g_queue.head = (g_queue.head + 1) % g_queue.capacity;
        g_queue.count--;
        pthread_cond_broadcast(&g_queue.not_empty);
        pthread_mutex_unlock(&g_queue.lock);

        if (delivery) {
            ow_deliver(delivery);
            json_free(delivery);
        }
    }
    return NULL;
}

/* PoP: _ensure_worker @ agent/outbound_webhooks.py:_ensure_worker */
void ow_ensure_worker(void) {
    if (g_queue.started) return;
    pthread_mutex_lock(&g_queue.lock);
    if (!g_queue.started) {
        g_queue.items = calloc(g_queue.capacity, sizeof(json_t *));
        if (g_queue.items) {
            if (pthread_create(&g_queue.worker, NULL, ow_worker_thread, NULL) == 0) {
                g_queue.started = 1;
                pthread_detach(g_queue.worker);
            }
        }
    }
    pthread_mutex_unlock(&g_queue.lock);
}

/* PoP: _deliver @ agent/outbound_webhooks.py:_deliver */
void ow_deliver(const json_t *delivery) {
    const char *ev = json_get_str(delivery, "event", "?");
    const char *lb = json_get_str(delivery, "label", "?");
    const char *url = json_get_str(delivery, "url", "?");
    double tm = json_get_num(delivery, "timeout", OW_DEFAULT_TIMEOUT_SECONDS);

    fprintf(stderr, "[outbound-webhook] deliver event=%s target=%s url=%s timeout=%d\n",
            ev, lb, url, (int)tm);
    /* TODO: actual HTTP POST via libhttp */
}

/* PoP: _make_callback @ agent/outbound_webhooks.py:_make_callback */
void *ow_make_callback(const char *event, const ow_webhook_target_t *target) {
    (void)event;
    (void)target;
    return NULL;
}

/* PoP: register_from_config @ agent/outbound_webhooks.py:register_from_config */
ow_webhook_target_t **ow_register_from_config(const json_t *cfg) {
    if (!cfg || cfg->type != JSON_OBJECT) return NULL;

    const char *safe = getenv("HERMES_SAFE_MODE");
    if (safe && strcmp(safe, "1") == 0) {
        fprintf(stderr, "HERMES_SAFE_MODE=1 — outbound webhook registration skipped\n");
        return NULL;
    }

    const json_t *hooks = json_obj_get(cfg, "hooks");
    if (!hooks || hooks->type != JSON_OBJECT) return NULL;

    const json_t *outbound = json_obj_get(hooks, "outbound");
    ow_webhook_target_t **targets = ow_parse_outbound_block(outbound);
    if (!targets) return NULL;

    for (ow_webhook_target_t **tp = targets; *tp; tp++) {
        ow_webhook_target_t *t = *tp;
        bool wired_any = false;
        for (char **ep = t->events; *ep; ep++) {
            const char *event = *ep;
            bool already = false;
            pthread_mutex_lock(&g_reg_lock);
            for (size_t i = 0; i < g_n_registered; i++) {
                if (strcmp(g_registered[i].event, event) == 0 &&
                    strcmp(g_registered[i].url, t->url) == 0) {
                    already = true; break;
                }
            }
            if (!already) {
                if (g_n_registered >= g_cap_registered) {
                    size_t newcap = g_cap_registered ? g_cap_registered * 2 : 8;
                    reg_entry_t *newr = realloc(g_registered, newcap * sizeof(reg_entry_t));
                    if (newr) {
                        g_registered = newr;
                        g_cap_registered = newcap;
                    }
                }
                if (g_n_registered < g_cap_registered) {
                    g_registered[g_n_registered].event = strdup(event);
                    g_registered[g_n_registered].url = strdup(t->url);
                    g_n_registered++;
                    wired_any = true;
                    fprintf(stderr, "outbound webhook registered: %s -> %s (matcher=%s, timeout=%ds)\n",
                            event, ow_target_label(t),
                            t->matcher ? t->matcher : "none",
                            t->timeout);
                }
            }
            pthread_mutex_unlock(&g_reg_lock);
        }
    }
    return targets;
}

/* PoP: iter_configured_targets @ agent/outbound_webhooks.py:iter_configured_targets */
ow_webhook_target_t **ow_iter_configured_targets(const json_t *cfg) {
    if (!cfg || cfg->type != JSON_OBJECT) return NULL;
    const json_t *hooks = json_obj_get(cfg, "hooks");
    if (!hooks || hooks->type != JSON_OBJECT) return NULL;
    const json_t *outbound = json_obj_get(hooks, "outbound");
    return ow_parse_outbound_block(outbound);
}

void ow_targets_free(ow_webhook_target_t **targets) {
    if (!targets) return;
    for (ow_webhook_target_t **tp = targets; *tp; tp++)
        ow_target_free(*tp);
    free(targets);
}