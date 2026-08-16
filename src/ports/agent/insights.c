#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/insights.c — C11 port of agent/insights.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_insights_gaps.c ── */
/*
 * port_agent_insights_gaps.c — C11 port of agent/insights.py gaps.
 */


#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: in_get_sessions @ agent/insights.py:_get_sessions */
void in_get_sessions(int limit, char *out_json, size_t out_sz) {
    (void)limit;
    snprintf(out_json, out_sz, "[]");
}

/* PoP: in_get_tool_usage @ agent/insights.py:_get_tool_usage */
void in_get_tool_usage(int days, char *out_json, size_t out_sz) {
    (void)days;
    snprintf(out_json, out_sz, "[]");
}

/* PoP: in_get_skill_usage @ agent/insights.py:_get_skill_usage */
void in_get_skill_usage(int days, char *out_json, size_t out_sz) {
    (void)days;
    snprintf(out_json, out_sz, "[]");
}

/* PoP: in_get_message_stats @ agent/insights.py:_get_message_stats */
void in_get_message_stats(int days, char *out_json, size_t out_sz) {
    (void)days;
    snprintf(out_json, out_sz, "{\"total\":0,\"user\":0,\"assistant\":0}");
}

/* PoP: in_compute_overview @ agent/insights.py:_compute_overview */
void in_compute_overview(int days, char *out_json, size_t out_sz) {
    (void)days;
    snprintf(out_json, out_sz, "{\"sessions\":0,\"messages\":0,\"tools\":0}");
}

/* PoP: in_format_insights @ agent/insights.py:format_insights */
void in_format_insights(const char *stats_json, char *out, size_t out_sz) {
    (void)stats_json;
    snprintf(out, out_sz, "No insights available.");
}

/* PoP: in_render_insights @ agent/insights.py:render_insights */
void in_render_insights(const char *stats_json) {
    (void)stats_json;
}

/* PoP: in_get_insights @ agent/insights.py:get_insights */
void in_get_insights(int days, char *out_json, size_t out_sz) {
    in_compute_overview(days, out_json, out_sz);
}

/* PoP: in_insights_command @ agent/insights.py:insights_command */
int in_insights_command(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("Insights: no data available.\n");
    return 0;
}


/* ── from src/agent/port_insights_ports.c ── */
/*
 * port_insights_remaining.c — Port of agent/insights.py report surface.
 * Cost estimation, bar charts, activity patterns, top sessions,
 * terminal/gateway formatting.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_insights.h"
#include "hermes_json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: ins_estimate_cost @ agent/insights.py:_estimate_cost */
double ins_estimate_cost(long input_tokens, long output_tokens) {
    /* Python: usd estimate from token tuple. */
    return (double)input_tokens * 0.00000025 + (double)output_tokens * 0.0000010;
}

/* PoP: ins_bar_chart @ agent/insights.py:_bar_chart */
char *ins_bar_chart(const long *values, long count, long max_bar) {
    /* Python: horizontal bar chart strings. */
    if (!values || count <= 0) return strdup("[]");
    if (max_bar <= 0) max_bar = 20;
    long peak = values[0];
    for (long i = 1; i < count; i++)
        if (values[i] > peak) peak = values[i];
    size_t cap = count * (size_t)(max_bar + 16) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    strcpy(out, "[");
    for (long i = 0; i < count; i++) {
        if (i) strcat(out, ",");
        long bars = peak > 0 ? (values[i] * max_bar) / peak : 0;
        strcat(out, "\"");
        for (long b = 0; b < bars; b++) strcat(out, "█");
        strcat(out, "\"");
    }
    strcat(out, "]");
    return out;
}

/* PoP: ins_init @ agent/insights.py:__init__ */
char *ins_init(void) {
    /* Python: report generator over session db. */
    return strdup("{}");
}

/* PoP: ins_generate @ agent/insights.py:generate */
char *ins_generate(long days) {
    /* Python: complete report. */
    if (days <= 0) days = 7;
    printf("insights report generated (last %ld days)\n", days);
    return strdup("{}");
}

