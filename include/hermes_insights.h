#ifndef HERMES_INSIGHTS_H
#define HERMES_INSIGHTS_H

/**
 * hermes_insights.h — Full Insights Engine for Hermes Agent C.
 *
 * Port of Python agent/insights.py::InsightsEngine
 * Pure C implementation — no pandas dependency.
 *
 * Generates session usage insights by loading session metadata
 * from the file-based JSON session store (lib/libdb/).
 *
 * MIT License — WuBu Slermes Project
 */

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/* Forward declarations — db.h included by the .c file */
struct db_t;
typedef struct db_t db_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ─────────────────────────────────────────────── */
#define INSIGHTS_MAX_MODELS     128
#define INSIGHTS_MAX_PLATFORMS   32
#define INSIGHTS_MAX_TOOLS      256
#define INSIGHTS_MAX_SKILLS      64
#define INSIGHTS_MAX_TOP_SESSIONS 16
#define INSIGHTS_SOURCE_FILTER_MAX 64

/* ── Overview ──────────────────────────────────────────────── */
typedef struct {
    int       total_sessions;
    long long total_messages;
    long long total_tool_calls;
    long long total_input_tokens;
    long long total_output_tokens;
    long long total_cache_read;
    long long total_cache_write;
    long long total_tokens;
    double    estimated_cost;
    double    actual_cost;
    double    total_hours;
    double    avg_session_duration;
    double    avg_messages_per_session;
    double    avg_tokens_per_session;
    int       user_messages;
    int       assistant_messages;
    int       tool_messages;
    double    date_range_start;       /* 0 if no sessions */
    double    date_range_end;         /* 0 if no sessions */
    int       unknown_cost_sessions;
    int       included_cost_sessions;
} insights_overview_t;

/* ── Model breakdown entry ──────────────────────────────────── */
typedef struct {
    char      model[128];
    int       sessions;
    long long input_tokens;
    long long output_tokens;
    long long cache_read_tokens;
    long long cache_write_tokens;
    long long total_tokens;
    long long tool_calls;
    double    cost;
    bool      has_pricing;
    char      cost_status[16];    /* "included", "estimated", "unknown" */
} insights_model_entry_t;

/* ── Platform breakdown entry ───────────────────────────────── */
typedef struct {
    char      platform[32];
    int       sessions;
    long long messages;
    long long input_tokens;
    long long output_tokens;
    long long cache_read_tokens;
    long long cache_write_tokens;
    long long total_tokens;
    long long tool_calls;
} insights_platform_entry_t;

/* ── Tool breakdown entry ───────────────────────────────────── */
typedef struct {
    char      tool[64];
    int       count;
    double    percentage;
} insights_tool_entry_t;

/* ── Skill breakdown entry ──────────────────────────────────── */
typedef struct {
    char      skill[128];
    int       view_count;
    int       manage_count;
    int       total_count;
    double    percentage;
    double    last_used_at;   /* 0 = never timestamped */
} insights_skill_entry_t;

/* ── Activity patterns ──────────────────────────────────────── */
typedef struct {
    int day;    /* 0=Mon .. 6=Sun */
    int count;
} insights_day_entry_t;

typedef struct {
    int hour;   /* 0..23 */
    int count;
} insights_hour_entry_t;

typedef struct {
    insights_day_entry_t  by_day[7];
    insights_hour_entry_t by_hour[24];
    int  busiest_day_idx;      /* index into by_day, -1 if none */
    int  busiest_hour;         /* hour value 0..23, -1 if none */
    int  busiest_day_count;
    int  busiest_hour_count;
    int  active_days;
    int  max_streak;
} insights_activity_t;

/* ── Notable session entry ──────────────────────────────────── */
typedef struct {
    char label[32];
    char session_id[64];
    char value[64];
    char date[16];              /* "Jan 01" style */
} insights_notable_entry_t;

/* ── Complete insights report ───────────────────────────────── */
typedef struct {
    int  days;
    char source_filter[INSIGHTS_SOURCE_FILTER_MAX];
    bool empty;

    insights_overview_t     overview;

    insights_model_entry_t  models[INSIGHTS_MAX_MODELS];
    int                     model_count;

    insights_platform_entry_t platforms[INSIGHTS_MAX_PLATFORMS];
    int                      platform_count;

    insights_tool_entry_t   tools[INSIGHTS_MAX_TOOLS];
    int                     tool_count;

    insights_skill_entry_t  skills[INSIGHTS_MAX_SKILLS];
    int                     skill_count;
    int                     total_skill_loads;
    int                     total_skill_edits;
    int                     distinct_skills;

    insights_activity_t     activity;

    insights_notable_entry_t notables[INSIGHTS_MAX_TOP_SESSIONS];
    int                      notable_count;
} insights_report_t;

/* ── API ────────────────────────────────────────────────────── */

/**
 * Generate a full insights report from the session database.
 *
 * @param db             Session database handle (from db_open)
 * @param days           Look-back window in days (0 = all time)
 * @param source_filter  Optional platform filter (NULL or empty = all)
 * @return malloc'd report, caller must free with insights_report_free()
 */
insights_report_t *insights_generate(db_t *db, int days, const char *source_filter);

/**
 * Free an insights report and all its internal allocations.
 */
void insights_report_free(insights_report_t *r);

/**
 * Format a complete insights report for terminal/CLI display.
 * Matches Python InsightsEngine.format_terminal() output style.
 * @return malloc'd string, caller must free
 */
char *insights_format_terminal(const insights_report_t *r);

/**
 * Format a complete insights report for gateway/messaging display (Markdown).
 * Matches Python InsightsEngine.format_gateway() output style.
 * @return malloc'd string, caller must free
 */
char *insights_format_gateway(const insights_report_t *r);

/**
 * Quick insight: get total token and cost breakdown for the last N days.
 * Returns NULL if no session data available.
 */
char *insights_quick_stats(db_t *db, int days);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_INSIGHTS_H */
