/*
 * chat_completion_helpers.c — Chat completion helper functions for Hermes C.
 * AG26: estimate_request_context_tokens, build_api_kwargs,
 *       build_assistant_message, handle_max_iterations,
 *       try_activate_fallback, cleanup_task_resources.
 *
 * Mirrors Python's agent/chat_completion_helpers.py (partial).
 * Functions that map to existing C code (interruptible_api_call,
 * interruptible_streaming_api_call) are implemented in agent_loop.c
 * and llm_client.c respectively.
 */

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* AG26: Port of Python agent/chat_completion_helpers.py:_is_openai_codex_backend(). Returns true if api_mode
 * equals "codex_responses". */
bool is_openai_codex_backend(const char *api_mode)
{
    /* Check if the api_mode string matches "codex_responses".
     * This identifies OpenAI Codex backend mode for the agent. */
    if (!api_mode) return false;
    return strcmp(api_mode, "codex_responses") == 0;
}

/* AG26: Port of Python agent/chat_completion_helpers.py:_env_float(). Reads env var, parses as double.
 * Returns default_val on missing/invalid/empty. */
double env_float(const char *name, double default_val) {
    if (!name) return default_val;
    const char *val = getenv(name);
    if (!val || !*val) return default_val;
    while (*val == ' ' || *val == '\t') val++;
    if (!*val) return default_val;
    char *endptr = NULL;
    double result = strtod(val, &endptr);
    if (endptr == val) return default_val; /* no digits consumed */
    return result;
}

/* ================================================================
 *  estimate_request_context_tokens
 *  Port of Python agent/chat_completion_helpers.py:estimate_request_context_tokens().
 *  Estimate context tokens from an API payload.
 *  Handles: bare list (Chat Completions messages),
 *           dict with "messages" (Chat Completions + tools),
 *           dict with "input" (Responses API).
 *  Returns estimated token count.
 * ================================================================ */

/* Forward: existing context.c helper */
extern int context_total_tokens(const agent_state_t *state);

/* Port of Python agent/chat_completion_helpers.py:estimate_request_context_tokens(). */
int estimate_request_context_tokens(const message_t **messages, size_t message_count,
                                    const json_node_t *tools_json) {
    if (!messages || message_count == 0) return 0;

    int total_chars = 0;

    for (size_t i = 0; i < message_count; i++) {
        if (!messages[i]) continue;
        if (messages[i]->content) {
            total_chars += (int)strlen(messages[i]->content);
        }
        if (messages[i]->reasoning) {
            total_chars += (int)strlen(messages[i]->reasoning);
        }
        for (int t = 0; t < messages[i]->tool_calls_count; t++) {
            if (messages[i]->tool_calls[t].name)
                total_chars += (int)strlen(messages[i]->tool_calls[t].name);
            if (messages[i]->tool_calls[t].arguments)
                total_chars += (int)strlen(messages[i]->tool_calls[t].arguments);
        }
    }

    /* Add tools JSON chars if present */
    if (tools_json) {
        char *tools_str = json_serialize(tools_json);
        if (tools_str) {
            total_chars += (int)strlen(tools_str);
            free(tools_str);
        }
    }

    return total_chars / 4; /* rough estimate: 4 chars per token */
}

/* ================================================================
 *  build_api_kwargs (AG26)
 *  Port of Python agent/chat_completion_helpers.py:build_api_kwargs().
 *  Build API keyword arguments for the active API mode.
 *  Returns a JSON object with the appropriate fields.
 * ================================================================ */