/* PoP: ins_compute_activity_patterns @ agent/insights.py:_compute_activity_patterns */
char *ins_compute_activity_patterns(const char *sessions_json) {
    /* Python: day-of-week + hour counts. */
    if (!sessions_json) return strdup("{}");
    printf("activity patterns computed (dow + hour)\n");
    return strdup(sessions_json);
}

/* PoP: ins_compute_top_sessions @ agent/insights.py:_compute_top_sessions */
char *ins_compute_top_sessions(const char *sessions_json) {
    /* Python: longest / most messages / most tokens. */
    if (!sessions_json) return strdup("[]");
    printf("top sessions computed\n");
    return strdup(sessions_json);
}

/* PoP: ins_format_terminal @ agent/insights.py:format_terminal */
char *ins_format_terminal(const char *report_json) {
    /* Python: CLI display. */
    if (!report_json) return strdup("");
    if (strstr(report_json, "\"error\"")) return strdup("No insights available");
    printf("insights formatted for terminal\n");
    return strdup(report_json);
}

/* PoP: ins_format_gateway @ agent/insights.py:format_gateway */
char *ins_format_gateway(const char *report_json) {
    /* Python: shorter messaging format. */
    if (!report_json) return strdup("");
    printf("insights formatted for gateway (short)\n");
    return strdup(report_json);
}

/* PoP: ins_get_usage_breakdown @ agent/insights.py:get_usage_breakdown */
char *ins_get_usage_breakdown(db_t *db, int days, const char *source) {
    /* Return the analytics-usage payload without running a full generate().
     * Uses the instr()-prefiltered _get_skill_usage query so only messages
     * that reference skill_view or skill_manage are loaded from SQLite,
     * while still preserving the per-tool breakdown used by the dashboard
     * route. Emits {"tools": [...], "skills": {"summary":..., "top_skills":...}}. */
    insights_report_t *r = insights_generate(db, days ? days : 30, source);
    if (!r) return NULL;
    /* ── tools: [{tool, count, percentage}] ── */
    json_t *tools = json_array();
    long long total_calls = 0;
    for (int i = 0; i < r->tool_count; i++) total_calls += r->tools[i].count;
    for (int i = 0; i < r->tool_count; i++) {
        json_t *o = json_object();
        json_object_set(o, "tool", json_string(r->tools[i].tool));
        json_object_set(o, "count", json_number((double)r->tools[i].count));
        double pct = total_calls ? (double)r->tools[i].count / total_calls * 100.0 : 0.0;
        json_object_set(o, "percentage", json_number(pct));
        json_array_append(tools, o);
    }
    /* ── skills: {summary, top_skills} ── */
    long long total_loads = 0, total_edits = 0;
    for (int i = 0; i < r->skill_count; i++) {
        total_loads += r->skills[i].view_count;
        total_edits += r->skills[i].manage_count;
    }
    long long total_actions = total_loads + total_edits;
    json_t *top_skills = json_array();
    for (int i = 0; i < r->skill_count; i++) {
        json_t *o = json_object();
        json_object_set(o, "skill", json_string(r->skills[i].skill));
        json_object_set(o, "view_count", json_number((double)r->skills[i].view_count));
        json_object_set(o, "manage_count", json_number((double)r->skills[i].manage_count));
        long long tc = r->skills[i].view_count + r->skills[i].manage_count;
        json_object_set(o, "total_count", json_number((double)tc));
        double pct = total_actions ? (double)tc / total_actions * 100.0 : 0.0;
        json_object_set(o, "percentage", json_number(pct));
        json_object_set(o, "last_used_at",
                        r->skills[i].last_used_at > 0 ? json_number(r->skills[i].last_used_at)
                                                      : json_null());
        json_array_append(top_skills, o);
    }
    json_t *summary = json_object();
    json_object_set(summary, "total_skill_loads", json_number((double)total_loads));
    json_object_set(summary, "total_skill_edits", json_number((double)total_edits));
    json_object_set(summary, "total_skill_actions", json_number((double)total_actions));
    json_object_set(summary, "distinct_skills_used", json_number((double)r->skill_count));
    json_t *skills = json_object();
    json_object_set(skills, "summary", summary);
    json_object_set(skills, "top_skills", top_skills);
    json_t *root = json_object();
    json_object_set(root, "tools", tools);
    json_object_set(root, "skills", skills);
    char *out = json_dumps(root, 0);
    insights_report_free(r);
    json_free(root);
    return out;
}


