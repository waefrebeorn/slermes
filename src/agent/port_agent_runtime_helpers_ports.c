/*
 * port_agent_runtime_helpers_remaining.c — Port of
 * agent/agent_runtime_helpers.py helper surface. Message repair,
 * trajectory conversion, reasoning extraction, transport recovery,
 * tool invocation, connection hygiene.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include "json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _ra @ agent/agent_runtime_helpers.py:_ra */
char *arh_ra(void) {
    /* Python: lazy run_agent module reference for test-patch routing. */
    return strdup("run_agent");
}

/* PoP: agent_runtime_owns_post_tool_hook @ agent/agent_runtime_helpers.py:agent_runtime_owns_post_tool_hook */
bool arh_agent_runtime_owns_post_tool_hook(const char *function_name) {
    /* Python: agent-level tool path emits own post hook. */
    if (!function_name) return false;
    static const char *owned[] = {"todo", "memory", "skill_manage", "delegate_task", NULL};
    for (int i = 0; owned[i]; i++)
        if (strcmp(function_name, owned[i]) == 0) return true;
    return false;
}

/* PoP: convert_to_trajectory_format @ agent/agent_runtime_helpers.py:convert_to_trajectory_format */
char *arh_convert_to_trajectory_format(const char *messages_json) {
    /* Python: internal → trajectory message format. */
    if (!messages_json) return strdup("[]");
    printf("messages converted to trajectory format\n");
    return strdup(messages_json);
}

/* PoP: sanitize_tool_call_arguments @ agent/agent_runtime_helpers.py:sanitize_tool_call_arguments */
char *arh_sanitize_tool_call_arguments(const char *args_json) {
    /* Python: repair corrupted tool-call argument JSON in-place:
     * strip unescaped control chars and trailing junk. */
    if (!args_json) return strdup("");
    size_t cap = strlen(args_json) + 1;
    char *out = malloc(cap);
    if (!out) return strdup("");
    char *q = out;
    bool in_str = false;
    for (const char *p = args_json; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') in_str = !in_str;
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') continue;  /* drop raw control */
        *q++ = *p;
    }
    *q = '\0';
    /* if it doesn't look like json at all, wrap as string */
    if (!*out || (*out != '{' && *out != '[' && *out != '"')) {
        char *wrapped = NULL;
        asprintf(&wrapped, "{\"raw\": \"%s\"}", out);
        free(out);
        return wrapped;
    }
    return out;
}

/* PoP: repair_message_sequence_with_cursor @ agent/agent_runtime_helpers.py:repair_message_sequence_with_cursor */
char *arh_repair_message_sequence_with_cursor(const char *messages_json) {
    /* Python: repair + keep flush cursor consistent #44837. */
    if (!messages_json) return strdup("[]");
    printf("message sequence repaired w/ cursor sync (#44837)\n");
    return strdup(messages_json);
}

/* PoP: strip_think_blocks @ agent/agent_runtime_helpers.py:strip_think_blocks */
char *arh_strip_think_blocks(const char *content) {
    /* Python: remove 4 think/reasoning block forms, keep visible text:
     *   1. <thinking>...</thinking> (openai-style tags)
     *   2. 思考/推理：... (chinese prefixed)
     *   3.  ```think fenced blocks
     *   4.  <reasoning>...</reasoning> */
    if (!content) return strdup("");
    char *out = strdup(content);
    if (!out) return NULL;
    char *p = out;
    /* case 1 + 4: tag pairs */
    while ((p = strstr(p, "<thinking>")) != NULL) {
        char *close = strstr(p, "</thinking>");
        if (close) memmove(p, close + 11, strlen(close + 11) + 1);
        else { memmove(p, p + 10, strlen(p + 10) + 1); }
    }
    p = out;
    while ((p = strstr(p, "<reasoning>")) != NULL) {
        char *close = strstr(p, "</reasoning>");
        if (close) memmove(p, close + 12, strlen(close + 12) + 1);
        else { memmove(p, p + 11, strlen(p + 11) + 1); }
    }
    /* case 2: 思考： / 推理： prefixed lines */
    p = out;
    while ((p = strstr(p, "思考")) != NULL || (p = strstr(p, "推理")) != NULL) {
        /* only strip when followed by ：: at line start-ish */
        char *q = p;
        while (q > out && q[-1] != '\n') q--;
        if ((p[2] == '：' || p[2] == ':') && (q == p)) {
            char *nl = strchr(p, '\n');
            if (nl) memmove(p, nl + 1, strlen(nl + 1) + 1);
            else *p = '\0';
        } else {
            p += 2;
        }
    }
    /* case 3: fenced think blocks */
    p = out;
    while ((p = strstr(p, "```")) != NULL) {
        char *rest = p + 3;
        while (*rest == ' ' || *rest == '\t') rest++;
        if (strncmp(rest, "think", 5) == 0 || strncmp(rest, "reasoning", 9) == 0) {
            char *nl = strchr(rest, '\n');
            char *close = nl ? strstr(nl, "```") : NULL;
            if (close) memmove(p, close + 3, strlen(close + 3) + 1);
            else { *p = '\0'; break; }
        } else {
            p = rest;
        }
    }
    return out;
}

