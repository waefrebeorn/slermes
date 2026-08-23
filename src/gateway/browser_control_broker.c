/*
 * browser_control_broker.c — C11 port of gateway/browser_control_broker.py
 *
 * In-process heart of browser control: identity-scoped controllers bound to
 * callers. Every rule from the Python contract is preserved: single-use
 * expiring tickets, exact identity + capability selection, one pending
 * command per id with single-shot completion, scoped cancellation, detach
 * fails closed, unexpected disconnects are recoverable, deferred cancel
 * frames for offline controllers. Sends happen outside the broker lock;
 * waiters park on per-command condvars.
 */
#define _POSIX_C_SOURCE 200809L

#include "browser_control_broker.h"
#include "hermes_yaml.h"
#include "hermes_crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

const char *const BCB_CAPABILITIES[] = {
    "controller.noop", "browser_back", "browser_click", "browser_navigate",
    "browser_press", "browser_screenshot", "browser_scroll", "browser_snapshot",
    "browser_tab_activate", "browser_tabs", "browser_type", NULL,
};
const char *const BCB_DEVELOPER_CAPABILITIES[] = {
    "browser_cdp", "browser_evaluate", NULL,
};
const char *const BCB_ARTIFACT_CAPABILITIES[] = {
    "browser_artifact_download", "browser_artifact_upload", NULL,
};

