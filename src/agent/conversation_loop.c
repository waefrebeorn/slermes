/*
 * conversation_loop.c — Port of Python agent/conversation_loop.py
 *
 * Contains run_conversation() — the main LLM tool-calling loop —
 * and all conversation-loop-level helpers (tool dispatch, snapshot,
 * chat, injection, inference routing, billing, etc.).
 *
 * Split from agent_loop.c (lines 748+).
 * Turn finalization delegated to turn_finalizer.c:finalize_turn().
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_agent.h"
#include "approval.h"
#include "provider.h"
#include "plugin.h"
#include "hermes_system_prompt.h"
#include "budget_tracker.h"
#include "hermes_subdir_hints.h"
#include "hermes_onboarding.h"
#include "hermes_logger.h"
#include "hermes_hooks.h"
#include "image_routing.h"
#include "hermes_trajectory.h"
#include "nous_rate_guard.h"
#include "hermes_url_safety.h"
#include "hermes_tool_guardrails.h"
#include "acp/edit_approval.h"
#include "todo_hydrate.h"
#include "file_mutation_verifier.h"
#include "provider_metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <strings.h>
#ifndef _WIN32
#include <pthread.h>
#include "hermes_sanitize.h"
#include "hermes_memory.h"
#include "registry.h"
#endif

/* ================================================================
 *  Core loop
 * ================================================================ */

/* P87: Tool dispatch thread wrapper (for pthread_create) */
struct tool_dispatch_arg {
    const char *session_id;
    char *tool_name;
    char *tool_args;
    char **result_out;
    int  *duration_ms_out;  /* S14 gap #13: latency tracking */
};

static void *tool_dispatch_thread(void *arg) {
    struct tool_dispatch_arg *a = (struct tool_dispatch_arg *)arg;
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    *a->result_out = registry_dispatch(a->tool_name, a->tool_args, a->session_id);
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    if (a->duration_ms_out) {
        int64_t ns = (int64_t)(ts_end.tv_sec - ts_start.tv_sec) * 1000000000L +
                     (int64_t)(ts_end.tv_nsec - ts_start.tv_nsec);
        *a->duration_ms_out = (int)(ns / 1000000);
    }
    return NULL;
}

/* P93: Tool result classification */
typedef enum {
    TOOL_RESULT_SUCCESS = 0,
    TOOL_RESULT_ERROR,   /* Non-fatal error — loop continues */
    TOOL_RESULT_FATAL,   /* Fatal error — abort loop */
} tool_result_class_t;

/* Classify a tool result string. Returns classification level. */
static tool_result_class_t classify_tool_result(const char *result, const char *tool_name) {
    (void)tool_name;
    if (!result || !*result) return TOOL_RESULT_ERROR;

    /* Check for error patterns in JSON result */
    if (strstr(result, "\"error\"")) {
        /* Check for fatal patterns */
        if (strstr(result, "fatal") || strstr(result, "FATAL") ||
            strstr(result, "not found") || strstr(result, "permission denied") ||
            strstr(result, "access denied") || strstr(result, "unauthorized"))
            return TOOL_RESULT_FATAL;
        return TOOL_RESULT_ERROR;
    }

    return TOOL_RESULT_SUCCESS;
}

/* Convert tool registry to JSON for tool_choice */
static json_node_t *tools_to_json(tool_registry_t *reg) {
    json_node_t *tools = json_new_array();
    for (size_t i = 0; i < reg->count; i++) {
        if (!reg->tools[i].available) continue;

        json_node_t *tool = json_new_object();
        json_object_set(tool, "type", json_new_string("function"));

        json_node_t *fn = json_new_object();
        json_object_set(fn, "name", json_new_string(reg->tools[i].name));
        json_object_set(fn, "description", json_new_string(reg->tools[i].description));

        /* Parse schema — ensure valid JSON Schema even when none provided */
        if (reg->tools[i].schema_json[0]) {
            char *err = NULL;
            json_node_t *schema = json_parse(reg->tools[i].schema_json, &err);
            if (schema) {
                /* Ensure schema has "type": "object" — required by strict providers */
                json_object_set(schema, "type", json_new_string("object"));
                json_object_set(fn, "parameters", schema);
            } else {
                json_node_t *params = json_new_object();
                json_object_set(params, "type", json_new_string("object"));
                json_object_set(params, "properties", json_new_object());
                json_object_set(fn, "parameters", params);
                free(err);
            }
        } else {
            json_node_t *params = json_new_object();
            json_object_set(params, "type", json_new_string("object"));
            json_object_set(params, "properties", json_new_object());
            json_object_set(fn, "parameters", params);
        }

        json_object_set(tool, "function", fn);
        json_array_append(tools, tool);
    }
    return tools;
}

/* Port of Python: run_conversation */
/*     Also handles build_turn_context inline (agent/turn_context.py) —
 *     the Python function constructs a TurnContext dataclass; C does
 *     the same setup directly in the loop prologue (lines 846-972+). */
