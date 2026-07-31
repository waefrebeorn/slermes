/*
 * agent_loop.c — Core agent conversation loop for Hermes C.
 *
 * The loop:
 * 1. Read next message from input
 * 2. Send to LLM (with tools if enabled)
 * 3. If LLM returns text → output and done
 * 4. If LLM returns tool_calls → execute each, append results, loop
 * 5. Repeat until max_iterations or final response
 */
#define _XOPEN_SOURCE 700  /* strptime() — glibc X/Open extension */

#include "hermes_memory.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_agent.h"
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
#include "provider_metadata.h"
#include "hermes_tool_guardrails.h"
#include "acp/edit_approval.h"
#include "hermes_http.h"
#include <time.h>
#include <strings.h>
/* (hermes_gap_fixes.h removed: split into todo_hydrate.h / file_mutation_verifier.h / api_error_summary.h; this TU used no symbols from it) */
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
#endif

/* ================================================================
 *  Agent initialization
 * ================================================================ */

/* P89: Global state pointer for SIGINT handler */
#ifndef _WIN32
static agent_state_t *g_signal_state = NULL;

static void sigint_handler(int sig) {
    (void)sig;
    if (g_signal_state) {
        g_signal_state->interrupted = true;
        /* G35: Force interrupt via SIGINT */
        g_signal_state->interrupt_type = INTERRUPT_FORCE;
        /* Print message to stderr so it interrupts cleanly */
        fprintf(stderr, "\n! Interrupted (SIGINT). Use /stop to force quit.\n");
    }
}
#endif

/* Port of Python hermes_cli/cli_agent_setup_mixin.py:_init_agent(). */
/* Port of Python: init_agent */
void init_agent(agent_state_t *state) {
    memset(state, 0, sizeof(*state));
    setup_logging();
    hermes_log(LOG_INFO, "agent_loop", "Agent initialized");
    state->compress_tail_messages = 2;  /* P99b: default tail keep */
    state->compress_cooldown_secs = 30;         /* L02: default cooldown */
    state->compress_failure_cooldown_secs = 600; /* L02: default failure cooldown */
    state->message_capacity = 64;
    state->messages = (message_t **)calloc(state->message_capacity, sizeof(message_t *));
    state->max_iterations = HERMES_MAX_TOOL_CALLS;
    state->snapshot_capacity = 0;  /* lazy init on first snapshot_take */
    /* P86: Create budget tracker */
    state->budget = (budget_tracker_t *)calloc(1, sizeof(budget_tracker_t));
    if (state->budget) budget_tracker_init(state->budget);
    /* P89: Register SIGINT handler */
#ifndef _WIN32
    g_signal_state = state;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);
#endif
    context_init(state);
    agent_generate_session_id(state);
    /* Register built-in LLM providers */
    register_provider_builtins();
    /* P97: Initialize compression feedback tracker */
    compression_feedback_init(&state->compression_fb);
    /* P98: Initialize checkpoint manager */
    checkpoint_init(&state->checkpoints);

    /* G21: Default compression strategy */
    state->compression_strategy = COMPRESS_OLDEST_TOOL_FIRST;
    /* G31: Default prefill as assistant message */
    state->prefill_role = MSG_ASSISTANT;
    /* G35: No interrupt */
    state->interrupt_type = INTERRUPT_NONE;

    /* L09: Memory nudge defaults — 10 turns between nudges, starts at 0 */
    state->memory_nudge_interval = 10;
    state->turns_since_memory = 0;

    /* S1: Memory pointer initialized to NULL (created in agent_configure_from_config) */
    state->memory = NULL;

    /* L10: Skill nudge defaults — 15 iterations between nudges, disabled by default */
    state->skill_nudge_interval = 0;
    state->iters_since_skill = 0;

    /* L28: Tool delay default — 1.0 second between tool iterations */
    state->tool_delay = 1.0f;
}

/* P99: Initialize agent infrastructure from configuration.
 * Called after init_agent() to apply config settings. */