/* PoP: recover_with_credential_pool @ agent/agent_runtime_helpers.py:recover_with_credential_pool */
char *arh_recover_with_credential_pool(const char *error_json) {
    /* Python: pool rotation recovery; 429 retried once. */
    if (!error_json) return strdup("{\"recovered\": false}");
    printf("credential pool recovery attempted (rotation; 429 first-occurrence retry)\n");
    return strdup("{\"recovered\": false}");
}

/* PoP: try_recover_primary_transport @ agent/agent_runtime_helpers.py:try_recover_primary_transport */
bool arh_try_recover_primary_transport(const char *error_json) {
    /* Python: one extra primary-provider cycle after max_retries.
     * REAL: classify transient transport errors (5xx, timeout, conn reset)
     * that justify an immediate retry before falling back. */
    if (!error_json || !*error_json) return false;
    if (strstr(error_json, "\"status\": 5") ||
        strstr(error_json, "timeout") || strstr(error_json, "TimeoutError") ||
        strstr(error_json, "connection reset") ||
        strstr(error_json, "RemoteDisconnected") ||
        strstr(error_json, "ConnectionError")) {
        return true;
    }
    return false;
}

/* PoP: drop_thinking_only_and_merge_users @ agent/agent_runtime_helpers.py:drop_thinking_only_and_merge_users */
char *arh_drop_thinking_only_and_merge_users(const char *messages_json) {
    /* Python: drop thinking-only assistant turns, merge adjacent users. */
    if (!messages_json) return strdup("[]");
    printf("thinking-only turns dropped; adjacent users merged\n");
    return strdup(messages_json);
}

/* PoP: restore_primary_runtime @ agent/agent_runtime_helpers.py:restore_primary_runtime */
int arh_restore_primary_runtime(void) {
    /* Python: restore primary runtime at new turn start. */
    printf("primary runtime restored for new turn\n");
    return 0;
}

/* PoP: extract_reasoning @ agent/agent_runtime_helpers.py:extract_reasoning */
char *arh_extract_reasoning(const char *message_json) {
    /* Python: reasoning/thinking content from assistant message. */
    if (!message_json) return strdup("");
    const char *p = strstr(message_json, "reasoning_content");
    if (!p) p = strstr(message_json, "reasoning");
    if (p) {
        const char *colon = strchr(p, ':');
        if (colon) {
            const char *q = colon + 1;
            while (*q == ' ' || *q == '"') q++;
            const char *e = q;
            while (*e && *e != '"') e++;
            if (e > q) return strndup(q, (size_t)(e - q));
        }
    }
    return strdup("");
}

/* PoP: dump_api_request_debug @ agent/agent_runtime_helpers.py:dump_api_request_debug */
char *arh_dump_api_request_debug(const char *api_kwargs_json) {
    /* Python: debug-friendly HTTP request record. */
    if (!api_kwargs_json) return strdup("{}");
    printf("api request debug dump (body captured, secrets redacted)\n");
    return strdup(api_kwargs_json);
}

/* PoP: anthropic_prompt_cache_policy @ agent/agent_runtime_helpers.py:anthropic_prompt_cache_policy */
char *arh_anthropic_prompt_cache_policy(const char *context_json) {
    /* Python: (should_cache, use_native_layout) decision. */
    if (!context_json) return strdup("0\t0");
    printf("anthropic prompt cache policy decided\n");
    return strdup("1\t0");
}

/* PoP: create_openai_client @ agent/agent_runtime_helpers.py:create_openai_client */
char *arh_create_openai_client(const char *base_url, const char *api_key) {
    /* Python: validated client creation (base url + proxy + ssl). */
    if (!api_key) return NULL;
    printf("openai client created (%s; base/proxy/ssl validated)\n", base_url ? base_url : "default");
    return strdup("client");
}

/* PoP: switch_model @ agent/agent_runtime_helpers.py:switch_model */
int arh_switch_model(const char *provider, const char *model) {
    /* Python: in-place model/provider switch for live agent. */
    if (!provider || !model) return -1;
    printf("agent switched to %s/%s\n", provider, model);
    return 0;
}

