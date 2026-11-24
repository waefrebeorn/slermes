/*
 * webhook_coalesce.c -- real C11 port of the pure half of upstream Hermes
 *   gateway/platforms/webhook_coalesce.py.
 *
 * PoP: w20_webhookcoa_e2fcee_validate @ gateway/platforms/webhook_coalesce.py:validate_coalesce_config
 * PoP: w20_webhookcoa_e2fcee_prompt_with_note @ gateway/platforms/webhook_coalesce.py:PendingEvent.prompt_with_note
 * PoP: w20_webhookcoa_e2fcee_placeholder_present @ gateway/platforms/webhook_coalesce.py:_PLACEHOLDER_RE
 * PoP: w20_webhookcoa_e2fcee_group_key @ gateway/platforms/webhook_coalesce.py:group_key
 * PoP: w20_webhookcoa_e2fcee_normalize_template @ gateway/platforms/webhook_coalesce.py:group_key
 * PoP: w20_webhookcoa_e2fcee_enqueue_plan @ gateway/platforms/webhook_coalesce.py:enqueue
 *
 * The asyncio side (self._timers, asyncio.create_task, flush's gather) and the
 * adapter's render()/dispatch() callbacks stay upstream: render is the seam the
 * group key is built from, so the rendered string is an argument here.
 */
#include "hermes_json.h"
#include "noc_w20_webhookcoa_e2fcee.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WCEE_DEFAULT_WINDOW_SECONDS 30.0
#define WCEE_DEFAULT_MAX_WAIT_SECONDS 300.0

static char *wcee_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *wcee_dupn(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Python truthiness for a decoded value (absent == None). */
static int wcee_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0;
    }
    return 0;
}

static int wcee_is_py_int(double d) {
    if (!isfinite(d)) return 0;
    if (floor(d) != d) return 0;
    return d >= -9.007199254740992e15 && d <= 9.007199254740992e15;
}

/* repr() for the scalar shapes a coalesce block carries. */
static char *wcee_repr(const json_t *v) {
    char buf[64];
    if (!v) return wcee_dup("None");
    switch (v->type) {
    case JSON_NULL: return wcee_dup("None");
    case JSON_BOOL: return wcee_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: {
        size_t n = strlen(v->str_val ? v->str_val : "");
        char *out = (char *)malloc(n + 3);
        if (!out) return NULL;
        out[0] = '\'';
        memcpy(out + 1, v->str_val ? v->str_val : "", n);
        out[n + 1] = '\'';
        out[n + 2] = '\0';
        return out;
    }
    case JSON_NUMBER:
        if (wcee_is_py_int(v->num_val)) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return wcee_dup(buf);
    default: {
        char *s = json_dumps(v, 0);
        char *out = wcee_dup(s ? s : "None");
        free(s);
        return out;
    }
    }
}

/* str.strip() */
static char *wcee_strip(const char *s) {
    size_t a = 0, b;
    if (!s) return wcee_dup("");
    b = strlen(s);
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r' ||
                     s[a] == '\f' || s[a] == '\v')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' ||
                     s[b - 1] == '\r' || s[b - 1] == '\f' || s[b - 1] == '\v')) b--;
    return wcee_dupn(s + a, b - a);
}

/* --- validate_coalesce_config ---------------------------------------------- */

static int wcee_is_number(const json_t *v) {
    return v && v->type == JSON_NUMBER;
}