void agent_configure_from_config(agent_state_t *state, const hermes_config_t *cfg) {
    if (!state || !cfg) return;

    /* Set LLM config from provider config */
    if (cfg->provider_cfg.model[0])
        snprintf(state->llm.model, sizeof(state->llm.model), "%s", cfg->provider_cfg.model);
    if (cfg->provider_cfg.provider[0])
        snprintf(state->llm.provider, sizeof(state->llm.provider), "%s", cfg->provider_cfg.provider);
    if (cfg->provider_cfg.base_url[0])
        snprintf(state->llm.base_url, sizeof(state->llm.base_url), "%s", cfg->provider_cfg.base_url);
    if (cfg->provider_cfg.api_key[0])
        snprintf(state->llm.api_key, sizeof(state->llm.api_key), "%s", cfg->provider_cfg.api_key);

    /* Forward LLM request params from config */
    state->llm.max_tokens = cfg->provider_cfg.max_tokens;
    state->llm.temperature = cfg->provider_cfg.temperature;
    state->llm.top_p = cfg->provider_cfg.top_p;
    state->llm.stop_count = cfg->provider_cfg.stop_count;
    memcpy(state->llm.stop_sequences, cfg->provider_cfg.stop_sequences,
           sizeof(state->llm.stop_sequences));
    state->llm.presence_penalty = cfg->provider_cfg.presence_penalty;
    state->llm.frequency_penalty = cfg->provider_cfg.frequency_penalty;
    state->llm.seed = cfg->provider_cfg.seed;
    state->llm.logprobs = cfg->provider_cfg.logprobs;
    state->llm.top_logprobs = cfg->provider_cfg.top_logprobs;
    memcpy(state->llm.user, cfg->provider_cfg.user, sizeof(state->llm.user));
    memcpy(state->llm.service_tier, cfg->provider_cfg.service_tier,
           sizeof(state->llm.service_tier));
    memcpy(state->llm.reasoning_effort, cfg->provider_cfg.reasoning_effort,
           sizeof(state->llm.reasoning_effort));
    state->llm.max_thinking_tokens = cfg->provider_cfg.max_thinking_tokens;
    memcpy(state->llm.response_format, cfg->provider_cfg.response_format,
           sizeof(state->llm.response_format));
    memcpy(state->llm.metadata, cfg->provider_cfg.metadata,
           sizeof(state->llm.metadata));
    memcpy(state->llm.tool_choice, cfg->provider_cfg.tool_choice,
           sizeof(state->llm.tool_choice));
    state->llm.parallel_tool_calls = cfg->provider_cfg.parallel_tool_calls;
    state->llm.max_tool_calls = cfg->provider_cfg.max_tool_calls;
    state->llm.n = cfg->provider_cfg.n;
    state->llm.top_k = cfg->provider_cfg.top_k;
    state->llm.candidate_count = cfg->provider_cfg.candidate_count;
    state->llm.json_mode = cfg->provider_cfg.json_mode;
    state->llm.response_format_strict = cfg->provider_cfg.response_format_strict;
    memcpy(state->llm.safety_settings, cfg->provider_cfg.safety_settings,
           sizeof(state->llm.safety_settings));
    memcpy(state->llm.extra_body, cfg->provider_cfg.extra_body,
           sizeof(state->llm.extra_body));

    /* Phase 113: Forward retry and fallback config */
    state->llm.max_retries = cfg->agent.api_max_retries;
    memcpy(state->llm.fallback_model, cfg->provider_cfg.fallback_model,
           sizeof(state->llm.fallback_model));
    memcpy(state->llm.fallback_providers, cfg->provider_cfg.fallback_providers,
           sizeof(state->llm.fallback_providers));
    /* AL07: Copy api_mode so llm_client can select Responses API provider */
    memcpy(state->llm.api_mode, cfg->provider_cfg.api_mode,
           sizeof(state->llm.api_mode));

    /* Credential pool: track API key health for rotation hints */
    {
        credential_pool_t *pool = credential_pool_create(cfg->provider_cfg.provider);
        if (pool) {
            credential_pool_add_key(pool, cfg->provider_cfg.api_key, "primary");
            state->llm.cred_pool = (void *)pool;
        }
    }

    /* Max iterations from agent config */
    if (cfg->agent.max_iterations > 0)
        state->max_iterations = cfg->agent.max_iterations;

    /* L28: Tool delay from agent config */
    if (cfg->agent.tool_delay >= 0)
        state->tool_delay = cfg->agent.tool_delay;

    /* S14 gap #12: Result size limit from tools config */
    state->max_result_size = cfg->tools.max_result_size;

    /* Compress enabled */
    state->compress_enabled = cfg->compress_enabled;
    /* P99b: Tail messages from config (0 = use state default) */
    if (cfg->agent.compress_tail_messages >= 2)
        state->compress_tail_messages = cfg->agent.compress_tail_messages;

    /* L02: Compression cooldowns from compression config */
    state->compress_cooldown_secs = cfg->compression.cooldown_secs > 0
        ? cfg->compression.cooldown_secs : 30;
    state->compress_failure_cooldown_secs = cfg->compression.failure_cooldown_secs > 0
        ? cfg->compression.failure_cooldown_secs : 600;

    /* P150: Forward enabled/disabled toolsets */
    if (cfg->tools.enabled_toolsets[0])
        snprintf(state->enabled_toolsets, sizeof(state->enabled_toolsets), "%s", cfg->tools.enabled_toolsets);
    if (cfg->tools.disabled_toolsets[0])
        snprintf(state->disabled_toolsets, sizeof(state->disabled_toolsets), "%s", cfg->tools.disabled_toolsets);

    /* G21: Compression strategy from config */
    if (strcasecmp(cfg->compression.strategy, "oldest_tool_first") == 0)
        state->compression_strategy = EVICT_OLDEST_TOOL_FIRST;
    else if (strcasecmp(cfg->compression.strategy, "oldest_user") == 0)
        state->compression_strategy = EVICT_OLDEST_USER;
    else if (strcasecmp(cfg->compression.strategy, "keep_recent_n") == 0)
        state->compression_strategy = EVICT_KEEP_RECENT_N;

    /* G23: Preserve attachment metadata during compression */
    state->preserve_attachments = cfg->compression.preserve_system; /* reuse preserve_system flag */

    /* G27: Wire checkpoint auto-save interval from config */
    checkpoint_set_limits(&state->checkpoints,
        cfg->checkpoints.max_checkpoints > 0 ? cfg->checkpoints.max_checkpoints : 10,
        cfg->checkpoints.interval > 0 ? cfg->checkpoints.interval : 5);

    /* G24: Per-turn tool call limit from guardrails config or agent config */
    int per_turn = cfg->guardrails.max_tool_calls_per_turn;
    if (per_turn <= 0) per_turn = cfg->agent.max_tool_calls_round;
    if (state->budget)
        budget_tracker_set_per_turn_limit(state->budget, per_turn);

    /* G28-G30: Initialize tool call loop guardrails from config */
    tool_guardrail_init(&state->guardrails_ctrl);
    if (cfg->guardrails.max_consecutive_failures > 0) {
        state->guardrails_ctrl.exact_failure_block_after = cfg->guardrails.max_consecutive_failures;
        state->guardrails_ctrl.same_tool_failure_halt_after = cfg->guardrails.max_consecutive_failures + 3;
    }
    state->guardrails_ctrl.hard_stop_enabled = cfg->guardrails.abort_on_safety_violation;

    /* G26: Budget hard limit mode */
    if (state->budget)
        budget_tracker_set_hard_limit(state->budget, false); /* soft by default */

    /* G20: Derive model family from model name */
    if (cfg->provider_cfg.model[0]) {
        const char *m = cfg->provider_cfg.model;
        if (strstr(m, "claude"))      snprintf(state->model_family, sizeof(state->model_family), "claude");
        else if (strstr(m, "gpt"))    snprintf(state->model_family, sizeof(state->model_family), "gpt");
        else if (strstr(m, "gemini")) snprintf(state->model_family, sizeof(state->model_family), "gemini");
        else if (strstr(m, "deepseek")) snprintf(state->model_family, sizeof(state->model_family), "deepseek");
        else if (strstr(m, "grok"))   snprintf(state->model_family, sizeof(state->model_family), "grok");
        else                          snprintf(state->model_family, sizeof(state->model_family), "unknown");
    }

    /* Budget tracker limits from config */
    if (state->budget) {
        budget_tracker_set_limits(state->budget,
            cfg->agent.max_output_tokens > 0 ? (long long)cfg->agent.max_output_tokens * 10 : 0,
            0,  /* output: no limit from config yet */
            0.0, /* cost: no limit from config yet */
            state->max_iterations > 0 ? state->max_iterations : 0);
    }

    /* B07: Wire shell hooks from config hooks_json */
    if (cfg->hooks_json[0]) {
        json_t *hooks_root = json_parse(cfg->hooks_json, NULL);
        if (hooks_root) {
            shell_hooks_parse_json(hooks_root);
            shell_hooks_register_all();
            json_free(hooks_root);
        }
    }

    /* S1: Initialize memory manager from config */
    if (!state->memory) {
        state->memory = (memory_t *)calloc(1, sizeof(memory_t));
    }
    if (state->memory) {
        memory_init_from_config(state->memory, &cfg->memory);
        /* Load persisted memory data (safe no-op for in-memory, file, or missing backends) */
        memory_load(state->memory);
    }
}

