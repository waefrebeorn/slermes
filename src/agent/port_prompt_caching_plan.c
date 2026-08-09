/*
 * port_prompt_caching_plan.c — Port of agent/prompt_caching.py's cache-plan
 * builder and its helpers.  Pure json_t* manipulation (no agent/loop deps):
 *   _can_carry_marker
 *   _apply_cache_marker
 *   _apply_system_cache_markers
 *   apply_anthropic_cache_control
 *   _completed_transaction_endpoint_indexes
 *   _count_cache_markers
 *   build_prompt_cache_plan
 *
 * Reuses: libjson (json_copy, json_obj_get/set/del, json_is_*).
 * Reuses: agent_runtime_pure (direct_native_anthropic_tool_cache_capability,
 *          blank_cache_policy_stub) for plan_cache_sections_for_destination.
 */

#define _POSIX_C_SOURCE 200809L
#include "port_prompt_caching_plan.h"
#include "port_prompt_caching_strip.h"
#include "agent_runtime_pure.h"   /* direct_native_anthropic_tool_cache_capability */
#include <hermes_json.h>

#include <sys/types.h>
#include <stdio.h>      /* snprintf */
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>      /* tolower */
#include <strings.h>    /* strcasecmp */

static bool is_dict(const json_t *v) { return json_is_object(v); }

/* Reuse URL helpers ported in port_provider_registry.c / provider_anthropic.c. */
extern char *provider_base_url_hostname(const char *base_url);
extern bool  provider_base_url_host_matches(const char *base_url, const char *domain);
/* Reuse the dedicated PoP port (port_anthropic_adapter_ports.c), not the
 * heavy duplicate in provider_anthropic.c — avoids dragging http.o etc. */
extern bool antr_model_name_is_kimi_family(const char *model);

/* ── _build_marker ───────────────────────────────────────────────────────── */
/* Python: {"type":"ephemeral"} + ("1h" → ttl:"1h").  NOT the buggy port
 * in port_prompt_caching_ports.c (which always emits ttl). */
static json_t *pc_build_marker(const char *ttl)
{
    json_t *m = json_object();
    if (!m) return NULL;
    json_set(m, "type", json_string("ephemeral"));
    if (ttl && strcmp(ttl, "1h") == 0)
        json_set(m, "ttl", json_string("1h"));
    return m;
}

/* ── _can_carry_marker ───────────────────────────────────────────────────── */
/* PoP: _can_carry_marker @ agent/prompt_caching.py:_can_carry_marker */
static bool pc_can_carry_marker(const json_t *msg, bool native_anthropic)
{
    if (!msg || msg->type != JSON_OBJECT) return false;
    if (native_anthropic) return true;
    json_t *content = json_obj_get(msg, "content");
    if (!content || content->type == JSON_NULL) return false;
    if (content->type == JSON_STRING && content->str_val && content->str_val[0] == '\0')
        return false;
    /* content is None → null; content == "" → empty string above */
    if (content->type == JSON_ARRAY) {
        /* bool(content) and isinstance(content[-1], dict) */
        if (content->c.count == 0) return false;
        json_t *last = content->c.items[content->c.count - 1];
        return last && last->type == JSON_OBJECT;
    }
    /* isinstance(content, str) — non-empty string */
    if (content->type == JSON_STRING) return true;
    /* Python falls through to `return isinstance(content, str)` — for dict/
     * number/bool content the function returns False (not a str). */
    return false;
}

