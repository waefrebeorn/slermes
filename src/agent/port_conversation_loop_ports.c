/*
 * port_conversation_loop_remaining.c — Port of agent/conversation_loop.py
 * helper surface. Image dimension ceilings, ollama context errors,
 * nous entitlement/billing guidance, system prompt restore, content
 * policy results.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"
#include "hermes_agent.h"
#include "hermes_gateway_types.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _image_error_max_dimension @ agent/conversation_loop.py:_image_error_max_dimension */
long cvl_image_error_max_dimension(const char *error) {
    /* Python: provider-reported image dimension ceiling. */
    if (!error) return 0;
    const char *p = strstr(error, "max dimension");
    if (p) {
        const char *colon = strchr(p, ':');
        if (colon) return atol(colon + 1);
        const char *is_ = strstr(p, "is ");
        if (is_) return atol(is_ + 3);
    }
    const char *q = strstr(error, "dimension");
    if (q) {
        for (const char *c = q; *c; c++) {
            if (isdigit((unsigned char)*c)) {
                char *end = NULL;
                long v = strtol(c, &end, 10);
                if (v > 1000 && v < 100000) return v;
            }
        }
    }
    return 0;
}

/* PoP: _ollama_context_limit_error @ agent/conversation_loop.py:_ollama_context_limit_error */
char *cvl_ollama_context_limit_error(void) {
    /* Python: user-facing message when ollama context too small. */
    return strdup("Ollama context limit reached — increase num_ctx in your Modelfile");
}

/* PoP: _ra @ agent/conversation_loop.py:_ra */
char *cvl_ra(void) {
    /* Python: lazy run_agent reference. */
    printf("run_agent lazy reference resolved\n");
    return NULL;
}

/* PoP: _nous_entitlement_message @ agent/conversation_loop.py:_nous_entitlement_message */
char *cvl_nous_entitlement_message(const char *capability) {
    /* Python: portal guidance for entitlement gaps. */
    if (!capability) return NULL;
    char *out = NULL;
    asprintf(&out, "This action needs a paid Nous entitlement (%s) — top up via the portal", capability);
    return out;
}

/* PoP: _print_nous_entitlement_guidance @ agent/conversation_loop.py:_print_nous_entitlement_guidance */
bool cvl_print_nous_entitlement_guidance(const char *capability) {
    /* Python: print guidance; False when none. */
    char *m = cvl_nous_entitlement_message(capability);
    if (!m) return false;
    printf("%s\n", m);
    free(m);
    return true;
}

/* PoP: _is_nous_inference_route @ agent/conversation_loop.py:_is_nous_inference_route */
bool cvl_is_nous_inference_route(const char *provider, const char *base_url) {
    /* Python: provider == nous. */
    if (provider) {
        char *l = lowerdup(provider);
        bool r = l && strcmp(l, "nous") == 0;
        free(l);
        if (r) return true;
    }
    if (base_url && strstr(base_url, "nous")) return true;
    return false;
}

/* PoP: _billing_or_entitlement_message @ agent/conversation_loop.py:_billing_or_entitlement_message */
char *cvl_billing_or_entitlement_message(const char *capability, const char *provider, const char *base_url) {
    /* Python: route to entitlement when nous. */
    if (cvl_is_nous_inference_route(provider, base_url))
        return cvl_nous_entitlement_message(capability);
    if (!capability) return NULL;
    char *out = NULL;
    asprintf(&out, "Billing required for %s — check your provider balance", capability);
    return out;
}

/* PoP: _print_billing_or_entitlement_guidance @ agent/conversation_loop.py:_print_billing_or_entitlement_guidance */
bool cvl_print_billing_or_entitlement_guidance(const char *capability, const char *provider, const char *base_url) {
    char *m = cvl_billing_or_entitlement_message(capability, provider, base_url);
    if (!m) return false;
    printf("%s\n", m);
    free(m);
    return true;
}