void agent_free(agent_state_t *state) {
    /* S1: Persist and cleanup memory manager */
    if (state->memory) {
        memory_persist(state->memory);
        memory_cleanup(state->memory);
        free(state->memory);
        state->memory = NULL;
    }

    context_clear(state);
    /* Free plugin registry if loaded */
    if (state->plugin_reg) {
        plugin_registry_free((plugin_registry_t *)state->plugin_reg);
        state->plugin_reg = NULL;
    }
    /* Free budget tracker */
    free(state->budget);
    state->budget = NULL;
    /* Free message pointer array */
    free(state->messages);
    state->messages = NULL;
    state->message_capacity = 0;
    /* P89: Unregister SIGINT handler */
#ifndef _WIN32
    if (g_signal_state == state) {
        g_signal_state = NULL;
        signal(SIGINT, SIG_DFL);
    }
#endif
    /* P98: Free checkpoint manager */
    checkpoint_free(&state->checkpoints);

    /* GAP2: Free per-turn file mutation tracker */
    free(state->file_mutations);
    state->file_mutations = NULL;

    /* Free cached system prompt */
    free(state->cached_system_prompt);
    state->cached_system_prompt = NULL;
}

void agent_generate_session_id(agent_state_t *state) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    snprintf(state->session_id, sizeof(state->session_id),
             "%04d%02d%02d_%02d%02d%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/* Open session database. Returns true on success. */
bool agent_open_db(agent_state_t *state) {
    if (state->db) return true;
    char db_dir[HERMES_PATH_MAX + 64];
    snprintf(db_dir, sizeof(db_dir), "%s/sessions", state->hermes_home[0] ?
             state->hermes_home : (getenv("HOME") ? getenv("HOME") : "/tmp"));
    /* Ensure directory exists */
    mkdir(db_dir, 0755);
    state->db = db_open(db_dir, NULL);
    return state->db != NULL;
}

/* Serialize all messages to JSON for DB storage */
char *serialize_messages(const agent_state_t *state) {
    json_node_t *arr = json_new_array();
    for (size_t i = 0; i < state->message_count; i++) {
        json_node_t *msg = json_new_object();
        const char *role_str;
        switch (state->messages[i]->role) {
            case MSG_SYSTEM:    role_str = "system";    break;
            case MSG_USER:      role_str = "user";      break;
            case MSG_ASSISTANT: role_str = "assistant"; break;
            case MSG_TOOL:      role_str = "tool";      break;
            default:            role_str = "user";      break;
        }
        json_object_set(msg, "role", json_new_string(role_str));
        if (state->messages[i]->content)
            json_object_set(msg, "content", json_new_string(state->messages[i]->content));
        json_array_append(arr, msg);
    }
    char *json = json_serialize(arr);
    json_free(arr);
    return json;
}

/* Deserialize messages from JSON into state */
static bool deserialize_messages(agent_state_t *state, const char *json_str) {
    if (!json_str || !*json_str) return false;
    char *err = NULL;
    json_node_t *arr = json_parse(json_str, &err);
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
    return state->message_count > 0;
}

/* Save current session to database */
bool agent_save_session(agent_state_t *state) {
    if (!state->db) return false;
    char *json = serialize_messages(state);
    bool ok = db_save(state->db, state->session_id, json);
    free(json);
    return ok;
}

/* Load a session from database into state */
bool agent_load_session(agent_state_t *state, const char *session_id) {
    if (!state->db) return false;
    context_clear(state);
    if (session_id && *session_id)
        snprintf(state->session_id, sizeof(state->session_id), "%s", session_id);

    char *json = db_load(state->db, state->session_id, NULL);
    if (!json) return false;
    bool ok = deserialize_messages(state, json);
    free(json);
    return ok;
}

/* Close session database */
void agent_close_db(agent_state_t *state) {
    if (state->db) {
        /* Save session + metadata before closing */
        agent_save_session(state);
        agent_save_meta(state);
        db_close(state->db);
        state->db = NULL;
    }
}

/* ================================================================
 *  P141: Session metadata
 * ================================================================ */

/* Save session metadata (title, model, token counts, timestamps) */
bool agent_save_meta(agent_state_t *state) {
    if (!state->db) return false;

    session_meta_t meta;
    db_load_meta(state->db, state->session_id, &meta);

    /* Update fields from current state */
    if (state->user_title[0])
        snprintf(meta.title, sizeof(meta.title), "%s", state->user_title);
    else
        snprintf(meta.title, sizeof(meta.title), "%s", state->session_id);
    if (state->llm.model[0])
        snprintf(meta.model, sizeof(meta.model), "%s", state->llm.model);
    meta.message_count = (int)state->message_count;
    /* Populate token tracking from agent state */
    meta.token_count = state->session_total_tokens;
    meta.input_tokens = state->session_input_tokens;
    meta.output_tokens = state->session_output_tokens;
    meta.cache_read_tokens = state->session_cache_read_tokens;
    meta.cache_write_tokens = state->session_cache_write_tokens;
    /* P141b: Save reasoning tokens and estimated cost */
    meta.reasoning_tokens = state->session_reasoning_tokens;
    meta.estimated_cost = state->session_estimated_cost_usd;
    meta.tool_call_count = 0;
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i])
            meta.tool_call_count += state->messages[i]->tool_calls_count;
    }
    meta.schema_version = SESSION_SCHEMA_VERSION;
    meta.updated_at = time(NULL);
    if (meta.created_at == 0) meta.created_at = meta.updated_at;

    return db_save_meta(state->db, state->session_id, &meta);
}

