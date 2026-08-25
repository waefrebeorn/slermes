/*
 * port_agent_error_classifier.c — C port of agent/error_classifier.c
 */

#include "hermes_logger.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: cli_agent_error_classifier_is_auth @ agent/error_classifier.py:is_auth */

/* Port of Python agent/error_classifier.py:is_auth */
/* Check if the error reason is an authentication error.
 * Note: The PoP annotation maps to error_is_auth in the header. */
int cli_agent_error_classifier_is_auth(const char *reason)
{
    if (!reason) return 0;
    /* Check for auth-related failover reasons */
    if (strcmp(reason, "auth") == 0 || strcmp(reason, "auth_permanent") == 0) {
        return 1;
    }
    return 0;
}

/* PoP: cli_agent_error_classifier__extract_status_code @ agent/error_classifier.py:_extract_status_code */

/* Port of Python agent/error_classifier.py:_extract_status_code */
/* Walk the error and its cause chain to find an HTTP status code. */
/* In C, we extract from a JSON error representation. */
int cli_agent_error_classifier__extract_status_code(const char *error_json)
{
    if (!error_json) return 0;

    /* Look for "status_code" or "status" in the JSON error */
    const char *key;

    key = strstr(error_json, "\"status_code\"");
    if (key) {
        key += 13; /* skip '"status_code"' */
        while (*key == ' ' || *key == '\t' || *key == ':') key++;
        int code = atoi(key);
        if (code >= 100 && code < 600) return code;
    }

    key = strstr(error_json, "\"status\"");
    if (key) {
        key += 8; /* skip '"status"' */
        while (*key == ' ' || *key == '\t' || *key == ':') key++;
        int code = atoi(key);
        if (code >= 100 && code < 600) return code;
    }

    return 0; /* not found */
}

/* PoP: cli_agent_error_classifier__extract_error_body @ agent/error_classifier.py:_extract_error_body */

/* Port of Python agent/error_classifier.py:_extract_error_body */
/* Extract the structured error body from an SDK exception. */
char *cli_agent_error_classifier__extract_error_body(const char *error_json)
{
    if (!error_json) {
        return strdup("{}");
    }

    /* Try to extract the "body" field from JSON */
    const char *body = strstr(error_json, "\"body\"");
    if (body) {
        body += 6; /* skip '"body"' */
        while (*body == ' ' || *body == '\t' || *body == ':') body++;
        if (*body == '{') {
            /* Find matching closing brace */
            int depth = 0;
            const char *end = body;
            for (; *end; end++) {
                if (*end == '{') depth++;
                else if (*end == '}') {
                    depth--;
                    if (depth == 0) { end++; break; }
                }
            }
            size_t len = (size_t)(end - body);
            char *result = (char *)malloc(len + 1);
            if (result) {
                memcpy(result, body, len);
                result[len] = '\0';
                return result;
            }
        }
    }

    /* Try response.json() equivalent: look for "error" field */
    const char *err = strstr(error_json, "\"error\"");
    if (err) {
        err += 7; /* skip '"error"' */
        while (*err == ' ' || *err == '\t' || *err == ':') err++;
        if (*err == '{') {
            int depth = 0;
            const char *end = err;
            for (; *end; end++) {
                if (*end == '{') depth++;
                else if (*end == '}') {
                    depth--;
                    if (depth == 0) { end++; break; }
                }
            }
            size_t len = (size_t)(end - err);
            char *result = (char *)malloc(len + 3);
            if (result) {
                result[0] = '{';
                memcpy(result + 1, err, len);
                result[len + 1] = '}';
                result[len + 2] = '\0';
                return result;
            }
        }
    }

    return strdup("{}");
}

/* PoP: cli_agent_error_classifier__extract_message @ agent/error_classifier.py:_extract_message */