char *run_conversation(agent_state_t *state,
                              const char *user_message,
                              const char *system_message)
{
    /* Set system message if provided */
    if (system_message && system_message[0])
        context_set_system(state, system_message);

    hermes_log(LOG_INFO, "agent_loop", "Starting conversation (user msg: %.80s)", user_message ? user_message : "(null)");

    /* P92: Inject prefill message (before user message) */
    if (state->prefill[0]) {
        message_t *prefill_msg = message_new(state->prefill_role, state->prefill);
        if (prefill_msg) context_push(state, prefill_msg);
    }

    /* ME01: Trigger background memory prefetch on user message */
    if (state->memory && user_message && user_message[0]) {
        /* Free any previous prefetch result */
        if (state->prefetch_result) {
            free(state->prefetch_result);
            state->prefetch_result = NULL;
        }
        state->prefetch_in_progress = 0;
/* PoP: prefetch @ agent/memory_provider.py:prefetch */
        /* Do synchronous prefetch (C is single-threaded per agent, so
           background thread adds complexity without benefit for the
           typical case). Search memory with the user query. */
        memory_entry_t *entry = memory_search(state->memory, user_message, 5);
        if (entry) {
            state->prefetch_result = strdup(entry->content);
        } else {
            state->prefetch_result = NULL;
        }
        state->prefetch_in_progress = 2; /* complete */
    }

    /* Onboarding: check for OpenClaw residue on first conversation */
    {
        char *opath = onboarding_default_path();
        if (opath && !is_seen(opath, ONBOARDING_OPENCLAW_RESIDUE_FLAG)) {
            if (detect_openclaw_residue()) {
                printf("%s\n", openclaw_residue_hint_cli());
                mark_seen(opath, ONBOARDING_OPENCLAW_RESIDUE_FLAG);
            }
        }
        free(opath);
    }

    /* Initialize subdirectory hint tracker */
    subdir_hints_init(NULL);

    /* GAP1: Hydrate todo state from conversation history on session resume */
    todo_hydrate_from_context(state);

    /* GAP2: Initialize per-turn file mutation tracker */
    if (!state->file_mutations) {
        state->file_mutations = (file_mutation_tracker_t *)calloc(1, sizeof(file_mutation_tracker_t));
    }
    if (state->file_mutations)
        file_mutation_tracker_init(state->file_mutations);

    /* Add user message (with surrogate character sanitization) */
    char *clean_input = sanitize_surrogates(user_message);
    const char *msg_content = clean_input ? clean_input : (user_message ? user_message : "");
    message_t *user_msg = message_new(MSG_USER, msg_content);
    free(clean_input);
    if (!user_msg) return strdup("Error: OOM");
    context_push(state, user_msg);
    /* G09: Count this as a user turn */
    state->user_turn_count++;

    /* G10: Set last activity on user message */
    state->last_activity_ts = time(NULL);

    /* L09: Memory nudge — check if we should suggest a memory review */
    if (state->memory_nudge_interval > 0) {
        state->turns_since_memory++;
        if (state->turns_since_memory >= state->memory_nudge_interval) {
            state->turns_since_memory = 0;
            /* Inject memory nudge via steer queue */
            if (state->steer_count < HERMES_MAX_STEERS) {
                snprintf(state->steer_queue[state->steer_count],
                         sizeof(state->steer_queue[0]),
                         "[Memory reminder] You can use the memory tool to save "
                         "or retrieve information. Consider updating memory with "
                         "any new facts from this session.");
                state->steer_roles[state->steer_count] = MSG_SYSTEM;
                state->steer_count++;
            }
        }
    }

    /* G33-G34: Process steer queue — inject all queued steers in priority order */
    if (state->steer_count > 0) {
        for (int si = 0; si < state->steer_count && si < HERMES_MAX_STEERS; si++) {
            if (state->steer_queue[si][0]) {
                message_role_t role = state->steer_roles[si];
                /* Map invalid/unknown roles to system message (default steer type) */
                if (role != MSG_SYSTEM && role != MSG_USER && role != MSG_ASSISTANT)
                    role = MSG_SYSTEM;
                message_t *steer_msg = message_new(role, state->steer_queue[si]);
                if (steer_msg) context_push(state, steer_msg);
            }
        }
        /* Clear queue after processing */
        for (int si = 0; si < state->steer_count && si < HERMES_MAX_STEERS; si++)
            state->steer_queue[si][0] = '\0';
        state->steer_count = 0;
    }
    /* Backward compat: process single pending_steer if queue is empty */
    else if (state->pending_steer[0]) {
        message_t *steer_msg = message_new(MSG_SYSTEM, state->pending_steer);
        if (steer_msg) context_push(state, steer_msg);
        state->pending_steer[0] = '\0';
    }

    /* Build tools JSON from registry */
    /* S14 gap #9: Refresh toolset availability before building tool list */
    registry_refresh_availability();
    json_node_t *tools_json = NULL;
    if (state->tools.count > 0) {
        /* P150: Apply enabled/disabled toolsets filter before building tool list */
        if (state->enabled_toolsets[0] || state->disabled_toolsets[0]) {
            tool_registry_t filtered = state->tools;
            registry_filter_by_toolset(&filtered,
                state->enabled_toolsets[0] ? state->enabled_toolsets : NULL,
                state->disabled_toolsets[0] ? state->disabled_toolsets : NULL);
            tools_json = tools_to_json(&filtered);
        } else {
            tools_json = tools_to_json(&state->tools);
        }
    }

    /* Build environment hints (Host, Home, cwd, WSL) — always inject */
    char *env_hints = build_environment_hints();

    /* Build default identity if no personality/system_message set */
    char *identity = NULL;
    if (!state->system_message[0]) {
        /* Use default identity + guidance when no personality configured */
        system_prompt_config_t sp_cfg;
        memset(&sp_cfg, 0, sizeof(sp_cfg));
        sp_cfg.use_soul = true;
        sp_cfg.has_memory = true;
        sp_cfg.has_session_search = true;
        sp_cfg.has_skills = true;
        sp_cfg.has_kanban = true;
        sp_cfg.has_computer_use = true;
        sp_cfg.enforce_tools = true;
        /* U05-U06: Provider family detection for guidance injection */
        if (strcmp(state->llm.provider, "google") == 0) {
            sp_cfg.is_google_family = true;
            sp_cfg.is_openai_family = false;
        } else {
            sp_cfg.is_openai_family = true;
        }
        if (strncmp(state->llm.provider, "alibaba", 7) == 0) {
            sp_cfg.is_alibaba = true;
            sp_cfg.alibaba_model = state->llm.model;
        }
        sp_cfg.platform_hint = platform_hint_get("cli");
        if (state->hermes_home[0]) {
            char profile_path[4096];
            snprintf(profile_path, sizeof(profile_path), "%s/USER.md", state->hermes_home);
            struct stat st;
            if (stat(profile_path, &st) == 0 && st.st_size > 0)
                sp_cfg.user_profile = "User profile loaded."; /* signal that profile exists */
        }
        identity = system_prompt_build_stable(&sp_cfg);
    }

    /* U02-U04: Build volatile tier (model, provider, session, timestamp, user profile) */
    system_prompt_config_t vol_cfg;
    memset(&vol_cfg, 0, sizeof(vol_cfg));
    vol_cfg.model_name = state->llm.model[0] ? state->llm.model : NULL;
    vol_cfg.provider_name = state->llm.provider[0] ? state->llm.provider : NULL;
    vol_cfg.session_id = state->session_id[0] ? state->session_id : NULL;
    vol_cfg.pass_session_id = (vol_cfg.session_id != NULL);

    /* U03: Load user profile from USER.md */
    char *user_profile_content = NULL;
    if (state->hermes_home[0]) {
        char profile_path[4096];
        snprintf(profile_path, sizeof(profile_path), "%s/USER.md", state->hermes_home);
        FILE *fp = fopen(profile_path, "rb");
        if (fp) {
            struct stat st;
            if (stat(profile_path, &st) == 0 && st.st_size > 0 && st.st_size < 65536) {
                user_profile_content = (char *)malloc((size_t)st.st_size + 1);
                if (user_profile_content) {
                    size_t n = fread(user_profile_content, 1, (size_t)st.st_size, fp);
                    user_profile_content[n] = '\0';
                    vol_cfg.user_profile = user_profile_content;
                }
            }
            fclose(fp);
        }
    }

    /* S1: Inject memory snapshot into volatile prompt */
    char *memory_block = NULL;
    if (state->memory) {
        /* ME01: Use prefetch result if available (fresh search on user query) */
        if (state->prefetch_result && state->prefetch_in_progress == 2) {
            memory_block = strdup(state->prefetch_result);
        } else {
            memory_block = memory_format_snapshot(state->memory, state->memory->search_limit);
        }
        if (memory_block) {
            vol_cfg.memory_snapshot = memory_block;
        }
    }

    char *volatile_block = system_prompt_build_volatile(&vol_cfg);

    /* Load context files (SOUL.md, .hermes.md, AGENTS.md, etc.) */
    /* AG19: Use resolve_agent_cwd() for proper CWD resolution
     * (session override > TERMINAL_CWD > PWD > getcwd()). */
    const char *cwd = resolve_agent_cwd();
    char *context_block = build_context_files_prompt(cwd, true);

    /* Assemble final system message: env_hints + [identity] + [context] + [volatile] + [personality] */
    size_t total = 0;
    if (env_hints) total += strlen(env_hints) + 3;
    if (identity) total += strlen(identity) + 3;
    if (context_block) total += strlen(context_block) + 3;
    if (volatile_block) total += strlen(volatile_block) + 3;
    if (state->system_message[0]) total += strlen(state->system_message);
    total += 1;

    char *full_sys = malloc(total);
    if (full_sys) {
        full_sys[0] = '\0';
        if (env_hints) {
            strcat(full_sys, env_hints);
            strcat(full_sys, "\n\n");
        }
        if (identity) {
            strcat(full_sys, identity);
            strcat(full_sys, "\n\n");
        }
        if (context_block && context_block[0]) {
            strcat(full_sys, context_block);
            strcat(full_sys, "\n\n");
        }
        if (volatile_block) {
            strcat(full_sys, volatile_block);
            strcat(full_sys, "\n\n");
        }
        if (state->system_message[0]) {
            strcat(full_sys, state->system_message);
        }
        context_set_system(state, full_sys);
        free(full_sys);
    }
    free(env_hints);
    free(identity);
    free(volatile_block);
    free(memory_block);
    free(user_profile_content);
    free(context_block);

    /* G13-G14: Apply runtime tool_choice/parallel_tool_calls overrides */
    if (state->tool_choice[0])
        snprintf(state->llm.tool_choice, sizeof(state->llm.tool_choice), "%s", state->tool_choice);
    if (state->parallel_tool_calls)
        state->llm.parallel_tool_calls = true;

    int iteration = 0;
    bool grace_call = false; /* P88: one extra LLM call without tools after budget */

    while (iteration < state->max_iterations && !state->interrupted &&
           !(iteration > 0 && grace_call)) {
        state->iteration_count = iteration;

        /* L14: Update log context for this turn */
        hermes_log_set_context(
            state->session_id[0] ? state->session_id : NULL,
            state->llm.model[0] ? state->llm.model : NULL,
            state->llm.provider[0] ? state->llm.provider : NULL,
            iteration);

        /* P86: Check budget exceeded — trigger grace call */
        if (state->budget && budget_tracker_is_exceeded(state->budget) && !grace_call) {
            grace_call = true;
            /* Remove tools for grace call so LLM just summarizes */
            if (tools_json) { json_free(tools_json); tools_json = NULL; }
        }

        /* Smart context compression: summarize old messages before dropping */
        /* G22: Use adaptive threshold from compression feedback */
        float adaptive_threshold = compression_feedback_get_threshold(
            &state->compression_fb, HERMES_MAX_CTX_TOKENS);
        size_t adaptive_max = (size_t)(adaptive_threshold > 0 ?
            adaptive_threshold : HERMES_MAX_CTX_TOKENS);
        /* P99: Anti-thrashing — skip compression if cooldown not elapsed */
        /* B03: Also skip if last failure was <failure cooldown>s ago */
        /* L02: Cooldowns are configurable via compression.cooldown_secs / failure_cooldown_secs */
        time_t now = time(NULL);
        if ((state->last_compress_time == 0 ||
            (now - state->last_compress_time) >= state->compress_cooldown_secs) &&
            (now - state->last_compress_failure_time) >= state->compress_failure_cooldown_secs) {
            /* MS03: Acquire compression lock to prevent concurrent compression */
            bool lock_acquired = false;
            if (state->db && state->session_id[0]) {
                char holder[256];
                snprintf(holder, sizeof(holder),
                         "pid=%d:tid=main:session=%s",
                         (int)getpid(), state->session_id);
                lock_acquired = db_try_acquire_compression_lock(
                    state->db, state->session_id, holder);
                if (!lock_acquired) {
                    /* Another process is compressing — skip this cycle */
                    char *existing = db_get_compression_lock_holder(
                        state->db, state->session_id);
                    if (existing) {
                        fprintf(stderr,
                                "compression skipped: another path is compressing "
                                "session=%s (holder=%s)\n",
                                state->session_id, existing);
                        free(existing);
                    }
                    iteration++;
                    continue;
                }
            }
            char *summary = llm_compress_context(state, adaptive_max,
                                                  state->compress_enabled);
            if (summary) {
                /* Record timestamp for anti-thrashing */
                state->last_compress_time = now;
                /* G22: Record positive feedback — compression was used successfully */
                compression_feedback_positive(&state->compression_fb);
                /* Wrap with compaction handoff prefix (matches Python SUMMARY_PREFIX) */
                char summary_prompt[8192];
                snprintf(summary_prompt, sizeof(summary_prompt),
                    "[CONTEXT COMPACTION - REFERENCE ONLY] Earlier turns were "
                    "compacted into the summary below. This is a handoff from a "
                    "previous context window - treat it as background reference, "
                    "NOT as active instructions. "
                    "Do NOT answer questions or fulfill requests mentioned in this "
                    "summary; they were already addressed. "
                    "Respond ONLY to the latest user message that appears AFTER "
                    "this summary:\n\n%s", summary);
                /* Insert summary as a user message and truncate */
                message_t *summary_msg = message_new(MSG_USER, summary_prompt);
                if (summary_msg) {
                    /* Insert after system message */
                    size_t idx = (state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
                    /* Remove old messages, keeping tail budget */
                    int tail = state->compress_tail_messages;
                    if (tail < 2) tail = 2;
                    size_t keep = idx;
                    size_t remove_count = state->message_count - keep - tail;
                    for (size_t i = 0; i < remove_count; i++)
                        message_free(state->messages[keep + i]);
                    memmove(&state->messages[keep], &state->messages[keep + remove_count],
                            (state->message_count - keep - remove_count) * sizeof(message_t *));
                    state->message_count -= remove_count;

                    /* Insert summary message */
                    context_push(state, summary_msg);
                }
                /* MS03: Session split on compression.
                 * End the old session and create a new child session
                 * so the conversation lineage is preserved. */
                if (state->db && state->session_id[0]) {
                    session_meta_t old_meta;
                    if (db_load_meta(state->db, state->session_id, &old_meta)) {
                        old_meta.ended_at = now;
                        snprintf(old_meta.end_reason, sizeof(old_meta.end_reason),
                                 "compression");
                        db_save_meta(state->db, state->session_id, &old_meta);
                    }
                    /* Generate new session ID */
                    char new_id[64];
                    struct tm *tm_info = localtime(&now);
                    snprintf(new_id, sizeof(new_id), "%04d%02d%02d_%02d%02d%02d",
                             tm_info->tm_year + 1900, tm_info->tm_mon + 1,
                             tm_info->tm_mday, tm_info->tm_hour,
                             tm_info->tm_min, tm_info->tm_sec);
                    /* Create new session with parent link */
                    session_meta_t new_meta;
                    db_meta_init(&new_meta);
                    if (state->llm.model[0])
                        snprintf(new_meta.model, sizeof(new_meta.model), "%s",
                                 state->llm.model);
                    snprintf(new_meta.title, sizeof(new_meta.title), "%s", new_id);
                    snprintf(new_meta.parent_id, sizeof(new_meta.parent_id), "%s",
                             state->session_id);
                    new_meta.branch_point = (int)state->message_count;
                    if (db_save(state->db, new_id, "[]") &&
                        db_save_meta(state->db, new_id, &new_meta)) {
                        /* Save current messages into new session */
                        char *json = serialize_messages(state);
                        if (json) {
                            db_save(state->db, new_id, json);
                            free(json);
                        }
                        /* Update agent state to new session */
                        snprintf(state->session_id, sizeof(state->session_id),
                                 "%s", new_id);
                    }
                }
                free(summary);
            } else {
                /* B03: Record failure time for cooldown */
                state->last_compress_failure_time = time(NULL);
            }
            /* MS03: Release compression lock */
            if (lock_acquired && state->db && state->session_id[0]) {
                db_release_compression_lock(state->db, state->session_id,
                                            NULL);
            }
        }

        /* Truncate context if too long (128K token budget) */
        llm_truncate_context(state, HERMES_MAX_CTX_TOKENS);

        /* G21: Apply strategy-aware eviction if non-default strategy configured */
        if (state->compression_strategy != COMPRESS_OLDEST_TOOL_FIRST && state->message_count > 20) {
            context_evict_smart(state, 20, state->compression_strategy);
        }

        /* N01: Pre-request context window check — warn if approaching limit */
        {
            size_t ctx_max = hermes_token_context_size(state->llm.model);
            if (ctx_max > 0) {
                size_t est_msg_tok = 0;
                for (size_t i = 0; i < state->message_count && est_msg_tok < ctx_max; i++) {
                    if (state->messages[i])
                        est_msg_tok += hermes_token_count(state->messages[i]->content,
                            hermes_token_family_from_model(state->llm.model));
                    est_msg_tok += 5; /* overhead per message */
                }
                if (est_msg_tok > ctx_max * 0.9) {
                    fprintf(stderr, "[context] ~%zu/%zu tokens (%zu%%) — approaching model limit\n",
                            est_msg_tok, ctx_max, est_msg_tok * 100 / ctx_max);
                }
            }
        }

        /* S14 #1 gap #4: Repair message sequence for provider compatibility.
         * Drops stray tool messages and merges consecutive user messages
         * (tool→user/user→user wedges). Prevents provider API errors. */
        {
            int msg_count = (int)state->message_count;
            repair_message_sequence(state->messages, &msg_count);
            state->message_count = (size_t)msg_count;
        }

        /* S14 #1 gap #12: Sanitize surrogate characters across all messages
         * before LLM call. Prevents provider API errors from lone surrogate
         * codepoints (U+D800-U+DFFF) in tool results or user content. */
        for (size_t si = 0; si < state->message_count; si++) {
            message_t *m = state->messages[si];
            if (!m) continue;
            if (m->content) {
                char *fixed = sanitize_surrogates(m->content);
                if (fixed) { free(m->content); m->content = fixed; }
            }
            for (int tj = 0; tj < m->tool_calls_count && tj < 64; tj++) {
                char *fixed = sanitize_surrogates(m->tool_calls[tj].arguments);
                if (fixed) {
                    size_t flen = strlen(fixed);
                    if (flen < sizeof(m->tool_calls[tj].arguments))
                        memcpy(m->tool_calls[tj].arguments, fixed, flen + 1);
                    free(fixed);
                }
            }
        }

        /* Call LLM with retry and fallback support (Phase 113) */
        llm_response_t *llm_resp = NULL;
        int retries = state->llm.max_retries > 0 ? state->llm.max_retries : 0;
        bool dont_retry = false; /* skip retries for certain errors */

        /* S14 #1 gap #1: Plugin context injection via pre_llm_call hook */
        /* Plugins may return {"context":"..."} which is appended to the user message */
        char *_plugin_context = NULL;
        {
            char payload[2048];
            snprintf(payload, sizeof(payload),
                "{\"event\":\"pre_llm_call\",\"model\":\"%s\",\"provider\":\"%s\","
                "\"iteration\":%d,\"messages\":%zu,\"tools\":%d}",
                state->llm.model, state->llm.provider,
                iteration, state->message_count,
                tools_json ? (int)json_len(tools_json) : 0);
            char *hook_resp = invoke_hook("pre_llm_call", payload);
            if (hook_resp) {
                json_t *arr = json_parse(hook_resp, NULL);
                if (arr && arr->type == JSON_ARRAY) {
                    size_t n = json_len(arr);
                    size_t ctx_cap = 0;
                    size_t ctx_pos = 0;
                    for (size_t i = 0; i < n; i++) {
                        json_t *el = json_get(arr, i);
                        if (!el) continue;
                        const char *part = NULL;
                        if (el->type == JSON_OBJECT) {
                            json_t *ctx_val = json_obj_get(el, "context");
                            if (ctx_val && ctx_val->type == JSON_STRING)
                                part = ctx_val->str_val;
                        } else if (el->type == JSON_STRING) {
                            part = el->str_val;
                        }
                        if (part && part[0]) {
                            size_t plen = strlen(part);
                            if (ctx_pos > 0) plen += 2; /* "\n\n" separator */
                            if (ctx_pos + plen + 1 > ctx_cap) {
                                ctx_cap = ctx_cap ? ctx_cap * 2 : plen + 64;
                                char *new_ctx = realloc(_plugin_context, ctx_cap);
                                if (!new_ctx) break;
                                _plugin_context = new_ctx;
                            }
                            if (ctx_pos > 0) {
                                memcpy(_plugin_context + ctx_pos, "\n\n", 2);
                                ctx_pos += 2;
                            }
                            memcpy(_plugin_context + ctx_pos, part, plen);
                            ctx_pos += plen;
                            _plugin_context[ctx_pos] = '\0';
                        }
                    }
                    if (_plugin_context && ctx_pos > 0) {
                        /* Append to last user message */
                        if (state->message_count > 0) {
                            message_t *last = state->messages[state->message_count - 1];
                            if (last && last->role == MSG_USER && last->content) {
                                size_t old_len = strlen(last->content);
                                size_t new_len = old_len + 3 + ctx_pos; /* "\n\n" + ctx */
                                char *newc = realloc(last->content, new_len + 1);
                                if (newc) {
                                    newc[old_len] = '\n';
                                    newc[old_len + 1] = '\n';
                                    memcpy(newc + old_len + 2, _plugin_context, ctx_pos);
                                    newc[old_len + 2 + ctx_pos] = '\0';
                                    last->content = newc;
                                }
                            }
                        }
                    }
                }
                json_free(arr);
                free(hook_resp);
            }
        }
        free(_plugin_context);
        _plugin_context = NULL;

        /* S14 #1 gap #8: Nous Portal rate limit guard — check before API call */
        {
            double remaining = nous_rate_guard_remaining(state->hermes_home);
            if (remaining > 0.0) {
                char buf[64];
                format_remaining(remaining, buf, sizeof(buf));
                fprintf(stderr, "[rate-limit] Nous Portal: %s remaining\n", buf);
                char *result = malloc(512);
                if (result) snprintf(result, 512,
                    "⏳ Rate-limited by Nous Portal (resets in %s). "
                    "No fallback provider configured. Try again later.",
                    buf);
                llm_response_free(llm_resp);
                json_free(tools_json);
                return result ? result : strdup("Rate-limited by Nous Portal");
            }
        }

        /* Retry loop */
        for (int attempt = 0; attempt <= retries && !dont_retry; attempt++) {
            /* Backoff between retries: 1s, 2s, 4s... cap at 16s */
            if (attempt > 0) {
                int delay = 1 << (attempt - 1);
                if (delay > 16) delay = 16;
                fprintf(stderr, "[retry] attempt %d/%d after %ds...\n", attempt, retries, delay);
                struct timespec ts = {delay, 0};
                nanosleep(&ts, NULL);
            }

            /* Free previous failed response before retrying */
            if (llm_resp) { llm_response_free(llm_resp); llm_resp = NULL; }

            if (state->stream_cb) {
                llm_resp = llm_chat_completion_stream(
                    &state->llm,
                    (const message_t **)state->messages,
                    state->message_count,
                    tools_json,
                    state->stream_cb,
                    state->stream_data);
            } else {
                llm_resp = llm_chat_completion(
                    &state->llm,
                    (const message_t **)state->messages,
                    state->message_count,
                    tools_json);
            }

            /* L03: Check if response error suggests vision not supported */
            if (llm_resp && llm_resp->content && state->vision_disabled == false) {
                image_routing_notify_error(state, llm_resp->content);
            }

            if (!llm_resp) {
                fprintf(stderr, "[retry] LLM call returned NULL (attempt %d/%d)\n",
                        attempt + 1, retries);
                continue; /* retry */
            }

            /* Check for empty response — but not if reasoning content exists (thinking-only turn) */
            if ((!llm_resp->content || !llm_resp->content[0]) && llm_resp->tool_calls_count == 0 && !llm_resp->reasoning) {
                fprintf(stderr, "[retry] Empty LLM response (attempt %d/%d)\n",
                        attempt + 1, retries);
                continue; /* retry */
            }

            /* B22: Content filter response → skip retry of filtered model */
            if (llm_resp && strcmp(llm_resp->finish_reason, "content_filter") == 0) {
                fprintf(stderr, "[content_filter] Model returned content filter response, "
                        "skipping retry and triggering fallback.\n");
                dont_retry = true;
                /* Free the filtered response so fallback logic triggers */
                llm_response_free(llm_resp);
                llm_resp = NULL;
                break;
            }

            /* Credential expired → rotate key or trigger fallback */
            if (llm_resp && llm_resp->credential_expired) {
                fprintf(stderr, "[credential] Credential expired for %s/%s, "
                        "triggering fallback.\n",
                        state->llm.provider, state->llm.model);
                llm_response_free(llm_resp);
                llm_resp = NULL;
                dont_retry = true;
                break;
            }

            /* Success — break out of retry loop */
            break;
        }

        /* If retries exhausted and no credential/content-filter block, try
         * primary transport recovery: one more attempt on same provider
         * with a fresh connection (port of try_recover_primary_transport). */
        if (!llm_resp || ((!llm_resp->content || !llm_resp->content[0]) && llm_resp->tool_calls_count == 0
                          && !llm_resp->reasoning)) {
            if (!dont_retry) {
                fprintf(stderr, "[recover] Retries exhausted — trying transport recovery...\n");
                llm_response_t *recovery_resp = try_recover_primary_transport(
                    &state->llm,
                    (const message_t **)state->messages,
                    state->message_count,
                    tools_json,
                    state->stream_cb,
                    state->stream_data);
                if (recovery_resp && (recovery_resp->content || recovery_resp->tool_calls_count > 0)) {
                    if (llm_resp) llm_response_free(llm_resp);
                    llm_resp = recovery_resp;
                    fprintf(stderr, "[recover] Transport recovery succeeded — continuing\n");
                    goto retry_done;
                }
                if (recovery_resp) llm_response_free(recovery_resp);
                fprintf(stderr, "[recover] Transport recovery failed — proceeding to fallback\n");
            }
        }

        /* If all retries failed, try fallback providers (skip if thinking-only) */
        if (!llm_resp || ((!llm_resp->content || !llm_resp->content[0]) && llm_resp->tool_calls_count == 0
                          && !llm_resp->reasoning)) {
            char saved_provider[64] = {0};
            char saved_model[128] = {0};
            memcpy(saved_provider, state->llm.provider, sizeof(saved_provider));
            memcpy(saved_model, state->llm.model, sizeof(saved_model));

            /* Try fallback model first if set */
            if (state->llm.fallback_model[0]) {
                fprintf(stderr, "[fallback] Trying fallback model: %s\n",
                        state->llm.fallback_model);
                memcpy(state->llm.model, state->llm.fallback_model,
                       sizeof(state->llm.model));

                if (state->stream_cb) {
                    llm_resp = llm_chat_completion_stream(
                        &state->llm,
                        (const message_t **)state->messages,
                        state->message_count,
                        tools_json,
                        state->stream_cb,
                        state->stream_data);
                } else {
                    llm_resp = llm_chat_completion(
                        &state->llm,
                        (const message_t **)state->messages,
                        state->message_count,
                        tools_json);
                }

                if (llm_resp && (llm_resp->content || llm_resp->tool_calls_count > 0)) {
                    /* Restore original provider but keep using fallback model */
                    memcpy(state->llm.provider, saved_provider, sizeof(state->llm.provider));
                    goto retry_done;
                }
                if (llm_resp) { llm_response_free(llm_resp); llm_resp = NULL; }
                /* Restore original model */
                memcpy(state->llm.model, saved_model, sizeof(state->llm.model));
            }

            /* Try fallback providers */
            if (state->llm.fallback_providers[0]) {
                char fb_copy[1024];
                memcpy(fb_copy, state->llm.fallback_providers, sizeof(fb_copy));
                char *provider = fb_copy;
                char *next;

                while (provider && *provider) {
                    /* Skip whitespace */
                    while (*provider == ' ' || *provider == ',') provider++;
                    if (!*provider) break;

                    next = strchr(provider, ',');
                    if (next) *next++ = '\0';

                    /* Trim trailing spaces */
                    char *end = provider + strlen(provider) - 1;
                    while (end > provider && *end == ' ') end--;
                    *(end + 1) = '\0';

                    if (!*provider) { provider = next; continue; }

                    fprintf(stderr, "[fallback] Trying provider: %s\n", provider);
                    memcpy(state->llm.provider, provider, sizeof(state->llm.provider));

                    if (state->stream_cb) {
                        llm_resp = llm_chat_completion_stream(
                            &state->llm,
                            (const message_t **)state->messages,
                            state->message_count,
                            tools_json,
                            state->stream_cb,
                            state->stream_data);
                    } else {
                        llm_resp = llm_chat_completion(
                            &state->llm,
                            (const message_t **)state->messages,
                            state->message_count,
                            tools_json);
                    }

                    if (llm_resp && (llm_resp->content || llm_resp->tool_calls_count > 0)) {
                        memcpy(state->llm.model, saved_model, sizeof(state->llm.model));
                        goto retry_done;
                    }
                    if (llm_resp) { llm_response_free(llm_resp); llm_resp = NULL; }

                    provider = next;
                }
                /* All fallback providers failed — restore original provider */
                memcpy(state->llm.provider, saved_provider, sizeof(state->llm.provider));
                memcpy(state->llm.model, saved_model, sizeof(state->llm.model));
            }

            /* All retries and fallbacks exhausted */
            json_free(tools_json);
            if (llm_resp) llm_response_free(llm_resp);
            return strdup("{\"error\":\"LLM call failed after retries and fallbacks. "
                          "Check provider/network connectivity and config.\"}");
        }

retry_done:
        /* Port of Python agent/agent_runtime_helpers.py:restore_primary_runtime().
         * Ensure primary provider/model are restored after fallback. In C the
         * fallback is per-call, so saved values are already restored above, but
         * the label exists for goto targets that skip the restore. */
/* PoP: success @ hermes_cli/mcp_config.py:_success */
        /* P95: Log upstream diagnostic headers on success (verbose only) */
        if (llm_resp && llm_resp->diag.upstream_headers[0] && getenv("SLERMES_DEBUG")) {
            fprintf(stderr, "[llm] upstream=[%s]\n",
                    llm_resp->diag.upstream_headers);
        }

        /* P91: Mark system prompt as cached after first successful call */
        if (!state->llm.system_cached) {
            state->llm.system_cached = true;
        }

        /* P186: Invoke post_llm_call hook */
        {
            char payload[2048];
            snprintf(payload, sizeof(payload),
                "{\"event\":\"post_llm_call\",\"model\":\"%s\",\"provider\":\"%s\","
                "\"iteration\":%d,\"input_tokens\":%d,\"output_tokens\":%d,"
                "\"tool_calls\":%d,\"finish_reason\":\"%s\"}",
                state->llm.model, state->llm.provider,
                iteration, llm_resp ? llm_resp->input_tokens : 0,
                llm_resp ? llm_resp->output_tokens : 0,
                llm_resp ? llm_resp->tool_calls_count : 0,
                llm_resp ? llm_resp->finish_reason : "error");
            char *hook_resp = invoke_hook("post_llm_call", payload);
            free(hook_resp);
        }

        iteration++;

        /* L10: Increment skill nudge counter per tool iteration */
        if (state->skill_nudge_interval > 0)
            state->iters_since_skill++;

        /* L10: Check skill nudge threshold after this iteration */
        if (state->skill_nudge_interval > 0
                && state->iters_since_skill >= state->skill_nudge_interval) {
            state->iters_since_skill = 0;
            /* Inject skill nudge via steer queue */
            if (state->steer_count < HERMES_MAX_STEERS) {
                snprintf(state->steer_queue[state->steer_count],
                         sizeof(state->steer_queue[0]),
                         "[Skill reminder] You can use the skill_manage tool to "
                         "create, edit, or review skills. Consider reviewing your "
                         "available skills for this task.");
                state->steer_roles[state->steer_count] = MSG_SYSTEM;
                state->steer_count++;
            }
        }

        /* P86: Report turn to budget tracker */
        if (state->budget) {
            double cost = budget_tracker_estimate_cost(
                state->llm.model,
                llm_resp->input_tokens,
                llm_resp->output_tokens);
            budget_tracker_report_turn(state->budget,
                                        llm_resp->input_tokens,
                                        llm_resp->output_tokens,
                                        cost,
                                        state->llm.model);
        }

        /* G01-G03: Track session token totals */
        state->session_input_tokens += llm_resp->input_tokens;
        state->session_output_tokens += llm_resp->output_tokens;
        state->session_total_tokens = state->session_input_tokens + state->session_output_tokens;

        /* G04-G06: Deep token tracking */
        state->session_reasoning_tokens += llm_resp->reasoning_tokens;
        state->session_cache_read_tokens += llm_resp->cache_read_tokens;
        state->session_cache_write_tokens += llm_resp->cache_write_tokens;

        /* G07-G08: Track cumulative cost from budget tracker */
        if (state->budget) {
            state->session_estimated_cost_usd = (double)state->budget->total_cost_usd;
            snprintf(state->session_cost_source, sizeof(state->session_cost_source), "budget_tracker");
        }

        /* G09: Count tool turns */
        if (llm_resp->tool_calls_count > 0) {
            state->tool_turn_count++;
        }

        /* G10: Update last activity timestamp */
        state->last_activity_ts = time(NULL);

        /* If no tool calls, we're done — return final content */
        if (llm_resp->tool_calls_count == 0) {
            /* GAP-9: Truncated response handling — finish_reason "length"
             * means model hit max output tokens. Continue with a brief
             * prompt to complete the response. */
            if (llm_resp->content && strcmp(llm_resp->finish_reason, "length") == 0) {
                fprintf(stderr, "[truncated] Response truncated (length), continuing...\n");
                message_t *cont_msg = message_new(MSG_USER,
                    "[continue output from where you stopped]");
                if (cont_msg) {
                    /* Push the partial response as an assistant message */
                    message_t *partial = message_new_assistant(llm_resp->content,
                        NULL, NULL, llm_resp->reasoning, llm_resp->encrypted_content);
                    if (partial) {
                        context_push(state, partial);
                        context_push(state, cont_msg);
                        llm_response_free(llm_resp);
                        continue;
                    }
                    message_free(cont_msg);
                }
                /* OOM — fall through to return what we have */
            }
            /* GAP-5: Thinking-only turn — LLM returned reasoning but no text and no
             * tool calls (e.g. DeepSeek R1 after chain-of-thought). Add the reasoning
             * as a reasoning-only assistant message and loop back for the actual response.
             * This avoids retrying a valid thinking-only response as "empty". */
            if (!llm_resp->content && llm_resp->reasoning) {
                message_t *reason_msg = message_new_assistant(NULL, NULL, NULL,
                    llm_resp->reasoning, llm_resp->encrypted_content);
                llm_response_free(llm_resp);
                if (reason_msg) {
                    context_push(state, reason_msg);
                    fprintf(stderr, "[loop] Thinking-only turn appended, continuing...\n");
                    continue;
                }
                /* OOM — fall through to return empty content */
            }
            /* AL09: Truncated response handling — if finish_reason is "length",
             * the LLM hit its max_tokens limit mid-response. Append a continuation
             * prompt and loop to get the rest, then merge. */
            if (llm_resp && llm_resp->finish_reason[0] &&
                strcmp(llm_resp->finish_reason, "length") == 0 &&
                llm_resp->content && llm_resp->content[0]) {
                fprintf(stderr, "[loop] Truncated response (length), continuing...\n");
                /* Push the partial response as an assistant message */
                message_t *partial = message_new_assistant(llm_resp->content,
                    NULL, NULL, llm_resp->reasoning, llm_resp->encrypted_content);
                if (partial) {
                    hermes_message_sanitize(partial);
                    context_push(state, partial);
                }
                /* Add a user continuation prompt */
                message_t *cont = message_new(MSG_USER, "Continue your previous response exactly where you left off. Do not repeat any text already generated.");
                if (cont) context_push(state, cont);
                llm_response_free(llm_resp);
                llm_resp = NULL;
                json_free(tools_json);
                tools_json = NULL;
                iteration++;
                continue;  /* loop back for continuation */
            }

            /* Take snapshot for undo before finalizing */
            agent_snapshot_take(state);
            char *result = llm_resp->content ? strdup(llm_resp->content) : strdup("");
            llm_response_free(llm_resp);
            /* ME01: Free prefetch result */
            if (state->prefetch_result) {
                free(state->prefetch_result);
                state->prefetch_result = NULL;
                state->prefetch_in_progress = 0;
            }
            json_free(tools_json);
            /* Auto-save session + metadata */
            if (state->db) {
                agent_save_session(state);
                agent_save_meta(state);
            }
            /* G25: Reset budget on successful completion */
            if (state->budget) budget_tracker_reset(state->budget);
            /* Save trajectory (JSONL) for analysis */
            char *traj = serialize_messages(state);
            if (traj) {
                save_trajectory(traj, state->llm.model, true, NULL);
                free(traj);
            }
            return result;
        }

        /* Has tool calls — create assistant message with tool_calls */
        /* Snapshot before tool execution for undo */
        agent_snapshot_take(state);
        /* P98: Try auto-save checkpoint */
        checkpoint_try_autosave(&state->checkpoints, state);
        message_t *assistant_msg = message_new_assistant_with_toolcalls(
            llm_resp->content, llm_resp->tool_calls, llm_resp->tool_calls_count,
            llm_resp->reasoning, llm_resp->encrypted_content);
        /* L26: Sanitize assistant message (surrogate fix, think-block
         * stripping, secret redaction) before storage. Port of Python's
         * build_assistant_message() sanitization steps. */
        hermes_message_sanitize(assistant_msg);
        context_push(state, assistant_msg);

        /* S14 #1 gap #3: Sanitize tool call arguments before dispatch.
         * LLM sometimes returns None/null/empty args instead of valid JSON.
         * sanitize_tool_call_arguments() repairs these in-place and
         * adds corruption markers to affected tool results.
         * Use a temporary contiguous buffer since the function takes message_t*. */
        {
            int msg_count = (int)state->message_count;
            if (msg_count > 0 && state->messages) {
                /* Allocate temp contiguous array (capacity for 16 extra inserts) */
                int cap = msg_count + 16;
                message_t *contig = malloc(sizeof(message_t) * (size_t)cap);
                if (contig) {
                    for (int i = 0; i < msg_count; i++)
                        if (state->messages[i])
                            contig[i] = *state->messages[i];
                        else
                            memset(&contig[i], 0, sizeof(contig[i]));

                    sanitize_tool_call_arguments(contig, &msg_count);

                    /* Copy back through pointer array */
                    int old_count = (int)state->message_count;
                    for (int i = 0; i < msg_count; i++) {
                        if (i < old_count) {
                            if (state->messages[i])
                                *state->messages[i] = contig[i];
                        } else {
                            /* Newly inserted message — allocate */
                            state->messages[i] = malloc(sizeof(message_t));
                            if (state->messages[i])
                                *state->messages[i] = contig[i];
                        }
                    }
                    state->message_count = (size_t)msg_count;
                    free(contig);
                }
            }
        }

        /* P87: Parallel tool dispatch — execute independent tools concurrently */
        /* Port of Python agent/tool_executor.py: execute_tool_calls_concurrent */
        int n_calls = llm_resp->tool_calls_count;

        /* G24: Reset per-turn tool call counter and check per-turn limit */
        if (state->budget) budget_tracker_reset_turn_tools(state->budget);
        if (state->budget && state->budget->max_tool_calls_per_turn > 0 &&
            n_calls > state->budget->max_tool_calls_per_turn) {
            n_calls = state->budget->max_tool_calls_per_turn;
        }

        /* G28: Reset guardrail controller for this turn */
        tool_guardrail_reset(&state->guardrails_ctrl);

        typedef struct {
            int    index;
            char  *tool_name;
            char  *tool_args;
            char  *tool_id;
            int    approved;
            char  *result;  /* output */
            int    duration_ms;  /* S14 gap #13: latency tracking */
            bool   async;        /* S14 gap #8: async handler (detached thread) */
        } tool_work_t;

        tool_work_t *works = (tool_work_t *)calloc((size_t)n_calls, sizeof(tool_work_t));
        if (!works) {
            llm_response_free(llm_resp);
            json_free(tools_json);
            return strdup("Error: OOM for tool dispatch");
        }

        /* Phase 1: Security approval + guardrail check (sequential) */

        /* P186: Invoke pre_tool_call hook — check for block signals */
        bool hook_blocked_all = false;
        {
            char payload[2048];
            snprintf(payload, sizeof(payload),
                "{\"event\":\"pre_tool_call\",\"count\":%d,\"iteration\":%d}",
                n_calls, iteration);
            char *hook_resp = invoke_hook("pre_tool_call", payload);
            if (hook_resp) {
                char *err = NULL;
                json_node_t *arr = json_parse(hook_resp, &err);
                if (arr && arr->type == JSON_ARRAY) {
                    size_t n = json_len(arr);
                    for (size_t hi = 0; hi < n && hi < 32; hi++) {
                        json_node_t *item = json_get(arr, hi);
                        if (item && json_get_bool(item, "block", false)) {
                            hook_blocked_all = true;
                            const char *reason = json_get_str(item, "reason", "blocked by hook");
                            fprintf(stderr, "[hook] pre_tool_call blocked: %s\n", reason);
                            break;
                        }
                    }
                }
                free(err);
                json_free(arr);
                free(hook_resp);
            }
        }

        for (int i = 0; i < n_calls; i++) {
            works[i].index = i;
            works[i].tool_name = strdup(llm_resp->tool_calls[i].name);
            works[i].tool_args = strdup(llm_resp->tool_calls[i].arguments);
            works[i].tool_id = strdup(llm_resp->tool_calls[i].id);
            works[i].approved = approval_check(
                llm_resp->tool_calls[i].name,
                llm_resp->tool_calls[i].arguments);

            /* G29: Guardrail before_call — block tools that are in a loop */
            if (works[i].approved) {
                tool_guardrail_decision_t gd = tool_guardrail_before_call(
                    &state->guardrails_ctrl,
                    llm_resp->tool_calls[i].name,
                    llm_resp->tool_calls[i].arguments);
                if (gd.action == GUARDRAIL_BLOCK || gd.action == GUARDRAIL_HALT) {
                    works[i].approved = 0;
                    works[i].result = toolguard_synthetic_result(&gd);
                    fprintf(stderr, "[guardrail] Blocked %s: %s\n",
                            gd.tool_name, gd.code);
                }
            }

            /* S14 gap #5: ACP edit approval — block write_file/patch on sensitive paths */
            if (works[i].approved) {
                char approval_reason[512];
                if (!acp_maybe_require_edit_approval(
                        llm_resp->tool_calls[i].name,
                        llm_resp->tool_calls[i].arguments,
                        approval_reason, sizeof(approval_reason))) {
                    works[i].approved = 0;
                    char denied[1024];
                    snprintf(denied, sizeof(denied),
                        "{\"error\":\"%s\"}", approval_reason);
                    works[i].result = strdup(denied);
                    fprintf(stderr, "[acp] Edit blocked: %s\n", approval_reason);
                }
            }

            if (!works[i].result)
                works[i].result = NULL;

            /* S14 gap #8: Resolve async flag from registry */
            if (works[i].approved) {
                tool_t *t = registry_find(works[i].tool_name);
                if (t) works[i].async = t->async;
            }

            /* Fire tool.started event for approved tools */
            if (works[i].approved && state->tool_event_cb) {
                state->tool_event_cb("tool.started", works[i].tool_name,
                                     works[i].tool_args, state->tool_event_data);
            }
        }

        /* Apply hook block after approval loop — overrides all approvals */
        if (hook_blocked_all) {
            for (int i = 0; i < n_calls; i++)
                works[i].approved = 0;
        }

        /* S14 gap #3: Agent loop tool redirect — intercept tools that must be handled
         * by the loop rather than dispatched to a tool handler.
         * Port of Python model_tools.handle_function_call() special tool handling. */
        for (int i = 0; i < n_calls; i++) {
            if (!works[i].approved || works[i].result) continue;

            if (strcmp(works[i].tool_name, "finish") == 0 ||
                strcmp(works[i].tool_name, "_finished") == 0) {
                /* Finish tool — signal conversation complete */
                works[i].approved = 0;
                works[i].result = strdup("{\"success\":true,\"finished\":true}");
                state->interrupted = true;
                state->interrupt_type = INTERRUPT_GRACEFUL;
                snprintf(state->interrupt_message, sizeof(state->interrupt_message),
                         "Tool '%s' signaled conversation complete.", works[i].tool_name);
            }
        }

        /* Phase 2: Parallel execution via pthreads */
#if defined(_WIN32) || defined(WIN32)
        /* No pthreads on Windows — fallback to sequential */
        /* Port of Python agent/tool_executor.py: execute_tool_calls_sequential */
        for (int i = 0; i < n_calls; i++) {
            if (works[i].approved) {
                struct timespec ts_s, ts_e;
                clock_gettime(CLOCK_MONOTONIC, &ts_s);
                works[i].result = registry_dispatch(
                    works[i].tool_name, works[i].tool_args, state->session_id);
                clock_gettime(CLOCK_MONOTONIC, &ts_e);
                int64_t ns = (int64_t)(ts_e.tv_sec - ts_s.tv_sec) * 1000000000L +
                             (int64_t)(ts_e.tv_nsec - ts_s.tv_nsec);
                works[i].duration_ms = (int)(ns / 1000000);
            } else {
                char denied[1024];
                snprintf(denied, sizeof(denied),
                    "{\"error\":\"Operation denied by security approval: "
                    "tool '%s' was blocked. Use /approve in interactive mode "
                    "to allow it.\"}",
                    works[i].tool_name);
                works[i].result = strdup(denied);
            }
        }
#else
        /* POSIX — parallel dispatch */
        pthread_t *threads = (pthread_t *)calloc((size_t)n_calls, sizeof(pthread_t));
        struct tool_dispatch_arg *args = (struct tool_dispatch_arg *)calloc(
            (size_t)n_calls, sizeof(struct tool_dispatch_arg));

        for (int i = 0; i < n_calls; i++) {
            if (works[i].approved) {
                args[i].session_id = state->session_id;
                args[i].tool_name = works[i].tool_name;
                args[i].tool_args = works[i].tool_args;
                args[i].result_out = &works[i].result;
                args[i].duration_ms_out = &works[i].duration_ms;
                pthread_create(&threads[i], NULL, tool_dispatch_thread, &args[i]);
            } else {
                char denied[1024];
                snprintf(denied, sizeof(denied),
                    "{\"error\":\"Operation denied by security approval: "
                    "tool '%s' was blocked.\"}",
                    works[i].tool_name);
                works[i].result = strdup(denied);
            }
        }

        /* Join all threads (skip async tools — they run detached) */
        for (int i = 0; i < n_calls; i++) {
            if (works[i].approved && !works[i].async) {
                pthread_join(threads[i], NULL);
            } else if (works[i].approved && works[i].async) {
                pthread_detach(threads[i]);
            }
        }

        free(threads);
        free(args);
#endif

        /* S14 gap #12: Result size limiting — truncate oversize tool results */
        {
            int max_size = state->max_result_size;
            if (max_size > 0) {
                for (int i = 0; i < n_calls; i++) {
                    if (works[i].result && (int)strlen(works[i].result) > max_size) {
                        char *truncated = malloc((size_t)max_size + 64);
                        if (truncated) {
                            memcpy(truncated, works[i].result, (size_t)max_size);
                            snprintf(truncated + max_size, 64,
                                " [truncated: %zu chars, max %d]",
                                strlen(works[i].result), max_size);
                            free(works[i].result);
                            works[i].result = truncated;
                        }
                    }
                }
            }
        }

        /* Phase 3: Create tool result messages (in original order) */
        for (int i = 0; i < n_calls; i++) {
            /* P186: Fire post_tool_call hook per tool with metadata */
            {
                char payload[4096];
                char preview_buf[512];
                const char *res_preview = works[i].result;
                if (res_preview && strlen(res_preview) > 200) {
                    size_t rlen = strlen(res_preview);
                    snprintf(preview_buf, sizeof(preview_buf), "%.197s...(%zu more)", res_preview, rlen - 197);
                    res_preview = preview_buf;
                }
                snprintf(payload, sizeof(payload),
                    "{\"event\":\"post_tool_call\",\"tool_name\":\"%s\","
                    "\"iteration\":%d,\"index\":%d,\"result_preview\":\"%s\","
                    "\"duration_ms\":%d}",
                    works[i].tool_name ? works[i].tool_name : "",
                    iteration, i,
                    res_preview ? res_preview : "",
                    works[i].duration_ms);
                char *hook_resp = invoke_hook("post_tool_call", payload);
                /* S14 gap #7: Transform tool result hook — plugins can replace result */
                if (hook_resp) {
                    char *err = NULL;
                    json_node_t *arr = json_parse(hook_resp, &err);
                    if (arr && arr->type == JSON_ARRAY) {
                        size_t n = json_len(arr);
                        for (size_t hi = 0; hi < n; hi++) {
                            json_node_t *item = json_get(arr, hi);
                            if (item && json_get_bool(item, "transform", false)) {
                                const char *new_result = json_get_str(item, "result", NULL);
                                if (new_result) {
                                    free(works[i].result);
                                    works[i].result = strdup(new_result);
                                }
                            }
                        }
                    }
                    free(err);
                    json_free(arr);
                    free(hook_resp);
                }
            }
            /* G24: Track per-turn tool call count */
            if (state->budget) budget_tracker_increment_tool_call(state->budget);

            /* P177: Subdirectory hint discovery — append context from new dirs */
            if (works[i].result) {
                char *hints = subdir_hints_check(works[i].tool_name, works[i].tool_args);
                if (hints) {
                    /* Append hints to tool result */
                    size_t old_len = strlen(works[i].result);
                    size_t hints_len = strlen(hints);
                    char *updated = (char *)realloc(works[i].result, old_len + hints_len + 1);
                    if (updated) {
                        memcpy(updated + old_len, hints, hints_len + 1);
                        works[i].result = updated;
                    }
                    free(hints);
                }
            }

            /* L09: Reset memory nudge counter when memory tool is used */
            if (strcmp(works[i].tool_name, "memory") == 0)
                state->turns_since_memory = 0;

            /* L10: Reset skill nudge counter when skill_manage tool is used */
            if (strcmp(works[i].tool_name, "skill_manage") == 0)
                state->iters_since_skill = 0;

            message_t *tool_msg = message_new_tool(
                works[i].tool_id,
                works[i].result ? works[i].result :
                    "{\"error\":\"Tool returned NULL\"}");
            context_push(state, tool_msg);
            /* P93: Classify tool result — abort on fatal */
            /* G35: Use typed interrupt for graceful vs force distinction */
            bool tool_failed = (classify_tool_result(works[i].result, works[i].tool_name) == TOOL_RESULT_FATAL);
            if (tool_failed) {
                state->interrupted = true;
                state->interrupt_type = INTERRUPT_GRACEFUL;
                /* G12: Set interrupt message with context */
                snprintf(state->interrupt_message, sizeof(state->interrupt_message),
                         "Fatal tool result from '%s': %s",
                         works[i].tool_name,
                         works[i].result ? works[i].result : "unknown error");
            }

            /* G30: Guardrail after_call — track tool results for loop detection */
            {
                /* Use classify_tool_failure for guardrail tracking (more accurate
                 * than classify_tool_result — handles terminal exit codes,
                 * file-mutation success, memory-full, etc.). */
                bool g_failed = classify_tool_failure(works[i].tool_name,
                                                      works[i].result);
                tool_guardrail_decision_t gd = tool_guardrail_after_call(
                    &state->guardrails_ctrl,
                    works[i].tool_name,
                    works[i].tool_args,
                    works[i].result,
                    g_failed);
                if (gd.action == GUARDRAIL_HALT) {
                    state->interrupted = true;
                    state->interrupt_type = INTERRUPT_GRACEFUL;
                    snprintf(state->interrupt_message, sizeof(state->interrupt_message),
                             "Guardrail halt: %s", gd.message);
                    fprintf(stderr, "[guardrail] Halt: %s\n", gd.code);
                } else if (gd.action == GUARDRAIL_WARN) {
                    /* Append warning to tool result */
                    char *updated = append_toolguard_guidance(
                        works[i].result ? works[i].result : "", &gd);
                    if (updated) {
                        free(works[i].result);
                        works[i].result = updated;
                    }
                    fprintf(stderr, "[guardrail] Warn: %s\n", gd.code);
                }
            }

            /* Fire tool.completed event */
            if (state->tool_event_cb) {
                const char *completed_type = tool_failed ? "tool.failed" : "tool.completed";
                state->tool_event_cb(completed_type, works[i].tool_name,
                                     works[i].result, state->tool_event_data);
            }

            /* Optional background review for sensitive tools. Activate by setting
             * state->enable_background_review = true in agent init. */
            if (state->enable_background_review && !tool_failed) {
                char *review = llm_background_review(&state->llm,
                    works[i].tool_name, works[i].tool_args, works[i].result);
                if (review) {
                    /* Append review to tool result as guidance */
                    size_t rlen = strlen(works[i].result ? works[i].result : "");
                    size_t vlen = strlen(review);
                    char *updated = (char *)realloc(works[i].result, rlen + vlen + 64);
                    if (updated) {
                        snprintf(updated + rlen, vlen + 64, "\n\n[Background Review]\n%s", review);
                        works[i].result = updated;
                    }
                    free(review);
                }
            }

            free(works[i].tool_name);
            free(works[i].tool_args);
            free(works[i].tool_id);
            free(works[i].result);
        }
        free(works);

        llm_response_free(llm_resp);
        /* P93: Break on fatal tool result */
        if (!state->interrupted) {
            /* Loop back to LLM with tool results appended */
        }
    }

    json_free(tools_json);

    /* ── Turn finalization (delegated to turn_finalizer.c) ────────────── */
    /* Port of Python turn_finalizer.py:finalize_turn. */
    return finalize_turn(state);
}

/* ================================================================
 *  P28: Undo snapshot
 * ================================================================ */

/* Note: message_clone() is now in context.c (exported from hermes_agent.h) */

void agent_snapshot_take(agent_state_t *state) {
    /* Free old snapshot if any */
    if (state->snapshot) {
        for (size_t i = 0; i < state->snapshot_count; i++)
            message_free(state->snapshot[i]);
        free(state->snapshot);
        state->snapshot = NULL;
        state->snapshot_count = 0;
    }

    /* Allocate snapshot array */
    state->snapshot_capacity = state->message_count + 16;
    state->snapshot = (message_t **)calloc(state->snapshot_capacity, sizeof(message_t *));
    if (!state->snapshot) return;

    /* Clone all messages */
    for (size_t i = 0; i < state->message_count; i++) {
        state->snapshot[i] = message_clone(state->messages[i]);
        if (state->snapshot[i])
            state->snapshot_count++;
    }

    snprintf(state->snapshot_id, sizeof(state->snapshot_id), "%s", state->session_id);
}

bool agent_snapshot_restore(agent_state_t *state) {
    if (!state->snapshot || state->snapshot_count == 0) return false;

    /* Free current messages */
    context_clear(state);

    /* Allocate space */
    if (state->message_capacity < state->snapshot_count + 16) {
        state->message_capacity = state->snapshot_count + 16;
        message_t **new_msgs = (message_t **)realloc(state->messages,
                                                      state->message_capacity * sizeof(message_t *));
        if (!new_msgs) return false;
        state->messages = new_msgs;
    }

    /* Clone snapshot back */
    for (size_t i = 0; i < state->snapshot_count; i++) {
        if (state->snapshot[i]) {
            state->messages[i] = message_clone(state->snapshot[i]);
            state->message_count++;
        }
    }

    /* Restore session ID from snapshot */
    if (state->snapshot_id[0])
        snprintf(state->session_id, sizeof(state->session_id), "%s", state->snapshot_id);

    return true;
}

/* Simple chat interface */
/* Port of Python cli.py:chat() */
char *agent_chat(agent_state_t *state, const char *message) {
    return run_conversation(state, message, NULL);
}

/* G18: Inject conversation history — preload messages from JSON array.
 * Each element: {"role":"user|assistant|tool","content":"..."}
 * Existing messages are preserved; new ones appended. */
bool agent_inject_history(agent_state_t *state, const char *history_json) {
    if (!state || !history_json || !*history_json) return false;
    char *err = NULL;
    json_node_t *arr = json_parse(history_json, &err);
    if (!arr || arr->type != JSON_ARRAY) { free(err); json_free(arr); return false; }
    free(err);

    size_t n = json_len(arr);
    for (size_t i = 0; i < n; i++) {
        json_node_t *item = json_get(arr, i);
        const char *role = json_get_str(item, "role", "user");
        const char *content = json_get_str(item, "content", "");

        message_role_t r = MSG_USER;
        if (strcmp(role, "system") == 0) r = MSG_SYSTEM;
        else if (strcmp(role, "assistant") == 0) r = MSG_ASSISTANT;
        else if (strcmp(role, "tool") == 0) r = MSG_TOOL;

        message_t *msg = message_new(r, content);
        if (msg) context_push(state, msg);
    }
    json_free(arr);
    return n > 0;
}

/* ================================================================
 *  Conversation loop helpers (Port of Python conversation_loop.py)
 * ================================================================ */

/* Port of Python conversation_loop.py:_is_nous_inference_route */
/* PoP: is_nous_inference_route @ agent/billing_links.py:is_nous_inference_route */
bool is_nous_inference_route(const char *provider, const char *base_url) {
    if (!provider) return false;
    char prov[128];
    snprintf(prov, sizeof(prov), "%s", provider);
    for (int i = 0; prov[i]; i++)
        prov[i] = (char)tolower((unsigned char)prov[i]);
    /* Trim trailing spaces */
    size_t len = strlen(prov);
    while (len > 0 && prov[len - 1] == ' ') prov[--len] = '\0';
    /* Skip leading spaces */
    const char *p = prov;
    while (*p == ' ') p++;
    if (strcmp(p, "nous") == 0) return true;

    if (!base_url || !base_url[0]) return false;
    return url_host_matches(base_url, "inference-api.nousresearch.com") ||
           url_host_matches(base_url, "inference.nousresearch.com");
}

/* Port of Python conversation_loop.py:_billing_or_entitlement_message */
char *billing_or_entitlement_message(const char *capability,
                                     const char *provider,
                                     const char *base_url,
                                     const char *model) {
    /* Buffer for the message — sized for typical billing message */
    char buf[1024];
    int pos = 0;

    if (is_nous_inference_route(provider, base_url)) {
        /* Fallback: Nous-specific message without nous_account module */
        const char *prov_label = (provider && provider[0]) ? provider : "the selected provider";
        const char *mod_label = (model && model[0]) ? model : "the selected model";
        pos = snprintf(buf, sizeof(buf),
            "%s reported that billing, credits, or account "
            "entitlement is exhausted for %s.\n"
            "Add credits or update billing with that provider, then retry.\n"
            "Visit https://nousresearch.com for account details.\n"
            "You can switch providers temporarily with /model <model> --provider <provider>.",
            prov_label, mod_label);
    } else {
        const char *prov_label = (provider && provider[0]) ? provider : "the selected provider";
        const char *mod_label = (model && model[0]) ? model : "the selected model";
        int remaining;
        pos = snprintf(buf, sizeof(buf),
            "%s reported that billing, credits, or account "
            "entitlement is exhausted for %s.\n"
            "Add credits or update billing with that provider, then retry.\n",
            prov_label, mod_label);

        if (base_url && url_host_matches(base_url, "openrouter.ai")) {
            remaining = (int)sizeof(buf) - pos;
            if (remaining > 0)
                pos += snprintf(buf + pos, (size_t)remaining,
                    "OpenRouter credits: https://openrouter.ai/settings/credits\n");
        }
        remaining = (int)sizeof(buf) - pos;
        if (remaining > 0)
            snprintf(buf + pos, (size_t)remaining,
                "You can switch providers temporarily with /model <model> --provider <provider>.");
    }

    return strdup(buf);
}

/* Port of Python conversation_loop.py:_print_billing_or_entitlement_guidance */
bool print_billing_or_entitlement_guidance(const char *capability,
                                           const char *provider,
                                           const char *base_url,
                                           const char *model) {
    char *msg = billing_or_entitlement_message(capability, provider, base_url, model);
    if (!msg) return false;
    if (!msg[0]) {
        free(msg);
        return false;
    }
    /* Print the message */
    fprintf(stderr, "%s\n", msg);
    hermes_log(LOG_WARNING, "agent_loop",
        "Billing/entitlement guidance: %s", msg);
    free(msg);
    return true;
}

/* Port of Python conversation_loop.py:_nous_entitlement_message */
char *nous_entitlement_message(const char *capability) {
    (void)capability; /* Fallback: C doesn't have nous_account module */
    return strdup(
        "Your Nous account may need additional credits or entitlements. "
        "Visit https://nousresearch.com to check your account status.");
}

/* Port of Python conversation_loop.py:_print_nous_entitlement_guidance */
bool print_nous_entitlement_guidance(const char *capability) {
    char *msg = nous_entitlement_message(capability);
    if (!msg || !msg[0]) {
        free(msg);
        return false;
    }
    fprintf(stderr, "   \xF0\x9F\x92\xA1 %s\n", msg);
    hermes_log(LOG_WARNING, "agent_loop",
        "Nous entitlement guidance: %s", msg);
    free(msg);
    return true;
}

/* Port of Python conversation_loop.py:_ollama_context_limit_error */
char *ollama_context_limit_error(bool has_tools, int ollama_num_ctx,
                                 const char *model, const char *base_url,
                                 const char *provider, int tool_count,
                                 int request_tokens) {
    if (!has_tools) return NULL;
    if (ollama_num_ctx <= 0) return NULL;
    if (ollama_num_ctx >= MINIMUM_CONTEXT_LENGTH) return NULL;

    const char *mod_label = (model && model[0]) ? model : "the selected model";
    const char *url_label = (base_url && base_url[0]) ? base_url : "unknown base URL";
    const char *prov_label = (provider && provider[0]) ? provider : "unknown";

    hermes_log(LOG_WARNING, "agent_loop",
        "Ollama runtime context too small for Hermes tool use: "
        "model=%s provider=%s base_url=%s runtime_context=%d "
        "minimum_context=%d estimated_request_tokens=%d tool_count=%d",
        mod_label, prov_label, url_label,
        ollama_num_ctx, MINIMUM_CONTEXT_LENGTH, request_tokens, tool_count);

    char buf[2048];
    snprintf(buf, sizeof(buf),
        "Ollama loaded `%s` with only %d tokens of runtime "
        "context, but Hermes needs at least %d tokens "
        "for reliable tool use.\n\n"
        "Increase the Ollama context for this model and restart/reload the "
        "model before trying again. A known-good starting point is 65,536 "
        "tokens. In Hermes config, set `model.ollama_num_ctx: 65536` "
        "(and `model.context_length: 65536` if you also override the displayed "
        "model context). If you manage the model through an Ollama Modelfile, "
        "set `num_ctx 65536` or higher.",
        mod_label, ollama_num_ctx, MINIMUM_CONTEXT_LENGTH);
    return strdup(buf);
}
