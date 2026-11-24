/*
 * shared_metrics.c - C11 port of upstream
 * hermes_cli/web_routers/shared_metrics.py (gate w24_sharedmetr_18600d).
 *
 * PoP: w24_sharedmetr_18600d_answer @ hermes_cli/web_routers/shared_metrics.py:ConsentAnswer
 * PoP: w24_sharedmetr_18600d_consent_view @ hermes_cli/web_routers/shared_metrics.py:_read_consent
 * PoP: w24_sharedmetr_18600d_set_answer @ hermes_cli/observability/shared_metrics_consent.py:set_answer
 * PoP: w24_sharedmetr_18600d_constants @ hermes_cli/observability/shared_metrics_consent.py:OFFER_CHOICES
 *
 * The router is two thin endpoints over one pure core, and that core is what
 * is ported: the request model's two booleans (with the model's own lax
 * coercion and its refusal), the ``{enabled, send, decided, reask}`` state the
 * GET merges a ``managed`` flag into, the ``telemetry.shared_metrics`` write
 * the PUT performs, and the offer's choice table.  The FastAPI router, the
 * ``asyncio.to_thread`` hop, the config file and ``is_managed()`` are seams:
 * the caller passes the raw mapping and the managed flag in, and gets values
 * back.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_sharedmetr_18600d.h"

#define SM_DOCS_URL "https://hermes-agent.nousresearch.com/docs/developer-guide/relay-shared-metrics"

/* Python truthiness of a JSON value. */
static int sm_truthy(const json_t *v) {
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

static int sm_ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* pydantic's lax bool: a real bool, 0/1 (int or float), or the accepted words.
 * Anything else - None, 2, "maybe", a list - is a ValidationError. */
static int sm_lax_bool(const json_t *v, int *out) {
    static const char *const TRUE_WORDS[] = {"1", "on", "t", "true", "y", "yes", NULL};
    static const char *const FALSE_WORDS[] = {"0", "off", "f", "false", "n", "no", NULL};
    int i;
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        if (v->num_val == 0.0) { *out = 0; return 1; }
        if (v->num_val == 1.0) { *out = 1; return 1; }
        return 0;
    }
    if (v->type == JSON_STRING && v->str_val) {
        for (i = 0; TRUE_WORDS[i]; i++)
            if (sm_ieq(v->str_val, TRUE_WORDS[i])) { *out = 1; return 1; }
        for (i = 0; FALSE_WORDS[i]; i++)
            if (sm_ieq(v->str_val, FALSE_WORDS[i])) { *out = 0; return 1; }
    }
    return 0;
}

/* cfg.get("telemetry") when it is a dict, else None. */
static const json_t *sm_telemetry(const json_t *cfg) {
    const json_t *t;
    if (!cfg || cfg->type != JSON_OBJECT) return NULL;
    t = json_obj_get(cfg, "telemetry");
    return t && t->type == JSON_OBJECT ? t : NULL;
}

/* _section(cfg): telemetry.shared_metrics when both are dicts, else {}. */
static const json_t *sm_section(const json_t *cfg) {
    const json_t *t = sm_telemetry(cfg);
    const json_t *s;
    if (!t) return NULL;
    s = json_obj_get(t, "shared_metrics");
    return s && s->type == JSON_OBJECT ? s : NULL;
}

json_t *w24_sharedmetr_18600d_answer(const json_t *req) {
    const json_t *enabled = json_obj_get(req, "enabled");
    const json_t *send = json_obj_get(req, "send");
    json_t *out = json_object();
    int e = 0, s = 0;

    if (!sm_lax_bool(enabled, &e)) {
        json_set(out, "error", json_string("ValidationError"));
        return out;
    }
    if (send) {
        if (!sm_lax_bool(send, &s)) {
            json_set(out, "error", json_string("ValidationError"));
            return out;
        }
    } else {
        /* send: bool = False */
        s = 0;
    }
    json_set(out, "enabled", json_bool(e));
    json_set(out, "send", json_bool(s));
    return out;
}