/* Load session metadata into provided struct */
bool agent_load_meta(agent_state_t *state, session_meta_t *meta) {
    if (!state->db || !meta) return false;
    return db_load_meta(state->db, state->session_id, meta);
}

/* ================================================================
 *  P143: Session CRUD
 * ================================================================ */

/* Create a new session with generated ID and default metadata */
bool agent_session_create(agent_state_t *state, const char *session_id) {
    if (!state->db) return false;
    char new_id[64];
    if (!session_id || !*session_id) {
        /* Generate new session ID */
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        snprintf(new_id, sizeof(new_id), "%04d%02d%02d_%02d%02d%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
        session_id = new_id;
    }

    /* Save empty session data */
    if (!db_save(state->db, session_id, "[]")) return false;

    /* Create and save default metadata */
    session_meta_t meta;
    db_meta_init(&meta);
    if (state->llm.model[0])
        snprintf(meta.model, sizeof(meta.model), "%s", state->llm.model);
    snprintf(meta.title, sizeof(meta.title), "%s", session_id);
    return db_save_meta(state->db, session_id, &meta);
}

/* List sessions with optional tag filtering. Returns malloc'd array, caller must free. */
session_entry_t *agent_session_list(size_t *count, const char *tag_filter, int limit) {
    if (!count) return NULL;
    *count = 0;

    /* Get sessions directory */
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    char db_dir[4096];
    snprintf(db_dir, sizeof(db_dir), "%s/.hermes/sessions", home);

    db_t *db = db_open(db_dir, NULL);
    if (!db) return NULL;

    size_t total = 0;
    db_session_entry_t *entries = db_list_with_meta(db, &total);
    if (!entries) { db_close(db); return NULL; }

    /* Build result with optional tag filtering and limit */
    size_t result_idx = 0;
    size_t result_cap = total > 0 ? total : 1;
    session_entry_t *result = (session_entry_t *)calloc(result_cap, sizeof(session_entry_t));
    if (!result) {
        for (size_t i = 0; i < total; i++) free(entries[i].id);
        free(entries);
        db_close(db);
        return NULL;
    }

    for (size_t i = 0; i < total && result_idx < (size_t)limit; i++) {
        bool include = true;

        /* Apply tag filter */
        if (tag_filter && *tag_filter) {
            include = false;
            for (int t = 0; t < entries[i].meta.tag_count; t++) {
                if (strstr(entries[i].meta.tags[t], tag_filter) ||
                    strstr(tag_filter, entries[i].meta.tags[t])) {
                    include = true;
                    break;
                }
            }
        }

        if (include) {
            snprintf(result[result_idx].id, sizeof(result[result_idx].id), "%s",
                     entries[i].id);
            memcpy(&result[result_idx].meta, &entries[i].meta, sizeof(session_meta_t));
            result_idx++;
        }
    }

    *count = result_idx;
    for (size_t i = 0; i < total; i++) free(entries[i].id);
    free(entries);
    db_close(db);
    return result;
}

/* Delete a session by ID */
bool agent_session_delete(agent_state_t *state, const char *session_id) {
    if (!state->db || !session_id) return false;
    return db_delete(state->db, session_id);
}

/* ================================================================
 *  P144: Auto-save and crash recovery
 * ================================================================ */

/* Auto-save current session if interval turns have passed.
 * Returns true if save was performed. */
bool agent_auto_save(agent_state_t *state, int interval) {
    if (!state->db) return false;
    if (interval <= 0) return false; /* auto-save disabled */

    /* Use a static/state-based turn counter */
    static int auto_save_counter = 0;
    auto_save_counter++;

    if (auto_save_counter >= interval) {
        auto_save_counter = 0;
        agent_save_session(state);
        agent_save_meta(state);
        return true;
    }
    return false;
}

/* Crash recovery: check for .tmp files from interrupted saves.
 * Returns true if recovery was performed. */
bool agent_crash_recover(agent_state_t *state) {
    if (!state->db) return false;

    /* Determine sessions directory */
    char dir_path[HERMES_PATH_MAX + 64];
    snprintf(dir_path, sizeof(dir_path), "%s/sessions",
             state->hermes_home[0] ? state->hermes_home :
             (getenv("HOME") ? getenv("HOME") : "/tmp"));

    /* The db_save function already uses atomic write (write to .tmp, rename to .json).
     * Check for orphaned .tmp files from interrupted writes. */
    DIR *dir = opendir(dir_path);
    if (!dir) return false;

    struct dirent *entry;
    bool recovered = false;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len > 4 && strcmp(name + len - 4, ".tmp") == 0) {
            /* Clean up orphaned .tmp files */
            char tmp_path[HERMES_PATH_MAX * 2];
            snprintf(tmp_path, sizeof(tmp_path), "%s/%s", dir_path, name);
            unlink(tmp_path);
            recovered = true;
        }
    }
    closedir(dir);
    return recovered;
}