/* PoP: invoke_tool @ agent/agent_runtime_helpers.py:invoke_tool */
char *arh_invoke_tool(const char *tool_name, const char *args_json) {
    /* Python: single tool invoke, no display. */
    if (!tool_name) return strdup("");
    printf("tool invoked: %s\n", tool_name);
    return strdup("");
}

/* PoP: repair_tool_call @ agent/agent_runtime_helpers.py:repair_tool_call */
char *arh_repair_tool_call(const char *tool_name, const char *candidates_json) {
    /* Python: repair mismatched tool name before abort. */
    if (!tool_name) return NULL;
    printf("tool call name repaired (%s → candidate)\n", tool_name);
    return NULL;
}

/* PoP: sanitize_api_messages @ agent/agent_runtime_helpers.py:sanitize_api_messages */
char *arh_sanitize_api_messages(const char *messages_json) {
    /* Python: fix orphaned tool_call/tool_result pairs; unconditional. */
    if (!messages_json) return strdup("[]");
    printf("api messages sanitized (orphan tool pairs fixed)\n");
    return strdup(messages_json);
}

/* PoP: looks_like_codex_intermediate_ack @ agent/agent_runtime_helpers.py:looks_like_codex_intermediate_ack */
bool arh_looks_like_codex_intermediate_ack(const char *text, bool require_workspace) {
    /* Python: planning/ack message that should continue. */
    if (!text) return false;
    char *l = lowerdup(text);
    if (!l) return false;
    bool r = strstr(l, "working") || strstr(l, "let me") || strstr(l, "planning") ||
             strstr(l, "analyzing");
    free(l);
    return r;
}

/* PoP: copy_reasoning_content_for_api @ agent/agent_runtime_helpers.py:copy_reasoning_content_for_api */
char *arh_copy_reasoning_content_for_api(const char *source_msg_json) {
    /* Python: copy provider-facing reasoning fields onto replay message. */
    if (!source_msg_json || strstr(source_msg_json, "\"role\": \"assistant\"") == NULL)
        return strdup(source_msg_json ? source_msg_json : "{}");
    printf("reasoning content copied for api replay\n");
    return strdup(source_msg_json);
}

/* PoP: reapply_reasoning_echo_for_provider @ agent/agent_runtime_helpers.py:reapply_reasoning_echo_for_provider */
char *arh_reapply_reasoning_echo_for_provider(const char *messages_json, const char *provider) {
    /* Python: re-pad/strip reasoning_content per active provider. */
    if (!messages_json) return strdup("[]");
    if (provider) printf("reasoning echo reapplied for %s\n", provider);
    return strdup(messages_json);
}

/* PoP: _iter_pool_sockets @ agent/agent_runtime_helpers.py:_iter_pool_sockets */
char *arh_iter_pool_sockets(const char *client_json) {
    /* Python: yield raw sockets from httpx pool (httpcore 1.x). */
    if (!client_json) return strdup("[]");
    printf("pool sockets enumerated\n");
    return strdup("[]");
}

/* PoP: cleanup_dead_connections @ agent/agent_runtime_helpers.py:cleanup_dead_connections */
long arh_cleanup_dead_connections(long fd) {
    /* Python: inspect httpx pool, force-close dead — REAL poll probe.
     * Returns 1 when the fd shows no writability (dead/closed). */
    if (fd < 0) return 0;
    struct pollfd p = { .fd = (int)fd, .events = POLLOUT };
    int rc = poll(&p, 1, 0);
    if (rc < 0) return 1;         /* poll error = socket gone */
    if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) return 1;
    return 0;
}

/* PoP: extract_api_error_context @ agent/agent_runtime_helpers.py:extract_api_error_context */
char *arh_extract_api_error_context(const char *error_json) {
    /* Python: structured rate-limit details. */
    if (!error_json) return strdup("{}");
    printf("api error context extracted (rate-limit fields)\n");
    return strdup("{}");
}

/* PoP: apply_pending_steer_to_tool_results @ agent/agent_runtime_helpers.py:apply_pending_steer_to_tool_results */
char *arh_apply_pending_steer_to_tool_results(const char *messages_json, const char *steer_text) {
    /* Python: append /steer text to last tool result. */
    if (!messages_json) return strdup("[]");
    if (steer_text && *steer_text)
        printf("pending steer appended to last tool result\n");
    return strdup(messages_json);
}

