/*
 * port_hermes_cli_shared_metrics_contract.c — C11 port of
 * hermes_cli/observability/shared_metrics_contract.py.
 *
 * Bounded product contract for the first Hermes shared-metrics slice.
 * Pure deterministic functions: allowlisted dimensions, families,
 * buckets, and validators. No I/O, no SQLite, no relay dependency.
 *
 * This is a cohesive PoP port of ONE Python module (18 functions).
 * It is NOT a monolith — do not split speculatively.
  */

 #define _POSIX_C_SOURCE 200809L  /* for strdup */

 #include "port_hermes_cli_shared_metrics_contract.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════
 * PoP: smc_execution_surfaces @
 *   hermes_cli/observability/shared_metrics_contract.py:EXECUTION_SURFACES
 * PoP: smc_provider_families @
 *   hermes_cli/observability/shared_metrics_contract.py:PROVIDER_FAMILIES
 * PoP: smc_model_localities @
 *   hermes_cli/observability/shared_metrics_contract.py:MODEL_LOCALITIES
 * PoP: smc_model_outcomes @
 *   hermes_cli/observability/shared_metrics_contract.py:MODEL_OUTCOMES
 * PoP: smc_task_outcomes @
 *   hermes_cli/observability/shared_metrics_contract.py:TASK_OUTCOMES
 * PoP: smc_task_end_reasons @
 *   hermes_cli/observability/shared_metrics_contract.py:TASK_END_REASONS
 * PoP: smc_task_terminations @
 *   hermes_cli/observability/shared_metrics_contract.py:TASK_TERMINATIONS
 * PoP: smc_task_entrypoints @
 *   hermes_cli/observability/shared_metrics_contract.py:TASK_ENTRYPOINTS
 * PoP: smc_duration_buckets @
 *   hermes_cli/observability/shared_metrics_contract.py:DURATION_BUCKETS
 * PoP: smc_count_buckets @
 *   hermes_cli/observability/shared_metrics_contract.py:COUNT_BUCKETS
 * PoP: smc_model_families @
 *   hermes_cli/observability/shared_metrics_contract.py:MODEL_FAMILIES
 * PoP: smc_counter_metrics @
 *   hermes_cli/observability/shared_metrics_contract.py:COUNTER_METRICS
 * ═══════════════════════════════════════════════════════════════════ */

/* ── Allowlisted value sets ────────────────────────────────────── */

const char *smc_execution_surfaces[] = {
    "api", "batch", "cli", "desktop", "gateway", "python",
    "scheduled_task", "tui", "other", "unknown",
};
const size_t smc_execution_surfaces_count =
    sizeof(smc_execution_surfaces) / sizeof(smc_execution_surfaces[0]);

const char *smc_provider_families[] = {
    "aggregator", "custom", "direct", "local", "unknown",
};
const size_t smc_provider_families_count =
    sizeof(smc_provider_families) / sizeof(smc_provider_families[0]);

const char *smc_model_localities[] = {
    "local", "remote", "unknown",
};
const size_t smc_model_localities_count =
    sizeof(smc_model_localities) / sizeof(smc_model_localities[0]);

const char *smc_model_outcomes[] = {
    "cancelled", "failed", "success",
};
const size_t smc_model_outcomes_count =
    sizeof(smc_model_outcomes) / sizeof(smc_model_outcomes[0]);

const char *smc_task_outcomes[] = {
    "cancelled", "failed", "success", "timed_out", "unknown",
};
const size_t smc_task_outcomes_count =
    sizeof(smc_task_outcomes) / sizeof(smc_task_outcomes[0]);

const char *smc_task_end_reasons[] = {
    "approval_denied", "completed", "failed", "guardrail_blocked",
    "iteration_limit", "system_aborted", "timed_out", "unknown",
    "user_cancelled",
};
const size_t smc_task_end_reasons_count =
    sizeof(smc_task_end_reasons) / sizeof(smc_task_end_reasons[0]);