/* Add auto-save call after each LLM iteration in the main loop.
 * This is used internally by run_conversation. */

/* ================================================================
 *  P145: Auto-prune
 * ================================================================ */

/* Remove sessions older than retention_days. Returns number removed. */
int agent_auto_prune(agent_state_t *state, int retention_days) {
    if (!state->db || retention_days <= 0) return 0;
    return db_prune_by_age(state->db, retention_days);
}

/* ================================================================
 *  P146: Session tags
 * ================================================================ */

/* Add a tag to the current session's metadata */
bool agent_session_add_tag(agent_state_t *state, const char *tag) {
    if (!state->db || !tag || !*tag) return false;

    session_meta_t meta;
    db_load_meta(state->db, state->session_id, &meta);

    /* Check if tag already exists */
    for (int i = 0; i < meta.tag_count; i++) {
        if (strcmp(meta.tags[i], tag) == 0)
            return true; /* already present */
    }

    /* Add tag */
    if (meta.tag_count < 32) {
        snprintf(meta.tags[meta.tag_count], sizeof(meta.tags[0]), "%s", tag);
        meta.tag_count++;
        meta.updated_at = time(NULL);
        return db_save_meta(state->db, state->session_id, &meta);
    }
    return false;
}

/* Remove a tag from the current session's metadata */
bool agent_session_remove_tag(agent_state_t *state, const char *tag) {
    if (!state->db || !tag || !*tag) return false;

    session_meta_t meta;
    if (!db_load_meta(state->db, state->session_id, &meta))
        return false;

    int found = -1;
    for (int i = 0; i < meta.tag_count; i++) {
        if (strcmp(meta.tags[i], tag) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) return false;

    /* Shift remaining tags */
    for (int i = found; i < meta.tag_count - 1; i++)
        snprintf(meta.tags[i], sizeof(meta.tags[0]), "%s", meta.tags[i + 1]);
    meta.tag_count--;
    meta.updated_at = time(NULL);
    return db_save_meta(state->db, state->session_id, &meta);
}

/* ================================================================
 *  P148: Session export
 * ================================================================ */

/* Export session as JSON. Caller must free. */
char *agent_session_export_json(agent_state_t *state, const char *session_id) {
    if (!state->db || !session_id) return NULL;
    return db_export_json(state->db, session_id);
}

/* Export session as Markdown. Caller must free. */
char *agent_session_export_markdown(agent_state_t *state, const char *session_id) {
    if (!state->db || !session_id) return NULL;
    return db_export_markdown(state->db, session_id);
}

/* ================================================================
 *  P149: Session branch
 * ================================================================ */

/* Branch session at message index N. Creates new session with shared prefix. */
bool agent_session_branch(agent_state_t *state, const char *new_id, int branch_point) {
    if (!state->db || !new_id || branch_point < 0) return false;

    /* Generate new session ID if not provided */
    char generated_id[64];
    if (!*new_id) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        snprintf(generated_id, sizeof(generated_id), "%04d%02d%02d_%02d%02d%02d_br",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
        new_id = generated_id;
    }

    char *branched_data = db_branch(state->db, state->session_id, new_id, branch_point);
    if (!branched_data) return false;
    free(branched_data);
    return true;
}

/* ================================================================
 *  P150: Session migration
 * ================================================================ */

/* Migrate all sessions to latest schema. Returns number migrated. */
int agent_session_migrate(agent_state_t *state) {
    if (!state->db) return 0;
    return migrate(state->db);
}

/* ================================================================
 *  Port of Python agent/conversation_loop.py functions (REAL_GAP closure)
 * ================================================================ */

/* Port of Python agent/conversation_loop.py:_image_error_max_dimension().
 * Extract a provider-reported image dimension ceiling from an error message.
 * Returns the dimension or 0 if not found. */
int _image_error_max_dimension(const char *error_str) {
    if (!error_str) return 0;
    
    /* Search for "max allowed size" pattern in error message */
    const char *p = strstr(error_str, "max allowed size");
    if (!p) return 0;
    
    /* Look for digits after the pattern */
    p += strlen("max allowed size");
    while (*p && (*p < '0' || *p > '9')) p++;
    
    if (!*p) return 0;
    
    int dimension = atoi(p);
    if (dimension >= 512 && dimension <= 8000) {
        return dimension;
    }
    return 0;
}

/* Port of Python agent/conversation_loop.py:_try_refresh_nous_paid_entitlement_credentials().
 * Refresh Nous runtime credentials after a fresh paid-entitlement check.
 * Returns true if credentials were refreshed.
 *
 * Implementation: loads auth.json for the "nous" provider, checks the
 * access_token expiry, and if expired or force-refreshed, POSTs to the
 * portal OAuth refresh endpoint with the refresh token to get a new
 * access token. Mirrors hermes_cli/auth.py:refresh_nous_oauth_pure().
 * If no refresh is needed (token still valid), returns false — same as
 * Python's no-op path. */
static json_t *_arl_load_nous_state(void) {
    /* Reuse the auth.json reader from chat_completion_helpers —
     * but that's static, so we inline the equivalent logic here. */
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
    json_t *state = providers ? json_obj_get(providers, "nous") : NULL;
    json_t *result = state ? json_copy(state) : NULL;
    json_free(store);
    return result;
}

static int _arl_save_nous_state(json_t *new_state) {
    /* Write the updated nous provider state back to auth.json.
     * We load the full auth.json, update the nous key, and write it back. */
    const char *home = getenv("HERMES_HOME");
    char path[4096];
    if (home && *home)
        snprintf(path, sizeof(path), "%s/auth.json", home);
    else {
        const char *h = getenv("HOME");
        if (!h || !*h) return -1;
        snprintf(path, sizeof(path), "%s/.hermes/auth.json", h);
    }
    /* Load existing auth.json */
    FILE *f = fopen(path, "r");
    json_t *store = NULL;
    if (f) {
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            char *buf = malloc((size_t)sz + 1);
            if (buf) {
                size_t rd = fread(buf, 1, (size_t)sz, f);
                buf[rd] = '\0';
                store = json_parse(buf, NULL);
                free(buf);
            }
        }
        fclose(f);
    }
    if (!store) store = json_new_object();
    json_t *providers = json_obj_get(store, "providers");
    if (!providers) {
        providers = json_new_object();
        json_object_set(store, "providers", providers);
    }
    /* Replace the nous entry */
    json_object_set(providers, "nous", new_state);
    /* Write back */
    char *json_str = json_dumps(store, 0);
    json_free(store);
    if (!json_str) return -1;
    f = fopen(path, "w");
    if (!f) { free(json_str); return -1; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);
    return 0;
}

