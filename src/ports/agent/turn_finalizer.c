/*
 * turn_finalizer.c — Port of Python agent/turn_finalizer.py
 *
 * Contains finalize_turn() — post-loop turn finalization extracted from
 * run_conversation() in conversation_loop.c.
 *
 * Handles: budget-exhaustion summary, trajectory save, turn-exit
 * diagnostics, interrupt/budget explainer messages, and result return.
 *
 * Split from agent_loop.c: post-loop finalization (original lines 2263-2399).
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_logger.h"
#include "hermes_trajectory.h"
#include "budget_tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * extern: serialize_messages is defined in agent_loop.c (session mgmt module).
 */
extern char *serialize_messages(const agent_state_t *state);

/* Port of Python agent/turn_finalizer.py:finalize_turn(). */
/**
 * finalize_turn — Post-loop turn finalization.
 *
 * Port of Python turn_finalizer.py:finalize_turn.
 * Called at the end of run_conversation() after the main tool-calling loop.
 *
 * Determines the exit reason, logs turn diagnostics, and returns
 * a user-facing explainer string based on the exit condition:
 *   - Interrupt (force/graceful) → pause explainer
 *   - Hard budget exceeded → stop explainer
 *   - Soft budget exceeded → budget explainer
 *   - Max iterations → iterations explainer + failed trajectory save
 *
 * Returns malloc'd string (caller must free), or NULL on OOM.
 */
char *finalize_turn(agent_state_t *state) {
    /* ── Determine exit reason ──────────────────────────────────────── */
    const char *_exit_reason_str = NULL;
    char _reason_buf[128];
    if (state->interrupt_type != INTERRUPT_NONE) {
        _exit_reason_str = state->interrupt_type == INTERRUPT_FORCE
            ? "interrupt_force" : "interrupt_graceful";
    } else if (state->budget && budget_tracker_is_hard_exceeded(state->budget)) {
        _exit_reason_str = "hard_budget_exceeded";
    } else if (state->budget && budget_tracker_is_exceeded(state->budget)) {
        _exit_reason_str = "soft_budget_exceeded";
    } else {
        snprintf(_reason_buf, sizeof(_reason_buf),
                 "max_iterations(%d/%d)", state->iteration_count, state->max_iterations);
        _exit_reason_str = _reason_buf;
    }

    /* ── Turn-exit diagnostic log ────────────────────────────────────── */
    {
        const char *exit_reason = NULL;
        char reason_buf[128];
        if (state->interrupt_type != INTERRUPT_NONE) {
            exit_reason = state->interrupt_type == INTERRUPT_FORCE
                ? "interrupt_force" : "interrupt_graceful";
        } else if (state->budget && budget_tracker_is_hard_exceeded(state->budget)) {
            exit_reason = "hard_budget_exceeded";
        } else if (state->budget && budget_tracker_is_exceeded(state->budget)) {
            exit_reason = "soft_budget_exceeded";
        } else {
            snprintf(reason_buf, sizeof(reason_buf),
                     "max_iterations(%d/%d)", state->iteration_count, state->max_iterations);
            exit_reason = reason_buf;
        }

        /* Count tool turns in current conversation */
        int tool_turns = 0;
        for (size_t _ti = 0; _ti < state->message_count; _ti++) {
            if (state->messages[_ti]->tool_calls_count > 0)
                tool_turns++;
        }

        /* Get last message role */
        const char *last_role = "none";
        if (state->message_count > 0) {
            switch (state->messages[state->message_count - 1]->role) {
                case MSG_USER:      last_role = "user"; break;
                case MSG_ASSISTANT: last_role = "assistant"; break;
                case MSG_TOOL:      last_role = "tool"; break;
                case MSG_SYSTEM:    last_role = "system"; break;
                default:            last_role = "unknown"; break;
            }
        }

        int budget_used = 0, budget_max = 0;
        if (state->budget) {
            budget_used = state->budget->iterations_used;
            budget_max  = state->budget->max_iterations;
        }

        hermes_log(LOG_INFO, "turn_finalizer",
            "Turn ended: reason=%s model=%s api_calls=%d/%d budget=%d/%d "
            "tool_turns=%d last_msg_role=%s response_len=%d session=%s",
            exit_reason,
            state->llm.model,
            state->iteration_count, state->max_iterations,
            budget_used, budget_max,
            tool_turns, last_role,
            state->message_count > 0 && state->messages[state->message_count - 1]->content
                ? (int)strlen(state->messages[state->message_count - 1]->content) : 0,
            state->session_id[0] ? state->session_id : "none");
    }

    /* ── Interrupt handling ──────────────────────────────────────────── */
    if (state->interrupt_type != INTERRUPT_NONE) {
        state->interrupted = true;
        if (state->interrupt_type == INTERRUPT_GRACEFUL && state->message_count > 0) {
            state->partial_results_saved = true;
            if (state->message_count > 0 &&
                state->messages[state->message_count - 1]->role == MSG_TOOL) {
                hermes_log(LOG_WARNING, "turn_finalizer",
                    "Turn ended with pending tool result (agent may appear stuck)");
            }
        }
        char explainer[512];
        snprintf(explainer, sizeof(explainer),
            "\xF0\x9F\x8F\xB8\xEF\xB8\x8F The conversation was interrupted (reason: %s). "
            "Use /resume to continue or /new to start fresh.",
            _exit_reason_str ? _exit_reason_str : "unknown");
        return strdup(explainer);
    }

    /* ── Hard budget limit ───────────────────────────────────────────── */
    if (state->budget && budget_tracker_is_hard_exceeded(state->budget)) {
        hermes_log(LOG_WARNING, "turn_finalizer", "Hard budget limit exceeded");
        char explainer[512];
        snprintf(explainer, sizeof(explainer),
            "\xE2\x9B\x94 Turn ended: hard budget limit exceeded (reason: %s). "
            "The session has consumed its allocated token/cost budget.",
            _exit_reason_str ? _exit_reason_str : "hard_budget_exceeded");
        return strdup(explainer);
    }

    /* ── Soft budget exceeded ────────────────────────────────────────── */
    if (state->budget && budget_tracker_is_exceeded(state->budget)) {
        hermes_log(LOG_WARNING, "turn_finalizer", "Budget exceeded (token/cost/turn limit)");
        char explainer[512];
        snprintf(explainer, sizeof(explainer),
            "\xE2\x9B\x94 Turn ended: budget exceeded (reason: %s). "
            "The session reached its token/cost/turn limit.",
            _exit_reason_str ? _exit_reason_str : "budget_exceeded");
        return strdup(explainer);
    }

    /* ── Max iterations ──────────────────────────────────────────────── */
    hermes_log(LOG_INFO, "turn_finalizer", "Max iterations reached (%d)", state->max_iterations);

    /* Save failed trajectory */
    {
        char *traj = serialize_messages(state);
        if (traj) {
            save_trajectory(traj, state->llm.model, false, NULL);
            free(traj);
        }
    }

    /* Return user-friendly explanation */
    {
        char explainer[512];
        snprintf(explainer, sizeof(explainer),
            "\xE2\x8F\xB9 Turn ended: the agent reached the maximum number of iterations (%d/%d) "
            "(reason: %s). Try simplifying your request or using /continue.",
            state->iteration_count, state->max_iterations,
            _exit_reason_str ? _exit_reason_str : "max_iterations");
        return strdup(explainer);
    }
}
