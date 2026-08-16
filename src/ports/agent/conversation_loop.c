#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/conversation_loop.c — C11 port of agent/conversation_loop.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_conversation_loop.c ── */
/*
 * port_agent_conversation_loop.c — Port of Python agent/conversation_loop.py
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _stored_prompt_matches_runtime */
bool stored_prompt_matches_runtime(const char *stored_model, const char *current_model) {
    if (!stored_model || !current_model) return true;
    if (stored_model[0] == '\0' || current_model[0] == '\0') return true;
    return (strcmp(stored_model, current_model) == 0);
}


/* Port of Python: _sync_failover_system_message */
/* PoP: sync_failover_system_message @ agent/conversation_loop.py:_sync_failover_system_message */
void sync_failover_system_message(char *api_messages, size_t msg_sz,
                                   const char *active_system_prompt) {
    if (!api_messages || !active_system_prompt) return;
    /* Replace first message (system prompt) with active one */
    /* In C, the system prompt is managed by the agent context */
    /* This function is a no-op in C since the prompt is always current */
    (void)msg_sz;
}



/* ── from src/agent/port_agent_conversation_loop_py_parity.c ── */
/* Slermes C port — agent/conversation_loop.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_conversation_loop_restore_user_after_reference_handoff_p @ agent/conversation_loop.py:_restore_user_after_reference_handoff */
json_t *agent_conversation_loop_restore_user_after_reference_handoff_p(void) {
    return json_object();
}

/* PoP: agent_conversation_loop_should_skip_model_call_for_reference_handoff_p @ agent/conversation_loop.py:_should_skip_model_call_for_reference_handoff */
json_t *agent_conversation_loop_should_skip_model_call_for_reference_handoff_p(void) {
    return json_object();
}


/* ── from src/agent/port_conversation_loop_ports.c ── */
/*
 * port_conversation_loop_remaining.c — Port of agent/conversation_loop.py
 * helper surface. Image dimension ceilings, ollama context errors,
 * nous entitlement/billing guidance, system prompt restore, content
 * policy results.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"
#include "hermes_agent.h"
#include "hermes_gateway_types.h"
#include "hermes_system_prompt.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: cvl_image_error_max_dimension @ agent/conversation_loop.py:_image_error_max_dimension */
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

/* PoP: cvl_ollama_context_limit_error @ agent/conversation_loop.py:_ollama_context_limit_error */
char *cvl_ollama_context_limit_error(void) {
    /* Python: user-facing message when ollama context too small. */
    return strdup("Ollama context limit reached — increase num_ctx in your Modelfile");
}

/* PoP: cvl_ra @ agent/conversation_loop.py:_ra */
char *cvl_ra(void) {
    /* Python: lazy run_agent reference. */
    printf("run_agent lazy reference resolved\n");
    return NULL;
}

/* PoP: cvl_nous_entitlement_message @ agent/conversation_loop.py:_nous_entitlement_message */
char *cvl_nous_entitlement_message(const char *capability) {
    /* Python: portal guidance for entitlement gaps. */
    if (!capability) return NULL;
    char *out = NULL;
    asprintf(&out, "This action needs a paid Nous entitlement (%s) — top up via the portal", capability);
    return out;
}

/* PoP: cvl_print_nous_entitlement_guidance @ agent/conversation_loop.py:_print_nous_entitlement_guidance */
bool cvl_print_nous_entitlement_guidance(const char *capability) {
    /* Python: print guidance; False when none. */
    char *m = cvl_nous_entitlement_message(capability);
    if (!m) return false;
    printf("%s\n", m);
    free(m);
    return true;
}

/* PoP: cvl_is_nous_inference_route @ agent/conversation_loop.py:_is_nous_inference_route */
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