/* ── _apply_cache_marker ─────────────────────────────────────────────────── */
/* PoP: _apply_cache_marker @ agent/prompt_caching.py:_apply_cache_marker */
static void pc_apply_cache_marker(json_t *msg, json_t *cache_marker,
                                  bool native_anthropic)
{
    if (!msg || msg->type != JSON_OBJECT || !cache_marker) return;

    json_t *role = json_obj_get(msg, "role");
    const char *role_str = (json_is_string(role) && role->str_val) ? role->str_val : "";

    json_t *content = json_obj_get(msg, "content");

    if (strcmp(role_str, "tool") == 0 && native_anthropic) {
        /* msg["cache_control"] = cache_marker */
        json_set(msg, "cache_control", json_copy(cache_marker));
        return;
    }

    bool content_none_or_empty = (!content || content->type == JSON_NULL ||
                                  (content->type == JSON_STRING &&
                                   (!content->str_val || content->str_val[0] == '\0')));

    if (content_none_or_empty) {
        if (strcmp(role_str, "tool") == 0 && !native_anthropic) return;  /* skip */
        if (strcmp(role_str, "assistant") == 0 && !native_anthropic) return; /* skip */
        json_set(msg, "cache_control", json_copy(cache_marker));
        return;
    }

    if (content && content->type == JSON_STRING) {
        /* msg["content"] = [ {"type":"text","text":content,"cache_control":marker} ] */
        json_t *part = json_object();
        json_set(part, "type", json_string("text"));
        json_set(part, "text", json_copy(content));
        json_set(part, "cache_control", json_copy(cache_marker));
        json_t *arr = json_array();
        json_array_append(arr, part);
        json_set(msg, "content", arr);
        return;
    }

    if (content && content->type == JSON_ARRAY && content->c.count > 0) {
        json_t *last = content->c.items[content->c.count - 1];
        if (last && last->type == JSON_OBJECT)
            json_set(last, "cache_control", json_copy(cache_marker));
    }
}

/* ── _apply_system_cache_markers ─────────────────────────────────────────── */
/* PoP: _apply_system_cache_markers @ agent/prompt_caching.py:_apply_system_cache_markers */
/* Returns the number of markers applied (0, 1, or 2). */
static int pc_apply_system_cache_markers(json_t *msg, json_t *marker,
                                         const char *static_system_prefix,
                                         bool native_anthropic,
                                         bool mark_suffix,
                                         bool fallback_to_whole)
{
    if (!msg || msg->type != JSON_OBJECT) return 0;
    json_t *content = json_obj_get(msg, "content");

    /* Only the prefix-split path handles string content. */
    if (content && content->type == JSON_STRING && content->str_val &&
        static_system_prefix && static_system_prefix[0]) {
        size_t plen = strlen(static_system_prefix);
        if (strncmp(content->str_val, static_system_prefix, plen) == 0) {
            const char *suffix = content->str_val + plen;
            if (suffix[0] != '\0') {
                /* Non-empty suffix: split into [marked-prefix, suffix_part]. */
                json_t *parts = json_array();
                json_t *p1 = json_object();
                json_set(p1, "type", json_string("text"));
                json_set(p1, "text", json_string(static_system_prefix));
                json_set(p1, "cache_control", json_copy(marker));
                json_array_append(parts, p1);
                json_t *p2 = json_object();
                json_set(p2, "type", json_string("text"));
                json_set(p2, "text", json_string(suffix));
                if (mark_suffix)
                    json_set(p2, "cache_control", json_copy(marker));
                json_array_append(parts, p2);
                json_set(msg, "content", parts);
                return mark_suffix ? 2 : 1;
            }
            /* Empty suffix: prompt IS the prefix.  Mark whole (no empty
             * block split).  Falls through to _apply_cache_marker below. */
        }
        /* Prefix given but doesn't match — fall through to fallback logic. */
    }

    if (!fallback_to_whole) return 0;
    pc_apply_cache_marker(msg, marker, native_anthropic);
    return 1;
}

/* ── apply_anthropic_cache_control ───────────────────────────────────────── */
/* PoP: apply_anthropic_cache_control @ agent/prompt_caching.py:apply_anthropic_cache_control */
/*
 * Returns a shallow copy of messages list with deep copies of modified
 * messages.  Caller frees with json_free.
 */