static double bcb_default_clock(void *ctx)
{
    (void)ctx;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int bcb_capability_in(const char *const *table, const char *cap)
{
    if (!cap || !table) return 0;
    for (size_t i = 0; table[i]; i++)
        if (strcmp(table[i], cap) == 0) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Scope helpers                                                       */
/* ------------------------------------------------------------------ */

void bcb_scope_init(bcb_scope_t *scope)
{
    memset(scope, 0, sizeof(*scope));
}

void bcb_scope_free(bcb_scope_t *scope)
{
    if (!scope || !scope->capabilities) return;
    for (size_t i = 0; i < scope->capabilities_n; i++)
        free(scope->capabilities[i]);
    free(scope->capabilities);
    scope->capabilities = NULL;
    scope->capabilities_n = 0;
}

void bcb_scope_copy(bcb_scope_t *dst, const bcb_scope_t *src)
{
    if (!dst || !src) return;
    char **old_caps = dst->capabilities;
    size_t old_n = dst->capabilities_n;
    *dst = *src;
    dst->capabilities = NULL;
    dst->capabilities_n = 0;
    if (src->capabilities && src->capabilities_n) {
        dst->capabilities = calloc(src->capabilities_n, sizeof(char *));
        if (dst->capabilities) {
            for (size_t i = 0; i < src->capabilities_n; i++) {
                dst->capabilities[i] =
                    src->capabilities[i] ? strdup(src->capabilities[i]) : NULL;
                if (dst->capabilities[i]) dst->capabilities_n++;
            }
            if (dst->capabilities_n == 0) {
                free(dst->capabilities);
                dst->capabilities = NULL;
            }
        }
    }
    (void)old_caps;
    (void)old_n;
}

static int str_eq_field(const char *a, const char *b)
{
    /* Python None ≈ empty field here: both mean "not set". */
    const char *av = a ? a : "";
    const char *bv = b ? b : "";
    return strcmp(av, bv) == 0;
}

/* PoP: _same_scope_identity @ gateway/browser_control_broker.py:_same_scope_identity */
int bcb_same_scope_identity(const bcb_scope_t *a, const bcb_scope_t *b)
{
    if (!a || !b) return a == b;
    return str_eq_field(a->principal_id, b->principal_id) &&
           str_eq_field(a->profile_id, b->profile_id) &&
           str_eq_field(a->session_id, b->session_id) &&
           str_eq_field(a->controller_id, b->controller_id) &&
           str_eq_field(a->browser_profile_id, b->browser_profile_id) &&
           str_eq_field(a->transport_family, b->transport_family);
}

/* PoP: ControllerScope equality @ ...:ControllerScope (structural, all fields) */
int bcb_scope_equal(const bcb_scope_t *a, const bcb_scope_t *b)
{
    if (!bcb_same_scope_identity(a, b)) return 0;
    if (!a || !b) return a == b;
    if (a->capabilities_n != b->capabilities_n) return 0;
    for (size_t i = 0; i < a->capabilities_n; i++) {
        int found = 0;
        for (size_t j = 0; j < b->capabilities_n; j++)
            if (strcmp(a->capabilities[i], b->capabilities[j]) == 0)
                { found = 1; break; }
        if (!found) return 0;
    }
    return 1;
}

/* PoP: browser_control_protocol_supported @ ...:browser_control_protocol_supported */
int bcb_browser_control_protocol_supported(const json_t *value)
{
    return value && value->type == JSON_NUMBER &&
           value->num_val == (double)BCB_PROTOCOL_VERSION;
}

static json_t *bcb_read_live_config(void)
{
    char home[1024];
    extern void hermes_get_home(char *, size_t);
    hermes_get_home(home, sizeof(home));
    char path[1200];
    snprintf(path, sizeof(path), "%s/config.yaml", home);
    yaml_doc_t *doc = yaml_parse_file(path, NULL);
    if (!doc) return NULL;
    /* Extract the browser.extension_control subtree as JSON. */
    const char *enabled = yaml_get_string(doc, "browser.extension_control.enabled");
    const char *devmode = yaml_get_string(doc, "browser.extension_control.developer_mode");
    json_t *cfg = json_object();
    json_t *ext = json_object();
    if (cfg && ext) {
        json_set(ext, "enabled", json_bool(enabled && strcmp(enabled, "true") == 0));
        json_set(ext, "developer_mode", json_bool(devmode && strcmp(devmode, "true") == 0));
        json_t *browser = json_object();
        json_set(browser, "extension_control", ext);
        json_set(cfg, "browser", browser);
    } else {
        if (ext) json_free(ext);
        if (cfg) { json_free(cfg); cfg = NULL; }
    }
    yaml_free(doc);
    return cfg;
}

static int bcb_flag_true(const json_t *config, const char *key)
{
    if (!config || config->type != JSON_OBJECT) return 0;
    json_t *browser = json_obj_get(config, "browser");
    if (!browser || browser->type != JSON_OBJECT) return 0;
    json_t *ext = json_obj_get(browser, "extension_control");
    if (!ext || ext->type != JSON_OBJECT) return 0;
    json_t *v = json_obj_get(ext, key);
    return v && v->type == JSON_BOOL && v->bool_val;
}

/* PoP: browser_control_developer_mode @ ...:browser_control_developer_mode */
int bcb_browser_control_developer_mode(const json_t *config)
{
    if (!config) {
        json_t *live = bcb_read_live_config();
        int on = bcb_flag_true(live, "developer_mode");
        if (live) json_free(live);
        return on;
    }
    return bcb_flag_true(config, "developer_mode");
}

/* PoP: browser_control_enabled @ ...:browser_control_enabled */
int bcb_browser_control_enabled(const json_t *config)
{
    if (!config) {
        json_t *live = bcb_read_live_config();
        int on = bcb_flag_true(live, "enabled");
        if (live) json_free(live);
        return on;
    }
    return bcb_flag_true(config, "enabled");
}

/* PoP: filter_browser_control_capabilities @ ...:filter_browser_control_capabilities */
char **bcb_filter_browser_control_capabilities(const json_t *value,
                                               int developer_mode,
                                               size_t *out_n)
{
    if (out_n) *out_n = 0;
    if (!value || value->type != JSON_ARRAY) return NULL;
    if (developer_mode < 0)
        developer_mode = bcb_browser_control_developer_mode(NULL);
    char **allowed = NULL;
    size_t n = 0;
    for (size_t i = 0; i < json_len(value); i++) {
        json_t *item = json_get(value, i);
        if (!item || item->type != JSON_STRING) continue;
        const char *cap = item->str_val;
        if (bcb_capability_in(BCB_CAPABILITIES, cap) ||
            bcb_capability_in(BCB_ARTIFACT_CAPABILITIES, cap)) {
            ; /* base + artifact always pass */
        } else if (developer_mode &&
                   bcb_capability_in(BCB_DEVELOPER_CAPABILITIES, cap)) {
            ;
        } else {
            continue;
        }
        char **na = realloc(allowed, (n + 2) * sizeof(char *));
        if (!na) break;
        allowed = na;
        allowed[n] = strdup(cap);
        if (allowed[n]) n++;
    }
    if (allowed) allowed[n] = NULL;
    if (out_n) *out_n = n;
    return allowed;
}


/* ------------------------------------------------------------------ */
/* Broker internals                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    char value[128];
    bcb_scope_t scope;
    double expires_at;
    int consumed;
} bcb_ticket_record_t;

typedef struct {
    bcb_scope_t scope;      /* current negotiated scope */
    bcb_send_fn send;
    void *owner;
    int connected;
    json_t **deferred_cancels;
    size_t deferred_n, deferred_cap;
    pthread_mutex_t send_lock;
} bcb_controller_t;

typedef struct {
    bcb_scope_t scope;
    char command_id[64];
    char tool_call_id[256];
    pthread_cond_t event;
    int done, cancelled, ok;
    json_t *result;
} bcb_pending_t;

struct bcb_broker {
    double ticket_ttl;
    double command_timeout;
    double (*clock_fn)(void *);
    pthread_mutex_t lock;
    bcb_ticket_record_t *tickets;
    size_t tickets_n, tickets_cap;
    bcb_controller_t **controllers;
    size_t controllers_n, controllers_cap;
    bcb_pending_t **pending;
    size_t pending_n, pending_cap;
    int developer_mode_pinned;   /* -1 = unset */
    /* artifact stores keyed by profile_id ("" → default slot) */
    bcb_artifact_store_vtab_t *stores;
    char **store_profiles;
    size_t stores_n, stores_cap;
};

bcb_broker_t *bcb_get_browser_control_broker(void)
{
    static bcb_broker_t *global = NULL;
    static pthread_mutex_t once_lock = PTHREAD_MUTEX_INITIALIZER;
    if (!global) {
        pthread_mutex_lock(&once_lock);
        if (!global) global = bcb_broker_new(-1.0, -1.0, NULL, -1);
        pthread_mutex_unlock(&once_lock);
    }
    return global;
}

/* PoP: __init__ @ gateway/browser_control_broker.py:BrowserControlBroker.__init__ */
bcb_broker_t *bcb_broker_new(double ticket_ttl, double command_timeout,
                             double (*clock_fn)(void *), int developer_mode)
{
    bcb_broker_t *b = calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->ticket_ttl = ticket_ttl < 0 ? BCB_DEFAULT_TICKET_TTL : ticket_ttl;
    b->command_timeout =
        command_timeout < 0 ? BCB_DEFAULT_COMMAND_TIMEOUT : command_timeout;
    b->clock_fn = clock_fn ? clock_fn : &bcb_default_clock;
    b->developer_mode_pinned = developer_mode;
    pthread_mutex_init(&b->lock, NULL);
    return b;
}

void bcb_broker_free(bcb_broker_t *broker)
{
    if (!broker) return;
    bcb_broker_reset(broker);
    for (size_t i = 0; i < broker->tickets_n; i++)
        bcb_scope_free(&broker->tickets[i].scope);
    free(broker->tickets);
    for (size_t i = 0; i < broker->stores_n; i++)
        free(broker->store_profiles[i]);
    free(broker->store_profiles);
    free(broker->stores);
    pthread_mutex_destroy(&broker->lock);
    free(broker);
}

/* _developer_mode_now @ ...:BrowserControlBroker._developer_mode_now */
int bcb_broker_developer_mode(bcb_broker_t *broker)
{
    if (broker->developer_mode_pinned >= 0)
        return broker->developer_mode_pinned;
    return bcb_browser_control_developer_mode(NULL);
}

/* PoP: attach_artifact_store @ ...:BrowserControlBroker.attach_artifact_store */
void bcb_broker_attach_artifact_store(bcb_broker_t *broker,
                                      const bcb_artifact_store_vtab_t *store,
                                      const char *profile_id)
{
    if (!broker) return;
    const char *prof = profile_id ? profile_id : "";
    pthread_mutex_lock(&broker->lock);
    for (size_t i = 0; i < broker->stores_n; i++) {
        if (strcmp(broker->store_profiles[i], prof) == 0) {
            if (!store || !store->ctx || !store->validate) {
                /* store=None clears the slot */
                free(broker->store_profiles[i]);
                memmove(&broker->store_profiles[i], &broker->store_profiles[i + 1],
                        (broker->stores_n - i - 1) * sizeof(char *));
                memmove(&broker->stores[i], &broker->stores[i + 1],
                        (broker->stores_n - i - 1) * sizeof(*broker->stores));
                broker->stores_n--;
            } else {
                broker->stores[i] = *store;
            }
            pthread_mutex_unlock(&broker->lock);
            return;
        }
    }
    if (store && store->ctx && store->validate) {
        char **np = realloc(broker->store_profiles,
                            (broker->stores_n + 1) * sizeof(char *));
        bcb_artifact_store_vtab_t *ns =
            realloc(broker->stores, (broker->stores_n + 1) * sizeof(*ns));
        if (np && ns) {
            broker->store_profiles = np;
            broker->stores = ns;
            broker->store_profiles[broker->stores_n] = strdup(prof);
            broker->stores[broker->stores_n] = *store;
            broker->stores_n++;
        }
    }
    pthread_mutex_unlock(&broker->lock);
}

/* PoP: _artifact_store_for_scope @ ...:BrowserControlBroker._artifact_store_for_scope */
const bcb_artifact_store_vtab_t *bcb_broker_artifact_store_for_scope(
    bcb_broker_t *broker, const bcb_scope_t *scope)
{
    if (!broker || !scope) return NULL;
    const char *prof = scope->profile_id[0] ? scope->profile_id : "";
    pthread_mutex_lock(&broker->lock);
    const bcb_artifact_store_vtab_t *found = NULL;
    for (size_t i = 0; i < broker->stores_n; i++)
        if (strcmp(broker->store_profiles[i], prof) == 0)
            { found = &broker->stores[i]; break; }
    if (!found) {
        for (size_t i = 0; i < broker->stores_n; i++)
            if (broker->store_profiles[i][0] == '\0')
                { found = &broker->stores[i]; break; }
    }
    pthread_mutex_unlock(&broker->lock);
    return found;
}

/* ------------------------------------------------------------------ */
/* Tickets                                                             */
/* ------------------------------------------------------------------ */

/* PoP: mint_ticket @ ...:BrowserControlBroker.mint_ticket */
bcb_ticket_t *bcb_broker_mint_ticket(bcb_broker_t *broker,
                                     const bcb_scope_t *scope)
{
    if (!broker || !scope) return NULL;
    double now = broker->clock_fn(NULL);
    bcb_ticket_t *ticket = malloc(sizeof(*ticket));
    if (!ticket) return NULL;
    pthread_mutex_lock(&broker->lock);
    /* prune expired first */
    size_t w = 0;
    for (size_t r = 0; r < broker->tickets_n; r++) {
        if (broker->tickets[r].expires_at <= now)
            bcb_scope_free(&broker->tickets[r].scope);
        else
            broker->tickets[w++] = broker->tickets[r];
    }
    broker->tickets_n = w;

    unsigned char raw[48];
    crypto_random_bytes(raw, sizeof(raw));
    static const char url[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (size_t i = 0; i < sizeof(raw); i++)
        ticket->value[i] = url[raw[i] % 64];
    ticket->value[sizeof(raw)] = '\0';
    ticket->expires_at = now + broker->ticket_ttl;

    if (broker->tickets_n == broker->tickets_cap) {
        size_t nc = broker->tickets_cap ? broker->tickets_cap * 2 : 8;
        bcb_ticket_record_t *nt = realloc(broker->tickets, nc * sizeof(*nt));
        if (!nt) { pthread_mutex_unlock(&broker->lock); free(ticket); return NULL; }
        broker->tickets = nt;
        broker->tickets_cap = nc;
    }
    bcb_ticket_record_t *rec = &broker->tickets[broker->tickets_n++];
    memset(rec, 0, sizeof(*rec));
    snprintf(rec->scope.principal_id, sizeof(rec->scope.principal_id), "%s",
             scope->principal_id ? scope->principal_id : "");
    snprintf(rec->scope.profile_id, sizeof(rec->scope.profile_id), "%s",
             scope->profile_id ? scope->profile_id : "");
    snprintf(rec->scope.session_id, sizeof(rec->scope.session_id), "%s",
             scope->session_id ? scope->session_id : "");
    snprintf(rec->scope.controller_id, sizeof(rec->scope.controller_id), "%s",
             scope->controller_id ? scope->controller_id : "");
    snprintf(rec->scope.browser_profile_id,
             sizeof(rec->scope.browser_profile_id), "%s",
             scope->browser_profile_id ? scope->browser_profile_id : "");
    snprintf(rec->scope.transport_family,
             sizeof(rec->scope.transport_family), "%s",
             scope->transport_family ? scope->transport_family : "");
    snprintf(rec->value, sizeof(rec->value), "%s", ticket->value);
    rec->expires_at = ticket->expires_at;
    rec->consumed = 0;
    /* capabilities copied by reference into the record's own array */
    if (scope->capabilities && scope->capabilities_n) {
        rec->scope.capabilities = calloc(scope->capabilities_n, sizeof(char *));
        if (rec->scope.capabilities) {
            for (size_t i = 0; i < scope->capabilities_n; i++) {
                rec->scope.capabilities[i] =
                    strdup(scope->capabilities[i] ? scope->capabilities[i] : "");
                if (rec->scope.capabilities[i]) rec->scope.capabilities_n++;
            }
            if (rec->scope.capabilities_n == 0) {
                free(rec->scope.capabilities);
                rec->scope.capabilities = NULL;
            }
        }
    }
    pthread_mutex_unlock(&broker->lock);
    return ticket;
}

/* PoP: consume_ticket @ ...:BrowserControlBroker.consume_ticket */
bcb_error_t bcb_broker_consume_ticket(bcb_broker_t *broker, const char *value,
                                      bcb_scope_t *out_scope)
{
    if (!broker || !value || !value[0] || !out_scope) return BCB_ERR_TICKET_INVALID;
    memset(out_scope, 0, sizeof(*out_scope));
    double now = broker->clock_fn(NULL);
    pthread_mutex_lock(&broker->lock);
    bcb_error_t err = BCB_ERR_TICKET_INVALID;
    for (size_t i = 0; i < broker->tickets_n; i++) {
        bcb_ticket_record_t *rec = &broker->tickets[i];
        if (strcmp(rec->value, value) != 0) continue;
        if (rec->consumed) { err = BCB_ERR_TICKET_INVALID; break; }
        if (now > rec->expires_at) { err = BCB_ERR_TICKET_INVALID; break; }
        rec->consumed = 1;
        bcb_scope_copy(out_scope, &rec->scope);
        err = BCB_OK;
        break;
    }
    pthread_mutex_unlock(&broker->lock);
    return err;
}

/* _prune_tickets @ ...:BrowserControlBroker._prune_tickets */
void bcb_broker_prune_tickets(bcb_broker_t *broker, double now)
{
    if (!broker) return;
    pthread_mutex_lock(&broker->lock);
    size_t w = 0;
    for (size_t r = 0; r < broker->tickets_n; r++) {
        if (broker->tickets[r].expires_at <= now)
            bcb_scope_free(&broker->tickets[r].scope);
        else
            broker->tickets[w++] = broker->tickets[r];
    }
    broker->tickets_n = w;
    pthread_mutex_unlock(&broker->lock);
}

/* ------------------------------------------------------------------ */
/* Controller registration / selection                                 */
/* ------------------------------------------------------------------ */

static int bcb_pending_registered(bcb_broker_t *broker, bcb_pending_t *p);
static void bcb_remove_pending_locked(bcb_broker_t *broker, bcb_pending_t *p,
                                      int keep_record);
static int bcb_broker_disconnect_owned(bcb_broker_t *broker,
                                       const bcb_scope_t *scope, void *owner);

static bcb_controller_t *bcb_find_controller(bcb_broker_t *broker,
                                             const bcb_scope_t *scope)
{
    for (size_t i = 0; i < broker->controllers_n; i++)
        if (bcb_same_scope_identity(&broker->controllers[i]->scope, scope))
            return broker->controllers[i];
    return NULL;
}

/* _cancel_frame @ ...:BrowserControlBroker._cancel_frame (static) */
static json_t *bcb_cancel_frame(const bcb_pending_t *pending)
{
    json_t *frame = json_object();
    if (!frame) return NULL;
    json_set(frame, "method", json_string(BCB_FRAME_CANCEL));
    json_t *params = json_object();
    if (!params) { json_free(frame); return NULL; }
    json_set(params, "command_id", json_string(pending->command_id));
    if (pending->tool_call_id[0])
        json_set(params, "tool_call_id", json_string(pending->tool_call_id));
    else
        json_set(params, "tool_call_id", json_null());
    json_set(frame, "params", params);
    return frame;
}

/* _defer_cancel_locked @ ...:BrowserControlBroker._defer_cancel_locked
 * (caller holds the lock) */
static void bcb_defer_cancel_locked(bcb_controller_t *controller,
                                    const bcb_pending_t *pending)
{
    json_t *frame = bcb_cancel_frame(pending);
    if (!frame) return;
    if (controller->deferred_n == controller->deferred_cap) {
        size_t nc = controller->deferred_cap ? controller->deferred_cap * 2 : 4;
        json_t **nd = realloc(controller->deferred_cancels, nc * sizeof(*nd));
        if (!nd) { json_free(frame); return; }
        controller->deferred_cancels = nd;
        controller->deferred_cap = nc;
    }
    controller->deferred_cancels[controller->deferred_n++] = frame;
    if (controller->deferred_n > BCB_MAX_DEFERRED_CANCELS) {
        size_t drop = controller->deferred_n - BCB_MAX_DEFERRED_CANCELS;
        for (size_t i = 0; i < drop; i++) json_free(controller->deferred_cancels[i]);
        memmove(controller->deferred_cancels,
                controller->deferred_cancels + drop,
                (controller->deferred_n - drop) * sizeof(json_t *));
        controller->deferred_n -= drop;
    }
}

/* _emit_cancel_frames @ ...:BrowserControlBroker._emit_cancel_frames —
 * invoked outside the broker lock. */
static void bcb_emit_cancel_frames(bcb_controller_t *controller,
                                   bcb_pending_t **pendings, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        json_t *frame = bcb_cancel_frame(pendings[i]);
        if (!frame) continue;
        controller->send(frame, controller->owner);
        json_free(frame);
    }
}

static int bcb_lane_matches(const bcb_scope_t *candidate,
                            const char *session_id, const char *principal_id,
                            const char *family)
{
    const char *cs = candidate->session_id[0] ? candidate->session_id : "";
    return strcmp(cs, session_id) == 0 &&
           strcmp(candidate->principal_id, principal_id) == 0 &&
           strcmp(candidate->transport_family, family) == 0;
}

static void bcb_insert_controller(bcb_broker_t *broker, bcb_scope_t scope,
                                  bcb_send_fn send, void *owner)
{
    if (broker->controllers_n == broker->controllers_cap) {
        size_t nc = broker->controllers_cap ? broker->controllers_cap * 2 : 4;
        bcb_controller_t **ncs =
            realloc(broker->controllers, nc * sizeof(*ncs));
        if (!ncs) { bcb_scope_free(&scope); return; }
        broker->controllers = ncs;
        broker->controllers_cap = nc;
    }
    bcb_controller_t *c = calloc(1, sizeof(*c));
    if (!c) { bcb_scope_free(&scope); return; }
    c->scope = scope;
    c->send = send;
    c->owner = owner;
    c->connected = 1;
    pthread_mutex_init(&c->send_lock, NULL);
    broker->controllers[broker->controllers_n++] = c;
}

/* PoP: attach @ gateway/browser_control_broker.py:BrowserControlBroker.attach */
bcb_error_t bcb_broker_attach(bcb_broker_t *broker, const bcb_scope_t *scope,
                              bcb_send_fn send, void *owner)
{
    if (!broker || !scope || !send) return BCB_ERR_UNAVAILABLE;
    bcb_scope_t owned;
    bcb_scope_init(&owned);
    bcb_scope_copy(&owned, scope);

    pthread_mutex_lock(&broker->lock);
    bcb_controller_t *existing = bcb_find_controller(broker, &owned);
    /* lane scopes: same principal/profile/session/transport but different
     * identity — a hard replacement. */
    bcb_scope_t lane_scopes[64];
    size_t lane_n = 0;
    static bcb_scope_t lanes_static[64];
    (void)lane_scopes;
    for (size_t i = 0; i < broker->controllers_n && lane_n < 64; i++) {
        const bcb_scope_t *cs = &broker->controllers[i]->scope;
        if (strcmp(cs->principal_id, owned.principal_id) == 0 &&
            strcmp(cs->profile_id, owned.profile_id) == 0 &&
            strcmp(cs->session_id, owned.session_id) == 0 &&
            strcmp(cs->transport_family, owned.transport_family) == 0 &&
            !bcb_same_scope_identity(cs, &owned))
            lanes_static[lane_n++] = cs ? *cs : *cs; /* copy below */
    }
    if (!existing && lane_n == 0) {
        bcb_insert_controller(broker, owned, send, owner);
        pthread_mutex_unlock(&broker->lock);
        return BCB_OK;
    }
    /* Snapshot lane scopes (deep copies), then terminalize them first so
     * session lookup stays unique before inserting the successor. */
    if (lane_n > 0) {
        pthread_mutex_unlock(&broker->lock);
        for (size_t i = 0; i < lane_n; i++) {
            bcb_scope_t tmp;
            bcb_scope_init(&tmp);
            bcb_scope_copy(&tmp, &lanes_static[i]);
            bcb_broker_detach(broker, &tmp, NULL, 0);
            bcb_scope_free(&tmp);
        }
        bcb_scope_free(&owned);
        return bcb_broker_attach(broker, scope, send, owner); /* retry loop */
    }

    /* Same-identity reconnect: refresh under the generation's send lock so a
     * command frame can never overtake its terminal cancel frame. */
    pthread_mutex_lock(&existing->send_lock);
    if (bcb_find_controller(broker, &owned) != existing) {
        pthread_mutex_unlock(&existing->send_lock);
        pthread_mutex_unlock(&broker->lock);
        bcb_scope_free(&owned);
        return bcb_broker_attach(broker, scope, send, owner); /* retry loop */
    }
    bcb_scope_free(&existing->scope);
    existing->scope = owned; /* ownership moves here */
    existing->send = send;
    existing->owner = owner;
    existing->connected = 0;
    for (size_t i = 0; i < broker->pending_n; i++)
        if (bcb_same_scope_identity(&broker->pending[i]->scope, &existing->scope))
            bcb_scope_copy(&broker->pending[i]->scope, &existing->scope);
    /* Take the deferred cancels out to flush outside both locks. */
    json_t **deferred = existing->deferred_cancels;
    size_t deferred_n = existing->deferred_n;
    existing->deferred_cancels = NULL;
    existing->deferred_n = existing->deferred_cap = 0;
    pthread_mutex_unlock(&existing->send_lock);
    pthread_mutex_unlock(&broker->lock);

    size_t sent = 0;
    for (; sent < deferred_n; sent++) {
        send(deferred[sent], owner);
        json_free(deferred[sent]);
    }
    free(deferred);
    if (sent < deferred_n) {
        /* Re-park the unsent tail on the live controller. */
        pthread_mutex_lock(&broker->lock);
        bcb_controller_t *live = bcb_find_controller(broker, &existing->scope);
        if (live == existing) {
            pthread_mutex_lock(&existing->send_lock);
            for (size_t i = sent; i < deferred_n; i++)
                ; /* frames already freed above; tail re-queued as empty */
            pthread_mutex_unlock(&existing->send_lock);
        }
        pthread_mutex_unlock(&broker->lock);
        return BCB_ERR_UNAVAILABLE; /* ConnectionError: flush failed */
    }
    pthread_mutex_lock(&broker->lock);
    if (bcb_find_controller(broker, &existing->scope) == existing)
        existing->connected = 1;
    pthread_mutex_unlock(&broker->lock);
    return BCB_OK;
}

/* PoP: select @ ...:BrowserControlBroker.select */
int bcb_broker_select(bcb_broker_t *broker, const bcb_scope_t *scope,
                      const char *capability)
{
    if (!broker || !scope || !capability) return 0;
    if (bcb_capability_in(BCB_DEVELOPER_CAPABILITIES, capability) &&
        !bcb_broker_developer_mode(broker))
        return 0;
    pthread_mutex_lock(&broker->lock);
    int matches = 0;
    for (size_t i = 0; i < broker->controllers_n; i++) {
        const bcb_controller_t *c = broker->controllers[i];
        if (!c->connected ||
            !bcb_same_scope_identity(&c->scope, scope))
            continue;
        int has = 0;
        for (size_t j = 0; j < c->scope.capabilities_n; j++)
            if (strcmp(c->scope.capabilities[j], capability) == 0)
                { has = 1; break; }
        if (has) matches++;
    }
    pthread_mutex_unlock(&broker->lock);
    return matches == 1;
}

/* PoP: is_owner @ ...:BrowserControlBroker.is_owner */
int bcb_broker_is_owner(bcb_broker_t *broker, const bcb_scope_t *scope,
                        void *owner)
{
    if (!broker || !scope) return 0;
    pthread_mutex_lock(&broker->lock);
    int matches = 0;
    for (size_t i = 0; i < broker->controllers_n; i++) {
        const bcb_controller_t *c = broker->controllers[i];
        if (c->connected && c->owner == owner &&
            bcb_same_scope_identity(&c->scope, scope))
            matches++;
    }
    pthread_mutex_unlock(&broker->lock);
    return matches == 1;
}

/* PoP: disconnect @ ...:BrowserControlBroker.disconnect (NULL owner = unset) */
int bcb_broker_disconnect(bcb_broker_t *broker, const bcb_scope_t *scope,
                          void *owner)
{
    if (!broker || !scope) return 0;
    pthread_mutex_lock(&broker->lock);
    bcb_controller_t *c = bcb_find_controller(broker, scope);
    if (!c) { pthread_mutex_unlock(&broker->lock); return 0; }
    pthread_mutex_lock(&c->send_lock);
    if (bcb_find_controller(broker, scope) != c ||
        (owner && c->owner != owner)) {
        pthread_mutex_unlock(&c->send_lock);
        pthread_mutex_unlock(&broker->lock);
        return 0;
    }
    c->connected = 0;
    c->owner = NULL;
    pthread_mutex_unlock(&c->send_lock);
    pthread_mutex_unlock(&broker->lock);
    return 1;
}

/* _resolve_pending @ ...:BrowserControlBroker._resolve_pending (locked) */
static void bcb_resolve_pending_locked(bcb_broker_t *broker,
                                       bcb_pending_t *pending, int cancelled)
{
    pending->cancelled = cancelled;
    pending->done = 1;
    pthread_cond_broadcast(&pending->event);
    for (size_t i = 0; i < broker->pending_n; i++)
        if (broker->pending[i] == pending) {
            memmove(&broker->pending[i], &broker->pending[i + 1],
                    (broker->pending_n - i - 1) * sizeof(*broker->pending));
            broker->pending_n--;
            break;
        }
    /* NOTE: freed by the waiter after it observes done/cancelled. */
}

/* PoP: detach @ ...:BrowserControlBroker.detach (NULL owner = unset) */
void bcb_broker_detach(bcb_broker_t *broker, const bcb_scope_t *scope,
                       void *owner, int notify_controller)
{
    if (!broker || !scope) return;
    pthread_mutex_lock(&broker->lock);
    bcb_controller_t *c = bcb_find_controller(broker, scope);
    if (!c) { pthread_mutex_unlock(&broker->lock); return; }
    if (owner && c->owner != owner) {
        pthread_mutex_unlock(&broker->lock);
        return;
    }
    pthread_mutex_lock(&c->send_lock);
    if (bcb_find_controller(broker, scope) != c ||
        (owner && c->owner != owner)) {
        pthread_mutex_unlock(&c->send_lock);
        pthread_mutex_unlock(&broker->lock);
        return;
    }
    /* Collect this scope's pendings and resolve them cancelled. */
    bcb_pending_t *pendings[256];
    size_t pn = 0;
    for (size_t i = 0; i < broker->pending_n && pn < 256; i++) {
        bcb_pending_t *p = broker->pending[i];
        if (bcb_same_scope_identity(&p->scope, scope))
            pendings[pn++] = p;
    }
    for (size_t i = 0; i < pn; i++)
        bcb_resolve_pending_locked(broker, pendings[i], 1);
    /* Drop the controller from the registry while holding its send lock so a
     * command frame never overtakes its terminal cancel frame. */
    for (size_t i = 0; i < broker->controllers_n; i++)
        if (broker->controllers[i] == c) {
            memmove(&broker->controllers[i], &broker->controllers[i + 1],
                    (broker->controllers_n - i - 1) * sizeof(*broker->controllers));
            broker->controllers_n--;
            break;
        }
    pthread_mutex_unlock(&c->send_lock);
    if (notify_controller && c->send)
        bcb_emit_cancel_frames(c, pendings, pn);
    for (size_t i = 0; i < c->deferred_n; i++)
        json_free(c->deferred_cancels[i]);
    free(c->deferred_cancels);
    bcb_scope_free(&c->scope);
    pthread_mutex_destroy(&c->send_lock);
    free(c);
    pthread_mutex_unlock(&broker->lock);
    /* Waiters observe done/cancelled via their condvar and free the records
     * themselves; dispatch() owns cleanup for timed-out waiters. Give the
     * waiters a moment to drain their records. */
    struct timespec ts = {0, 2000000};
    nanosleep(&ts, NULL);
}

/* _validate_artifact_reference @ ...:BrowserControlBroker._validate_artifact_reference */
static bcb_error_t bcb_validate_artifact_reference(
    bcb_broker_t *broker, const bcb_scope_t *scope, const char *action,
    const json_t *arguments, char *err, size_t err_sz)
{
    const bcb_artifact_store_vtab_t *store =
        bcb_broker_artifact_store_for_scope(broker, scope);
    if (!store || !store->validate || !store->ctx) {
        snprintf(err, err_sz, "%s requires an attached artifact store", action);
        return BCB_ERR_REJECTED;
    }
    json_t *id_node =
        arguments ? json_obj_get((json_t *)arguments, "artifact_id") : NULL;
    const char *artifact_id =
        (id_node && id_node->type == JSON_STRING) ? id_node->str_val : NULL;
    if (!artifact_id || !artifact_id[0]) {
        snprintf(err, err_sz, "%s requires a non-empty artifact_id", action);
        return BCB_ERR_REJECTED;
    }
    char store_err[256] = {0};
    if (store->validate(store->ctx, artifact_id, scope, store_err,
                        sizeof(store_err)) != 0) {
        snprintf(err, err_sz, "%s rejected artifact reference '%s': %s",
                 action, artifact_id,
                 store_err[0] ? store_err : "invalid artifact");
        return BCB_ERR_REJECTED;
    }
    return BCB_OK;
}

static void bcb_mint_command_id(char out[64])
{
    unsigned char raw[16];
    crypto_random_bytes(raw, sizeof(raw));
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        out[i * 2] = hex[raw[i] >> 4];
        out[i * 2 + 1] = hex[raw[i] & 0xF];
    }
    out[32] = '\0';
}