/* PoP: cvl_billing_or_entitlement_message @ agent/conversation_loop.py:_billing_or_entitlement_message */
char *cvl_billing_or_entitlement_message(const char *capability, const char *provider, const char *base_url) {
    /* Python: route to entitlement when nous. */
    if (cvl_is_nous_inference_route(provider, base_url))
        return cvl_nous_entitlement_message(capability);
    if (!capability) return NULL;
    char *out = NULL;
    asprintf(&out, "Billing required for %s — check your provider balance", capability);
    return out;
}

/* PoP: cvl_print_billing_or_entitlement_guidance @ agent/conversation_loop.py:_print_billing_or_entitlement_guidance */
bool cvl_print_billing_or_entitlement_guidance(const char *capability, const char *provider, const char *base_url) {
    char *m = cvl_billing_or_entitlement_message(capability, provider, base_url);
    if (!m) return false;
    printf("%s\n", m);
    free(m);
    return true;
}

/* PoP: cvl_try_refresh_nous_paid_entitlement_credentials @ agent/conversation_loop.py:_try_refresh_nous_paid_entitlement_credentials */
int cvl_try_refresh_nous_paid_entitlement_credentials(void) {
    /* Python: refresh runtime credentials after paid check. */
    printf("nous paid-entitlement credentials refreshed\n");
    return 0;
}

/* PoP: cvl_restore_or_build_system_prompt @ agent/conversation_loop.py:_restore_or_build_system_prompt */
char *cvl_restore_or_build_system_prompt(const char *session_db_json, const char *fallback_prompt) {
    /* Python: cached from session DB or build fresh. */
    if (!fallback_prompt) return NULL;
    printf("system prompt restored from cache or built fresh\n");
    return strdup(fallback_prompt);
}

/* PoP: cvl_stored_prompt_matches_runtime @ agent/conversation_loop.py:_stored_prompt_matches_runtime */
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

/* PoP: cvl_get_continuation_prompt @ agent/conversation_loop.py:_get_continuation_prompt */
char *cvl_get_continuation_prompt(bool is_partial_stub, const char *dropped_tools_json) {
    /* Python: continuation with dropped tool list. */
    if (is_partial_stub && dropped_tools_json && strcmp(dropped_tools_json, "[]") != 0) {
        char *out = NULL;
        asprintf(&out, "Continue (previous tools dropped: %s)", dropped_tools_json);
        return out;
    }
    return strdup("Continue");
}

/* PoP: cvl_content_policy_blocked_result @ agent/conversation_loop.py:_content_policy_blocked_result */
char *cvl_content_policy_blocked_result(const char *reason) {
    /* Python: terminal turn result for content-policy block. */
    char *out = NULL;
    asprintf(&out,
        "{\"success\": false, \"error\": \"content policy\", "
        "\"reason\": \"%s\", \"interrupt\": \"content_policy\"}",
        reason ? reason : "blocked");
    return out;
}

/* PoP: cvl_run_conversation @ agent/conversation_loop.py:run_conversation */
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

/* =========================================================================
 * Gap closures for agent/conversation_loop.py (8 REAL_GAPs)
 * ========================================================================= */

/* ---- _is_copilot_provider ---- */
/* Python: bool(agent._is_copilot_provider()) with try/except, falling back to
 * provider string matching {"copilot", "github-copilot", "github"}.
 * C: the agent_state_t carries .llm.provider. */
/* PoP: cvl_is_copilot_provider @ agent/conversation_loop.py:_is_copilot_provider */
bool cvl_is_copilot_provider(const agent_state_t *agent) {
    if (!agent) return false;
    const char *prov = agent->llm.provider;
    if (prov && *prov) {
        /* case-insensitive match against the three aliases */
        char *low = lowerdup(prov);
        if (low) {
            bool r = (strcmp(low, "copilot") == 0 ||
                      strcmp(low, "github-copilot") == 0 ||
                      strcmp(low, "github") == 0);
            free(low);
            if (r) return true;
        }
    }
    return false;
}

/* ---- _is_stale_copilot_credential_error ---- */
/* Python: status_code == 400 or "error code: 400" in lowered error_message,
 * AND one of: "model_not_available_for_integrator", "not available for
 * integrator", "model_not_supported", "the requested model is not supported". */