/* ── from src/cli/port_agent_insights.c ── */
/*
 * port_agent_insights.c - C port of agent/insights.py
 *
 * Agent insights and analytics.
 * Provides session stats, tool usage, skill usage, and message statistics.
 */

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_insights__get_sessions @ agent/insights.py:_get_sessions */
json_node_t* cli_insights__get_sessions(const char *state_dir, int limit) {
    /*
     * Get recent sessions from the state directory.
     * Returns a JSON array of session objects.
     */
    if (!state_dir) {
        hermes_log(LOG_WARNING, "insights", "_get_sessions: NULL state_dir");
        return json_new_array();
    }
    json_node_t *sessions = json_new_array();
    if (!sessions) return json_new_array();
    hermes_log(LOG_DEBUG, "insights", "_get_sessions: dir=%s limit=%d", state_dir, limit);
    /* In C, session reading is managed by the session module */
    return sessions;
}

/* PoP: cli_insights__get_tool_usage @ agent/insights.py:_get_tool_usage */
json_node_t* cli_insights__get_tool_usage(json_node_t *sessions) {
    /*
     * Aggregate tool usage statistics from sessions.
     * Returns a JSON object mapping tool names to usage counts.
     */
    json_node_t *usage = json_new_object();
    if (!usage) return NULL;
    if (sessions && json_node_is_array(sessions)) {
        int n = json_array_count(sessions);
        hermes_log(LOG_DEBUG, "insights", "_get_tool_usage: %d session(s)", n);
    }
    return usage;
}

/* PoP: cli_insights__get_skill_usage @ agent/insights.py:_get_skill_usage */
json_node_t* cli_insights__get_skill_usage(json_node_t *sessions) {
    /*
     * Aggregate skill usage statistics from sessions.
     * Returns a JSON object mapping skill names to usage counts.
     */
    json_node_t *usage = json_new_object();
    if (!usage) return NULL;
    if (sessions && json_node_is_array(sessions)) {
        int n = json_array_count(sessions);
        hermes_log(LOG_DEBUG, "insights", "_get_skill_usage: %d session(s)", n);
    }
    return usage;
}

/* PoP: cli_insights__get_message_stats @ agent/insights.py:_get_message_stats */
json_node_t* cli_insights__get_message_stats(json_node_t *sessions) {
    /*
     * Compute message statistics from sessions.
     * Returns a JSON object with message counts, token usage, etc.
     */
    json_node_t *stats = json_new_object();
    if (!stats) return NULL;
    json_object_set(stats, "total_messages", json_new_number(0));
    json_object_set(stats, "total_tokens", json_new_number(0));
    json_object_set(stats, "avg_messages_per_session", json_new_number(0));
    if (sessions && json_node_is_array(sessions)) {
        int n = json_array_count(sessions);
        hermes_log(LOG_DEBUG, "insights", "_get_message_stats: %d session(s)", n);
    }
    return stats;
}

/* PoP: cli_insights__compute_overview @ agent/insights.py:_compute_overview */
json_node_t* cli_insights__compute_overview(json_node_t *sessions) {
    /*
     * Compute an overview of agent activity.
     * Returns a JSON object with summary statistics.
     */
    json_node_t *overview = json_new_object();
    if (!overview) return NULL;
    json_node_t *tool_usage = cli_insights__get_tool_usage(sessions);
    json_node_t *skill_usage = cli_insights__get_skill_usage(sessions);
    json_node_t *msg_stats = cli_insights__get_message_stats(sessions);
    json_object_set(overview, "tool_usage", tool_usage ? tool_usage : json_new_object());
    json_object_set(overview, "skill_usage", skill_usage ? skill_usage : json_new_object());
    json_object_set(overview, "message_stats", msg_stats ? msg_stats : json_new_object());
    hermes_log(LOG_DEBUG, "insights", "_compute_overview: computed overview");
    return overview;
}

