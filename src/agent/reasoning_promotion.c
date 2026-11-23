/*
 * reasoning_promotion.c — C home for upstream Hermes agent/reasoning_promotion.py.
 *
 * Faithful C11 port of the trusted-route decision.  The live Python module
 * reads the route on every call and consults four external seams; the oracle
 * injects each seam result from the fixture, and the same values are consumed
 * here.  custom_provider_slug is reproduced verbatim from hermes_cli.providers.
 *
 * PoP: w11_reasoningp_f536f5_capability @ agent/reasoning_promotion.py:answer_in_reasoning_capability
 */
#include "noc_w11_reasoningp_f536f5.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define ANSWER_CAP "answer_in_reasoning"
#define NEMOTRON_MARKER "nemotron-3.5-lightning"

/* Python str(x or "") — no strip/lower. */
static void raw_str(json_t *v, char *out, size_t cap) {
    const char *s = json_is_string(v) ? json_string_value(v) : NULL;
    size_t n = s ? strlen(s) : 0;
    if (n >= cap) n = cap - 1;
    if (n) memcpy(out, s, n);
    out[n] = '\0';
}

/* Python str(x or "").strip().lower() (ASCII whitespace / case in fixtures). */
static void strip_lower(json_t *v, char *out, size_t cap) {
    char tmp[1024];
    raw_str(v, tmp, sizeof(tmp));
    const char *a = tmp, *b = tmp + strlen(tmp);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    if (n >= cap) n = cap - 1;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)a[i]);
    out[n] = '\0';
}

/* Same strip().lower() on a plain C string. */
static void strip_lower_c(const char *s, char *out, size_t cap) {
    const char *a = s ? s : "", *b = a + strlen(a);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    if (n >= cap) n = cap - 1;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)a[i]);
    out[n] = '\0';
}

/* Verbatim port of hermes_cli.providers.custom_provider_slug. */
static void provider_slug(const char *name, const char *key, char *out, size_t cap) {
    char id[1024];
    strip_lower_c(key ? key : "", id, sizeof(id));
    if (!id[0]) strip_lower_c(name ? name : "", id, sizeof(id));
    size_t n = 0;
    for (const char *p = id; *p && n + 1 < cap; p++)
        out[n++] = (*p == ' ') ? '-' : *p;   /* already lowercased */
    out[n] = '\0';
    if (strncmp(out, "custom:", 7) != 0) {
        char tmp[1080];
        snprintf(tmp, sizeof(tmp), "custom:%s", out);
        snprintf(out, cap, "%s", tmp);
    }
}

/* PoP: w11_reasoningp_f536f5_capability @ agent/reasoning_promotion.py:answer_in_reasoning_capability */
json_t *w11_reasoningp_f536f5_capability(json_t *a) {
    char base_url[1024], provider[256], api_mode[128], model[1024], requested[256];
    raw_str(json_obj_get(a, "base_url"), base_url, sizeof(base_url));
    raw_str(json_obj_get(a, "model"), model, sizeof(model));
    strip_lower(json_obj_get(a, "provider"), provider, sizeof(provider));
    strip_lower(json_obj_get(a, "api_mode"), api_mode, sizeof(api_mode));
    strip_lower(json_obj_get(a, "requested_provider"), requested, sizeof(requested));

    if (!strcmp(provider, "openrouter") ||
        json_get_bool(a, "openrouter_host_match", false)) return json_bool(false);
    if (api_mode[0] && strcmp(api_mode, "chat_completions")) return json_bool(false);

    json_t *configured = json_obj_get(a, "configured");
    if (configured && configured->type == JSON_BOOL)
        return json_bool(configured->bool_val);

    /* ids = {custom_provider_slug(p) for p in (provider, requested)
     *        if p.startswith("custom:")} */
    char ids[2][256];
    size_t nids = 0;
    const char *provs[2] = {provider, requested};
    for (int i = 0; i < 2; i++) {
        if (!strncmp(provs[i], "custom:", 7) && nids < 2)
            provider_slug(provs[i], NULL, ids[nids++], sizeof(ids[0]));
    }

    json_t *entries = json_obj_get(a, "entries");
    size_t nentries = json_is_array(entries) ? json_len(entries) : 0;
    /* live = [e for e in entries if slug(e.name, e.provider_key) in ids];
     * flags consider (live or entries). */
    int saw_true = 0, saw_false = 0, saw_live = 0;
    json_t *pass1[512];
    size_t np1 = 0;
    if (nentries <= 512) {
        for (size_t i = 0; i < nentries; i++) {
            json_t *e = json_get(entries, i);
            if (!json_is_object(e)) continue;
            char slug[256];
            provider_slug(json_get_str(e, "name", ""), json_get_str(e, "provider_key", ""),
                          slug, sizeof(slug));
            int in = 0;
            for (size_t k = 0; k < nids; k++) if (!strcmp(slug, ids[k])) { in = 1; break; }
            if (in) { pass1[np1++] = e; saw_live = 1; }
        }
    }
    size_t nchosen = saw_live ? np1 : nentries;
    for (size_t i = 0; i < nchosen; i++) {
        json_t *e = saw_live ? pass1[i] : (json_is_array(entries) ? json_get(entries, i) : NULL);
        if (!json_is_object(e)) continue;
        json_t *caps = json_obj_get(e, "capabilities");
        if (!json_is_object(caps)) continue;       /* (e.get("capabilities") or {}).get(...) */
        json_t *v = json_obj_get(caps, ANSWER_CAP);
        if (v && v->type == JSON_BOOL) {
            if (v->bool_val) saw_true = 1; else saw_false = 1;
        }
    }
    if (saw_true || saw_false) return json_bool(saw_true && !saw_false ? true : false);

    char mlower[1024];
    raw_str(json_obj_get(a, "model"), mlower, sizeof(mlower));
    for (char *p = mlower; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (!strstr(mlower, NEMOTRON_MARKER)) return json_bool(false);
    return json_bool(json_get_bool(a, "is_local", false) ? true : false);
}