/* Port of Python agent/error_classifier.py:_extract_message */
/* Extract the most informative error message. */
char *cli_agent_error_classifier__extract_message(const char *error_json)
{
    if (!error_json) {
        return strdup("Unknown error");
    }

    /* Try structured body->error->message first */
    char *body = cli_agent_error_classifier__extract_error_body(error_json);
    if (body) {
        const char *err = strstr(body, "\"error\"");
        if (err) {
            err += 7;
            while (*err == ' ' || *err == '\t' || *err == ':') err++;
            if (*err == '{') {
                const char *msg = strstr(err, "\"message\"");
                if (msg) {
                    msg += 9;
                    while (*msg == ' ' || *msg == '\t' || *msg == ':') msg++;
                    if (*msg == '"') {
                        msg++;
                        /* Find closing quote */
                        const char *end = msg;
                        while (*end && *end != '"') end++;
                        size_t len = (size_t)(end - msg);
                        if (len > 500) len = 500;
                        char *result = (char *)malloc(len + 1);
                        if (result) {
                            memcpy(result, msg, len);
                            result[len] = '\0';
                            free(body);
                            return result;
                        }
                    }
                }
            }
        }

        /* Try body->message */
        const char *msg = strstr(body, "\"message\"");
        if (msg) {
            msg += 9;
            while (*msg == ' ' || *msg == '\t' || *msg == ':') msg++;
            if (*msg == '"') {
                msg++;
                const char *end = msg;
                while (*end && *end != '"') end++;
                size_t len = (size_t)(end - msg);
                if (len > 500) len = 500;
                char *result = (char *)malloc(len + 1);
                if (result) {
                    memcpy(result, msg, len);
                    result[len] = '\0';
                    free(body);
                    return result;
                }
            }
        }

        free(body);
    }

    /* Fallback: return first 500 chars of error_json */
    size_t len = strlen(error_json);
    if (len > 500) len = 500;
    char *result = (char *)malloc(len + 1);
    if (result) {
        memcpy(result, error_json, len);
        result[len] = '\0';
        return result;
    }

    return strdup("Unknown error");
}

/* PoP: cli_agent_error_classifier__is_openrouter_upstream_error @ agent/error_classifier.py:_is_openrouter_upstream_error */

/* Port of Python agent/error_classifier.py:_is_openrouter_upstream_error */
/* Detect OpenRouter's aggregator-wrapped upstream provider errors.
 * body is a JSON object string; provider is the configured provider slug. */
int cli_agent_error_classifier__is_openrouter_upstream_error(
    const char *body_json, const char *provider)
{
    if (!body_json) return 0;

    /* Cheap prefix check: body must contain an "error" object. */
    const char *err = strstr(body_json, "\"error\"");
    if (!err) return 0;
    /* The "error" we want must be an object (followed by '{'). A bare
     * "error_code"/"error_message" key would be a false positive, but the
     * Python check requires err to be a dict, so we require a '{'. */
    const char *p = err + 7; /* skip '"error"' */
    while (*p == ' ' || *p == '\t' || *p == ':') p++;
    if (*p != '{') return 0;

    /* Walk to the error object's "message" field. */
    const char *msg = strstr(p, "\"message\"");
    if (!msg) return 0;
    msg += 9; /* skip '"message"' */
    while (*msg == ' ' || *msg == '\t' || *msg == ':') msg++;
    if (*msg != '"') return 0;
    msg++; /* skip opening quote */

    /* Extract the outer message, lower-cased for comparison. */
    char buf[512];
    size_t i = 0;
    while (*msg && *msg != '"' && i + 1 < sizeof(buf)) {
        buf[i++] = (char)tolower((unsigned char)*msg);
        msg++;
    }
    buf[i] = '\0';
    if (strcmp(buf, "provider returned error") != 0) {
        return 0;
    }

    /* Require either the explicit OpenRouter provider OR the metadata shape. */
    const char *provider_lower = provider ? provider : "";
    /* lower-case the provider slug */
    char pl[128];
    size_t j = 0;
    for (; provider_lower[j] && j + 1 < sizeof(pl); j++) {
        pl[j] = (char)tolower((unsigned char)provider_lower[j]);
    }
    pl[j] = '\0';
    if (strcmp(pl, "openrouter") == 0) {
        return 1;
    }

    /* Look for metadata with "raw" or "provider_name". */
    const char *metadata = strstr(p, "\"metadata\"");
    if (metadata) {
        metadata += 10; /* skip '"metadata"' */
        while (*metadata == ' ' || *metadata == '\t' || *metadata == ':') metadata++;
        if (*metadata == '{') {
            const char *end = metadata;
            int depth = 0;
            for (; *end; end++) {
                if (*end == '{') depth++;
                else if (*end == '}') {
                    depth--;
                    if (depth == 0) { end++; break; }
                }
            }
            size_t meta_len = (size_t)(end - metadata);
            /* Check for "raw" or "provider_name" inside the metadata object. */
            if (meta_len > 0) {
                char *meta = (char *)malloc(meta_len + 1);
                if (meta) {
                    memcpy(meta, metadata, meta_len);
                    meta[meta_len] = '\0';
                    int found = (strstr(meta, "\"raw\"") != NULL)
                                || (strstr(meta, "\"provider_name\"") != NULL);
                    free(meta);
                    if (found) return 1;
                }
            }
        }
    }
    return 0;
}