char *w20_webhookcoa_e2fcee_validate(const char *route_name, const json_t *route) {
    static const char *const EXCLUSIVE[] = {"deliver_only", "cron_job", NULL};
    static const char *const NUMERIC[] = {"window_seconds", "max_wait_seconds", NULL};
    const json_t *coalesce;
    int i;

    if (!route_name) route_name = "";
    coalesce = json_obj_get(route, "coalesce");
    if (!coalesce || coalesce->type == JSON_NULL) return NULL;   /* route.get("coalesce") is None */

    for (i = 0; EXCLUSIVE[i]; i++) {
        if (wcee_truthy(json_obj_get(route, EXCLUSIVE[i]))) {
            const char *head = "[webhook] Route '";
            const char *mid = "' combines ";
            const char *mid2 = " with coalesce. Coalescing only applies to agent-mode routes.";
            size_t len = strlen(head) + strlen(route_name) + strlen(mid) + strlen(EXCLUSIVE[i]) +
                         strlen(mid2) + 1;
            char *msg = (char *)malloc(len);
            if (!msg) return NULL;
            snprintf(msg, len, "%s%s%s%s%s", head, route_name, mid, EXCLUSIVE[i], mid2);
            return msg;
        }
    }

    {
        const json_t *key = (coalesce->type == JSON_OBJECT) ? json_obj_get(coalesce, "key") : NULL;
        int key_ok = 0;
        if (key && key->type == JSON_STRING) {
            char *stripped = wcee_strip(key->str_val);
            key_ok = stripped && stripped[0];
            free(stripped);
        }
        if (!key_ok) {
            const char *head = "[webhook] Route '";
            const char *mid = "' has a coalesce block without a non-empty string 'key'. Set "
                              "coalesce.key to a payload field (e.g. 'pull_request.number') or a "
                              "template (e.g. '{repository.full_name}#{number}').";
            size_t len = strlen(head) + strlen(route_name) + strlen(mid) + 1;
            char *msg = (char *)malloc(len);
            if (!msg) return NULL;
            snprintf(msg, len, "%s%s%s", head, route_name, mid);
            return msg;
        }
    }

    for (i = 0; NUMERIC[i]; i++) {
        const json_t *value = (coalesce->type == JSON_OBJECT) ? json_obj_get(coalesce, NUMERIC[i])
                                                              : NULL;
        int bad = 0;
        if (value && value->type != JSON_NULL) {
            if (value->type == JSON_BOOL) bad = 1;           /* isinstance(value, bool) */
            else if (!wcee_is_number(value)) bad = 1;
            else if (value->num_val <= 0.0) bad = 1;
        }
        if (bad) {
            char *shown = wcee_repr(value);
            const char *head = "[webhook] Route '";
            const char *mid = "' coalesce.";
            const char *mid2 = " must be a positive number, got ";
            const char *tail = ".";
            size_t len = strlen(head) + strlen(route_name) + strlen(mid) + strlen(NUMERIC[i]) +
                         strlen(mid2) + (shown ? strlen(shown) : 6) + strlen(tail) + 1;
            char *msg = (char *)malloc(len);
            if (!msg) { free(shown); return NULL; }
            snprintf(msg, len, "%s%s%s%s%s%s%s", head, route_name, mid, NUMERIC[i], mid2,
                     shown ? shown : "None", tail);
            free(shown);
            return msg;
        }
    }
    return NULL;
}

/* --- PendingEvent.prompt_with_note ----------------------------------------- */

char *w20_webhookcoa_e2fcee_prompt_with_note(const json_t *event) {
    const char *prompt = event ? json_get_str(event, "prompt", "") : "";
    double count = event ? json_get_num(event, "count", 0.0) : 0.0;
    static const char *const NOTE =
        "\n\n(Note: %lld webhook events for this item arrived in quick succession and were "
        "coalesced \xe2\x80\x94 this is the most recent one; earlier events are superseded.)";
    char *out;
    size_t cap;
    if (count <= 1.0) return wcee_dup(prompt);
    cap = strlen(prompt) + strlen(NOTE) + 32;
    out = (char *)malloc(cap);
    if (!out) return NULL;
    snprintf(out, cap, "%s\n\n(Note: %lld webhook events for this item arrived in quick succession "
                       "and were coalesced \xe2\x80\x94 this is the most recent one; earlier events "
                       "are superseded.)",
             prompt, (long long)count);
    return out;
}

/* --- _PLACEHOLDER_RE ------------------------------------------------------- */

static int wcee_re_class(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '.';
}