json_t *pca_apply_anthropic_cache_control(const json_t *api_messages,
                                          const char *cache_ttl,
                                          bool native_anthropic,
                                          const char *static_system_prefix)
{
    if (!api_messages || api_messages->type != JSON_ARRAY ||
        api_messages->c.count == 0) return json_copy(api_messages);

    /* messages = list(api_messages) — shallow copy of the array, deep copy
     * of mutated message dicts. */
    json_t *messages = json_array();
    if (!messages) return NULL;
    for (size_t i = 0; i < api_messages->c.count; i++)
        json_array_append(messages, json_copy(api_messages->c.items[i]));

    json_t *marker = pc_build_marker(cache_ttl);

    int breakpoints_used = 0;
    json_t *m0 = messages->c.items[0];
    json_t *r0 = m0 && m0->type == JSON_OBJECT ? json_obj_get(m0, "role") : NULL;
    if (json_is_string(r0) && r0->str_val && strcmp(r0->str_val, "system") == 0) {
        /* Deep-copy before mutation (Python: messages[0] = copy.deepcopy(...)) */
        json_t *deep = json_copy(m0);
        json_free(m0);
        messages->c.items[0] = deep;
        breakpoints_used = pc_apply_system_cache_markers(deep, marker,
                                                         static_system_prefix,
                                                         native_anthropic,
                                                         /*mark_suffix=*/true,
                                                         /*fallback_to_whole=*/true);
    }

    int remaining = 4 - breakpoints_used;
    /* non_sys = indices where role != system and _can_carry_marker(msg, ...) */
    int *non_sys = malloc(sizeof(int) * messages->c.count);
    if (!non_sys) { json_free(messages); json_free(marker); return messages; }
    size_t ns_count = 0;
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        if (!msg || msg->type != JSON_OBJECT) continue;
        json_t *role = json_obj_get(msg, "role");
        if (json_is_string(role) && role->str_val &&
            strcmp(role->str_val, "system") != 0 &&
            pc_can_carry_marker(msg, native_anthropic))
            non_sys[ns_count++] = (int)i;
    }
    /* for idx in non_sys[-remaining:] — last `remaining` eligible messages */
    int start = (int)ns_count - remaining;
    if (start < 0) start = 0;
    for (int s = start; s < (int)ns_count; s++) {
        int idx = non_sys[s];
        json_t *msg = json_copy(messages->c.items[idx]);
        json_free(messages->c.items[idx]);
        messages->c.items[idx] = msg;
        pc_apply_cache_marker(msg, marker, native_anthropic);
    }

    free(non_sys);
    json_free(marker);
    return messages;
}

/* ── _completed_transaction_endpoint_indexes ───────────────────────────── */
/* PoP: _completed_transaction_endpoint_indexes @ agent/prompt_caching.py:_completed_transaction_endpoint_indexes */
/* Returns a json_t array of int endpoints. */
json_t *pc_completed_transaction_endpoint_indexes(const json_t *messages,
                                                     bool native_anthropic)
{
    json_t *endpoints = json_array();
    if (!messages || messages->type != JSON_ARRAY) return endpoints;

    size_t index = 0;
    while (index < messages->c.count) {
        json_t *message = messages->c.items[index];
        if (!message || message->type != JSON_OBJECT) { index++; continue; }

        json_t *role = json_obj_get(message, "role");
        const char *role_str = (json_is_string(role) && role->str_val) ?
                               role->str_val : "";
        if (strcmp(role_str, "system") == 0) { index++; continue; }

        if (strcmp(role_str, "assistant") == 0 &&
            json_obj_get(message, "tool_calls")) {
            size_t result_start = index + 1;
            size_t result_end = result_start;
            while (result_end < messages->c.count) {
                json_t *result = messages->c.items[result_end];
                if (!result || result->type != JSON_OBJECT) break;
                json_t *rr = json_obj_get(result, "role");
                if (!json_is_string(rr) || !rr->str_val ||
                    strcmp(rr->str_val, "tool") != 0) break;
                result_end++;
            }
            if (result_end > result_start) {
                json_t *endpoint_msg = messages->c.items[result_end - 1];
                if (pc_can_carry_marker(endpoint_msg, native_anthropic))
                    json_array_append(endpoints, json_int((double)(result_end - 1)));
            }
            index = result_end;
            continue;
        }

        if (strcmp(role_str, "tool") == 0) {
            while (index < messages->c.count) {
                json_t *result = messages->c.items[index];
                if (!result || result->type != JSON_OBJECT) break;
                json_t *rr = json_obj_get(result, "role");
                if (!json_is_string(rr) || !rr->str_val ||
                    strcmp(rr->str_val, "tool") != 0) break;
                index++;
            }
            continue;
        }

        if (strcmp(role_str, "user") == 0 && index + 1 < messages->c.count) {
            index++; continue;
        }

        if (strcmp(role_str, "assistant") == 0) {
            json_t *content = json_obj_get(message, "content");
            if (!content || content->type == JSON_NULL ||
                (content->type == JSON_STRING &&
                 (!content->str_val || content->str_val[0] == '\0'))) {
                index++; continue;
            }
        }

        if (pc_can_carry_marker(message, native_anthropic))
            json_array_append(endpoints, json_int((double)index));
        index++;
    }
    return endpoints;
}