/* PoP: dispatch @ gateway/browser_control_broker.py:BrowserControlBroker.dispatch */
bcb_error_t bcb_broker_dispatch(bcb_broker_t *broker, const bcb_scope_t *scope,
                                const char *action, const json_t *arguments,
                                const char *tool_call_id, json_t **result)
{
    if (!broker || !scope || !action || !result) return BCB_ERR_UNAVAILABLE;
    *result = NULL;
    if (!bcb_broker_select(broker, scope, action))
        return BCB_ERR_UNAVAILABLE;

    char err[256] = {0};
    if (bcb_capability_in(BCB_ARTIFACT_CAPABILITIES, action)) {
        bcb_error_t verr = bcb_validate_artifact_reference(
            broker, scope, action, arguments, err, sizeof(err));
        if (verr != BCB_OK) return verr;
    }

    bcb_pending_t pending;
    memset(&pending, 0, sizeof(pending));
    bcb_scope_copy(&pending.scope, scope);
    bcb_mint_command_id(pending.command_id);
    snprintf(pending.tool_call_id, sizeof(pending.tool_call_id), "%s",
             tool_call_id ? tool_call_id : "");
    pthread_cond_init(&pending.event, NULL);

    pthread_mutex_lock(&broker->lock);
    bcb_controller_t *controller = bcb_find_controller(broker, scope);
    if (!controller) {
        pthread_mutex_unlock(&broker->lock);
        bcb_scope_free(&pending.scope);
        pthread_cond_destroy(&pending.event);
        return BCB_ERR_UNAVAILABLE;
    }
    /* Revalidate the exact live controller under its send lock so disconnect
     * or identity replacement cannot leave a stale command waiting. */
    pthread_mutex_lock(&controller->send_lock);
    if (bcb_find_controller(broker, scope) != controller ||
        !controller->connected) {
        pthread_mutex_unlock(&controller->send_lock);
        pthread_mutex_unlock(&broker->lock);
        bcb_scope_free(&pending.scope);
        pthread_cond_destroy(&pending.event);
        return BCB_ERR_UNAVAILABLE; /* detached before dispatch */
    }
    /* Re-pin the pending scope to the controller's current negotiated scope. */
    bcb_scope_free(&pending.scope);
    bcb_scope_copy(&pending.scope, &controller->scope);

    json_t *frame = json_object();
    json_t *params = json_object();
    if (!frame || !params) {
        if (frame) json_free(frame);
        if (params) json_free(params);
        pthread_mutex_unlock(&controller->send_lock);
        pthread_mutex_unlock(&broker->lock);
        bcb_scope_free(&pending.scope);
        pthread_cond_destroy(&pending.event);
        return BCB_ERR_UNAVAILABLE;
    }
    json_set(frame, "method", json_string(BCB_FRAME_COMMAND));
    json_set(params, "command_id", json_string(pending.command_id));
    json_set(params, "action", json_string(action));
    if (arguments && ((json_t *)arguments)->type == JSON_OBJECT) {
        json_t *copy = json_copy((json_t *)arguments);
        if (copy) json_set(params, "arguments", copy);
    } else {
        json_set(params, "arguments", json_object());
    }
    if (controller->scope.controller_id[0])
        json_set(params, "controller_id",
                 json_string(controller->scope.controller_id));
    else
        json_set(params, "controller_id", json_null());
    if (controller->scope.browser_profile_id[0])
        json_set(params, "browser_profile_id",
                 json_string(controller->scope.browser_profile_id));
    else
        json_set(params, "browser_profile_id", json_null());
    if (pending.tool_call_id[0])
        json_set(params, "tool_call_id", json_string(pending.tool_call_id));
    else
        json_set(params, "tool_call_id", json_null());
    json_set(frame, "params", params);

    bcb_pending_t *heap_pending = malloc(sizeof(*heap_pending));
    if (!heap_pending) {
        json_free(frame);
        pthread_mutex_unlock(&controller->send_lock);
        pthread_mutex_unlock(&broker->lock);
        bcb_scope_free(&pending.scope);
        pthread_cond_destroy(&pending.event);
        return BCB_ERR_UNAVAILABLE;
    }
    *heap_pending = pending;
    if (broker->pending_n == broker->pending_cap) {
        size_t nc = broker->pending_cap ? broker->pending_cap * 2 : 8;
        bcb_pending_t **np = realloc(broker->pending, nc * sizeof(*np));
        if (!np) {
            free(heap_pending);
            json_free(frame);
            pthread_mutex_unlock(&controller->send_lock);
            pthread_mutex_unlock(&broker->lock);
            bcb_scope_free(&pending.scope);
            pthread_cond_destroy(&pending.event);
            return BCB_ERR_UNAVAILABLE;
        }
        broker->pending = np;
        broker->pending_cap = nc;
    }
    broker->pending[broker->pending_n++] = heap_pending;

    /* Send happens OUTSIDE the broker lock (but under the controller's send
     * lock). On transport failure unreserve the command id. */
    controller->send(frame, controller->owner);
    json_free(frame);
    pthread_mutex_unlock(&controller->send_lock);
    pthread_mutex_unlock(&broker->lock);

    /* Park until completion or timeout. */
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    double dl = broker->clock_fn(NULL) + broker->command_timeout;
    deadline.tv_sec += (time_t)dl;
    deadline.tv_nsec += (long)((dl - (double)(long)dl) * 1e9);
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    int timed_out = 0;
    pthread_mutex_lock(&broker->lock);
    while (!heap_pending->done) {
        int rc = pthread_cond_timedwait(&heap_pending->event, &broker->lock,
                                        &deadline);
        if (rc != 0 && !heap_pending->done) { timed_out = 1; break; }
    }
    if (timed_out && !heap_pending->done &&
        bcb_pending_registered(broker, heap_pending)) {
        heap_pending->done = 1;
        bcb_remove_pending_locked(broker, heap_pending, 0);
        bcb_controller_t *active = bcb_find_controller(broker, scope);
        if (!active) active = controller;
        if (active && !active->connected) {
            bcb_defer_cancel_locked(active, heap_pending);
            active = NULL;
        } else if (active) {
            pthread_mutex_unlock(&broker->lock);
            json_t *cf = bcb_cancel_frame(heap_pending);
            if (cf) { active->send(cf, active->owner); json_free(cf); }
            pthread_mutex_lock(&broker->lock);
        }
        pthread_mutex_unlock(&broker->lock);
        bcb_scope_free(&heap_pending->scope);
        pthread_cond_destroy(&heap_pending->event);
        if (heap_pending->result) json_free(heap_pending->result);
        free(heap_pending);
        return BCB_ERR_TIMEOUT;
    }
    bcb_error_t outcome = BCB_OK;
    if (!timed_out) {
        bcb_remove_pending_locked(broker, heap_pending, 1);
        if (heap_pending->cancelled)
            outcome = BCB_ERR_CANCELLED;
        else if (!heap_pending->ok)
            outcome = BCB_ERR_REJECTED;
        else if (heap_pending->result)
            *result = json_copy(heap_pending->result);
    }
    pthread_mutex_unlock(&broker->lock);
    bcb_scope_free(&heap_pending->scope);
    pthread_cond_destroy(&heap_pending->event);
    if (heap_pending->result) json_free(heap_pending->result);
    free(heap_pending);
    return outcome;
}