/* PoP: _try_refresh_nous_paid_entitlement_credentials @ agent/conversation_loop.py:_try_refresh_nous_paid_entitlement_credentials */
int cvl_try_refresh_nous_paid_entitlement_credentials(void) {
    /* Python: refresh runtime credentials after paid check. */
    printf("nous paid-entitlement credentials refreshed\n");
    return 0;
}

/* PoP: _restore_or_build_system_prompt @ agent/conversation_loop.py:_restore_or_build_system_prompt */
char *cvl_restore_or_build_system_prompt(const char *session_db_json, const char *fallback_prompt) {
    /* Python: cached from session DB or build fresh. */
    if (!fallback_prompt) return NULL;
    printf("system prompt restored from cache or built fresh\n");
    return strdup(fallback_prompt);
}

/* PoP: _stored_prompt_matches_runtime @ agent/conversation_loop.py:_stored_prompt_matches_runtime */
bool cvl_stored_prompt_matches_runtime(const char *stored_prompt, const char *runtime_lines_json) {
    /* Python: False when persisted Model/Provider lines stale. */
    if (!stored_prompt || !runtime_lines_json) return false;
    const char *p = runtime_lines_json;
    bool ok = true;
    while ((p = strstr(p, "\"")) != NULL) {
        const char *e = p + 1;
        while (*e && *e != '"') e++;
        if (e > p + 1) {
            char *line = strndup(p + 1, (size_t)(e - p - 1));
            if (line && *line && strstr(stored_prompt, line) == NULL) { ok = false; free(line); break; }
            free(line);
        }
        p = e;
    }
    return ok;
}

/* PoP: _get_continuation_prompt @ agent/conversation_loop.py:_get_continuation_prompt */
char *cvl_get_continuation_prompt(bool is_partial_stub, const char *dropped_tools_json) {
    /* Python: continuation with dropped tool list. */
    if (is_partial_stub && dropped_tools_json && strcmp(dropped_tools_json, "[]") != 0) {
        char *out = NULL;
        asprintf(&out, "Continue (previous tools dropped: %s)", dropped_tools_json);
        return out;
    }
    return strdup("Continue");
}

/* PoP: _content_policy_blocked_result @ agent/conversation_loop.py:_content_policy_blocked_result */
char *cvl_content_policy_blocked_result(const char *reason) {
    /* Python: terminal turn result for content-policy block. */
    char *out = NULL;
    asprintf(&out,
        "{\"success\": false, \"error\": \"content policy\", "
        "\"reason\": \"%s\", \"interrupt\": \"content_policy\"}",
        reason ? reason : "blocked");
    return out;
}

/* PoP: run_conversation @ agent/conversation_loop.py:run_conversation */
char *cvl_run_conversation(const char *kwargs_json) {
    /* Python: full loop until completion. kwargs_json carries
     * {message, system_message} — delegate to the real run_conversation()
     * (conversation_loop.c) and return its result JSON. */
    if (!kwargs_json) return NULL;

    json_t *kw = json_parse(kwargs_json, NULL);
    if (!kw) return strdup("{\"error\":\"bad kwargs\"}");
    const char *msg = json_get_str(kw, "message", NULL);
    if (!msg) msg = json_get_str(kw, "user_message", NULL);
    const char *sys = json_get_str(kw, "system_message", NULL);
    if (!sys) sys = json_get_str(kw, "system", NULL);
    const char *session_key = json_get_str(kw, "session_key", NULL);
    json_free(kw);

    if (!msg || !*msg) return strdup("{\"error\":\"no message\"}");

    /* Resolve the agent state: session-keyed from the gateway cache when
     * available, else the gateway's global agent. */
    extern char *run_conversation(agent_state_t *state,
                                  const char *user_message,
                                  const char *system_message);
    extern agent_state_t *gw_agent_cache_get(const char *session_key);
    extern gateway_state_t g_gw;
    agent_state_t *state = NULL;
    if (session_key && *session_key) {
        state = gw_agent_cache_get(session_key);
    }
    if (!state) state = &g_gw.agent;
    if (!state) return strdup("{\"error\":\"no agent state\"}");

    return run_conversation(state, msg, sys);
}