bool _try_refresh_nous_paid_entitlement_credentials(agent_state_t *state) {
    if (!state) return false;

    /* Only refresh for Nous provider */
    if (!state->llm.provider[0] || strcasecmp(state->llm.provider, "nous") != 0)
        return false;

    /* Load the current auth state */
    json_t *auth_state = _arl_load_nous_state();
    if (!auth_state) {
        hermes_log(LOG_WARNING, "agent",
            "nous credential refresh: no auth state found");
        return false;
    }

    /* Check if access token is still valid */
    const char *access_token = json_object_get_string(auth_state, "access_token", "");
    const char *expires_at = json_object_get_string(auth_state, "expires_at", "");
    const char *refresh_token = json_object_get_string(auth_state, "refresh_token", "");
    const char *portal_url = json_object_get_string(auth_state, "portal_base_url",
                                                      "https://portal.nousresearch.com");

    /* If we have a valid access token that hasn't expired, no refresh needed */
    if (access_token[0] && expires_at[0]) {
        /* Simple expiry check: compare expires_at against current time.
         * expires_at is ISO 8601 format. We do a basic parse. */
        time_t now = time(NULL);
        /* Try to parse ISO 8601 — basic approach, compare strings.
         * A real implementation would use strptime, but for a simple
         * freshness check we look at whether we're within 60s of expiry. */
        struct tm tm_expires = {0};
        if (strptime(expires_at, "%Y-%m-%dT%H:%M:%S", &tm_expires)) {
            time_t expires = mktime(&tm_expires);
            if (expires - now > 60) {
                /* Token still valid — no refresh needed */
                json_free(auth_state);
                return false;
            }
        }
        /* If we can't parse, attempt refresh anyway */
    }

    /* Need refresh — check we have a refresh token */
    if (!refresh_token[0]) {
        hermes_log(LOG_WARNING, "agent",
            "nous credential refresh: no refresh token, re-auth required");
        json_free(auth_state);
        return false;
    }

    /* POST to the portal OAuth refresh endpoint */
    /* Build request: grant_type=refresh_token&refresh_token=...&client_id=... */
    const char *client_id = json_object_get_string(auth_state, "client_id",
                                                     "hermes-cl87");
    char url[512];
    snprintf(url, sizeof(url), "%s/oauth/token", portal_url);

    /* Build form-encoded body */
    char body[4096];
    snprintf(body, sizeof(body),
        "grant_type=refresh_token&refresh_token=%s&client_id=%s",
        refresh_token, client_id);

    /* Make the HTTP request */
    http_t *client = http_new(30);
    if (!client) {
        json_free(auth_state);
        return false;
    }
    http_resp_t *resp = http_request(client, HTTP_POST, url,
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Accept: application/json\r\n",
        body, strlen(body));
    http_free(client);

    if (!resp || resp->status < 200 || resp->status >= 300) {
        hermes_log(LOG_WARNING, "agent",
            "nous credential refresh: HTTP %d from %s",
            resp ? resp->status : 0, url);
        if (resp) http_resp_free(resp);
        json_free(auth_state);
        return false;
    }

    /* Parse the response JSON */
    json_t *refreshed = json_parse(resp->body, NULL);
    http_resp_free(resp);
    if (!refreshed) {
        hermes_log(LOG_WARNING, "agent",
            "nous credential refresh: failed to parse response");
        json_free(auth_state);
        return false;
    }

    /* Extract new tokens */
    const char *new_access = json_object_get_string(refreshed, "access_token", "");
    const char *new_refresh = json_object_get_string(refreshed, "refresh_token", "");
    const char *new_scope = json_object_get_string(refreshed, "scope", "");
    int expires_in = (int)json_get_num(refreshed, "expires_in", 0);

    if (!new_access[0]) {
        hermes_log(LOG_WARNING, "agent",
            "nous credential refresh: no access_token in response");
        json_free(refreshed);
        json_free(auth_state);
        return false;
    }

    /* Update auth state */
    json_object_set(auth_state, "access_token", json_string(new_access));
    if (new_refresh[0])
        json_object_set(auth_state, "refresh_token", json_string(new_refresh));
    if (new_scope[0])
        json_object_set(auth_state, "scope", json_string(new_scope));
    if (expires_in > 0) {
        /* Compute new expires_at */
        time_t exp = time(NULL) + expires_in;
        char exp_str[32];
        strftime(exp_str, sizeof(exp_str), "%Y-%m-%dT%H:%M:%S", gmtime(&exp));
        json_object_set(auth_state, "expires_at", json_string(exp_str));
    }

    /* Save updated state back to auth.json */
    _arl_save_nous_state(auth_state);

    /* Update the agent state's API key with the new access token */
    snprintf(state->llm.api_key, sizeof(state->llm.api_key), "%s", new_access);

    hermes_log(LOG_INFO, "agent",
        "nous credential refresh: success, new token valid until %s",
        json_object_get_string(auth_state, "expires_at", "unknown"));

    json_free(refreshed);
    json_free(auth_state);
    return true;
}