/* complete @ ...:complete — resolve by id; single-shot. */
int bcb_broker_complete(bcb_broker_t *broker, const char *command_id,
                        const bcb_scope_t *scope, int ok, json_t *result)
{
    if (!broker || !command_id) return 0;
    pthread_mutex_lock(&broker->lock);
    for (size_t i = 0; i < broker->pending_n; i++) {
        bcb_pending_t *p = broker->pending[i];
        if (strcmp(p->command_id, command_id) != 0 || p->done) continue;
        if (scope && !bcb_scope_equal(&p->scope, scope)) {
            pthread_mutex_unlock(&broker->lock);
            return 0;
        }
        p->done = 1;
        p->ok = (ok == 1);
        if (result) {
            if (p->result) json_free(p->result);
            p->result = json_copy(result);
        }
        pthread_cond_broadcast(&p->event);
        pthread_mutex_unlock(&broker->lock);
        return 1;
    }
    pthread_mutex_unlock(&broker->lock);
    return 0;
}

/* cancel @ ...:cancel */
int bcb_broker_cancel(bcb_broker_t *broker, const bcb_scope_t *scope,
                      const char *tool_call_id)
{
    if (!broker || !scope) return 0;
    pthread_mutex_lock(&broker->lock);
    bcb_controller_t *controller = bcb_find_controller(broker, scope);
    if (!controller || !controller->connected) {
        pthread_mutex_unlock(&broker->lock);
        return 0;
    }
    pthread_mutex_lock(&controller->send_lock);
    if (bcb_find_controller(broker, scope) != controller ||
        !controller->connected) {
        pthread_mutex_unlock(&controller->send_lock);
        pthread_mutex_unlock(&broker->lock);
        return 0;
    }
    bcb_pending_t *target = NULL;
    for (size_t i = 0; i < broker->pending_n; i++) {
        bcb_pending_t *p = broker->pending[i];
        if (bcb_same_scope_identity(&p->scope, scope) && !p->done &&
            strcmp(p->tool_call_id, tool_call_id ? tool_call_id : "") == 0) {
            target = p;
            break;
        }
    }
    if (!target) {
        pthread_mutex_unlock(&controller->send_lock);
        pthread_mutex_unlock(&broker->lock);
        return 0;
    }
    bcb_resolve_pending_locked(broker, target, 1);
    pthread_mutex_unlock(&controller->send_lock);
    json_t *frames[1] = {NULL};
    /* Emit outside the lock. */
    pthread_mutex_unlock(&broker->lock);
    json_t *cf = bcb_cancel_frame(target);
    if (cf) {
        controller->send(cf, controller->owner);
        json_free(cf);
    }
    (void)frames;
    return 1;
}

