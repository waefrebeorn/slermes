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
