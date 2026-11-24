/*
 * egress.c — C home for upstream Hermes gateway/relay/egress.py
 * (gate w34_egress_308cfb). Behaviour port; see noc_w34_egress_308cfb.h.
 *
 * Gateway-side relay EGRESS AUTHORIZATION as pure value -> value decisions:
 *
 *   is_egress_decline / declined_send / decline_error / log_decline's message
 *   _is_missing_gateway_relay
 *   _has_live_native_adapter / relay_routed_platform
 *   _session_ids / attested_relay_targets
 *   _is_unresolved_handle / authorize_relay_target
 *
 * Every runtime seam (relay fronted-platform discovery, the gateway runner's
 * adapter registry, home-channel config, channel directory, gateway-session
 * origins, native-token config, the logger) is a fixture VALUE here. Nothing
 * connects, reads a config file, or starts a runner.
 *
 * PoP: gateway_relay_egress @ gateway/relay/egress.py:<module>
 */
#include "noc_w34_egress_308cfb.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EG_DECLINE_CODE   "egress_declined"
#define EG_DECLINE_MARKER "egress declined:" /* matched against lowercased text */
#define EG_AMBIGUOUS      "ack lost"

/* ------------------------------------------------------------------ */
/* small value helpers                                                */
/* ------------------------------------------------------------------ */