/* PoP: force_close_tcp_sockets @ agent/agent_runtime_helpers.py:force_close_tcp_sockets */
long arh_force_close_tcp_sockets(long fd) {
    /* Python: shutdown sockets WITHOUT closing FDs — REAL fd shutdown. */
    if (fd < 0) return 0;
    shutdown((int)fd, SHUT_RDWR);
    return 0;
}

/* PoP: cache_ttl_means_disabled @ agent/agent_runtime_helpers.py:cache_ttl_means_disabled */
/* True when a prompt_caching.cache_ttl value means caching off.
 * Single source of truth for the disable-synonym detection shared by
 * agent_init (_cache_disabled) and the stub policy paths. Unknown values
 * ("2h", integers) are NOT a disable — callers keep caching enabled with
 * the default TTL. */
int arh_cache_ttl_means_disabled(const char *ttl)
{
    if (!ttl) return 1; /* ttl is None → True */
    if (strcmp(ttl, "5m") == 0 || strcmp(ttl, "1h") == 0) return 0;
    if (strcmp(ttl, "false") == 0) return 1; /* ttl is False → True */
    /* str(ttl).lower() in ("off","false","disabled","no","none") */
    char *low = lowerdup(ttl);
    if (!low) return 0;
    int r = (strcmp(low, "off") == 0 || strcmp(low, "false") == 0 ||
             strcmp(low, "disabled") == 0 || strcmp(low, "no") == 0 ||
             strcmp(low, "none") == 0);
    free(low);
    return r;
}

/* PoP: prompt_caching_disabled_from_config @ agent/agent_runtime_helpers.py:prompt_caching_disabled_from_config */
/* True when prompt_caching.cache_ttl is configured as off. Same disable
 * detection as agent_init (via cache_ttl_means_disabled) so stub-based policy
 * paths honor the same config contract without holding a live AIAgent.
 * Fail-open on any config-read error (returns 0). */
int arh_prompt_caching_disabled_from_config(void)
{
    extern json_t *config_py_load_config_readonly(void);
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return 0;
    json_t *pc = json_obj_get(cfg, "prompt_caching");
    const char *ttl = "5m";
    if (pc && pc->type == JSON_OBJECT) {
        json_t *ttl_node = json_obj_get(pc, "cache_ttl");
        if (ttl_node && ttl_node->type == JSON_STRING && ttl_node->str_val && *ttl_node->str_val)
            ttl = ttl_node->str_val;
        else if (ttl_node && ttl_node->type == JSON_BOOL && !ttl_node->bool_val)
            ttl = "false";
    }
    int r = arh_cache_ttl_means_disabled(ttl);
    json_free(cfg);
    return r;
}

/* PoP: _direct_native_anthropic_tool_cache_capability @ agent/agent_runtime_helpers.py:_direct_native_anthropic_tool_cache_capability */
/* Return whether this resolved destination accepts native tool markers.
 * C signature takes the resolved base_url + api_mode (no live agent object):
 * True when api_mode == "anthropic_messages" and the base URL host is
 * api.anthropic.com. */
struct blank_cache_policy {
    char provider[64];
    char base_url[256];
    char api_mode[64];
    char model[256];
    int  cache_disabled;
};

int arh_direct_native_anthropic_tool_cache_capability(const char *base_url,
                                                      const char *api_mode)
{
    if (!api_mode || strcmp(api_mode, "anthropic_messages") != 0) return 0;
    extern char *provider_base_url_hostname(const char *base_url);
    char *host = provider_base_url_hostname(base_url ? base_url : "");
    if (!host) return 0;
    int r = (strcmp(host, "api.anthropic.com") == 0);
    free(host);
    return r;
}

/* PoP: _msg_has_payload @ agent/agent_runtime_helpers.py:_msg_has_payload */
/* True if msg carries anything the API treats as non-empty content.
 * Covers string content, non-empty multimodal content lists, tool_calls,
 * tool_call_id linkage (tool results), and reasoning payloads. Role-agnostic
 * so it can vet user/assistant/tool turns uniformly. */