json_node_t *build_api_kwargs(agent_state_t *state, const message_t **messages,
                               size_t message_count, const json_node_t *tools_json) {
    if (!state) return NULL;

    json_node_t *kwargs = json_new_object();
    if (!kwargs) return NULL;

    /* Common fields */
    json_object_set(kwargs, "model", json_new_string(state->llm.model));
    json_object_set(kwargs, "max_tokens", json_new_number((double)state->llm.max_tokens));

    /* Build messages array */
    json_node_t *msgs_arr = json_new_array();
    for (size_t i = 0; i < message_count; i++) {
        if (!messages[i]) continue;
        json_node_t *msg = json_new_object();
        json_object_set(msg, "role", json_new_string(
            messages[i]->role == MSG_USER ? "user" :
            messages[i]->role == MSG_ASSISTANT ? "assistant" :
            messages[i]->role == MSG_TOOL ? "tool" : "system"));

        if (messages[i]->content)
            json_object_set(msg, "content", json_new_string(messages[i]->content));

        /* Tool calls */
        if (messages[i]->tool_calls_count > 0 && messages[i]->role == MSG_ASSISTANT) {
            json_node_t *tc_arr = json_new_array();
            for (int t = 0; t < messages[i]->tool_calls_count; t++) {
                json_node_t *tc = json_new_object();
                json_object_set(tc, "id", json_new_string(messages[i]->tool_calls[t].id));
                json_object_set(tc, "type", json_new_string("function"));
                json_node_t *fn = json_new_object();
                json_object_set(fn, "name", json_new_string(messages[i]->tool_calls[t].name));
                json_object_set(fn, "arguments", json_new_string(messages[i]->tool_calls[t].arguments));
                json_object_set(tc, "function", fn);
                json_array_append(tc_arr, tc);
            }
            json_object_set(msg, "tool_calls", tc_arr);
        }

        /* Tool call ID for tool role messages */
        if (messages[i]->role == MSG_TOOL && messages[i]->tool_call_id[0]) {
            json_object_set(msg, "tool_call_id", json_new_string(messages[i]->tool_call_id));
        }

        json_array_append(msgs_arr, msg);
    }
    json_object_set(kwargs, "messages", msgs_arr);

    /* Tools */
    if (tools_json) {
        json_object_set(kwargs, "tools", tools_json);
    }

    /* Temperature if set */
    if (state->llm.temperature > 0) {
        json_object_set(kwargs, "temperature", json_new_number(state->llm.temperature));
    }

    return kwargs;
}

/* ================================================================
 *  build_assistant_message (AG26)
 *  Port of Python agent/chat_completion_helpers.py:build_assistant_message().
 *  Build a normalized assistant message from an API response.
 *  Returns a message_t (caller must free via message_free).
 * ================================================================ */

message_t *build_assistant_message(const llm_response_t *resp) {
    if (!resp) return NULL;

    message_t *msg = message_new(MSG_ASSISTANT, resp->content ? resp->content : "");
    if (!msg) return NULL;

    if (resp->reasoning) {
        msg->reasoning = strdup(resp->reasoning);
    }

    msg->tool_calls_count = resp->tool_calls_count;
    for (int i = 0; i < resp->tool_calls_count && i < 64; i++) {
        snprintf(msg->tool_calls[i].id, sizeof(msg->tool_calls[i].id),
                 "%s", resp->tool_calls[i].id);
        snprintf(msg->tool_calls[i].name, sizeof(msg->tool_calls[i].name),
                 "%s", resp->tool_calls[i].name);
        snprintf(msg->tool_calls[i].arguments, sizeof(msg->tool_calls[i].arguments),
                 "%s", resp->tool_calls[i].arguments);
    }

    return msg;
}

/* ================================================================
 *  handle_max_iterations (AG26)
 *  Request a summary when max iterations are reached.
 *  Appends a summary request message and triggers one more LLM call.
 *  Returns true if summary was requested.
 * ================================================================ */

/* Port of Python agent/chat_completion_helpers.py:handle_max_iterations(). */
bool handle_max_iterations(agent_state_t *state) {
    if (!state) return false;

    fprintf(stderr, "⚠️  Reached maximum iterations (%d). Requesting summary...\n",
            state->max_iterations);

    const char *summary_request =
        "You've reached the maximum number of tool-calling iterations allowed. "
        "Please provide a final response summarizing what you've found and accomplished so far, "
        "without calling any more tools.";

    /* Append summary request as user message */
    message_t *summary_msg = message_new(MSG_USER, summary_request);
    if (!summary_msg) return false;

    /* Add to messages array */
    if (state->message_count < state->message_capacity) {
        state->messages[state->message_count++] = summary_msg;
        return true;
    } else {
        message_free(summary_msg);
        return false;
    }
}

