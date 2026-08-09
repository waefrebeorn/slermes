/*
 * port_turn_summary_cli.c — C port of cli.py's turn_summary methods:
 *   _spinner_token_flow, _turn_summary_is_active, _turn_summary_begin,
 *   _turn_summary_record, _turn_summary_emit
 *
 * These wrap the agent/turn_summary.py library (port_turn_summary.h/c).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "libjson/json.h"
#include "port_turn_summary.h"
#include "port_turn_summary_cli.h"
#include "hermes_logger.h"

/* monotonic clock in seconds (mirrors Python time.monotonic()) */
static double cli_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Helper: display width for terminal rendering (simplified — no CJK/wide
 * char handling here; the TUI layer in the Python CLI uses prompt_toolkit
 * get_cwidth which we don't replicate). For the CLI port, fall back to
 * byte/str len. */
static int cli_display_width(const char *s) {
    return s ? (int)strlen(s) : 0;
}

/* ── TurnSummaryManager ────────────────────────────────────────────────
 * Holds the per-CLI-instance state that Python attaches as attributes
 * (_turn_summary_collector, _turn_summary_start, _turn_token_baseline,
 * _turn_summary_enabled, _interactive_turn, tool_progress_mode,
 * _spinner_token_flow_enabled, _agent_running).
 */
struct cli_turn_summary_mgr {
    turn_summary_collector_t *collector;
    double start_time;           /* _turn_summary_start */
    long token_baseline;         /* _turn_token_baseline */
    bool enabled;                /* _turn_summary_enabled */
    bool interactive;            /* _interactive_turn */
    bool spinner_enabled;        /* _spinner_token_flow_enabled */
    bool agent_running;          /* _agent_running */
    bool quiet_mode;             /* agent.quiet_mode */
    const char *tool_progress_mode; /* "all" | "off" | ... */
    long agent_output_tokens;    /* set by CLI loop via cli_turn_set_agent_output_tokens */
    turn_summary_collector_t *(*agent_get_session_output_tokens)(void *agent);
};

/* ── Properties ───────────────────────────────────────────────────────── */

/* PoP: _spinner_token_flow @ cli.py:_spinner_token_flow */
char *cli_turn_spinner_token_flow(cli_turn_summary_mgr_t *mgr,
                                   void *agent) {
    (void)agent;
    if (!mgr) return strdup("");
    if (!mgr->spinner_enabled) return strdup("");
    if (!mgr->agent_running) return strdup("");
    /* produced = agent.session_output_tokens - self._turn_token_baseline */
    long produced = mgr->agent_output_tokens - mgr->token_baseline;
    /* Build a JSON number node to pass to turn_summary_format_token_flow. */
    json_t *tok = json_number((double)produced);
    char *result = turn_summary_format_token_flow(tok, "\xe2\x86\x93");
    json_free(tok);
    return result;
}

/* PoP: _turn_summary_is_active @ cli.py:_turn_summary_is_active */
bool cli_turn_summary_is_active(cli_turn_summary_mgr_t *mgr) {
    if (!mgr) return false;
    if (!mgr->enabled) return false;
    if (mgr->tool_progress_mode && strcmp(mgr->tool_progress_mode, "off") == 0)
        return false;
    if (mgr->quiet_mode) return false;
    if (!mgr->interactive) return false;
    return true;
}

/* ── Core methods ─────────────────────────────────────────────────────── */

/* PoP: _turn_summary_begin @ cli.py:_turn_summary_begin */
void cli_turn_summary_begin(cli_turn_summary_mgr_t *mgr, void *agent) {
    if (!mgr) return;
    if (!mgr->collector) {
        mgr->collector = turn_summary_collector_new();
    }
    turn_summary_collector_begin(mgr->collector);
    mgr->start_time = cli_monotonic();
    /* baseline from agent.session_output_tokens */
    if (agent && mgr->agent_get_session_output_tokens) {
        /* Store the baseline; the actual token count is fetched via
         * cli_turn_set_agent_output_tokens. */
    }
    /* For the port, we use manager->agent_output_tokens as it was at begin(). */
    mgr->token_baseline = mgr->agent_output_tokens;
}

/* PoP: _turn_summary_record @ cli.py:_turn_summary_record */
void cli_turn_summary_record(cli_turn_summary_mgr_t *mgr,
                              const char *function_name,
                              const json_t *result, bool is_error) {
    if (!mgr || !mgr->collector) return;
    turn_summary_collector_record(mgr->collector, function_name, result, is_error);
}

/* PoP: _turn_summary_emit @ cli.py:_turn_summary_emit */
char *cli_turn_summary_emit(cli_turn_summary_mgr_t *mgr) {
    if (!mgr || !mgr->collector) return NULL;
    if (!cli_turn_summary_is_active(mgr)) return NULL;
    double started = mgr->start_time;
    double elapsed = started > 0 ? (cli_monotonic() - started) : 0.0;
    if (elapsed < 0) elapsed = 0.0;
    char *line = turn_summary_collector_render(mgr->collector, elapsed);
    /* In the CLI, _cprint wraps with dim styling. Here we just return the line;
     * the caller decides how to print it. */
    return line;
}

/* ── Lifecycle ────────────────────────────────────────────────────────── */

/* PoP: cli_turn_summary_mgr_new @ cli.py:__init__ (default values) */
cli_turn_summary_mgr_t *cli_turn_summary_mgr_new(void) {
    cli_turn_summary_mgr_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->collector = NULL;
    m->start_time = 0.0;
    m->token_baseline = 0;
    m->enabled = false;
    m->interactive = false;
    m->spinner_enabled = false;
    m->agent_running = false;
    m->quiet_mode = false;
    m->tool_progress_mode = "all";
    m->agent_output_tokens = 0;
    m->agent_get_session_output_tokens = NULL;
    return m;
}

void cli_turn_summary_mgr_free(cli_turn_summary_mgr_t *m) {
    if (!m) return;
    if (m->collector) turn_summary_collector_free(m->collector);
    free(m);
}

/* Setters for state that the CLI agent loop drives. */
void cli_turn_set_enabled(cli_turn_summary_mgr_t *m, bool v) { if (m) m->enabled = v; }
void cli_turn_set_interactive(cli_turn_summary_mgr_t *m, bool v) { if (m) m->interactive = v; }
void cli_turn_set_spinner_enabled(cli_turn_summary_mgr_t *m, bool v) { if (m) m->spinner_enabled = v; }
void cli_turn_set_agent_running(cli_turn_summary_mgr_t *m, bool v) { if (m) m->agent_running = v; }
void cli_turn_set_quiet_mode(cli_turn_summary_mgr_t *m, bool v) { if (m) m->quiet_mode = v; }
void cli_turn_set_tool_progress_mode(cli_turn_summary_mgr_t *m, const char *mode) { if (m) m->tool_progress_mode = mode; }
void cli_turn_set_agent_output_tokens(cli_turn_summary_mgr_t *m, long tokens) { if (m) m->agent_output_tokens = tokens; }