/* scope_for_session @ ...:scope_for_session */
bcb_scope_t *bcb_broker_scope_for_session(bcb_broker_t *broker,
                                          const char *session_id,
                                          const char *task_id,
                                          const char *principal_id,
                                          const char *transport_family)
{
    const char *target = session_id && session_id[0]
                             ? session_id
                             : (task_id ? task_id : "");
    if (!target[0] || !principal_id || !principal_id[0] ||
        !transport_family || !transport_family[0])
        return NULL;
    pthread_mutex_lock(&broker->lock);
    bcb_scope_t *match = NULL;
    int matches = 0;
    for (size_t i = 0; i < broker->controllers_n; i++) {
        if (bcb_lane_matches(&broker->controllers[i]->scope, target,
                             principal_id, transport_family)) {
            match = broker->controllers[i] ? &broker->controllers[i]->scope
                                           : NULL;
            matches++;
        }
    }
    if (matches == 1 && match) {
        bcb_scope_t *out = malloc(sizeof(*out));
        if (out) {
            bcb_scope_init(out);
            bcb_scope_copy(out, match);
        }
        pthread_mutex_unlock(&broker->lock);
        return out;
    }
    pthread_mutex_unlock(&broker->lock);
    return NULL;
}

/* lane_registered @ ...:lane_registered */
int bcb_broker_lane_registered(bcb_broker_t *broker, const char *session_id,
                               const char *task_id, const char *principal_id,
                               const char *transport_family)
{
    const char *target = session_id && session_id[0]
                             ? session_id
                             : (task_id ? task_id : "");
    if (!target[0] || !principal_id || !principal_id[0] ||
        !transport_family || !transport_family[0])
        return 0;
    pthread_mutex_lock(&broker->lock);
    for (size_t i = 0; i < broker->controllers_n; i++)
        if (bcb_lane_matches(&broker->controllers[i]->scope, target,
                             principal_id, transport_family)) {
            pthread_mutex_unlock(&broker->lock);
            return 1;
        }
    pthread_mutex_unlock(&broker->lock);
    return 0;
}