/* ================================================================
 *  try_activate_fallback (AG26)
 *  Try to activate a fallback provider.
 *  Returns true if fallback was activated.
 * ================================================================ */

/* Port of Python agent/chat_completion_helpers.py:try_activate_fallback(). */
bool try_activate_fallback(agent_state_t *state) {
    if (!state) return false;

    /* Check if there's a fallback model configured */
    if (state->llm.fallback_model[0] == '\0') {
        return false;
    }

    fprintf(stderr, "[fallback] Trying fallback model: %s\n",
            state->llm.fallback_model);

    /* Swap model to fallback */
    snprintf(state->llm.model, sizeof(state->llm.model), "%s",
             state->llm.fallback_model);

    return true;
}

/* ================================================================
 *  cleanup_task_resources (AG26)
 *  Clean up VM and browser resources for a given task.
 *  In C, this is a no-op placeholder — resource cleanup is handled
 *  by the terminal tool's idle reaper.
 * ================================================================ */

/* Port of Python agent/chat_completion_helpers.py:cleanup_task_resources(). */
void cleanup_task_resources(const char *task_id) {
    if (!task_id || !*task_id) return;
    /* Placeholder: C resource cleanup handled by terminal idle reaper */
    (void)task_id;
}

/* ================================================================
 *  Interruptible API call wrappers
 * ================================================================ */

/* Port of Python agent/agent_init.py:_ra(), agent/conversation_loop.py:_ra(), agent/system_prompt.py:_ra().
 * Python's _ra() returns the run_agent module for test-patching purposes.
 * In C, callable functions are accessed directly without a module handle,
 * so this returns NULL as a sentinel indicating direct-call convention.
 * Callers in C never dereference this — they use function pointers directly. */
void *cch_ra(void) {
    /* Python's _ra() returns the run_agent module for test-patching purposes.
     * In C, callable functions are accessed directly without a module handle.
     * Three Python modules use _ra(): agent_init.py, conversation_loop.py,
     * and system_prompt.py — all for accessing run_agent attributes in tests.
     * Since C has no equivalent module system, NULL is returned as a sentinel. */
    return NULL;
}

/* Port of Python agent/chat_completion_helpers.py:interruptible_api_call().
 * Run the API call with interrupt detection.
 * C handles this synchronously via llm_chat_completion().
 * Uses the agent state's internal message list and config.
 * The caller should check state->interrupted before/after the call.
 * Returns llm_response_t* (caller must llm_response_free). */
llm_response_t *interruptible_api_call(agent_state_t *agent, json_node_t *api_kwargs_json) {
    if (!agent || agent->interrupted) return NULL;
    if (!api_kwargs_json) {
        /* Use state's internal messages */
        if (agent->message_count == 0) return NULL;
        return llm_chat_completion(&agent->llm,
            (const message_t **)agent->messages,
            agent->message_count,
            NULL);
    }

    /* Extract tools from kwargs JSON */
    json_node_t *tools_json = json_object_get(api_kwargs_json, "tools");

    return llm_chat_completion(&agent->llm,
        (const message_t **)agent->messages,
        agent->message_count,
        tools_json);
}

/* Port of Python agent/chat_completion_helpers.py:interruptible_streaming_api_call().
 * Streaming variant — delegates to llm_chat_completion_stream().
 * Uses the agent state's internal message list and config. */
static int noop_token_cb(const char *text, void *userdata)
{
    /* No-op token callback for streaming API calls.
     * Discards all tokens — used as a placeholder when streaming
     * callback processing is handled elsewhere. */
    (void)text;
    (void)userdata;
    return 0;
}

llm_response_t *interruptible_streaming_api_call(agent_state_t *agent, json_node_t *api_kwargs_json) {
    if (!agent || agent->interrupted) return NULL;
    if (agent->message_count == 0) return NULL;

    json_node_t *tools_json = NULL;
    if (api_kwargs_json)
        tools_json = json_object_get(api_kwargs_json, "tools");

    return llm_chat_completion_stream(&agent->llm,
        (const message_t **)agent->messages,
        agent->message_count,
        tools_json,
        noop_token_cb, NULL);
}