static char *eg_dup(const char *s, size_t n)
{
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    if (n && s) memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static char *eg_strdup(const char *s)
{
    return s ? eg_dup(s, strlen(s)) : eg_dup("", 0);
}

/* Python truthiness of a JSON value. An absent key reads as None, hence
 * falsey: json_type(NULL) is JSON_NULL. */
static int eg_truthy(const json_t *v)
{
    json_t *p = (json_t *)v;
    if (!p) return 0;
    switch (p->type) {
    case JSON_BOOL:   return p->bool_val ? 1 : 0;
    case JSON_NUMBER: return p->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (p->str_val && p->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return p->c.count ? 1 : 0;
    case JSON_NULL:
    default:          return 0;
    }
}

/* Python str() for the scalar shapes a destination can take. */
static char *eg_str(const json_t *v)
{
    json_t *p = (json_t *)v;
    char buf[64];
    if (!p) return eg_strdup("None");
    switch (p->type) {
    case JSON_NULL:
        return eg_strdup("None");
    case JSON_BOOL:
        return eg_strdup(p->bool_val ? "True" : "False");
    case JSON_STRING:
        return eg_strdup(p->str_val ? p->str_val : "");
    case JSON_NUMBER: {
        double d = p->num_val;
        long long ll = (long long)d;
        if ((double)ll == d && d < 1e15 && d > -1e15)
            snprintf(buf, sizeof buf, "%lld", ll);
        else
            snprintf(buf, sizeof buf, "%.17g", d);
        return eg_strdup(buf);
    }
    case JSON_ARRAY:
    case JSON_OBJECT:
    default: {
        char *s = json_serialize(p);
        if (!s) return eg_strdup("");
        {
            char *o = eg_strdup(s);
            free(s);
            return o;
        }
    }
    }
}

static void eg_lower(char *s)
{
    for (; s && *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

/* Python .strip() on the ASCII whitespace set. */
static char *eg_strip(char *s)
{
    size_t n;
    char *e;
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    n = strlen(s);
    e = s + n;
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = '\0';
    return s;
}

/* str(x or "") [.strip()] [.lower()] into a fresh allocation. */
static char *eg_norm(const json_t *v, int lower)
{
    char *s = eg_truthy(v) ? eg_str(v) : eg_strdup("");
    char *t;
    if (!s) return NULL;
    t = eg_strip(s);
    if (t != s) memmove(s, t, strlen(t) + 1);
    if (lower) eg_lower(s);
    return s;
}

/* A de-duplicated string set. Fixture-sized, so membership is linear. */
typedef struct {
    char **v;
    size_t n;
    size_t cap;
} eg_set;

static void eg_set_init(eg_set *s)
{
    s->v = NULL;
    s->n = 0;
    s->cap = 0;
}

static int eg_set_has(const eg_set *s, const char *x)
{
    size_t i;
    if (!s || !x) return 0;
    for (i = 0; i < s->n; i++)
        if (!strcmp(s->v[i], x)) return 1;
    return 0;
}

static void eg_set_add(eg_set *s, const char *x)
{
    char *c;
    if (!s || !x || eg_set_has(s, x)) return;
    if (s->n == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 8;
        char **nv = (char **)realloc(s->v, nc * sizeof(char *));
        if (!nv) return;
        s->v = nv;
        s->cap = nc;
    }
    c = eg_strdup(x);
    if (!c) return;
    s->v[s->n++] = c;
}

static void eg_set_free(eg_set *s)
{
    size_t i;
    if (!s) return;
    for (i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    s->v = NULL;
    s->n = s->cap = 0;
}

static int eg_cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Sorted JSON array of the set's members. */
static json_t *eg_set_json(eg_set *s)
{
    json_t *arr = json_array();
    size_t i;
    if (!arr) return NULL;
    if (s->n > 1) qsort(s->v, s->n, sizeof(char *), eg_cmp_str);
    for (i = 0; i < s->n; i++) {
        if (i && !strcmp(s->v[i], s->v[i - 1])) continue;
        json_append(arr, json_string(s->v[i]));
    }
    return arr;
}

/* ------------------------------------------------------------------ */
/* (b) decline visibility                                             */
/* ------------------------------------------------------------------ */

/* PoP: w34_egress_308cfb_is_egress_decline @ gateway/relay/egress.py:is_egress_decline */
int w34_egress_308cfb_is_egress_decline(const json_t *result)
{
    char *code = NULL, *err = NULL;
    int out = 0;

    if (!json_is_object(result)) return 0;
    if (eg_truthy(json_obj_get(result, "success"))) return 0;
    if (eg_truthy(json_obj_get(result, "ambiguous"))) return 0;

    code = eg_truthy(json_obj_get(result, "code")) ? eg_str(json_obj_get(result, "code")) : eg_strdup("");
    if (code && !strcmp(code, EG_DECLINE_CODE)) {
        out = 1;
    } else {
        err = eg_norm(json_obj_get(result, "error"), 1);
        out = (err && strstr(err, EG_DECLINE_MARKER)) ? 1 : 0;
    }
    free(code);
    free(err);
    return out;
}

/* PoP: w34_egress_308cfb_declined_send @ gateway/relay/egress.py:declined_send */
int w34_egress_308cfb_declined_send(const json_t *result)
{
    json_t *raw, *errv, *probe;
    char *low;
    int out;

    if (!json_is_object(result)) return 0;
    raw = json_obj_get(result, "raw_response");
    if (json_is_object(raw)) return w34_egress_308cfb_is_egress_decline(raw);

    errv = json_obj_get(result, "error");
    if (!eg_truthy(errv)) return 0;

    low = eg_str(errv);
    if (low) eg_lower(low);
    if (low && strstr(low, EG_AMBIGUOUS)) { /* may have been delivered */
        free(low);
        return 0;
    }
    free(low);

    probe = json_object();
    if (!probe) return 0;
    json_set(probe, "success", json_bool(false));
    json_set(probe, "error", json_copy(errv));
    out = w34_egress_308cfb_is_egress_decline(probe);
    json_free(probe);
    return out;
}

/* PoP: w34_egress_308cfb_decline_error @ gateway/relay/egress.py:decline_error */
json_t *w34_egress_308cfb_decline_error(const json_t *result)
{
    if (json_is_object(result)) {
        json_t *e = json_obj_get(result, "error");
        if (eg_truthy(e)) {
            char *s = eg_str(e);
            json_t *o = json_string(s ? s : "");
            free(s);
            return o;
        }
    }
    return json_string("relay egress declined");
}

/* PoP: w34_egress_308cfb_decline_log_line @ gateway/relay/egress.py:log_decline */
json_t *w34_egress_308cfb_decline_log_line(const json_t *op, const json_t *chat_id,
                                           const json_t *result)
{
    char *o = eg_str(op), *c = eg_str(chat_id);
    json_t *d = w34_egress_308cfb_decline_error(result);
    const char *dt = json_string_value_safe(d);
    size_t need = strlen(o ? o : "") + strlen(c ? c : "") + strlen(dt ? dt : "") + 32;
    char *buf = (char *)malloc(need);
    json_t *out;

    if (!buf) {
        free(o);
        free(c);
        json_free(d);
        return json_string("");
    }
    snprintf(buf, need, "relay %s DECLINED for %s: %s", o ? o : "", c ? c : "", dt ? dt : "");
    out = json_string(buf);
    free(buf);
    free(o);
    free(c);
    json_free(d);
    return out;
}

/* ------------------------------------------------------------------ */
/* absence vs fault                                                   */
/* ------------------------------------------------------------------ */

/* PoP: w34_egress_308cfb_is_missing_gateway_relay @ gateway/relay/egress.py:_is_missing_gateway_relay */
int w34_egress_308cfb_is_missing_gateway_relay(int is_module_not_found, const char *name)
{
    static const char *const mod = "gateway.relay.egress";
    size_t n;

    if (!is_module_not_found || !name) return 0; /* absence needs a name */
    if (!strcmp(name, mod)) return 1;
    n = strlen(name);
    if (!n) return 0;
    return (strncmp(mod, name, n) == 0 && mod[n] == '.') ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* (a) routing                                                        */
/* ------------------------------------------------------------------ */

/* PoP: w34_egress_308cfb_has_live_native_adapter @ gateway/relay/egress.py:_has_live_native_adapter */
json_t *w34_egress_308cfb_has_live_native_adapter(int runner_present, int adapter_present,
                                                  int config_error, const json_t *native_config)
{
    if (!runner_present) return json_string("false");   /* no gateway runner here */
    if (!adapter_present) return json_string("false");  /* nothing registered */
    if (config_error) return json_string("unknown");    /* routing UNKNOWN, not native */
    if (json_type((json_t *)native_config) == JSON_NULL)
        return json_string("true");                     /* absent from config => enabled */
    if (json_is_object(native_config)) {
        json_t *en = json_obj_get(native_config, "enabled");
        int on = en ? eg_truthy(en) : 1;                /* getattr(cfg, "enabled", True) */
        return json_string(on ? "true" : "false");
    }
    return json_string("true");                         /* attr default */
}

/* The relay's fronted set, normalized the way `_relay_fronted` does
 * (str(p).strip().lower()), read from in["fronted"]. */
static void eg_fronted(const json_t *in, eg_set *out)
{
    json_t *arr = json_obj_get(in, "fronted");
    size_t i, n = json_is_array(arr) ? json_array_size(arr) : 0;
    for (i = 0; i < n; i++) {
        char *p = eg_norm(json_get(arr, i), 1);
        if (p) {
            eg_set_add(out, p);
            free(p);
        }
    }
}

/* relay_routed_platform's verdict for an already-normalized name. */
static const char *eg_route(const json_t *in, const char *name, eg_set *fronted)
{
    json_t *nat;
    const char *verdict;

    if (!name || !name[0]) return "false";
    if (!strcmp(name, "relay")) return "true"; /* the generic plane needs no seam */
    if (json_get_bool(in, "route_unknown", 0)) return "unknown";
    eg_fronted(in, fronted);
    if (!eg_set_has(fronted, name)) return "false";
    nat = w34_egress_308cfb_has_live_native_adapter(
        json_get_bool(in, "runner_present", 0),
        json_get_bool(in, "adapter_present", 0),
        json_get_bool(in, "config_error", 0),
        json_obj_get(in, "native_config"));
    verdict = nat ? json_string_value_safe(nat) : NULL;
    if (verdict && !strcmp(verdict, "true")) {
        json_free(nat);
        return "false"; /* a native adapter always wins: not a relay egress */
    }
    if (verdict && !strcmp(verdict, "unknown")) {
        json_free(nat);
        return "unknown";
    }
    json_free(nat);
    return "true";
}

/* PoP: w34_egress_308cfb_relay_routed_platform @ gateway/relay/egress.py:relay_routed_platform */
json_t *w34_egress_308cfb_relay_routed_platform(const json_t *in)
{
    char *name = eg_norm(json_obj_get(in, "platform"), 1);
    eg_set fronted;
    const char *v;

    eg_set_init(&fronted);
    v = eg_route(in, name, &fronted);
    eg_set_free(&fronted);
    free(name);
    return json_string(v);
}

/* ------------------------------------------------------------------ */
/* provenance                                                         */
/* ------------------------------------------------------------------ */

/* `_session_ids` expansion over one platform's published origin entries:
 * the id itself, plus the parent recovered through the STRUCTURED thread_id
 * (never by splitting on ':'). */
static void eg_add_session_ids(const json_t *entries, eg_set *out)
{
    size_t i, n = json_is_array(entries) ? json_array_size(entries) : 0;
    for (i = 0; i < n; i++) {
        json_t *entry = json_get(entries, i);
        json_t *idv, *tidv;
        char *raw, *tid;
        if (!json_is_object(entry)) continue;
        idv = json_obj_get(entry, "id");
        if (!eg_truthy(idv)) continue;
        raw = eg_str(idv);
        if (!raw) continue;
        eg_set_add(out, raw);
        tidv = json_obj_get(entry, "thread_id");
        if (eg_truthy(tidv)) {
            tid = eg_str(tidv);
            if (tid) {
                size_t rl = strlen(raw), tl = strlen(tid);
                if (rl > tl + 1 && raw[rl - tl - 1] == ':' && !strcmp(raw + rl - tl, tid)) {
                    char *parent = eg_dup(raw, rl - tl - 1);
                    if (parent) {
                        eg_set_add(out, parent);
                        free(parent);
                    }
                }
                free(tid);
            }
        }
        free(raw);
    }
}

/* PoP: w34_egress_308cfb_session_ids @ gateway/relay/egress.py:_session_ids */
json_t *w34_egress_308cfb_session_ids(const json_t *entries)
{
    eg_set ids;
    json_t *out;
    eg_set_init(&ids);
    eg_add_session_ids(entries, &ids);
    out = eg_set_json(&ids);
    eg_set_free(&ids);
    return out;
}

/* The union for one normalized platform name. On the generic relay plane the
 * union spans every platform the connector fronts - the same source
 * `relay_routed_platform` reads, so it is loaded here rather than borrowed from
 * the routing call (which short-circuits the generic plane). */
static json_t *eg_attested(const json_t *in, const char *name)
{
    eg_set ids, names, fronted;
    json_t *sources = json_obj_get(in, "sources");
    json_t *out;
    size_t i;

    eg_set_init(&ids);
    eg_set_init(&names);
    eg_set_init(&fronted);
    eg_set_add(&names, name);
    if (name && !strcmp(name, "relay")) {
        eg_fronted(in, &fronted);
        for (i = 0; i < fronted.n; i++) eg_set_add(&names, fronted.v[i]);
    }

    for (i = 0; i < names.n; i++) {
        const char *cand = names.v[i];
        json_t *src = json_is_object(sources) ? json_obj_get(sources, cand) : NULL;
        json_t *home, *dir;
        if (!json_is_object(src)) continue;

        home = json_obj_get(src, "home");
        if (eg_truthy(home)) {
            char *h = eg_str(home);
            if (h) {
                eg_set_add(&ids, h);
                free(h);
            }
        }

        dir = json_obj_get(src, "directory");
        if (json_is_array(dir)) {
            size_t j, dn = json_array_size(dir);
            for (j = 0; j < dn; j++) {
                json_t *entry = json_get(dir, j);
                json_t *idv;
                if (!json_is_object(entry)) continue;
                idv = json_obj_get(entry, "id");
                if (!eg_truthy(idv)) continue;
                {
                    char *s = eg_str(idv);
                    if (s) {
                        eg_set_add(&ids, s);
                        free(s);
                    }
                }
            }
        }

        eg_add_session_ids(json_obj_get(src, "sessions"), &ids);
    }

    out = eg_set_json(&ids);
    eg_set_free(&ids);
    eg_set_free(&names);
    eg_set_free(&fronted);
    return out;
}

/* PoP: w34_egress_308cfb_attested_relay_targets @ gateway/relay/egress.py:attested_relay_targets */
json_t *w34_egress_308cfb_attested_relay_targets(const json_t *in)
{
    char *name = eg_norm(json_obj_get(in, "platform"), 1);
    json_t *out;

    if (name && !strcmp(name, "relay") && json_get_bool(in, "route_unknown", 0)) {
        free(name);
        return json_string("unknown"); /* RelayRouteUnknown */
    }
    out = eg_attested(in, name);
    free(name);
    return out;
}

/* ------------------------------------------------------------------ */
/* the guard                                                          */
/* ------------------------------------------------------------------ */

/* The handle exemption, with the platform name the caller already normalized. */
static int eg_unresolved(const char *platform, const char *target, const json_t *in)
{
    if (!platform || strcmp(platform, "telegram")) return 0; /* no other platform */
    if (!target || target[0] != '@') return 0;               /* a resolved id */
    if (json_get_bool(in, "native_token_present", 0))
        return eg_truthy(json_obj_get(in, "native_token")) ? 0 : 1;
    return eg_truthy(json_obj_get(in, "native_credential")) ? 0 : 1;
}

/* PoP: w34_egress_308cfb_is_unresolved_handle @ gateway/relay/egress.py:_is_unresolved_handle */
int w34_egress_308cfb_is_unresolved_handle(const json_t *in)
{
    return eg_unresolved(json_get_str(in, "platform", ""),
                         json_get_str(in, "target", NULL), in);
}

/* PoP: w34_egress_308cfb_authorize_relay_target @ gateway/relay/egress.py:authorize_relay_target */
json_t *w34_egress_308cfb_authorize_relay_target(const json_t *in)
{
    json_t *platform_v = json_obj_get(in, "platform");
    char *platform = eg_str(platform_v);            /* raw, as the refusal prints it */
    char *name = eg_norm(platform_v, 1);
    char *target = eg_norm(json_obj_get(in, "chat_id"), 0);
    eg_set fronted;
    const char *route;
    json_t *verdict = NULL;

    eg_set_init(&fronted);
    route = eg_route(in, name, &fronted);

    if (!strcmp(route, "unknown")) {
        size_t need = strlen(platform ? platform : "") + 160;
        char *buf = (char *)malloc(need);
        if (buf) {
            snprintf(buf, need,
                     "Refusing to send to '%s': relay routing could not be determined, "
                     "so this destination could not be verified.",
                     platform ? platform : "");
            verdict = json_string(buf);
            free(buf);
        }
        goto done;
    }
    if (!strcmp(route, "false")) { /* not relay-routed: this guard does not apply */
        verdict = json_null();
        goto done;
    }
    if (!target || !target[0]) { /* empty destination: nothing to authorize */
        verdict = json_null();
        goto done;
    }

    {
        json_t *attested = eg_attested(in, name);
        eg_set ids;
        int has;

        eg_set_init(&ids);
        if (json_is_array(attested)) {
            size_t i, n = json_array_size(attested);
            for (i = 0; i < n; i++) {
                const char *s = json_string_value_safe(json_get(attested, i));
                if (s) eg_set_add(&ids, s);
            }
        }
        json_free(attested);

        has = eg_set_has(&ids, target);
        if (has) {
            char *thread = eg_norm(json_obj_get(in, "thread_id"), 0);
            if (thread && thread[0] && strcmp(thread, target)) {
                size_t tlen = strlen(target) + strlen(thread) + 2;
                size_t need = tlen + strlen(name ? name : "") + 320;
                char *bound = (char *)malloc(tlen);
                char *buf = (char *)malloc(need);
                int bound_ok = 0;
                if (bound) {
                    snprintf(bound, tlen, "%s:%s", target, thread);
                    bound_ok = eg_set_has(&ids, bound);
                }
                if (bound_ok) {
                    verdict = json_null();
                } else if (buf) {
                    snprintf(buf, need,
                             "Refusing to send to relay target '%s:%s:%s': "
                             "the parent chat is attested but this gateway has no record of "
                             "the thread, and on this platform the thread IS the delivery "
                             "destination.",
                             name ? name : "", target, thread);
                    verdict = json_string(buf);
                }
                free(bound);
                free(buf);
            } else {
                verdict = json_null();
            }
            free(thread);
        }
        eg_set_free(&ids);

        if (has) goto done; /* the chat (or its bound thread) decided */

        if (eg_unresolved(name, target, in)) {
            /* deferred to the connector's egress floor, one layer out */
            verdict = json_null();
        } else {
            size_t need = strlen(name ? name : "") + strlen(target) + 220;
            char *buf = (char *)malloc(need);
            if (buf) {
                snprintf(buf, need,
                         "Refusing to send to unattested relay target '%s:%s': "
                         "this gateway has no record of that destination. Use "
                         "`hermes send --list` to see the targets it can reach.",
                         name ? name : "", target);
                verdict = json_string(buf);
                free(buf);
            }
        }
    }

done:
    eg_set_free(&fronted);
    free(platform);
    free(name);
    free(target);
    return verdict ? verdict : json_null();
}