/* ── _count_cache_markers ────────────────────────────────────────────────── */
/* PoP: _count_cache_markers @ agent/prompt_caching.py:_count_cache_markers */
int pca_count_cache_markers(const json_t *messages, const json_t *tools)
{
    int count = 0;
    if (messages && json_is_array(messages)) {
        for (size_t i = 0; i < messages->c.count; i++) {
            json_t *m = messages->c.items[i];
            if (is_dict(m) && json_obj_get(m, "cache_control"))
                count++;
            json_t *content = m && m->type == JSON_OBJECT ?
                              json_obj_get(m, "content") : NULL;
            if (json_is_array(content)) {
                for (size_t p = 0; p < content->c.count; p++) {
                    json_t *part = content->c.items[p];
                    if (is_dict(part) && json_obj_get(part, "cache_control"))
                        count++;
                }
            }
        }
    }
    if (tools && json_is_array(tools)) {
        for (size_t i = 0; i < tools->c.count; i++) {
            json_t *t = tools->c.items[i];
            if (is_dict(t) && json_obj_get(t, "cache_control"))
                count++;
        }
    }
    return count;
}

/* ── build_prompt_cache_plan ─────────────────────────────────────────────── */
/* PoP: build_prompt_cache_plan @ agent/prompt_caching.py:build_prompt_cache_plan */
/*
 * Returns a PromptCachePlan-like json_t object:
 *   {"messages": <json_t array>, "tools": <json_t array>}
 * Caller frees with json_free.
 */
json_t *pca_build_prompt_cache_plan(const json_t *api_messages,
                                    const json_t *tools,
                                    const char *cache_ttl,
                                    bool native_anthropic,
                                    const char *static_system_prefix,
                                    bool direct_native_tool_cache)
{
    /* messages = copy.deepcopy(api_messages or []); strip first */
    json_t *messages;
    if (!api_messages) messages = json_array();
    else messages = json_copy(api_messages);
    if (!messages) return NULL;

    /* strip_anthropic_cache_control is idempotent (no-op here since we just
     * deep-copied from the caller).  Still call it to mirror Python. */
    pca_strip_anthropic_cache_control(messages);

    json_t *planned_tools = pca_strip_anthropic_tool_cache_control(tools);

    if (!direct_native_tool_cache || !planned_tools || planned_tools->c.count == 0) {
        /* apply_anthropic_cache_control → planned_messages */
        json_t *planned_messages = pca_apply_anthropic_cache_control(
            messages, cache_ttl, native_anthropic, static_system_prefix);
        json_free(messages);
        json_t *plan = json_object();
        json_set(plan, "messages", planned_messages);
        json_set(plan, "tools", planned_tools);
        return plan;
    }

    /* Tool-cache layout: system gets marker + tools[-1] gets marker + endpoints */
    /* marker = _build_marker(cache_ttl) */
    json_t *marker = pc_build_marker(cache_ttl);

    json_t *m0 = messages->c.count > 0 ? messages->c.items[0] : NULL;
    json_t *r0 = m0 && m0->type == JSON_OBJECT ?
                 json_obj_get(m0, "role") : NULL;
    if (m0 && json_is_string(r0) && r0->str_val &&
        strcmp(r0->str_val, "system") == 0) {
        /* Tool-cache layout: only the static prefix carries a marker;
         * volatile suffix budget spent on tools array. */
        pc_apply_system_cache_markers(m0, marker,
                                      static_system_prefix,
                                      /*native_anthropic=*/true,
                                      /*mark_suffix=*/false,
                                      /*fallback_to_whole=*/false);
    }

    /* planned_tools[-1]["cache_control"] = dict(marker) */
    if (planned_tools && planned_tools->type == JSON_ARRAY &&
        planned_tools->c.count > 0) {
        json_t *last_tool = planned_tools->c.items[planned_tools->c.count - 1];
        if (last_tool && last_tool->type == JSON_OBJECT)
            json_set(last_tool, "cache_control", json_copy(marker));
    }

    /* Apply markers to the last 2 completed endpoints */
    json_t *endpoints = pc_completed_transaction_endpoint_indexes(messages, true);
    size_t n = endpoints->c.count;
    size_t start = n >= 2 ? n - 2 : 0;
    for (size_t i = start; i < n; i++) {
        json_t *ep = endpoints->c.items[i];
        if (!ep || ep->type != JSON_NUMBER) continue;
        size_t idx = (size_t)ep->num_val;
        if (idx < messages->c.count) {
            json_t *msg = json_copy(messages->c.items[idx]);
            json_free(messages->c.items[idx]);
            messages->c.items[idx] = msg;
            pc_apply_cache_marker(msg, marker, true);
        }
    }
    json_free(endpoints);
    json_free(marker);

    json_t *plan = json_object();
    json_set(plan, "messages", messages);
    json_set(plan, "tools", planned_tools);
    return plan;
}