/* ── Small helpers ported from chat_completion_helpers.py ──────── */

/* strip+lower a string into caller buffer; returns buf. */
static char *cch_strip_lower(const char *s, char *buf, size_t cap) {
    if (!s) { buf[0] = '\0'; return buf; }
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\n'||s[n-1]=='\r')) n--;
    if (n >= cap) n = cap - 1;
    size_t i;
    for (i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)s[i]);
    buf[i] = '\0';
    return buf;
}

/* strip only (no case change) into caller buffer. */
static char *cch_strip(const char *s, char *buf, size_t cap) {
    if (!s) { buf[0] = '\0'; return buf; }
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\n'||s[n-1]=='\r')) n--;
    if (n >= cap) n = cap - 1;
    memcpy(buf, s, n); buf[n] = '\0';
    return buf;
}

/* PoP: chat_completion_helpers__validated_openrouter_provider_sort @ agent/chat_completion_helpers.py:_validated_openrouter_provider_sort */
/* Return a normalized OpenRouter provider.sort value ("throughput"|"latency"|
 * "price") or NULL. Invalid non-empty values log a warning. Result is a static
 * string — do NOT free. */
const char *chat_completion_helpers__validated_openrouter_provider_sort(const char *raw_sort)
{
    if (!raw_sort) return NULL;
    char buf[64];
    cch_strip_lower(raw_sort, buf, sizeof(buf));
    if (!buf[0]) return NULL;
    if (strcmp(buf, "throughput") == 0) return "throughput";
    if (strcmp(buf, "latency") == 0) return "latency";
    if (strcmp(buf, "price") == 0) return "price";
    hermes_log(LOG_WARNING, "chat_completion",
               "Ignoring invalid OpenRouter provider.sort value '%s' (allowed: latency, price, throughput)",
               raw_sort);
    return NULL;
}

/* PoP: chat_completion_helpers__rewrite_prompt_model_identity @ agent/chat_completion_helpers.py:rewrite_prompt_model_identity */
/* Rewrite the LAST "Model: ..." and "Provider: ..." lines of the cached system
 * prompt in place, pointing them at the active runtime after a provider switch.
 * Takes the prompt string and returns a freshly-allocated rewritten copy
 * (caller frees). Empty/NULL prompt returns a copy of the input (or NULL). */
char *chat_completion_helpers__rewrite_prompt_model_identity(const char *sp, const char *model, const char *provider)
{
    if (!sp || !sp[0]) return sp ? strdup(sp) : NULL;
    char *cur = strdup(sp);
    const char *labels[2] = {"Model", "Provider"};
    const char *values[2] = {model, provider};
    for (int li = 0; li < 2; li++) {
        const char *value = values[li];
        if (!value || !value[0]) continue;
        char prefix[32];
        snprintf(prefix, sizeof(prefix), "%s: ", labels[li]);
        size_t plen = strlen(prefix);
        /* Find the LAST line that starts (at BOL) with "<label>: " */
        size_t curlen = strlen(cur);
        long best_start = -1, best_end = -1;
        for (size_t i = 0; i < curlen; i++) {
            int at_bol = (i == 0) || (cur[i-1] == '\n');
            if (!at_bol) continue;
            if (strncmp(cur + i, prefix, plen) == 0) {
                /* line runs to next '\n' or end */
                size_t j = i;
                while (j < curlen && cur[j] != '\n') j++;
                best_start = (long)i;
                best_end = (long)j;
            }
        }
        if (best_start >= 0) {
            /* rebuild: cur[:best_start] + "<label>: <value>" + cur[best_end:] */
            size_t vlen = strlen(value);
            size_t tail = curlen - (size_t)best_end;
            char *out = malloc((size_t)best_start + plen + vlen + tail + 1);
            memcpy(out, cur, (size_t)best_start);
            memcpy(out + best_start, prefix, plen);
            memcpy(out + best_start + plen, value, vlen);
            memcpy(out + best_start + plen + vlen, cur + best_end, tail);
            out[(size_t)best_start + plen + vlen + tail] = '\0';
            free(cur);
            cur = out;
        }
    }
    return cur;
}