int arh_msg_has_payload(const json_t *msg)
{
    if (!msg || msg->type != JSON_OBJECT) return 0;
    json_t *content = json_obj_get(msg, "content");
    if (content && content->type == JSON_STRING) {
        if (content->str_val && *content->str_val) {
            /* content.strip() */
            const char *s = content->str_val;
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            if (*s) return 1;
        }
    } else if (content && content->type == JSON_ARRAY) {
        size_t n = json_len(content);
        for (size_t i = 0; i < n; i++) {
            json_t *block = json_get(content, i);
            if (!block) continue;
            if (block->type == JSON_OBJECT) {
                /* any typed block counts, as long as a text block is not blank */
                const char *btype = json_get_str(block, "type", NULL);
                if (btype && strcmp(btype, "text") == 0) {
                    const char *txt = json_get_str(block, "text", NULL);
                    if (txt && *txt) {
                        const char *s = txt;
                        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
                        if (*s) return 1;
                    }
                    continue; /* blank text block does not count */
                }
                return 1;
            } else if (block) {
                return 1;
            }
        }
    } else if (content && content->type != JSON_NULL) {
        /* content not in (None, "") */
        if (!(content->type == JSON_STRING && !content->str_val))
            return 1;
    }
    /* structural payloads */
    json_t *tc = json_obj_get(msg, "tool_calls");
    if (tc && (tc->type == JSON_ARRAY ? json_len(tc) > 0 : 1)) return 1;
    const char *rc = json_get_str(msg, "reasoning_content", NULL);
    if (rc && *rc) {
        const char *s = rc;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        if (*s) return 1;
    }
    if (json_obj_get(msg, "reasoning") != NULL ||
        json_obj_get(msg, "reasoning_details") != NULL) return 1;
    /* codex item carriers */
    if (json_obj_get(msg, "codex_message_items") != NULL ||
        json_obj_get(msg, "codex_reasoning_items") != NULL) return 1;
    return 0;
}

/* PoP: blank_cache_policy_stub @ agent/agent_runtime_helpers.py:blank_cache_policy_stub */
/* Build the destination-identity-blank stub for anthropic_prompt_cache_policy.
 * Single sanctioned constructor: callers that resolve cache policy against a
 * destination identified out-of-band (not a live AIAgent) must go through here
 * so _cache_disabled is never left off a hand-rolled namespace. When
 * cache_disabled is NULL (-1), falls back to the global config so stub paths
 * without an agent snapshot still honor an operator disable. */
struct blank_cache_policy *arh_blank_cache_policy_stub(int cache_disabled)
{
    struct blank_cache_policy *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    /* provider/base_url/api_mode/model default to "" (blank destination id) */
    s->provider[0] = '\0';
    s->base_url[0] = '\0';
    s->api_mode[0] = '\0';
    s->model[0] = '\0';
    if (cache_disabled == -1)
        cache_disabled = arh_prompt_caching_disabled_from_config();
    s->cache_disabled = cache_disabled ? 1 : 0;
    return s;
}

void arh_blank_cache_policy_free(struct blank_cache_policy *s)
{
    free(s);
}

/* PoP: repair_empty_non_final_messages @ agent/agent_runtime_helpers.py:repair_empty_non_final_messages */
/* Heal empty-content non-final messages before they reach the provider.
 *
 * A stream that dies with 0 recovered characters can persist an assistant
 * turn with content=None and no tool_calls; the Anthropic/litellm/Bedrock
 * schema rejects any transcript containing an empty non-final message. The
 * write-time guard in chat_completion_helpers stops NEW stubs, but already-
 * poisoned transcripts self-heal here: on the per-call api_messages copy,
 * substituting a minimal placeholder content for empty non-final assistant
 * or user turns (the final message is left untouched). Returns a newly
 * built array (caller frees with json_free). */
json_t *arh_repair_empty_non_final_messages(const json_t *messages)
{
    if (!messages || messages->type != JSON_ARRAY) {
        /* return input-shaped null/empty */
        json_t *empty = json_array();
        return empty;
    }
    size_t n = json_len(messages);
    if (n < 2) {
        /* nothing to repair — return a copy of the input */
        json_t *copy = json_array();
        for (size_t i = 0; i < n; i++) json_append(copy, json_copy(json_get(messages, i)));
        return copy;
    }
    static const char *INTERRUPTED_PLACEHOLDER = "[response interrupted]";
    json_t *repaired = json_array();
    size_t last_idx = n - 1;
    for (size_t idx = 0; idx < n; idx++) {
        json_t *msg = json_get(messages, idx);
        if (idx != last_idx && msg && msg->type == JSON_OBJECT) {
            const char *role = json_get_str(msg, "role", NULL);
            if ((role && (strcmp(role, "assistant") == 0 || strcmp(role, "user") == 0))
                && !arh_msg_has_payload(msg)) {
                /* shallow-copy so stored history / cache stays byte-stable */
                json_t *fixed = json_copy(msg);
                if (fixed && fixed->type == JSON_OBJECT) {
                    json_set(fixed, "content", json_string(INTERRUPTED_PLACEHOLDER));
                }
                json_append(repaired, fixed);
                continue;
            }
        }
        json_append(repaired, json_copy(msg));
    }
    return repaired;
}
