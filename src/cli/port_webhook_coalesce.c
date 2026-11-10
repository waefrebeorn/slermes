/*
 * port_webhook_coalesce.c — pure helpers from gateway/platforms/webhook_coalesce.py.
 */

#include "port_webhook_coalesce.h"
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *wc_xstrdup(const char *s, size_t n)
{
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

char *wc_prompt_with_note(const char *prompt, long long count)
{
    static const char NOTE[] =
        "\n\n(Note: %lld webhook events for this item arrived in quick "
        "succession and were coalesced — this is the most recent one; earlier "
        "events are superseded.)";
    size_t plen = prompt ? strlen(prompt) : 0;
    size_t cap = plen + sizeof(NOTE) + 24;
    char *out;
    int n;
    if (count <= 1)
        return wc_xstrdup(prompt ? prompt : "", plen);
    out = (char *)malloc(cap);
    if (!out) return NULL;
    if (prompt)
        memcpy(out, prompt, plen);
    n = snprintf(out + plen, cap - plen, NOTE, (long long)count);
    if (n < 0) { free(out); return NULL; }
    return out;
}

/* _PLACEHOLDER_RE = \{ [a-zA-Z0-9_.]+ \} ; search anywhere. */
static int wc_has_placeholder(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    for (; *p; p++) {
        if (*p != '{') continue;
        const unsigned char *q = p + 1;
        int any = 0;
        while (*q && (isalnum(*q) || *q == '_' || *q == '.')) { q++; any = 1; }
        if (any && *q == '}')
            return 1;
    }
    return 0;
}

char *wc_group_key(const char *route_name, const char *rendered_key)
{
    const char *r = rendered_key ? rendered_key : "";
    size_t rl = strlen(r), rnl = route_name ? strlen(route_name) : 0;
    char *out;
    if (wc_has_placeholder(r))
        return NULL;
    out = (char *)malloc(rnl + 1 + rl + 1);
    if (!out) return NULL;
    if (route_name) memcpy(out, route_name, rnl);
    out[rnl] = '|';
    memcpy(out + rnl + 1, r, rl);
    out[rnl + 1 + rl] = '\0';
    return out;
}

/* Python truthiness of a JSON value (used for the deliver_only / cron_job
 * flags). */
static int wc_truthy(const json_t *n)
{
    if (!n) return 0;
    switch (n->type) {
    case JSON_BOOL:   return n->bool_val ? 1 : 0;
    case JSON_NUMBER: return n->num_val != 0.0;
    case JSON_STRING: return n->str_val && *n->str_val ? 1 : 0;
    default:          return 1; /* non-empty container is truthy */
    }
}

/* Python repr of a JSON scalar for the numeric-field error message. */
static void wc_append_repr(json_t *n, char *buf, size_t bufsz)
{
    if (n && n->type == JSON_BOOL) {
        snprintf(buf, bufsz, "%s", n->bool_val ? "True" : "False");
    } else if (n && n->type == JSON_NUMBER) {
        double v = n->num_val;
        if (v == (double)(long long)v && fabs(v) < 1e15)
            snprintf(buf, bufsz, "%lld", (long long)v);
        else
            snprintf(buf, bufsz, "%.17g", v);
    } else if (n && n->type == JSON_STRING) {
        const char *s = n->str_val ? n->str_val : "";
        /* Python string repr uses single quotes; keep it simple for the
         * fixture domain (no embedded quotes). */
        snprintf(buf, bufsz, "'%s'", s);
    } else {
        snprintf(buf, bufsz, "None");
    }
}

int wc_validate_coalesce_config(const char *route_name, const char *route_json,
                                char **err_msg)
{
    json_t *route = route_json ? json_parse(route_json, NULL) : NULL;
    json_t *coalesce = NULL;
    static const char *const EXCLUSIVES[] = { "deliver_only", "cron_job" };
    static const char *const FIELDS[] = { "window_seconds", "max_wait_seconds" };
    char rname[256];
    size_t i;

    if (err_msg) *err_msg = NULL;
    if (!route || route->type != JSON_OBJECT)
        goto ok_cleanup;
    coalesce = json_obj_get(route, "coalesce");
    if (!coalesce || coalesce->type == JSON_NULL)
        goto ok_cleanup;   /* coalesce None -> return (no check) */

    snprintf(rname, sizeof(rname), "%s", route_name ? route_name : "");

    for (i = 0; i < sizeof(EXCLUSIVES) / sizeof(EXCLUSIVES[0]); i++) {
        if (wc_truthy(json_obj_get(route, EXCLUSIVES[i]))) {
            static const char F1[] =
                "[webhook] Route '%s' combines %s with coalesce. Coalescing "
                "only applies to agent-mode routes.";
            char msg[512];
            snprintf(msg, sizeof(msg), F1, rname, EXCLUSIVES[i]);
            if (err_msg) *err_msg = wc_xstrdup(msg, strlen(msg));
            json_free(route);
            return -1;
        }
    }

    if (coalesce->type != JSON_OBJECT) {
        /* isinstance(coalesce, dict) false -> key is None -> error. */
        goto bad_key;
    }
    {
        json_t *key = json_obj_get(coalesce, "key");
        if (!key || key->type != JSON_STRING || !key->str_val
                || !key->str_val[strspn(key->str_val, " \t")]) {
            goto bad_key;
        }
    }

    for (i = 0; i < sizeof(FIELDS) / sizeof(FIELDS[0]); i++) {
        json_t *v = json_obj_get(coalesce, FIELDS[i]);
        if (!v || v->type == JSON_NULL)
            continue; /* None -> skip */
        int notnum = (v->type == JSON_BOOL) ||
                     (v->type != JSON_NUMBER);
        if (notnum || v->num_val <= 0) {
            static const char F2[] =
                "[webhook] Route '%s' coalesce.%s must be a positive number, "
                "got %s.";
            char repr[96], msg[512];
            wc_append_repr(v, repr, sizeof(repr));
            snprintf(msg, sizeof(msg), F2, rname, FIELDS[i], repr);
            if (err_msg) *err_msg = wc_xstrdup(msg, strlen(msg));
            json_free(route);
            return -3;
        }
    }
    json_free(route);
    return 0;

bad_key:
    {
        static const char F3[] =
            "[webhook] Route '%s' has a coalesce block without a non-empty "
            "string 'key'. Set coalesce.key to a payload field (e.g. "
            "'pull_request.number') or a template (e.g. "
            "'{repository.full_name}#{number}').";
        char msg[512];
        snprintf(msg, sizeof(msg), F3, rname);
        if (err_msg) *err_msg = wc_xstrdup(msg, strlen(msg));
        json_free(route);
        return -2;
    }

ok_cleanup:
    if (route) json_free(route);
    return 0;
}