/* Port of Python agent/conversation_loop.py:_restore_or_build_system_prompt().
 * Restore the cached system prompt from session DB or build it fresh.
 * Mutates state->cached_system_prompt and persists a freshly-built prompt.
 * Uses session DB meta_json "system_prompt" key for persistence. */
static char *load_system_prompt_from_db(db_t *db, const char *session_id) {
    if (!db || !session_id || !session_id[0]) return NULL;
    session_meta_t meta;
    if (!db_load_meta(db, session_id, &meta)) return NULL;
    if (!meta.meta_json[0]) return NULL;
    json_node_t *root = json_parse(meta.meta_json, NULL);
    if (!root) return NULL;
    const char *val = json_get_str(root, "system_prompt", NULL);
    char *result = NULL;
    if (val && val[0]) result = strdup(val);
    json_free(root);
    return result;
}

static bool save_system_prompt_to_db(db_t *db, const char *session_id, const char *prompt) {
    if (!db || !session_id || !session_id[0] || !prompt) return false;
    session_meta_t meta;
    db_load_meta(db, session_id, &meta);
    json_node_t *root = json_parse(meta.meta_json[0] ? meta.meta_json : "{}", NULL);
    if (!root) root = json_new_object();
    json_object_set(root, "system_prompt", json_new_string(prompt));
    char *new_json = json_serialize(root);
    json_free(root);
    if (!new_json) return false;
    snprintf(meta.meta_json, sizeof(meta.meta_json), "%s", new_json);
    free(new_json);
    meta.updated_at = time(NULL);
    return db_save_meta(db, session_id, &meta);
}