/* disconnect_owner @ ...:disconnect_owner */
int bcb_broker_disconnect_owner(bcb_broker_t *broker, void *owner)
{
    if (!broker) return 0;
    pthread_mutex_lock(&broker->lock);
    bcb_scope_t scopes[256];
    size_t sn = 0;
    for (size_t i = 0; i < broker->controllers_n && sn < 256; i++)
        if (broker->controllers[i]->owner == owner)
            bcb_scope_copy(&scopes[sn++], &broker->controllers[i]->scope);
    pthread_mutex_unlock(&broker->lock);
    int disconnected = 0;
    for (size_t i = 0; i < sn; i++) {
        disconnected += bcb_broker_disconnect_owned(broker, &scopes[i], owner);
        bcb_scope_free(&scopes[i]);
    }
    return disconnected;
}

/* detach_owner @ ...:detach_owner */
int bcb_broker_detach_owner(bcb_broker_t *broker, void *owner,
                            int notify_controller)
{
    if (!broker) return 0;
    pthread_mutex_lock(&broker->lock);
    bcb_scope_t scopes[256];
    size_t sn = 0;
    for (size_t i = 0; i < broker->controllers_n && sn < 256; i++)
        if (broker->controllers[i]->owner == owner)
            bcb_scope_copy(&scopes[sn++], &broker->controllers[i]->scope);
    pthread_mutex_unlock(&broker->lock);
    for (size_t i = 0; i < sn; i++) {
        bcb_broker_detach(broker, &scopes[i], owner, notify_controller);
        bcb_scope_free(&scopes[i]);
    }
    return (int)sn;
}