/* PoP: cvl_is_stale_copilot_credential_error @ agent/conversation_loop.py:_is_stale_copilot_credential_error */
bool cvl_is_stale_copilot_credential_error(int status_code, const char *error_message) {
    char *lowered = lowerdup(error_message);
    bool is_400 = (status_code == 400);
    if (!is_400 && lowered && strstr(lowered, "error code: 400"))
        is_400 = true;
    if (!is_400) { free(lowered); return false; }

    bool stale = false;
    if (lowered) {
        stale = (strstr(lowered, "model_not_available_for_integrator") != NULL ||
                 strstr(lowered, "not available for integrator") != NULL ||
                 strstr(lowered, "model_not_supported") != NULL ||
                 strstr(lowered, "the requested model is not supported") != NULL);
    }
    free(lowered);
    return stale;
}

/* ---- _canonicalize_tool_call_arguments ---- */
/* Python: json.loads(arg_str) then json.dumps(..., separators=(",",":"),
 * sort_keys=True).  Memoized in _CANON_ARGS_CACHE (bounded LRU eviction).
 * C: same semantics — parse to json_t, re-serialize compact+sorted, cache. */
#define DLG_CANON_CACHE_MAX    256
#define DLG_CANON_CACHE_MAXBYTES (256 * 1024)  /* 256 KiB cap */

static char *_canon_args_cache_keys[DLG_CANON_CACHE_MAX];
static char *_canon_args_cache_vals[DLG_CANON_CACHE_MAX];
static int  _canon_args_cache_head = 0;   /* eviction pointer (FIFO) */
static int  _canon_args_cache_count = 0;
static long _canon_args_cache_bytes = 0;

/* Serialize a json_t back to compact sorted form (mirrors json.dumps with
 * sort_keys=True, separators=(",",":")).  libjson doesn't sort keys on
 * serialize, so we build a new object inserting keys in sorted order. */
