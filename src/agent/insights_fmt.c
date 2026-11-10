/*
 * insights_fmt.c - insights report formatting helpers.
 *
 * Split from insights.c (soul.md: self-contained modules). Descending
 * comparators, line appenders and the bar renderer. No file-scope state.
 */
#include "insights_fmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <stdint.h>
#include "usage_pricing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



/* qsort comparators */ int cmp_tools_desc(const void *a, const void *b) {
    const insights_tool_entry_t *ta = (const insights_tool_entry_t *)a;
    const insights_tool_entry_t *tb = (const insights_tool_entry_t *)b;
    if (tb->count != ta->count) return tb->count - ta->count;
    return strcmp(ta->tool, tb->tool);
}
 int cmp_models_desc(const void *a, const void *b) {
    const insights_model_entry_t *ma = (const insights_model_entry_t *)a;
    const insights_model_entry_t *mb = (const insights_model_entry_t *)b;
    if (mb->total_tokens != ma->total_tokens)
        return (mb->total_tokens > ma->total_tokens) ? 1 : -1;
    return mb->sessions - ma->sessions;
}
 int cmp_platforms_desc(const void *a, const void *b) {
    const insights_platform_entry_t *pa = (const insights_platform_entry_t *)a;
    const insights_platform_entry_t *pb = (const insights_platform_entry_t *)b;
    return pb->sessions - pa->sessions;
}
 int cmp_skills_desc(const void *a, const void *b) {
    const insights_skill_entry_t *sa = (const insights_skill_entry_t *)a;
    const insights_skill_entry_t *sb = (const insights_skill_entry_t *)b;
    int ta = sa->view_count + sa->manage_count;
    int tb = sb->view_count + sb->manage_count;
    if (tb != ta) return tb - ta;
    if (sb->view_count != sa->view_count) return sb->view_count - sa->view_count;
    if (sb->manage_count != sa->manage_count) return sb->manage_count - sa->manage_count;
    if (sb->last_used_at > sa->last_used_at) return 1;
    if (sb->last_used_at < sa->last_used_at) return -1;
    return strcmp(sa->skill, sb->skill);
}


void insights_report_free(insights_report_t *r) {
    free(r);
}


/* ══════════════════════════════════════════════════════════════
 *  Formatting — Terminal (CLI)
 * ══════════════════════════════════════════════════════════════ */ void append_line(char **buf, size_t *cap, size_t *len, const char *line) {
    size_t line_len = strlen(line);
    size_t needed = *len + line_len + 2;  /* + \n + \0 */
    if (needed > *cap) {
        *cap = needed + 2048;
        *buf = (char *)realloc(*buf, *cap);
        if (!*buf) return;
    }
    memcpy(*buf + *len, line, line_len);
    *len += line_len;
    (*buf)[(*len)++] = '\n';
}
 void append_fmt(char **buf, size_t *cap, size_t *len, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char tmp[4096];
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    append_line(buf, cap, len, tmp);
}


/* Generate a bar string (simple horizontal bar) */ void bar_string(char *out, size_t out_size, int value, int max_value, int max_width) {
    int bar_len = (max_value > 0) ? (int)((double)value / max_value * max_width) : 0;
    if (bar_len < 1 && value > 0) bar_len = 1;
    int pos = 0;
    /* Use block characters for visual bar */
    for (int i = 0; i < bar_len && pos < (int)out_size - 5; i++) {
        /* UTF-8 full block: U+2588 = 3 bytes */
        if (pos + 3 < (int)out_size) {
            out[pos++] = (char)0xE2;
            out[pos++] = (char)0x96;
            out[pos++] = (char)0x88;
        }
    }
    out[pos] = '\0';
}