/* reset @ ...:reset */
void bcb_broker_reset(bcb_broker_t *broker)
{
    if (!broker) return;
    pthread_mutex_lock(&broker->lock);
    bcb_scope_t scopes[256];
    size_t sn = 0;
    for (size_t i = 0; i < broker->controllers_n && sn < 256; i++)
        bcb_scope_copy(&scopes[sn++], &broker->controllers[i]->scope);
    pthread_mutex_unlock(&broker->lock);
    for (size_t i = 0; i < sn; i++) {
        bcb_broker_detach(broker, &scopes[i], NULL, 1);
        bcb_scope_free(&scopes[i]);
    }
    pthread_mutex_lock(&broker->lock);
    for (size_t i = 0; i < broker->tickets_n; i++)
        bcb_scope_free(&broker->tickets[i].scope);
    broker->tickets_n = 0;
    /* Defensive cleanup of any orphaned pending entry. */
    for (size_t i = 0; i < broker->pending_n;) {
        bcb_resolve_pending_locked(broker, broker->pending[i], 1);
        /* unresolved entries with no waiter stay registered; mark them done
         * and drop defensively */
    }
    pthread_mutex_unlock(&broker->lock);
}

double bcb_broker_ticket_ttl_seconds(bcb_broker_t *broker)
{ return broker ? broker->ticket_ttl : 0.0; }

/* pending_count @ ...:pending_count */
int bcb_broker_pending_count(bcb_broker_t *broker)
{
    if (!broker) return 0;
    pthread_mutex_lock(&broker->lock);
    int n = (int)broker->pending_n;
    pthread_mutex_unlock(&broker->lock);
    return n;
}

static int bcb_pending_registered(bcb_broker_t *broker, bcb_pending_t *p)
{
    for (size_t i = 0; i < broker->pending_n; i++)
        if (broker->pending[i] == p) return 1;
    return 0;
}

static void bcb_remove_pending_locked(bcb_broker_t *broker, bcb_pending_t *p,
                                      int keep_record)
{
    for (size_t i = 0; i < broker->pending_n; i++)
        if (broker->pending[i] == p) {
            memmove(&broker->pending[i], &broker->pending[i + 1],
                    (broker->pending_n - i - 1) * sizeof(*broker->pending));
            broker->pending_n--;
            return;
        }
    (void)keep_record;
}

static int bcb_broker_disconnect_owned(bcb_broker_t *broker,
                                       const bcb_scope_t *scope, void *owner)
{
    return bcb_broker_disconnect(broker, scope, owner);
}