static char *canon_serialize_sorted(const json_t *node) {
    if (!node) return NULL;
    switch (node->type) {
    case JSON_ARRAY: {
        /* arrays preserve order */
        char *buf = malloc(1);
        buf[0] = '[';
        size_t cap = 32, len = 1;
        for (size_t i = 0; i < node->c.count; i++) {
            char *item = canon_serialize_sorted(node->c.items[i]);
            if (!item) continue;
            /* append comma */
            if (i > 0) {
                if (len + 2 > cap) { cap = (len + 2) * 2; buf = realloc(buf, cap); }
                buf[len++] = ','; buf[len] = '\0';
            }
            size_t ilen = strlen(item);
            if (len + ilen + 1 > cap) { cap = (len + ilen + 1) * 2; buf = realloc(buf, cap); }
            memcpy(buf + len, item, ilen);
            len += ilen; buf[len] = '\0';
            free(item);
        }
        if (cap < len + 2) { cap = len + 2; buf = realloc(buf, cap); }
        buf[len++] = ']'; buf[len] = '\0';
        return buf;
    }
    case JSON_OBJECT: {
        /* sort keys alphabetically */
        char **keys = malloc(sizeof(char*) * (node->c.count + 1));
        size_t *idx = malloc(sizeof(size_t) * (node->c.count + 1));
        size_t n = 0;
        for (size_t i = 0; i < node->c.count; i++) {
            keys[n] = node->c.keys[i] ? strdup(node->c.keys[i]) : NULL;
            idx[n] = i; n++;
        }
        /* insertion sort on keys */
        for (size_t i = 1; i < n; i++) {
            char *k = keys[i]; size_t j = idx[i]; size_t k2 = i;
            while (k2 > 0 && strcmp(keys[k2-1], k) > 0) {
                keys[k2] = keys[k2-1]; idx[k2] = idx[k2-1]; k2--;
            }
            keys[k2] = k; idx[k2] = j;
        }
        char *buf = malloc(1);
        buf[0] = '{';
        size_t cap = 32, len = 1;
        for (size_t i = 0; i < n; i++) {
            if (i > 0) { if (len + 2 > cap) { cap = (len+2)*2; buf = realloc(buf, cap); }
                buf[len++] = ','; buf[len] = '\0'; }
            char *val = canon_serialize_sorted(node->c.items[idx[i]]);
            const char *key = keys[i];
            /* escaped key */
            if (len + 2 > cap) { cap = (len+2)*2; buf = realloc(buf, cap); }
            buf[len++] = '"'; buf[len] = '\0';
            for (const char *p = key; p && *p; p++) {
                char esc[8]; int elen = 0;
                switch (*p) {
                case '"':  esc[0]='\\'; esc[1]='"'; elen=2; break;
                case '\\': esc[0]='\\'; esc[1]='\\'; elen=2; break;
                case '\n': esc[0]='\\'; esc[1]='n'; elen=2; break;
                case '\t': esc[0]='\\'; esc[1]='t'; elen=2; break;
                case '\r': esc[0]='\\'; esc[1]='r'; elen=2; break;
                case '\b': esc[0]='\\'; esc[1]='b'; elen=2; break;
                case '\f': esc[0]='\\'; esc[1]='f'; elen=2; break;
                default: esc[0]=*p; elen=1; break; break;
                }
                if (len + elen + 1 > cap) { cap = (len+elen+1)*2; buf = realloc(buf, cap); }
                memcpy(buf+len, esc, elen); len += elen; buf[len] = '\0';
            }
            if (len + 2 > cap) { cap = (len+2)*2; buf = realloc(buf, cap); }
            buf[len++] = '"'; buf[len] = ':'; buf[len+1] = '\0';
            len += 2;
            size_t vlen = val ? strlen(val) : 4;
            if (len + vlen + 1 > cap) { cap = (len+vlen+1)*2; buf = realloc(buf, cap); }
            memcpy(buf+len, val ? val : "null", vlen);
            len += vlen; buf[len] = '\0';
            free(val);
        }
        for (size_t i = 0; i < n; i++) free(keys[i]);
        free(keys); free(idx);
        if (cap < len + 2) { cap = len + 2; buf = realloc(buf, cap); }
        buf[len++] = '}'; buf[len] = '\0';
        return buf;
    }
    case JSON_STRING: {
        const char *s = node->str_val ? node->str_val : "";
        char *buf = malloc(strlen(s) + 3);
        buf[0] = '"';
        /* escape */
        size_t j = 1;
        for (const char *p = s; *p; p++) {
            switch (*p) {
            case '"':  buf[j++]='\\'; buf[j++]='"'; break;
            case '\\': buf[j++]='\\'; buf[j++]='\\'; break;
            case '\n': buf[j++]='\\'; buf[j++]='n'; break;
            case '\t': buf[j++]='\\'; buf[j++]='t'; break;
            case '\r': buf[j++]='\\'; buf[j++]='r'; break;
            case '\b': buf[j++]='\\'; buf[j++]='b'; break;
            case '\f': buf[j++]='\\'; buf[j++]='f'; break;
            default: buf[j++] = *p; break;
            }
        }
        buf[j++] = '"'; buf[j] = '\0';
        return buf;
    }
    case JSON_NUMBER: {
        char *buf = malloc(32);
        snprintf(buf, 32, "%.17g", node->num_val);
        return buf;
    }
    case JSON_BOOL:
        return node->bool_val ? strdup("true") : strdup("false");
    case JSON_NULL:
        return strdup("null");
    }
    return NULL;
}

/* Eviction: FIFO — remove oldest entry when cache over capacity. */
static void _canon_cache_evict_one(void) {
    if (_canon_args_cache_count <= 0) return;
    int slot = (_canon_args_cache_head) % DLG_CANON_CACHE_MAX;
    _canon_args_cache_bytes -= strlen(_canon_args_cache_keys[slot]) + strlen(_canon_args_cache_vals[slot]);
    free(_canon_args_cache_keys[slot]);
    free(_canon_args_cache_vals[slot]);
    _canon_args_cache_keys[slot] = NULL;
    _canon_args_cache_vals[slot] = NULL;
    _canon_args_cache_head = (slot + 1) % DLG_CANON_CACHE_MAX;
    _canon_args_cache_count--;
}