/* PoP: chat_completion_helpers__fallback_entry_key @ agent/chat_completion_helpers.py:_fallback_entry_key */
/* Build the (provider_lower, model, base_url_no_trailing_slash) identity key for
 * a fallback entry. Writes the three parts to caller buffers. */
void chat_completion_helpers__fallback_entry_key(const json_t *fb,
    char *provider_out, size_t pcap,
    char *model_out, size_t mcap,
    char *base_url_out, size_t bcap)
{
    const char *p = "", *m = "", *b = "";
    if (fb && fb->type == JSON_OBJECT) {
        const json_t *jp = json_obj_get(fb, "provider");
        const json_t *jm = json_obj_get(fb, "model");
        const json_t *jb = json_obj_get(fb, "base_url");
        if (jp && jp->type == JSON_STRING) p = jp->str_val;
        if (jm && jm->type == JSON_STRING) m = jm->str_val;
        if (jb && jb->type == JSON_STRING) b = jb->str_val;
    }
    cch_strip_lower(p, provider_out, pcap);
    cch_strip(m, model_out, mcap);
    cch_strip(b, base_url_out, bcap);
    /* rstrip("/") on base_url */
    size_t bl = strlen(base_url_out);
    while (bl > 0 && base_url_out[bl-1] == '/') base_url_out[--bl] = '\0';
}

/* Reads the persisted auth store (~/.hermes/auth.json) provider state for
 * provider_id. Returns a malloc'd JSON object (caller json_free) or NULL. */
static json_t *cch_load_provider_auth_state(const char *provider_id)
{
    const char *home = getenv("HERMES_HOME");
    char path[4096];
    if (home && *home)
        snprintf(path, sizeof(path), "%s/auth.json", home);
    else {
        const char *h = getenv("HOME");
        if (!h || !*h) return NULL;
        snprintf(path, sizeof(path), "%s/.hermes/auth.json", h);
    }
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f); buf[n] = '\0';
    json_t *store = json_parse(buf, NULL);
    free(buf);
    if (!store) return NULL;
    json_t *providers = json_obj_get(store, "providers");
    json_t *state = providers ? json_obj_get(providers, provider_id) : NULL;
    json_t *result = state ? json_copy(state) : NULL;
    json_free(store);
    return result;
}

/* PoP: chat_completion_helpers__fallback_entry_unavailable_without_network @ agent/chat_completion_helpers.py:_fallback_entry_unavailable_without_network */
/* Return a malloc'd skip reason for fallback entries known to be unusable
 * locally (currently only Nous without any token), or NULL when usable. */
char *chat_completion_helpers__fallback_entry_unavailable_without_network(const json_t *fb)
{
    if (!fb || fb->type != JSON_OBJECT) return NULL;
    const json_t *jp = json_obj_get(fb, "provider");
    char prov[64];
    cch_strip_lower(jp && jp->type == JSON_STRING ? jp->str_val : "", prov, sizeof(prov));
    if (strcmp(prov, "nous") != 0) return NULL;

    json_t *state = cch_load_provider_auth_state("nous");
    /* Python: get_provider_auth_state("nous") or {}; exception → auth_unreadable.
     * A missing/unparseable store here maps to an empty state (no tokens). */
    int has_access = 0, has_refresh = 0;
    if (state) {
        const json_t *acc = json_obj_get(state, "access_token");
        const json_t *ref = json_obj_get(state, "refresh_token");
        if (acc && acc->type == JSON_STRING) {
            has_access = (acc->str_val[0] != '\0') && (strspn(acc->str_val, " \t\n\r") != strlen(acc->str_val));
        }
        if (ref && ref->type == JSON_STRING) {
            has_refresh = (ref->str_val[0] != '\0') && (strspn(ref->str_val, " \t\n\r") != strlen(ref->str_val));
        }
        json_free(state);
    }
    if (!(has_access || has_refresh))
        return strdup("nous_token_missing");
    return NULL;
}
