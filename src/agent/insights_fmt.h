/*
 * insights_fmt.h - insights report formatting helpers.
 *
 * Module-internal header (insights.c <-> insights_fmt.c).
 */
#ifndef INSIGHTS_FMT_H
#define INSIGHTS_FMT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_insights.h"   /* insights_report_t */

void append_fmt(char **buf, size_t *cap, size_t *len, const char *fmt, ...);
void append_line(char **buf, size_t *cap, size_t *len, const char *line);
void bar_string(char *out, size_t out_size, int value, int max_value, int max_width);
int cmp_models_desc(const void *a, const void *b);
int cmp_platforms_desc(const void *a, const void *b);
int cmp_skills_desc(const void *a, const void *b);
int cmp_tools_desc(const void *a, const void *b);
void insights_report_free(insights_report_t *r);

#endif /* INSIGHTS_FMT_H */