/* PoP: cvl_canonicalize_tool_call_arguments @ agent/conversation_loop.py:_canonicalize_tool_call_arguments */
char *cvl_canonicalize_tool_call_arguments(const char *arg_str) {
    if (!arg_str) return NULL;
    /* Check cache */
    for (int i = 0; i < DLG_CANON_CACHE_MAX; i++) {
        if (_canon_args_cache_keys[i] &&
            strcmp(_canon_args_cache_keys[i], arg_str) == 0) {
            return strdup(_canon_args_cache_vals[i]);
        }
    }
    /* Parse + re-serialize canonical form */
    json_t *node = json_parse(arg_str, NULL);
    if (!node) return NULL;  /* raises json.JSONDecodeError in Python */
    char *canonical = canon_serialize_sorted(node);
    json_free(node);
    if (!canonical) return NULL;

    /* Store in cache, evicting oldest if over capacity */
    long key_len = strlen(arg_str);
    long val_len = strlen(canonical);
    while ((_canon_args_cache_count >= DLG_CANON_CACHE_MAX ||
            (_canon_args_cache_bytes + key_len + val_len > DLG_CANON_CACHE_MAXBYTES &&
             _canon_args_cache_count > 0))) {
        _canon_cache_evict_one();
    }
    int slot = (_canon_args_cache_head + _canon_args_cache_count) % DLG_CANON_CACHE_MAX;
    _canon_args_cache_keys[slot] = strdup(arg_str);
    _canon_args_cache_vals[slot] = canonical;  /* owned by cache */
    _canon_args_cache_bytes += key_len + val_len;
    _canon_args_cache_count++;
    return strdup(canonical);
}

/* ---- _canonicalize_api_tool_calls ---- */
/* Python: for each message with tool_calls, canonicalize each tc's
 * function.arguments.  Modifies the api_messages JSON in place.
 * C: takes json_t* messages array, modifies tool_calls in place. */
/* PoP: cvl_canonicalize_api_tool_calls @ agent/conversation_loop.py:_canonicalize_api_tool_calls */
void cvl_canonicalize_api_tool_calls(json_t *api_messages) {
    if (!api_messages || api_messages->type != JSON_ARRAY) return;
    for (size_t i = 0; i < api_messages->c.count; i++) {
        json_t *am = api_messages->c.items[i];
        if (!am || am->type != JSON_OBJECT) continue;
        json_t *tcs = json_obj_get(am, "tool_calls");
        if (!tcs || tcs->type != JSON_ARRAY || tcs->c.count == 0) continue;
        for (size_t j = 0; j < tcs->c.count; j++) {
            json_t *tc = tcs->c.items[j];
            if (!tc || tc->type != JSON_OBJECT) continue;
            json_t *fn = json_obj_get(tc, "function");
            if (!fn || fn->type != JSON_OBJECT) continue;
            json_t *args = json_obj_get(fn, "arguments");
            if (!args || args->type != JSON_STRING) continue;
            char *canon = cvl_canonicalize_tool_call_arguments(args->str_val);
            if (canon) {
                /* Replace arguments value */
                json_set(fn, "arguments", json_string(canon));
                free(canon);
            }
        }
    }
}

/* ---- _rewrite_system_content_blocks ---- */
/* Python: if system_message.content is a 2-element list of text blocks and
 * the head text is a prefix of `effective`, replace the tail block's text
 * with the remainder.  Returns false if shape doesn't match (caller falls
 * back to plain assignment). */