/* ── PromptCachePlan.marker_count (lazy @property) ───────────────────────── */
/* PoP: marker_count @ agent/prompt_caching.py:marker_count */
int pca_prompt_cache_plan_marker_count(const json_t *plan)
{
    if (!plan || plan->type != JSON_OBJECT) return 0;
    return pca_count_cache_markers(json_obj_get(plan, "messages"),
                                    json_obj_get(plan, "tools"));
}

/* ── anthropic_prompt_cache_policy ───────────────────────────────────────── */
/* PoP: anthropic_prompt_cache_policy @ agent/agent_runtime_helpers.py:anthropic_prompt_cache_policy
 * Resolves (should_cache, use_native_layout) from provider/base_url/api_mode/model.
 * MoA branch & kimi/alibabra/minimax sub-dispatch included. */
void pca_anthropic_prompt_cache_policy(const char *provider,
                                        const char *base_url,
                                        const char *api_mode,
                                        const char *model,
                                        bool cache_disabled,
                                        bool *out_should_cache,
                                        bool *out_native_layout)
{
    bool should_cache = false, native_layout = false;

    /* if getattr(agent, "_cache_disabled", False): return (False, False) */
    if (cache_disabled) goto done;

    /* MoA provider: resolve aggregator slot.  Defensive — Python catches
     * exceptions & returns (False, False) on failure.  We don't have moa_config
     * wired here, so treat MoA as non-caching (matches the except-branch
     * outcome for unresolved aggregator slots). */
    char eff_provider[256], eff_base_url[512], eff_api_mode[128], eff_model[256];
    snprintf(eff_provider,  sizeof(eff_provider),  "%s", provider  ? provider  : "");
    snprintf(eff_base_url,   sizeof(eff_base_url),  "%s", base_url  ? base_url  : "");
    snprintf(eff_api_mode,   sizeof(eff_api_mode),  "%s", api_mode  ? api_mode  : "");
    snprintf(eff_model,      sizeof(eff_model),     "%s", model     ? model     : "");

    char prov_lower[256], mlower[256], blower[512];
    for (size_t i = 0; i < sizeof(prov_lower)-1 && eff_provider[i]; i++)
        prov_lower[i] = (char)tolower((unsigned char)eff_provider[i]);
    prov_lower[sizeof(prov_lower)-1] = '\0';
    for (size_t i = 0; i < sizeof(mlower)-1 && eff_model[i]; i++)
        mlower[i] = (char)tolower((unsigned char)eff_model[i]);
    mlower[sizeof(mlower)-1] = '\0';
    for (size_t i = 0; i < sizeof(blower)-1 && eff_base_url[i]; i++)
        blower[i] = (char)tolower((unsigned char)eff_base_url[i]);
    blower[sizeof(blower)-1] = '\0';

    /* is_native: api_mode == "anthropic_messages" and
     * (provider == "anthropic" or base_url host == "api.anthropic.com") */
    bool is_anthropic_wire = (strcmp(eff_api_mode, "anthropic_messages") == 0);
    char *host = provider_base_url_hostname(eff_base_url);
    bool is_native_anthropic = is_anthropic_wire &&
        (strcasecmp(eff_provider, "anthropic") == 0 ||
         (host && strcasecmp(host, "api.anthropic.com") == 0));
    free(host);

    if (is_native_anthropic) { should_cache = true; native_layout = true; goto done; }

    /* is_claude = "claude" in model_lower */
    bool is_claude = strstr(mlower, "claude") != NULL;

    /* is_kimi */
    bool is_kimi = antr_model_name_is_kimi_family(eff_model) ||
        (strstr(mlower, "moonshot") != NULL);

    /* is_openrouter / is_nous_portal */
    bool is_openrouter = provider_base_url_host_matches(eff_base_url, "openrouter.ai");
    bool is_nous_portal = strstr(blower, "nousresearch") != NULL;

    /* (openrouter or nous_portal) and (is_claude or is_kimi) and not native */
    if ((is_openrouter || is_nous_portal) && (is_claude || is_kimi) && !is_anthropic_wire) {
        should_cache = true; native_layout = false; goto done;
    }

    /* nous_portal + qwen → envelope */
    if (is_nous_portal && strstr(mlower, "qwen") != NULL) {
        should_cache = true; native_layout = false; goto done;
    }

    /* third-party anthropic_messages + claude → native */
    if (is_anthropic_wire && is_claude) {
        should_cache = true; native_layout = true; goto done;
    }

    /* MiniMax on Anthropic-compatible endpoint */
    if (is_anthropic_wire) {
        bool is_mm_provider = strcasecmp(prov_lower, "minimax") == 0 ||
                              strcasecmp(prov_lower, "minimax-cn") == 0;
        bool is_mm_host = provider_base_url_host_matches(eff_base_url, "api.minimax.io") ||
                          provider_base_url_host_matches(eff_base_url, "api.minimaxi.com");
        if (is_mm_provider || is_mm_host) {
            should_cache = true; native_layout = true; goto done;
        }
    }

    /* Qwen/Alibaba family on OpenCode/DashScope → envelope */
    bool model_is_qwen = strstr(mlower, "qwen") != NULL;
    bool provider_is_alibaba = strcasecmp(prov_lower, "opencode") == 0 ||
                               strcasecmp(prov_lower, "opencode-zen") == 0 ||
                               strcasecmp(prov_lower, "opencode-go") == 0 ||
                               strcasecmp(prov_lower, "alibaba") == 0;
    if (provider_is_alibaba && model_is_qwen) {
        should_cache = true; native_layout = false; goto done;
    }

done:
    if (out_should_cache) *out_should_cache = should_cache;
    if (out_native_layout) *out_native_layout = native_layout;
}