json_t *w24_sharedmetr_18600d_consent_view(const json_t *req) {
    const json_t *cfg = json_obj_get(req, "config");
    const json_t *section = sm_section(cfg);
    const json_t *enabled_v = section ? json_obj_get(section, "enabled") : NULL;
    const json_t *send_v = section ? json_obj_get(section, "send") : NULL;
    const json_t *version = section ? json_obj_get(section, "offer_version") : NULL;
    int enabled = enabled_v && enabled_v->type == JSON_BOOL && enabled_v->bool_val;
    int answered = enabled_v != NULL || send_v != NULL;
    int version_is_offer;
    int reask;
    int send;
    json_t *out = json_object();

    /* section.get("offer_version") != OFFER_VERSION, with Python equality
     * (bool folds in: True == 1 != 2). */
    if (!version || version->type == JSON_NULL) {
        version_is_offer = 0;
    } else if (version->type == JSON_BOOL) {
        version_is_offer = (version->bool_val ? 1.0 : 0.0) == (double)W24_SHAREDMETR_18600D_OFFER_VERSION;
    } else if (version->type == JSON_NUMBER) {
        version_is_offer = version->num_val == (double)W24_SHAREDMETR_18600D_OFFER_VERSION;
    } else {
        version_is_offer = 0;
    }
    reask = answered && !enabled && !version_is_offer;
    send = enabled && send_v && send_v->type == JSON_BOOL && send_v->bool_val;
    json_set(out, "enabled", json_bool(enabled));
    json_set(out, "send", json_bool(send));
    json_set(out, "decided", json_bool(answered && !reask));
    json_set(out, "reask", json_bool(reask));
    json_set(out, "managed", json_bool(sm_truthy(json_obj_get(req, "managed"))));
    return out;
}

json_t *w24_sharedmetr_18600d_set_answer(const json_t *req) {
    const json_t *target = json_obj_get(req, "target");
    int enabled = sm_truthy(json_obj_get(req, "enabled"));
    int send = enabled && sm_truthy(json_obj_get(req, "send"));
    json_t *out = target ? json_copy(target) : json_object();
    json_t *telemetry, *section;

    if (!out || out->type != JSON_OBJECT) {
        json_free(out);
        out = json_object();
    }
    telemetry = json_obj_get(out, "telemetry");
    if (!telemetry || telemetry->type != JSON_OBJECT) {
        json_set(out, "telemetry", json_object());
        telemetry = json_obj_get(out, "telemetry");
    }
    section = json_obj_get(telemetry, "shared_metrics");
    if (!section || section->type != JSON_OBJECT) {
        json_set(telemetry, "shared_metrics", json_object());
        section = json_obj_get(telemetry, "shared_metrics");
    }
    json_set(section, "enabled", json_bool(enabled));
    json_set(section, "send", json_bool(send));
    json_set(section, "offer_version", json_number(W24_SHAREDMETR_18600D_OFFER_VERSION));
    return out;
}

json_t *w24_sharedmetr_18600d_constants(void) {
    json_t *o = json_object();
    json_t *choices = json_array();
    json_t *share = json_array();
    json_t *no = json_array();

    json_append(share, json_string("Share with Nous"));
    json_append(share, json_bool(1));
    json_append(share, json_bool(1));
    json_append(no, json_string("No thanks"));
    json_append(no, json_bool(0));
    json_append(no, json_bool(0));
    json_append(choices, share);
    json_append(choices, no);
    json_set(o, "offer_version", json_number(W24_SHAREDMETR_18600D_OFFER_VERSION));
    json_set(o, "choices", choices);
    json_set(o, "no_thanks", json_number(W24_SHAREDMETR_18600D_NO_THANKS));
    json_set(o, "docs_url", json_string(SM_DOCS_URL));
    return o;
}
