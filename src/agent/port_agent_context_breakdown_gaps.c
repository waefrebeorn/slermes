/*
 * port_agent_context_breakdown_gaps.c — C11 port of agent/context_breakdown.py gaps.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cb_render_context_grid @ agent/context_breakdown.py:render_context_grid */
void cb_render_context_grid(int total, int used, int available, char *out, size_t out_sz) {
    int bar_width = 40;
    int used_chars = (used > 0 && total > 0) ? (used * bar_width / total) : 0;
    if (used_chars > bar_width) used_chars = bar_width;
    int pos = 0;
    pos += snprintf(out + pos, out_sz - pos, "[");
    for (int i = 0; i < bar_width; i++) {
        if (i < used_chars)
            pos += snprintf(out + pos, out_sz - pos, "█");
        else
            pos += snprintf(out + pos, out_sz - pos, "░");
    }
    pos += snprintf(out + pos, out_sz - pos, "] %d/%d tokens", used, total);
}

/* PoP: cb_render_context_category_lines @ agent/context_breakdown.py:render_context_category_lines */
void cb_render_context_category_lines(const char *category, int tokens, int total,
                                        char *out, size_t out_sz) {
    int pct = (tokens > 0 && total > 0) ? (tokens * 100 / total) : 0;
    snprintf(out, out_sz, "  %-20s %6d tokens (%d%%)", category, tokens, pct);
}

/* PoP: cb_render_context_details_lines @ agent/context_breakdown.py:render_context_details_lines */
void cb_render_context_details_lines(const char *label, const char *value,
                                       char *out, size_t out_sz) {
    snprintf(out, out_sz, "  %-20s %s", label, value ? value : "");
}

/* PoP: cb_render_context_breakdown_lines @ agent/context_breakdown.py:render_context_breakdown_lines */
void cb_render_context_breakdown_lines(int total, int system, int history, int tools,
                                        char *out, size_t out_sz) {
    int pos = 0;
    pos += snprintf(out + pos, out_sz - pos, "Context Breakdown:\n");
    pos += snprintf(out + pos, out_sz - pos, "  Total:     %d tokens\n", total);
    pos += snprintf(out + pos, out_sz - pos, "  System:    %d tokens\n", system);
    pos += snprintf(out + pos, out_sz - pos, "  History:   %d tokens\n", history);
    pos += snprintf(out + pos, out_sz - pos, "  Tools:     %d tokens\n", tools);
}