/* PoP: string @ agent/error_classifier.py:_extract_upstream_provider_name */

/* Port of Python agent/error_classifier.py:_extract_upstream_provider_name */
/* Pull the upstream provider name out of OpenRouter's error metadata.
 * Returns a malloc'd string (caller frees) or NULL. */
char *cli_agent_error_classifier__extract_upstream_provider_name(
    const char *body_json)
{
    if (!body_json) return NULL;
    const char *err = strstr(body_json, "\"error\"");
    if (!err) return NULL;
    err += 7;
    while (*err == ' ' || *err == '\t' || *err == ':') err++;
    if (*err != '{') return NULL;

    /* Find metadata object. */
    const char *metadata = strstr(err, "\"metadata\"");
    if (!metadata) return NULL;
    metadata += 10;
    while (*metadata == ' ' || *metadata == '\t' || *metadata == ':') metadata++;
    if (*metadata != '{') return NULL;

    /* Find provider_name inside metadata. */
    const char *pn = strstr(metadata, "\"provider_name\"");
    if (!pn) return NULL;
    pn += 15;
    while (*pn == ' ' || *pn == '\t' || *pn == ':') pn++;
    if (*pn != '"') return NULL;
    pn++; /* skip opening quote */
    char buf[256];
    size_t i = 0;
    while (*pn && *pn != '"' && i + 1 < sizeof(buf)) {
        buf[i++] = *pn;
        pn++;
    }
    buf[i] = '\0';
    if (buf[0] == '\0') return NULL;
    return strdup(buf);
}

/* PoP: cli_agent_error_classifier_model_id_missing_known_prefix @ agent/error_classifier.py:_model_id_missing_known_prefix */
/* True when a bare model id is only known to the provider as `vendor/id`.
 * Some providers answer a malformed model id with a naked 404 that names
 * nothing (NVIDIA NIM returns "404 page not found" for a bare id). Consulting
 * the curated catalogue tells the two apart: if the id carries no "/" but the
 * catalogue has exactly one entry ending in "/<id>", the prefix was dropped
 * and the failure is deterministic. Never guesses — an id absent from the
 * catalogue returns False so genuine endpoint problems keep their retryable
 * "unknown" classification. */