int w20_webhookcoa_e2fcee_placeholder_present(const char *s) {
    size_t i;
    if (!s) return 0;
    for (i = 0; s[i]; i++) {
        if (s[i] != '{') continue;
        {
            size_t j = i + 1;
            while (s[j] && wcee_re_class((unsigned char)s[j])) j++;
            if (j > i + 1 && s[j] == '}') return 1;
        }
    }
    return 0;
}

char *w20_webhookcoa_e2fcee_group_key(const char *route_name, const char *rendered) {
    char *out;
    size_t len;
    if (!route_name) route_name = "";
    if (!rendered) rendered = "";
    if (w20_webhookcoa_e2fcee_placeholder_present(rendered)) return NULL;
    len = strlen(route_name) + strlen(rendered) + 2;
    out = (char *)malloc(len);
    if (!out) return NULL;
    snprintf(out, len, "%s|%s", route_name, rendered);
    return out;
}

char *w20_webhookcoa_e2fcee_normalize_template(const char *key_template) {
    char *stripped = wcee_strip(key_template);
    char *out;
    size_t len;
    if (!stripped) return NULL;
    if (strchr(stripped, '{')) return stripped;
    len = strlen(stripped) + 3;
    out = (char *)malloc(len);
    if (!out) { free(stripped); return NULL; }
    snprintf(out, len, "{%s}", stripped);
    free(stripped);
    return out;
}

/* --- enqueue --------------------------------------------------------------- */

json_t *w20_webhookcoa_e2fcee_enqueue_plan(const json_t *spec) {
    json_t *out = json_object();
    json_t *existing;
    char *group_key;
    const char *rendered = spec ? json_get_str(spec, "rendered", "") : "";
    double now = spec ? json_get_num(spec, "now", 0.0) : 0.0;
    double window, max_wait, first_at, delay;
    long long count;

    group_key = w20_webhookcoa_e2fcee_group_key(spec ? json_get_str(spec, "route_name", "") : "",
                                                rendered);
    if (!group_key) {
        /* the key did not resolve: the caller dispatches immediately */
        json_set(out, "queued", json_bool(0));
        json_set(out, "first_at", json_null());
        json_set(out, "count", json_null());
        json_set(out, "superseded", json_null());
        json_set(out, "delay", json_null());
        return out;
    }
    free(group_key);

    existing = spec ? json_obj_get(spec, "existing") : NULL;
    if (existing && existing->type != JSON_OBJECT) existing = NULL;
    {
        /* float(coalesce.get("window_seconds", DEFAULT)) -- default only when absent */
        const json_t *w = spec ? json_obj_get(spec, "window") : NULL;
        const json_t *m = spec ? json_obj_get(spec, "max_wait") : NULL;
        window = (w && w->type == JSON_NUMBER) ? w->num_val : WCEE_DEFAULT_WINDOW_SECONDS;
        max_wait = (m && m->type == JSON_NUMBER) ? m->num_val : WCEE_DEFAULT_MAX_WAIT_SECONDS;
    }

    first_at = existing ? json_get_num(existing, "first_at", 0.0) : now;
    count = existing ? (long long)json_get_num(existing, "count", 0.0) + 1 : 1;

    json_set(out, "queued", json_bool(1));
    json_set(out, "first_at", json_number(first_at));
    json_set(out, "count", json_number((double)count));
    {
        json_t *sup = json_array();
        if (existing) {
            const json_t *old = json_obj_get(existing, "superseded");
            size_t i;
            if (old && old->type == JSON_ARRAY) {
                for (i = 0; i < old->c.count; i++) json_append(sup, json_copy(old->c.items[i]));
            }
            {
                const json_t *prev_id = json_obj_get(existing, "delivery_id");
                if (prev_id) json_append(sup, json_copy(prev_id));
            }
        }
        json_set(out, "superseded", sup);
    }
    delay = first_at + max_wait - now;
    if (delay < 0.0) delay = 0.0;
    if (delay > window) delay = window;
    json_set(out, "delay", json_number(delay));
    return out;
}