const char *smc_task_terminations[] = {
    "none", "system_aborted", "timed_out", "unknown", "user_cancelled",
};
const size_t smc_task_terminations_count =
    sizeof(smc_task_terminations) / sizeof(smc_task_terminations[0]);

const char *smc_task_entrypoints[] = {
    "api", "background", "batch", "delegated", "gateway_message",
    "interactive", "other", "python", "scheduled_task", "unknown",
};
const size_t smc_task_entrypoints_count =
    sizeof(smc_task_entrypoints) / sizeof(smc_task_entrypoints[0]);

const char *smc_duration_buckets[] = {
    "1s_to_5s", "2m_to_10m", "30s_to_2m", "5s_to_30s",
    "gte_10m", "lt_1s",
};
const size_t smc_duration_buckets_count =
    sizeof(smc_duration_buckets) / sizeof(smc_duration_buckets[0]);

const char *smc_count_buckets[] = {
    "0", "1", "2", "3_to_5", "6_to_10", "gte_11",
};
const size_t smc_count_buckets_count =
    sizeof(smc_count_buckets) / sizeof(smc_count_buckets[0]);

const char *smc_model_families[] = {
    "claude", "deepseek", "gemini", "gemma", "glm", "gpt", "grok",
    "kimi", "llama", "minimax", "mimo", "mistral", "nemotron",
    "nova", "qwen", "step", "trinity", "o1", "o3", "o4", "unknown",
};
const size_t smc_model_families_count =
    sizeof(smc_model_families) / sizeof(smc_model_families[0]);

/* ── Counter metrics ──────────────────────────────────────────── */

const char *smc_counter_metrics[] = {
    SMC_MODEL_CALL_METRIC,
    SMC_TASK_STARTED_METRIC,
    SMC_TASK_FINISHED_METRIC,
};
const size_t smc_counter_metrics_count =
    sizeof(smc_counter_metrics) / sizeof(smc_counter_metrics[0]);

/* ── Telemetry aggregator overrides (internal) ────────────────── */

static const char *smc_aggregator_overrides[] = {
    "copilot-acp", "github-copilot", "moa", "nous",
};
static const size_t smc_aggregator_overrides_count =
    sizeof(smc_aggregator_overrides) / sizeof(smc_aggregator_overrides[0]);

/* ── Local custom provider aliases (internal) ─────────────────── */
static const char *smc_local_custom_aliases[] = {
    "mlx", "ollama",
};
static const size_t smc_local_custom_aliases_count =
    sizeof(smc_local_custom_aliases) / sizeof(smc_local_custom_aliases[0]);

/* ── Helper: str in sorted set (like frozenset.__contains__) ──── */

static bool str_in_set(const char *value,
                       const char * const *set,
                       size_t set_count)
{
    if (!value) return false;
    for (size_t i = 0; i < set_count; i++) {
        if (strcmp(value, set[i]) == 0)
            return true;
    }
    return false;
}

/* ── Helper: lowercase + strip ─────────────────────────────────── */

static char *str_lower_strip(const char *s)
{
    if (!s) return NULL;
    /* Skip leading whitespace */
    while (*s && (unsigned char)*s <= ' ') s++;
    if (!*s) return strdup("");
    size_t len = strlen(s);
    /* Trim trailing whitespace */
    while (len > 0 && (unsigned char)s[len - 1] <= ' ') len--;
    char *result = malloc(len + 1);
    if (!result) return NULL;
    for (size_t i = 0; i < len; i++)
        result[i] = (char)tolower((unsigned char)s[i]);
    result[len] = '\0';
    return result;
}

/* ── Helper: str replace _ with - ──────────────────────────────── */

static char *str_replace_char(const char *s, char old_ch, char new_ch)
{
    if (!s) return NULL;
    char *dup = strdup(s);
    if (!dup) return NULL;
    for (char *p = dup; *p; p++) {
        if (*p == old_ch) *p = new_ch;
    }
    return dup;
}

/* ═══════════════════════════════════════════════════════════════════
 * Pure function implementations
 * ═══════════════════════════════════════════════════════════════════ */