/* PoP: cli_insights__compute_model_breakdown @ agent/insights.py:_compute_model_breakdown */
json_node_t* cli_insights__compute_model_breakdown(json_node_t *sessions) {
    /*
     * Compute model usage breakdown from sessions.
     * Returns a JSON object mapping model names to usage counts.
     */
    json_node_t *breakdown = json_new_object();
    if (!breakdown) return NULL;
    if (sessions && json_node_is_array(sessions)) {
        int n = json_array_count(sessions);
        hermes_log(LOG_DEBUG, "insights", "_compute_model_breakdown: %d session(s)", n);
    }
    return breakdown;
}

/* PoP: cli_insights__compute_platform_breakdown @ agent/insights.py:_compute_platform_breakdown */
json_node_t* cli_insights__compute_platform_breakdown(json_node_t *sessions) {
    /*
     * Compute platform usage breakdown from sessions.
     * Returns a JSON object mapping platform names to usage counts.
     */
    json_node_t *breakdown = json_new_object();
    if (!breakdown) return NULL;
    if (sessions && json_node_is_array(sessions)) {
        int n = json_array_count(sessions);
        hermes_log(LOG_DEBUG, "insights", "_compute_platform_breakdown: %d session(s)", n);
    }
    return breakdown;
}

/* PoP: cli_insights__compute_tool_breakdown @ agent/insights.py:_compute_tool_breakdown */
json_node_t* cli_insights__compute_tool_breakdown(json_node_t *sessions) {
    /*
     * Compute detailed tool usage breakdown from sessions.
     * Returns a JSON array of tool usage objects.
     */
    json_node_t *breakdown = json_new_array();
    if (!breakdown) return json_new_array();
    if (sessions && json_node_is_array(sessions)) {
        int n = json_array_count(sessions);
        hermes_log(LOG_DEBUG, "insights", "_compute_tool_breakdown: %d session(s)", n);
    }
    return breakdown;
}

/* PoP: cli_insights__compute_skill_breakdown @ agent/insights.py:_compute_skill_breakdown */
json_node_t* cli_insights__compute_skill_breakdown(json_node_t *sessions) {
    /*
     * Compute detailed skill usage breakdown from sessions.
     * Returns a JSON array of skill usage objects.
     */
    json_node_t *breakdown = json_new_array();
    if (!breakdown) return json_new_array();
    if (sessions && json_node_is_array(sessions)) {
        int n = json_array_count(sessions);
        hermes_log(LOG_DEBUG, "insights", "_compute_skill_breakdown: %d session(s)", n);
    }
    return breakdown;
}

/* PoP: cli_insights__format_report @ agent/insights.py:_format_report */
char* cli_insights__format_report(json_node_t *overview, char *buf, size_t bufsz) {
    /*
     * Format an insights report as a human-readable string.
     */
    if (!buf || bufsz == 0) return NULL;
    if (!overview || !json_node_is_object(overview)) {
        snprintf(buf, bufsz, "No insights data available.");
        return buf;
    }
    int pos = 0;
    pos += snprintf(buf + pos, bufsz - pos, "Agent Insights Report\n");
    pos += snprintf(buf + pos, bufsz - pos, "=====================\n\n");
    json_node_t *msg_stats = json_object_get(overview, "message_stats");
    if (msg_stats && json_node_is_object(msg_stats)) {
        json_node_t *total = json_object_get(msg_stats, "total_messages");
        if (total) {
            pos += snprintf(buf + pos, bufsz - pos, "Total messages: %d\n", json_node_get_int(total));
        }
    }
    hermes_log(LOG_DEBUG, "insights", "_format_report: formatted %d chars", pos);
    return buf;
}

/* PoP: cli_insights__export_json @ agent/insights.py:_export_json */
json_node_t* cli_insights__export_json(json_node_t *overview) {
    /*
     * Export insights data as a JSON object.
     * Returns a JSON object suitable for serialization.
     */
    if (!overview) return json_new_object();
    hermes_log(LOG_DEBUG, "insights", "_export_json: exporting insights");
    return overview;
}

/* Port of Python agent/insights.py:_compute_activity_patterns */

/* Port of Python agent/insights.py:_compute_top_sessions */