/* ── plan_cache_sections_for_destination ─────────────────────────────────── */
/* PoP: pca_plan_cache_sections_for_destination @ agent/agent_runtime_helpers.py:plan_cache_sections_for_destination */
/*
 * Returns a json_t object {"messages":<array>,"tools":<array>} of
 * request-local copies (caller frees with json_free).  cache_ttl is the
 * resolved TTL string (e.g. "5m"); cache_disabled is the policy stub flag.
 */
json_t *pca_plan_cache_sections_for_destination(const json_t *messages,
                                                  const json_t *tools,
                                                  const char *provider,
                                                  const char *base_url,
                                                  const char *api_mode,
                                                  const char *model,
                                                  const char *cache_ttl,
                                                  bool cache_disabled)
{
    bool should_cache = false, native_layout = false;
    pca_anthropic_prompt_cache_policy(provider, base_url, api_mode, model,
                                      cache_disabled, &should_cache, &native_layout);

    if (!should_cache) {
        json_t *canonical = json_copy(messages ? messages : json_array());
        pca_strip_anthropic_cache_control(canonical);
        json_t *canonical_tools = pca_strip_anthropic_tool_cache_control(tools);
        json_t *result = json_object();
        json_set(result, "messages", canonical);
        json_set(result, "tools", canonical_tools);
        return result;
    }

    bool direct_native = direct_native_anthropic_tool_cache_capability(
            provider, base_url, api_mode, model);

    json_t *plan = pca_build_prompt_cache_plan(
        messages, tools,
        cache_ttl ? cache_ttl : "5m",
        native_layout,
        /*static_system_prefix=*/NULL,
        direct_native);
    return plan;
}