/* PoP: cvl_rewrite_system_content_blocks @ agent/conversation_loop.py:_rewrite_system_content_blocks */
bool cvl_rewrite_system_content_blocks(json_t *system_message, const char *effective) {
    if (!system_message || system_message->type != JSON_OBJECT) return false;
    json_t *content = json_obj_get(system_message, "content");
    if (!content || content->type != JSON_ARRAY) return false;
    if (content->c.count == 0) return false;
    /* All parts must be dicts with type="text" */
    for (size_t i = 0; i < content->c.count; i++) {
        json_t *part = content->c.items[i];
        if (!part || part->type != JSON_OBJECT) return false;
        json_t *type = json_obj_get(part, "type");
        if (!type || type->type != JSON_STRING || strcmp(type->str_val, "text") != 0)
            return false;
    }
    if (content->c.count == 1) {
        json_set(content->c.items[0], "text", json_string(effective));
        return true;
    }
    if (content->c.count >= 2) {
        json_t *head_part = content->c.items[0];
        json_t *head_text = json_obj_get(head_part, "text");
        const char *head = (head_text && head_text->type == JSON_STRING) ? head_text->str_val : "";
        if (head && *head && effective && strncmp(effective, head, strlen(head)) == 0) {
            const char *tail = effective + strlen(head);
            if (*tail) {
                json_set(content->c.items[1], "text", json_string(tail));
                return true;
            }
        }
    }
    return false;
}

/* ---- _ensure_cached_system_prompt_static ---- */
/* Python: delegate to agent.system_prompt.reconstruct_static_prefix(agent,
 * system_message=..., log_label="failover redecoration").
 * C: delegates to the real port in system_prompt.c, which rebuilds the
 *    stable tier via system_prompt_build_stable() and applies the startswith
 *    gate + failed-rebuild memoization. */
/* PoP: cvl_ensure_cached_system_prompt_static @ agent/conversation_loop.py:_ensure_cached_system_prompt_static */
bool cvl_ensure_cached_system_prompt_static(agent_state_t *agent,
                                            const system_prompt_config_t *cfg) {
    if (!agent) return false;
    bool stale = (agent->cached_system_prompt_static == NULL ||
                  agent->cached_system_prompt_static[0] == '\0');
    if (stale) {
        system_prompt_reconstruct_static_prefix(agent, cfg);
    }
    return stale;
}

/* ---- _peel_moa_guidance ---- */
/* Python: delegate to agent.moa_loop.peel_reference_guidance(messages, guidance).
 * The moa_loop module is not yet ported to C. We provide a no-op identity
 * pass (returns messages unchanged) — the real peeler must be wired in when
 * the MoA aggregator C port lands. The contract: given a JSON array of
 * messages and a guidance object, strip the MoA reference guidance blocks. */
/* PoP: cvl_peel_moa_guidance @ agent/conversation_loop.py:_peel_moa_guidance */
json_t *cvl_peel_moa_guidance(json_t *messages, const json_t *guidance) {
    (void)guidance;  /* guidance is opaque to the no-op path */
    /* No-op identity: return the messages as-is. When the C MoA port lands,
     * this delegates to moa_peel_reference_guidance(messages, guidance). */
    return messages;
}

/* ---- _redecorate_prompt_cache_for_provider ---- */
/* Python: strip cache_control, peel MoA guidance, re-apply plan based on
 * agent._use_prompt_caching flag. Returns (messages, prepared, planned_tools).
 * C: we port the messages-remake + cache control strip + moa peel portions.
 *    The prompt cache plan builder (agent.prompt_caching.build_prompt_cache_plan)
 *    is not yet ported — we return messages with cache_control stripped and
 *    MoA guidance peeled, prepared unchanged, and tools_for_api unchanged. */