/* PoP: smc_counter_dimensions_are_valid @
 *   hermes_cli/observability/shared_metrics_contract.py:counter_dimensions_are_valid
/* PoP: smc_counter_dimensions_are_valid @ hermes_cli/observability/shared_metrics_contract.py:counter_dimensions_are_valid */
bool smc_counter_dimensions_are_valid(const char *metric_name,
                                       const char * const *dim_keys,
                                       const char * const *dim_vals,
                                       size_t dim_count)
{
    if (!metric_name || !dim_keys || !dim_vals)
        return false;

    /* The Python contract defines dimension schemas per metric.
     * For the C port, we validate against the known schemas.
     * model_call: call_role, locality, model_family, outcome, provider_family
     * task_started: entrypoint, execution_surface
     * task_finished: 9 fields (duration_bucket, end_reason, entrypoint, ...)
     */
    size_t expected_count = 0;
    const char * const *expected_keys = NULL;

    if (strcmp(metric_name, SMC_MODEL_CALL_METRIC) == 0) {
        expected_count = 5;
        static const char *keys[] =
            {"call_role", "locality", "model_family", "outcome", "provider_family"};
        expected_keys = keys;
        /* Value validation: each dimension value must be in its allowlist */
        /* For the pure contract, we validate that values match known sets */
    } else if (strcmp(metric_name, SMC_TASK_STARTED_METRIC) == 0) {
        expected_count = 2;
        static const char *keys[] = {"entrypoint", "execution_surface"};
        expected_keys = keys;
    } else if (strcmp(metric_name, SMC_TASK_FINISHED_METRIC) == 0) {
        expected_count = 9;
        static const char *keys[] = {
            "duration_bucket", "end_reason", "entrypoint",
            "execution_surface", "model_call_count_bucket", "outcome",
            "retry_count_bucket", "termination", "tool_call_count_bucket",
        };
        expected_keys = keys;
    } else {
        return false;
    }

    if (dim_count != expected_count)
        return false;

    /* Check all expected keys present and in order (like Python's set(dimensions) != set(contract)) */
    for (size_t i = 0; i < expected_count; i++) {
        bool found = false;
        for (size_t j = 0; j < dim_count; j++) {
            if (strcmp(dim_keys[j], expected_keys[i]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    /* Validate each dimension value against its allowlist */
    for (size_t i = 0; i < dim_count; i++) {
        const char *val = dim_vals[i];
        if (!val) return false;
        if (strcmp(dim_keys[i], "call_role") == 0) {
            if (strcmp(val, SMC_PRIMARY_MODEL_CALL_ROLE) != 0)
                return false;
        } else if (strcmp(dim_keys[i], "locality") == 0) {
            if (!str_in_set(val, smc_model_localities, smc_model_localities_count))
                return false;
        } else if (strcmp(dim_keys[i], "model_family") == 0) {
            if (!str_in_set(val, smc_model_families, smc_model_families_count))
                return false;
        } else if (strcmp(dim_keys[i], "outcome") == 0) {
            if (!str_in_set(val, smc_model_outcomes, smc_model_outcomes_count)
                && !str_in_set(val, smc_task_outcomes, smc_task_outcomes_count))
                return false;
        } else if (strcmp(dim_keys[i], "provider_family") == 0) {
            if (!str_in_set(val, smc_provider_families, smc_provider_families_count))
                return false;
        } else if (strcmp(dim_keys[i], "entrypoint") == 0) {
            if (!str_in_set(val, smc_task_entrypoints, smc_task_entrypoints_count))
                return false;
        } else if (strcmp(dim_keys[i], "execution_surface") == 0) {
            if (!str_in_set(val, smc_execution_surfaces, smc_execution_surfaces_count))
                return false;
        } else if (strcmp(dim_keys[i], "duration_bucket") == 0) {
            if (!str_in_set(val, smc_duration_buckets, smc_duration_buckets_count))
                return false;
        } else if (strcmp(dim_keys[i], "end_reason") == 0) {
            if (!str_in_set(val, smc_task_end_reasons, smc_task_end_reasons_count))
                return false;
        } else if (strcmp(dim_keys[i], "termination") == 0) {
            if (!str_in_set(val, smc_task_terminations, smc_task_terminations_count))
                return false;
        } else if (strcmp(dim_keys[i], "model_call_count_bucket") == 0
                   || strcmp(dim_keys[i], "retry_count_bucket") == 0
                   || strcmp(dim_keys[i], "tool_call_count_bucket") == 0) {
            if (!str_in_set(val, smc_count_buckets, smc_count_buckets_count))
                return false;
        }
    }

    return true;
}

/* PoP: smc_execution_surface @
 *   hermes_cli/observability/shared_metrics_contract.py:execution_surface
/* PoP: smc_execution_surface @ hermes_cli/observability/shared_metrics_contract.py:execution_surface */
const char *smc_execution_surface(const char *platform_value)
{
    if (!platform_value) return "unknown";

    char *normalized = str_lower_strip(platform_value);
    if (!normalized) return "unknown";

    const char *result = NULL;

    /* Empty string maps to "unknown" */
    if (!*normalized) {
        free(normalized);
        return "unknown";
    }

    if (str_in_set(normalized, smc_execution_surfaces, smc_execution_surfaces_count)) {
        /* Find the canonical form (it may have been lowercased) */
        for (size_t i = 0; i < smc_execution_surfaces_count; i++) {
            if (strcmp(normalized, smc_execution_surfaces[i]) == 0) {
                result = smc_execution_surfaces[i];
                break;
            }
        }
    } else if (strcmp(normalized, "api_server") == 0) {
        result = "api";
    } else if (strcmp(normalized, "cron") == 0
               || strcmp(normalized, "scheduler") == 0
               || strcmp(normalized, "scheduled") == 0) {
        result = "scheduled_task";
    } else if (strcmp(normalized, "discord") == 0
               || strcmp(normalized, "email") == 0
               || strcmp(normalized, "slack") == 0
               || strcmp(normalized, "telegram") == 0
               || strcmp(normalized, "teams") == 0
               || strcmp(normalized, "whatsapp") == 0) {
        result = "gateway";
    } else {
        result = (strcmp(normalized, "unknown") == 0) ? "unknown" : "other";
    }

    free(normalized);
    return result;
}

/* PoP: smc_task_start_fields @
 *   hermes_cli/observability/shared_metrics_contract.py:task_start_fields
/* PoP: smc_task_start_fields @ hermes_cli/observability/shared_metrics_contract.py:task_start_fields */
char *smc_task_start_fields(const char *entrypoint_val,
                            const char *platform_val)
{
    const char *surface = smc_execution_surface(platform_val);
    const char *ep = smc_task_entrypoint(entrypoint_val, surface, false, false);

    /* Build JSON string */
    int needed = snprintf(NULL, 0,
        "{\"entrypoint\":\"%s\",\"execution_surface\":\"%s\"}", ep, surface);
    char *result = malloc((size_t)needed + 1);
    if (result) {
        snprintf(result, (size_t)needed + 1,
            "{\"entrypoint\":\"%s\",\"execution_surface\":\"%s\"}", ep, surface);
    }
    return result;
}

/* PoP: smc_task_entrypoint @
 *   hermes_cli/observability/shared_metrics_contract.py:task_entrypoint
/* PoP: smc_task_entrypoint @ hermes_cli/observability/shared_metrics_contract.py:task_entrypoint */
const char *smc_task_entrypoint(const char *entrypoint_val,
                                 const char *surface,
                                 bool has_parent_task,
                                 bool has_parent_session)
{
    if (!entrypoint_val) entrypoint_val = "";
    if (!surface) surface = "";

    char *normalized = str_lower_strip(entrypoint_val);
    if (!normalized) return "other";

    const char *result = NULL;

    if (str_in_set(normalized, smc_task_entrypoints, smc_task_entrypoints_count)) {
        for (size_t i = 0; i < smc_task_entrypoints_count; i++) {
            if (strcmp(normalized, smc_task_entrypoints[i]) == 0) {
                result = smc_task_entrypoints[i];
                goto cleanup;
            }
        }
    }

    /* Resolved surface logic */
    if (has_parent_task || has_parent_session) {
        result = "delegated";
        goto cleanup;
    }

    if (strcmp(surface, "api") == 0)
        result = "api";
    else if (strcmp(surface, "batch") == 0)
        result = "batch";
    else if (strcmp(surface, "cli") == 0)
        result = "interactive";
    else if (strcmp(surface, "desktop") == 0)
        result = "interactive";
    else if (strcmp(surface, "gateway") == 0)
        result = "gateway_message";
    else if (strcmp(surface, "python") == 0)
        result = "python";
    else if (strcmp(surface, "scheduled_task") == 0)
        result = "scheduled_task";
    else if (strcmp(surface, "tui") == 0)
        result = "interactive";
    else if (strcmp(surface, "unknown") == 0)
        result = "unknown";
    else
        result = "other";

cleanup:
    free(normalized);
    return result;
}

/* PoP: smc_task_terminal_fields @
 *   hermes_cli/observability/shared_metrics_contract.py:task_terminal_fields
/* PoP: smc_task_terminal_fields @ hermes_cli/observability/shared_metrics_contract.py:task_terminal_fields */
char *smc_task_terminal_fields(const char *entrypoint_val,
                                const char *platform_val,
                                int duration_ms,
                                int model_call_count,
                                int tool_call_count,
                                int retry_count)
{
    const char *surface = smc_execution_surface(platform_val);
    const char *ep = smc_task_entrypoint(entrypoint_val, surface, false, false);
    const char *dur_bucket = smc_duration_bucket(duration_ms);
    const char *mc_bucket = smc_count_bucket(model_call_count);
    const char *tc_bucket = smc_count_bucket(tool_call_count);
    const char *rc_bucket = smc_count_bucket(retry_count);

    /* Default outcome/end_reason/termination — caller should override */
    const char *outcome = "unknown";
    const char *end_reason = "unknown";
    const char *termination = "unknown";

    int needed = snprintf(NULL, 0,
        "{"
        "\"entrypoint\":\"%s\","
        "\"execution_surface\":\"%s\","
        "\"duration_bucket\":\"%s\","
        "\"end_reason\":\"%s\","
        "\"model_call_count_bucket\":\"%s\","
        "\"outcome\":\"%s\","
        "\"retry_count_bucket\":\"%s\","
        "\"termination\":\"%s\","
        "\"tool_call_count_bucket\":\"%s\""
        "}",
        ep, surface, dur_bucket, end_reason,
        mc_bucket, outcome, rc_bucket, termination, tc_bucket);

    char *result = malloc((size_t)needed + 1);
    if (result) {
        snprintf(result, (size_t)needed + 1,
            "{"
            "\"entrypoint\":\"%s\","
            "\"execution_surface\":\"%s\","
            "\"duration_bucket\":\"%s\","
            "\"end_reason\":\"%s\","
            "\"model_call_count_bucket\":\"%s\","
            "\"outcome\":\"%s\","
            "\"retry_count_bucket\":\"%s\","
            "\"termination\":\"%s\","
            "\"tool_call_count_bucket\":\"%s\""
            "}",
            ep, surface, dur_bucket, end_reason,
            mc_bucket, outcome, rc_bucket, termination, tc_bucket);
    }
    return result;
}

/* PoP: smc_task_terminal_state @
 *   hermes_cli/observability/shared_metrics_contract.py:task_terminal_state
/* PoP: smc_task_terminal_state @ hermes_cli/observability/shared_metrics_contract.py:task_terminal_state */
char *smc_task_terminal_state(const char *turn_exit_reason,
                               bool interrupted,
                               bool completed,
                               bool failed)
{
    const char *outcome = "unknown";
    const char *end_reason = "unknown";
    const char *termination = "unknown";

    if (!turn_exit_reason) turn_exit_reason = "";

    char *reason = str_lower_strip(turn_exit_reason);
    if (!reason) {
        /* fallback to defaults */
        goto build_result;
    }

    if (interrupted || strstr(reason, "interrupt") || strstr(reason, "cancel")) {
        outcome = "cancelled";
        end_reason = "user_cancelled";
        termination = "user_cancelled";
    } else if (strstr(reason, "timeout") || strstr(reason, "timed_out")) {
        outcome = "timed_out";
        end_reason = "timed_out";
        termination = "timed_out";
    } else if (strstr(reason, "max_iterations") || strstr(reason, "budget_exhausted")) {
        outcome = "failed";
        end_reason = "iteration_limit";
        termination = "system_aborted";
    } else if (strstr(reason, "approval") && (strstr(reason, "denied") || strstr(reason, "rejected"))) {
        outcome = "failed";
        end_reason = "approval_denied";
        termination = "none";
    } else if (strstr(reason, "guardrail")) {
        outcome = "failed";
        end_reason = "guardrail_blocked";
        termination = "system_aborted";
    } else if (strcmp(reason, "system_aborted") == 0) {
        outcome = "failed";
        end_reason = "system_aborted";
        termination = "system_aborted";
    } else if (completed) {
        outcome = "success";
        end_reason = "completed";
        termination = "none";
    } else if (failed || (reason[0] && strcmp(reason, "unknown") != 0)) {
        outcome = "failed";
        end_reason = "failed";
        termination = "none";
    }

    free(reason);

build_result:
    {
        int needed = snprintf(NULL, 0,
            "[\"%s\",\"%s\",\"%s\"]", outcome, end_reason, termination);
        char *result = malloc((size_t)needed + 1);
        if (result) {
            snprintf(result, (size_t)needed + 1,
                "[\"%s\",\"%s\",\"%s\"]", outcome, end_reason, termination);
        }
        return result;
    }
}

/* PoP: smc_duration_bucket @
 *   hermes_cli/observability/shared_metrics_contract.py:duration_bucket
/* PoP: smc_duration_bucket @ hermes_cli/observability/shared_metrics_contract.py:duration_bucket */
const char *smc_duration_bucket(int duration_ms)
{
    int value = duration_ms > 0 ? duration_ms : 0;
    if (value < 1000)
        return "lt_1s";
    if (value < 5000)
        return "1s_to_5s";
    if (value < 30000)
        return "5s_to_30s";
    if (value < 120000)
        return "30s_to_2m";
    if (value < 600000)
        return "2m_to_10m";
    return "gte_10m";
}

/* PoP: smc_count_bucket @
 *   hermes_cli/observability/shared_metrics_contract.py:count_bucket
/* PoP: smc_count_bucket @ hermes_cli/observability/shared_metrics_contract.py:count_bucket */
const char *smc_count_bucket(int count)
{
    int value = count > 0 ? count : 0;
    if (value <= 2) {
        /* Return "0", "1", or "2" */
        static const char *buckets[] = {"0", "1", "2"};
        return buckets[value > 2 ? 2 : value];
    }
    if (value <= 5)
        return "3_to_5";
    if (value <= 10)
        return "6_to_10";
    return "gte_11";
}

/* PoP: smc_model_family @
 *   hermes_cli/observability/shared_metrics_contract.py:model_family
/* PoP: smc_model_family @ hermes_cli/observability/shared_metrics_contract.py:model_family */
const char *smc_model_family(const char *declared_family,
                              const char *model_name,
                              const char *response_model)
{
    if (declared_family) {
        char *norm = str_lower_strip(declared_family);
        if (norm) {
            /* Check if it's a known family (excluding "unknown") */
            for (size_t i = 0; i < smc_model_families_count; i++) {
                if (strcmp(norm, smc_model_families[i]) == 0
                    && strcmp(norm, "unknown") != 0) {
                    free(norm);
                    return smc_model_families[i];
                }
            }
            free(norm);
        }
    }

    /* Try to extract family from model name using regex-like pattern */
    const char *model_str = NULL;
    if (response_model && *response_model)
        model_str = response_model;
    else if (model_name && *model_name)
        model_str = model_name;
    else
        model_str = "";
    if (!*model_str)
        return "unknown";

    /* Build patterns from known families (sorted longest-first) */
    /* In Python: (?:^|[/_.:-])(family1|family2|...)(?=$|[/_.:-]|\d) */
    /* We'll use a simpler approach: iterate families and check for substring
     * preceded by boundary chars at start or after / _ . : - */

    /* Known families sorted by length descending (longer match first) */
    static const char *families_by_len[] = {
        "deepseek", "minimax", "nemotron", "mistral", "claude",
        "gemini", "gemma", "gpt", "grok", "kimi", "llama",
        "mimo", "nova", "qwen", "step", "glm", "o1", "o3", "o4",
    };

    size_t model_len = strlen(model_str);
    for (size_t f = 0; f < sizeof(families_by_len)/sizeof(families_by_len[0]); f++) {
        const char *family = families_by_len[f];
        size_t family_len = strlen(family);
        if (family_len > model_len) continue;

        for (size_t i = 0; i <= model_len - family_len; i++) {
            if (strncmp(model_str + i, family, family_len) == 0) {
                /* Check boundary before: must be start-of-string or one of [/_.:-] */
                if (i > 0) {
                    char c = model_str[i - 1];
                    if (c != '/' && c != '_' && c != '.' && c != ':' && c != '-')
                        continue;
                }
                /* i == 0 means start-of-string — always matches (like Python ^) */
                /* Check boundary after */
                size_t after = i + family_len;
                if (after < model_len) {
                    char c = model_str[after];
                    if (c != '/' && c != '_' && c != '.' && c != ':' && c != '-'
                        && !(c >= '0' && c <= '9'))
                        continue;
                }
                return family;
            }
        }
    }

    return "unknown";
}

/* PoP: smc_model_call_outcome @
 *   hermes_cli/observability/shared_metrics_contract.py:model_call_outcome
/* PoP: smc_model_call_outcome @ hermes_cli/observability/shared_metrics_contract.py:model_call_outcome */
const char *smc_model_call_outcome(const char *outcome)
{
    if (!outcome) return "failed";
    char *norm = str_lower_strip(outcome);
    if (!norm) return "failed";
    const char *result = str_in_set(norm, smc_model_outcomes, smc_model_outcomes_count)
                             ? norm /* points to our static set */
                             : "failed";
    /* If result is pointing to our heap-allocated norm, we can't return that.
     * Check if the value is in the set and return the canonical string. */
    if (strcmp(result, "failed") == 0) {
        free(norm);
        return "failed";
    }
    for (size_t i = 0; i < smc_model_outcomes_count; i++) {
        if (strcmp(norm, smc_model_outcomes[i]) == 0) {
            free(norm);
            return smc_model_outcomes[i];
        }
    }
    free(norm);
    return "failed";
}

/* PoP: smc_is_aggregator_override @
 *   hermes_cli/observability/shared_metrics_contract.py:_TELEMETRY_AGGREGATOR_OVERRIDES
/* PoP: smc_is_aggregator_override @ hermes_cli/observability/shared_metrics_contract.py:_TELEMETRY_AGGREGATOR_OVERRIDES */
bool smc_is_aggregator_override(const char *provider)
{
    if (!provider) return false;
    char *norm = str_lower_strip(provider);
    if (!norm) return false;
    /* Replace _ with - */
    char *dashed = str_replace_char(norm, '_', '-');
    free(norm);
    if (!dashed) return false;
    bool result = str_in_set(dashed, smc_aggregator_overrides,
                              smc_aggregator_overrides_count);
    free(dashed);
    return result;
}

/* PoP: smc_is_local_custom_alias @
 *   hermes_cli/observability/shared_metrics_contract.py:_LOCAL_CUSTOM_PROVIDER_ALIASES
/* PoP: smc_is_local_custom_alias @ hermes_cli/observability/shared_metrics_contract.py:_LOCAL_CUSTOM_PROVIDER_ALIASES */
bool smc_is_local_custom_alias(const char *provider)
{
    if (!provider) return false;
    return str_in_set(provider, smc_local_custom_aliases,
                      smc_local_custom_aliases_count);
}

/* PoP: smc_provider_family @
 *   hermes_cli/observability/shared_metrics_contract.py:provider_family
/* PoP: smc_provider_family @ hermes_cli/observability/shared_metrics_contract.py:provider_family */
const char *smc_provider_family(const char *provider)
{
    if (!provider || !*provider) return "unknown";

    char *norm = str_lower_strip(provider);
    if (!norm) return "unknown";
    char *dashed = str_replace_char(norm, '_', '-');
    free(norm);
    if (!dashed) return "unknown";

    const char *result = NULL;

    if (!*dashed) {
        free(dashed);
        return "unknown";
    }

    if (str_in_set(dashed, smc_local_custom_aliases,
                   smc_local_custom_aliases_count)) {
        result = "local";
        goto done;
    }

    if (strcmp(dashed, "custom") == 0
        || strncmp(dashed, "custom-", 7) == 0
        || strncmp(dashed, "custom:", 7) == 0) {
        result = "custom";
        goto done;
    }

    /* Check for known local providers */
    if (strcmp(dashed, "lmstudio") == 0 || strcmp(dashed, "local") == 0) {
        result = "local";
        goto done;
    }

    /* Check aggregator overrides */
    if (str_in_set(dashed, smc_aggregator_overrides,
                   smc_aggregator_overrides_count)) {
        result = "aggregator";
        goto done;
    }

    /* Check if it's a direct provider (known via catalog) */
    /* For the C port without provider catalog, we classify direct providers
     * as those that aren't local/custom/aggregator/unknown */
    result = "direct";

done:
    free(dashed);
    return result;
}

/* PoP: smc_model_locality @
 *   hermes_cli/observability/shared_metrics_contract.py:model_locality
/* PoP: smc_model_locality @ hermes_cli/observability/shared_metrics_contract.py:model_locality */
const char *smc_model_locality(const char *base_url,
                                const char *provider_category)
{
    if (base_url && *base_url) {
        /* Check for local endpoints: localhost, 127.0.0.1, 0.0.0.0, ::1 */
        if (strstr(base_url, "localhost") != NULL
            || strstr(base_url, "127.0.0.1") != NULL
            || strstr(base_url, "0.0.0.0") != NULL
            || strstr(base_url, "::1") != NULL)
            return "local";
    }

    if (provider_category) {
        if (strcmp(provider_category, "local") == 0)
            return "local";
        if (strcmp(provider_category, "aggregator") == 0
            || strcmp(provider_category, "direct") == 0)
            return "remote";
    }

    return "unknown";
}

/* PoP: smc_model_call_fields @
 *   hermes_cli/observability/shared_metrics_contract.py:model_call_fields
/* PoP: smc_model_call_fields @ hermes_cli/observability/shared_metrics_contract.py:model_call_fields */
char *smc_model_call_fields(const char *provider,
                             const char *base_url,
                             const char *declared_family,
                             const char *model_name,
                             const char *response_model)
{
    const char *prov_cat = smc_provider_family(provider);
    const char *locality = smc_model_locality(base_url, prov_cat);
    const char *family = smc_model_family(declared_family, model_name, response_model);

    int needed = snprintf(NULL, 0,
        "{"
        "\"call_role\":\"%s\","
        "\"locality\":\"%s\","
        "\"model_family\":\"%s\","
        "\"provider_family\":\"%s\""
        "}",
        SMC_PRIMARY_MODEL_CALL_ROLE, locality, family, prov_cat);

    char *result = malloc((size_t)needed + 1);
    if (result) {
        snprintf(result, (size_t)needed + 1,
            "{"
            "\"call_role\":\"%s\","
            "\"locality\":\"%s\","
            "\"model_family\":\"%s\","
            "\"provider_family\":\"%s\""
            "}",
            SMC_PRIMARY_MODEL_CALL_ROLE, locality, family, prov_cat);
    }
    return result;
}