void _restore_or_build_system_prompt(agent_state_t *state, const char *system_message, bool has_conversation_history) {
    if (!state) return;

    /* If we have a cached system prompt, use it */
    if (state->cached_system_prompt && state->cached_system_prompt[0]) {
        return;
    }

    /* Try to restore from session DB */
    if (state->db && state->session_id[0] && has_conversation_history) {
        char *stored = load_system_prompt_from_db(state->db, state->session_id);
        if (stored && stored[0]) {
            state->cached_system_prompt = stored;
            return;
        }
        free(stored);
    }

    /* Build fresh system prompt using the real C API */
    system_prompt_config_t sp_cfg;
    memset(&sp_cfg, 0, sizeof(sp_cfg));
    sp_cfg.system_message = system_message;
    sp_cfg.model_name = state->llm.model;
    sp_cfg.provider_name = state->llm.provider;
    if (state->session_id[0]) {
        sp_cfg.session_id = state->session_id;
        sp_cfg.pass_session_id = true;
    }
    /* Use SOUL.md if available */
    sp_cfg.use_soul = true;
    /* Enable standard guidance blocks */
    sp_cfg.has_memory = (state->memory != NULL);
    sp_cfg.has_skills = true;
    sp_cfg.enforce_tools = true;
    sp_cfg.use_task_completion = true;

    if (state->cached_system_prompt) free(state->cached_system_prompt);
    state->cached_system_prompt = system_prompt_build(&sp_cfg);

    /* Persist to session DB for prefix-cache reuse on subsequent turns */
    if (state->db && state->session_id[0] && state->cached_system_prompt) {
        save_system_prompt_to_db(state->db, state->session_id, state->cached_system_prompt);
    }
}

/* Port of Python agent/conversation_loop.py:_content_policy_blocked_result().
 * Build the terminal turn result for a content-policy block.
 * Returns a JSON string that caller must free. */
char *_content_policy_blocked_result(const char *messages_json, int api_call_count,
                                      const char *final_response, const char *error_detail) {
    if (!final_response) final_response = "";
    if (!error_detail) error_detail = "content policy blocked";
    
    json_node_t *root = json_object();
    json_set(root, "final_response", json_string(final_response));
    if (messages_json) {
        json_node_t *msgs = json_parse(messages_json, NULL);
        if (msgs) json_set(root, "messages", msgs);
    }
    json_set(root, "api_calls", json_number(api_call_count));
    json_set(root, "completed", json_bool(false));
    json_set(root, "failed", json_bool(true));
    
    char error_buf[512];
    snprintf(error_buf, sizeof(error_buf), "content_policy_blocked: %s", error_detail);
    json_set(root, "error", json_string(error_buf));
    
    char *result = json_serialize(root);
    json_free(root);
    return result;
}