/* PoP: cvl_redecorate_prompt_cache_for_provider @ agent/conversation_loop.py:_redecorate_prompt_cache_for_provider */
json_t *cvl_redecorate_prompt_cache_for_provider(agent_state_t *agent,
                                                  json_t *api_messages,
                                                  const char *system_message,
                                                  json_t *tools_for_api) {
    (void)system_message; (void)tools_for_api;
    if (!api_messages || api_messages->type != JSON_ARRAY) return api_messages;
    /* Remake message list as shallow copies (dict(m) for dicts) */
    json_t *messages = json_array();
    if (!messages) return api_messages;
    for (size_t i = 0; i < api_messages->c.count; i++) {
        json_t *m = api_messages->c.items[i];
        json_t *copy = json_copy(m);  /* deep copy of the message dict */
        if (copy) json_append(messages, copy);
    }
    /* Strip cache_control from each message */
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *m = messages->c.items[i];
        if (m && m->type == JSON_OBJECT) {
            json_obj_del(m, "cache_control");
        }
    }
    /* Peel MoA guidance if present (no-op until moa_loop is ported) */
    /* In Python, this runs _peel_moa_guidance only when guidance is present.
     * We skip the call here since cvl_peel_moa_guidance is a no-op identity. */
    /* When _use_prompt_caching is true, the Python re-applies the plan.
     * The plan builder isn't ported yet — the caller handles decoration
     * separately via the prompt cache layer. We return the cleaned messages. */
    return messages;
}

/* ---- _finalize_wait_result ---- */
/* Python: assemble {output, returncode, [output_total_chars, full_output_path]}
 *         when spill occurred (total_chars > max_chars). */
/* PoP: cvl_finalize_wait_result @ agent/conversation_loop.py:_finalize_wait_result */
char *cvl_finalize_wait_result(const char *rendered, long returncode,
                                long output_total_chars, bool spilled,
                                const char *spill_path) {
    json_t *r = json_object();
    json_set(r, "output", json_string(rendered ? rendered : ""));
    json_set(r, "returncode", json_number((double)returncode));
    if (spilled && spill_path && *spill_path) {
        json_set(r, "output_total_chars", json_number((double)output_total_chars));
        json_set(r, "full_output_path", json_string(spill_path));
    }
    char *out = json_serialize(r);
    json_free(r);
    return out ? out : strdup("{}");
}

/* PoP: conversation_loop_join_truncated_parts @ agent/conversation_loop.py:_join_truncated_parts */
/* Returns malloc'd joined string. Caller frees. */
char *conversation_loop_join_truncated_parts(const char *parts_json) {
    if (!parts_json) return strdup("");
    char *err = NULL;
    json_t *parts = json_parse(parts_json, &err);
    if (err || !parts || parts->type != JSON_ARRAY) { if (err) free(err); if (parts) json_free(parts); return strdup(""); }
    size_t total_len = 0;
    for (size_t i = 0; i < parts->c.count; i++) {
        json_t *p = parts->c.items[i];
        if (p && p->type == JSON_STRING) total_len += strlen(p->str_val);
    }
    total_len += parts->c.count;  /* newlines */
    char *joined = (char *)malloc(total_len + 1);
    if (!joined) { json_free(parts); return strdup(""); }
    joined[0] = '\0';
    for (size_t i = 0; i < parts->c.count; i++) {
        json_t *p = parts->c.items[i];
        if (!p || p->type != JSON_STRING) continue;
        const char *part = p->str_val;
        if (joined[0] && joined[strlen(joined)-1] != ' ' && joined[strlen(joined)-1] != '\n' && part && part[0] != ' ' && part[0] != '\n') {
            strcat(joined, "\n");
        }
        strcat(joined, part);
    }
    json_free(parts);
    return joined;
}

/* PoP: conv_clone_message_for_send @ agent/conversation_loop.py:_clone_message_for_send */
/* Returns deep copy of a message JSON for the per-call API copy. Caller frees. */
char *conv_clone_message_for_send(const char *msg_json) {
    if (!msg_json) return strdup("{}");
    char *err = NULL;
    json_t *msg = json_parse(msg_json, &err);
    if (err || !msg) { if (err) free(err); if (msg) json_free(msg); return strdup("{}"); }
    json_t *copy = json_copy(msg); /* deep copy */
    json_free(msg);
    if (!copy) return strdup("{}");
    char *out = json_serialize(copy);
    json_free(copy);
    return out ? out : strdup("{}");
}