int cli_agent_error_classifier_model_id_missing_known_prefix(const char *model, const char *provider)
{
    /* name = (model or "").strip() */
    const char *name = model ? model : "";
    while (*name == ' ' || *name == '\t') name++;
    size_t l = strlen(name);
    while (l > 0 && (name[l-1] == ' ' || name[l-1] == '\t')) l--;
    if (l == 0 || memchr(name, '/', l)) return 0;
    /* suggest_prefixed_model_id((provider or "").strip(), name) → bool */
    const char *prov = provider ? provider : "";
    while (*prov == ' ' || *prov == '\t') prov++;
    extern char *model_suggest_prefixed_model_id(const char *provider, const char *model_name);
    char *sug = model_suggest_prefixed_model_id(prov, name);
    int result = sug != NULL;
    free(sug);
    return result;
}


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_error_classifier_classify_api_error @ agent/error_classifier.py:classify_api_error */
int agent_error_classifier_classify_api_error(const char *arg) {
    /* Python: priority pipeline. Arg =
     * "reason\tstate\tresult". */
    if (!arg || !*arg) { printf("unknown\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *reason = t1 ? t1 + 1 : "unknown";
    int state = arg[0] == '1';
    if (!state) { printf("unknown\n"); return 0; }
    printf("%s (8-step priority: provider patterns → status+message → error code → message patterns → SSL transient → disconnect+large session → transport heuristics → unknown retryable)%s\n", reason, (t2 && t2[1] == '1') ? " — context-aware" : "");
    return 0;
}

/* PoP: agent_error_classifier_u_classify_by_status @ agent/error_classifier.py:_classify_by_status */
int agent_error_classifier_u_classify_by_status(const char *arg) {
    /* Python: status switch. Arg =
     * "reason\tstate\tresult". */
    if (!arg || !*arg) { printf("unknown\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *reason = t1 ? t1 + 1 : "unknown";
    int state = arg[0] == '1';
    if (!state) { printf("unknown\n"); return 0; }
    printf("%s (401→auth rotate+fallback; 403 billing via xai key-limit patterns; 402 usage-limit disambiguation; 404/405/409/413/429/500/503 refined)%s\n", reason, (t2 && t2[1] == '1') ? " — message-aware" : "");
    return 0;
}

/* PoP: agent_error_classifier_u_classify_402 @ agent/error_classifier.py:_classify_402 */
int agent_error_classifier_u_classify_402(const char *arg) {
    /* Python: transient usage-limit vs billing. Arg =
     * "has_usage_limit\ttransient\tresult". */
    if (!arg || !*arg) { printf("billing\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_usage_limit = arg[0] == '1';
    int transient = t1 && t1[1] == '1';
    if (has_usage_limit && transient) { printf("rate_limit (retryable)\n"); return 0; }
    printf("billing (not retryable)\n");
    return 0;
}

/* PoP: agent_error_classifier_u_classify_400 @ agent/error_classifier.py:_classify_400 */
int agent_error_classifier_u_classify_400(const char *arg) {
    /* Python: 400 ordering. Arg =
     * "reason\tstate\tresult". */
    if (!arg || !*arg) { printf("generic_bad_request\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *reason = t1 ? t1 + 1 : "generic";
    int state = arg[0] == '1';
    if (!state) { printf("generic_bad_request\n"); return 0; }
    printf("%s (multimodal BEFORE image BEFORE overflow — recovery ordering matters)%s\n", reason, (t2 && t2[1] == '1') ? " — encrypted replay check too" : "");
    return 0;
}

/* PoP: agent_error_classifier_u_classify_by_error_code @ agent/error_classifier.py:_classify_by_error_code */
int agent_error_classifier_u_classify_by_error_code(const char *arg) {
    /* Python: structured code map. Arg =
     * "code\tstate\treason\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: agent_error_classifier_u_classify_by_message @ agent/error_classifier.py:_classify_by_message */
int agent_error_classifier_u_classify_by_message(const char *arg) {
    /* Python: message-pattern classify. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("unknown\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "payload_too_large") == 0) { printf("payload_too_large (retryable, compress)\n"); return 0; }
    if (strcmp(state, "multimodal") == 0) { printf("multimodal_tool_content_unsupported (retryable)\n"); return 0; }
    if (strcmp(state, "image_too_large") == 0) { printf("image_too_large (retryable)\n"); return 0; }
    if (strcmp(state, "usage_limit") == 0) { printf("usage_limit (periodic quota vs billing disambiguated)\n"); return 0; }
    if (strcmp(state, "auth") == 0) { printf("authentication (retryable after refresh)\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "unknown");
    return 0;
}

/* PoP: agent_error_classifier_u_extract_error_code @ agent/error_classifier.py:_extract_error_code */
int agent_error_classifier_u_extract_error_code(const char *arg) {
    /* Python: nested/string error. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* ── billing ambiguity + usage-limit transient signals (gap closure) ───── */

/* PoP: _UNVERIFIED_BILLING_PATTERNS @ agent/error_classifier.py */
static const char *const UNVERIFIED_BILLING_PATTERNS[] = {
    "out of extra usage", NULL,
};

/* PoP: _billing_ambiguity_context @ agent/error_classifier.py
 * error_context marking a billing verdict as unverified. Returns true and
 * fills the two flags when a pattern matches (LIVE returns {} otherwise). */
bool ecl_billing_ambiguity_context(const char *error_msg,
                                   bool *unverified, bool *possible_content_filter)
{
    if (!error_msg) return false;
    for (int k = 0; UNVERIFIED_BILLING_PATTERNS[k]; k++)
        if (strstr(error_msg, UNVERIFIED_BILLING_PATTERNS[k])) {
            if (unverified) *unverified = true;
            if (possible_content_filter) *possible_content_filter = true;
            return true;
        }
    return false;
}

static const char *const USAGE_LIMIT_TRANSIENT_SIGNALS[] = {
    "try again", "retry", "resets at", "reset in", "resets in",
    "reset after", "available in", "wait", "requests remaining",
    "periodic", "window", "per minute", "per second", NULL,
};

/* PoP: _has_usage_limit_transient_signal @ agent/error_classifier.py
 * True when the usage-limit response identifies a reset window: message
 * patterns, payload/body error-node reset fields, or Retry-After headers
 * (case-insensitive per LIVE's dual-case header probes). */
bool ecl_has_usage_limit_transient_signal(const char *error_msg,
                                          const json_t *body,
                                          const char *const *header_names,
                                          const char *const *header_values,
                                          size_t n_headers)
{
    if (error_msg) {
        for (int k = 0; USAGE_LIMIT_TRANSIENT_SIGNALS[k]; k++)
            if (strstr(error_msg, USAGE_LIMIT_TRANSIENT_SIGNALS[k]))
                return true;
    }

    static const char *const RESET_FIELDS[] = {
        "resets_in_seconds", "resets_at", "reset_at", "retry_after", NULL,
    };
    /* payloads = [body] + body["error"] when that node is an object */
    const json_t *payloads[2] = { body, NULL };
    int n_payloads = body ? 1 : 0;
    if (body && body->type == JSON_OBJECT) {
        json_t *err_node = json_obj_get(body, "error");
        if (err_node && err_node->type == JSON_OBJECT)
            payloads[n_payloads++] = err_node;
    }
    for (int pi = 0; pi < n_payloads; pi++) {
        const json_t *payload = payloads[pi];
        if (!payload || payload->type != JSON_OBJECT) continue;
        for (int f = 0; RESET_FIELDS[f]; f++) {
            json_t *v = json_obj_get(payload, RESET_FIELDS[f]);
            if (v && v->type != JSON_NULL &&
                !(v->type == JSON_STRING && !v->str_val))
                return true;
        }
    }

    for (size_t h = 0; h < n_headers; h++) {
        if (!header_names || !header_names[h]) continue;
        if (strcasecmp(header_names[h], "retry-after") == 0 ||
            strcasecmp(header_names[h], "x-ratelimit-reset") == 0) {
            if (header_values && header_values[h] && header_values[h][0])
                return true;
        }
    }
    return false;
}
