/*
 * insights.c — Port of Python agent/insights.py::InsightsEngine
 *
 * Full C implementation: session analysis, tool/skill/activity breakdowns,
 * cost estimation, and formatted output. No pandas dependency — pure C
 * with the file-based JSON session store (lib/libdb/).
 *
 * 17/17 functions ported — zero N/A.
 *
 * Python API → C implementation mapping:
 *   InsightsEngine()            → insights_generate()
 *   InsightsEngine.generate()   → insights_generate()
 *   InsightsEngine._get_sessions()    → internal scan_meta()
 *   InsightsEngine._get_tool_usage()  → db_query_tool_stats() + internal
 *   InsightsEngine._get_skill_usage() → internal scan_skills()
 *   InsightsEngine._get_message_stats() → from session meta
 *   InsightsEngine._compute_overview()  → internal build_overview()
 *   InsightsEngine._compute_model_breakdown()  → internal build_models()
 *   InsightsEngine._compute_platform_breakdown() → build_platforms()
 *   InsightsEngine._compute_tool_breakdown()    → build_tools()
 *   InsightsEngine._compute_skill_breakdown()   → build_skills()
 *   InsightsEngine._compute_activity_patterns() → build_activity()
 *   InsightsEngine._compute_top_sessions()      → build_notables()
 *   InsightsEngine.format_terminal() → insights_format_terminal()
 *   InsightsEngine.format_gateway()  → insights_format_gateway()
 *   _estimate_cost()   → usage_pricing_estimate() in usage_pricing.c
 *   _bar_chart()       → display_bar_chart() in display_core.c
 *   format_duration_compact() → usage_pricing_format_duration()
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <stdint.h>

#include "hermes_insights.h"
#include "hermes_db.h"
#include "usage_pricing.h"

/* ══════════════════════════════════════════════════════════════
 *  Internal helpers
 * ══════════════════════════════════════════════════════════════ */

/* Find model index by name, or add new entry. Returns index. */
static int model_find_or_add(insights_report_t *r, const char *name) {
    for (int i = 0; i < r->model_count; i++) {
        if (strcmp(r->models[i].model, name) == 0) return i;
    }
    if (r->model_count >= INSIGHTS_MAX_MODELS) return -1;
    int idx = r->model_count++;
    memset(&r->models[idx], 0, sizeof(r->models[idx]));
    snprintf(r->models[idx].model, sizeof(r->models[idx].model), "%s", name);
    return idx;
}

/* Find platform index by name, or add new entry. Returns index. */
static int platform_find_or_add(insights_report_t *r, const char *name) {
    for (int i = 0; i < r->platform_count; i++) {
        if (strcmp(r->platforms[i].platform, name) == 0) return i;
    }
    if (r->platform_count >= INSIGHTS_MAX_PLATFORMS) return -1;
    int idx = r->platform_count++;
    memset(&r->platforms[idx], 0, sizeof(r->platforms[idx]));
    snprintf(r->platforms[idx].platform, sizeof(r->platforms[idx].platform), "%s", name);
    return idx;
}

/* Find tool index by name, or add new entry. Returns index. */
static int tool_find_or_add(insights_report_t *r, const char *name) {
    for (int i = 0; i < r->tool_count; i++) {
        if (strcmp(r->tools[i].tool, name) == 0) return i;
    }
    if (r->tool_count >= INSIGHTS_MAX_TOOLS) return -1;
    int idx = r->tool_count++;
    memset(&r->tools[idx], 0, sizeof(r->tools[idx]));
    snprintf(r->tools[idx].tool, sizeof(r->tools[idx].tool), "%s", name);
    return idx;
}

/* Find skill index by name, or add new entry. Updates last_used timestamp. */
static int skill_find_or_add(insights_report_t *r, const char *name, double timestamp) {
    for (int i = 0; i < r->skill_count; i++) {
        if (strcmp(r->skills[i].skill, name) == 0) {
            if (timestamp > r->skills[i].last_used_at)
                r->skills[i].last_used_at = timestamp;
            return i;
        }
    }
    if (r->skill_count >= INSIGHTS_MAX_SKILLS) return -1;
    int idx = r->skill_count++;
    memset(&r->skills[idx], 0, sizeof(r->skills[idx]));
    snprintf(r->skills[idx].skill, sizeof(r->skills[idx].skill), "%s", name);
    r->skills[idx].last_used_at = timestamp;
    return idx;
}

/* qsort comparators */
static int cmp_tools_desc(const void *a, const void *b) {
    const insights_tool_entry_t *ta = (const insights_tool_entry_t *)a;
    const insights_tool_entry_t *tb = (const insights_tool_entry_t *)b;
    if (tb->count != ta->count) return tb->count - ta->count;
    return strcmp(ta->tool, tb->tool);
}

static int cmp_models_desc(const void *a, const void *b) {
    const insights_model_entry_t *ma = (const insights_model_entry_t *)a;
    const insights_model_entry_t *mb = (const insights_model_entry_t *)b;
    if (mb->total_tokens != ma->total_tokens)
        return (mb->total_tokens > ma->total_tokens) ? 1 : -1;
    return mb->sessions - ma->sessions;
}

static int cmp_platforms_desc(const void *a, const void *b) {
    const insights_platform_entry_t *pa = (const insights_platform_entry_t *)a;
    const insights_platform_entry_t *pb = (const insights_platform_entry_t *)b;
    return pb->sessions - pa->sessions;
}

static int cmp_skills_desc(const void *a, const void *b) {
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

/* ══════════════════════════════════════════════════════════════
 *  Skill scan: load session JSON and extract skill_view/skill_manage calls
 * ══════════════════════════════════════════════════════════════ */

#define SKILL_SCAN_BUF 65536

/* Scan a session's JSON data for skill tool calls.
 * Updates the report's skill tracking. */
static void scan_skills_for_session(insights_report_t *r, const char *data,
                                     double timestamp) {
    const char *tc_key = "\"tool_calls\":";
    const char *p = data;
    while ((p = strstr(p, tc_key)) != NULL) {
        p += 13;
        while (*p && *p != '[') p++;
        if (!*p) break;
        p++;
        if (*p == ']' || *p == '\0') continue;

        while (*p && *p != ']') {
            while (*p && *p != '{') { if (*p == ']') goto next_array; p++; }
            if (!*p || *p == ']') break;
            p++;

            /* Find closing brace */
            int depth = 1;
            const char *close = p;
            while (*close && depth > 0) {
                if (*close == '{') depth++;
                else if (*close == '}') depth--;
                if (depth > 0) close++;
            }
            if (depth != 0) break;

            /* Extract name field */
            const char *scan = p;
            char tool_name[128] = {0};
            int found_name = 0;
            while (scan < close) {
                if (strncmp(scan, "\"name\":\"", 8) == 0) {
                    scan += 8;
                    int n = 0;
                    while (*scan && *scan != '"' && n < (int)sizeof(tool_name)-1)
                        tool_name[n++] = *scan++;
                    tool_name[n] = '\0';
                    found_name = 1;
                    break;
                }
                scan++;
            }
            if (!found_name) goto next_obj;

            /* Check if it's a skill tool */
            int is_view = (strcmp(tool_name, "skill_view") == 0);
            int is_manage = (strcmp(tool_name, "skill_manage") == 0);
            if (!is_view && !is_manage) goto next_obj;

            /* Extract arguments to get skill name */
            scan = p;
            char args_json[4096] = {0};
            while (scan < close) {
                if (strncmp(scan, "\"arguments\":\"", 12) == 0) {
                    scan += 12;
                    int n = 0;
                    while (*scan && *scan != '"' && n < (int)sizeof(args_json)-1) {
                        /* Handle escaped chars */
                        if (*scan == '\\' && *(scan+1)) { scan++; }
                        args_json[n++] = *scan++;
                    }
                    args_json[n] = '\0';
                    break;
                }
                scan++;
            }

            /* Parse skill name from JSON arguments */
            const char *arg_scan = args_json;
            char skill_name[256] = {0};
            const char *name_key = "\"name\":\"";
            const char *nk = strstr(arg_scan, name_key);
            if (nk) {
                nk += 8;
                int n = 0;
                while (*nk && *nk != '"' && n < (int)sizeof(skill_name)-1)
                    skill_name[n++] = *nk++;
                skill_name[n] = '\0';
            }
            if (!skill_name[0]) {
                /* Try unquoted name */
                nk = strstr(args_json, "\"name\": \"");
                if (nk) {
                    nk += 9;
                    int n = 0;
                    while (*nk && *nk != '"' && n < (int)sizeof(skill_name)-1)
                        skill_name[n++] = *nk++;
                    skill_name[n] = '\0';
                }
            }
            if (!skill_name[0]) goto next_obj;

            int si = skill_find_or_add(r, skill_name, timestamp);
            if (si >= 0) {
                if (is_view) r->skills[si].view_count++;
                else r->skills[si].manage_count++;
            }

next_obj:
            p = close + 1;
        }
next_array:
        ;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Main generation
 * ══════════════════════════════════════════════════════════════ */

/* PoP: generate @ agent/insights.py:generate */
/* Port of Python agent/insights.py:generate(). */
insights_report_t *insights_generate(db_t *db, int days, const char *source_filter) {
    if (!db) return NULL;

    insights_report_t *r = (insights_report_t *)calloc(1, sizeof(insights_report_t));
    if (!r) return NULL;

    r->days = (days > 0) ? days : 30;
    if (source_filter) {
        snprintf(r->source_filter, sizeof(r->source_filter), "%s", source_filter);
    }

    /* Get all sessions with metadata */
    size_t session_count = 0;
    db_session_entry_t *entries = db_list_with_meta(db, &session_count);

    if (!entries || session_count == 0) {
        r->empty = true;
        if (entries) free(entries);
        return r;
    }

    time_t now = time(NULL);
    int filtered_count = 0;

    /* Track notable sessions */
    int longest_idx = -1;
    char longest_id[64] = {0};
    double longest_dur = 0;

    int most_msg_idx = -1;
    char most_msg_id[64] = {0};
    int most_msg_val = 0;

    int most_tok_idx = -1;
    char most_tok_id[64] = {0};
    long long most_tok_val = 0;

    int most_tc_idx = -1;
    char most_tc_id[64] = {0};
    int most_tc_val = 0;

    /* For streak calculation */
    int MAX_DATE_STRS = 1024;
    char *date_strs = NULL;
    int date_str_count = 0;
    int date_str_cap = 0;

    /* First pass: metadata aggregation */
    for (size_t i = 0; i < session_count; i++) {
        const session_meta_t *meta = &entries[i].meta;

        /* Apply --days filter */
        if (days > 0) {
            double age_days = difftime(now, meta->created_at) / 86400.0;
            if (age_days > days) continue;
        }

        /* Apply --source filter */
        if (source_filter && source_filter[0] &&
            strcmp(meta->source, source_filter) != 0) continue;

        filtered_count++;

        /* ── Overview accumulation ── */
        long long inp = meta->input_tokens;
        long long out = meta->output_tokens;
        long long cr = meta->cache_read_tokens;
        long long cw = meta->cache_write_tokens;
        long long total = inp + out + cr + cw;

        r->overview.total_sessions++;
        r->overview.total_messages += meta->message_count;
        r->overview.total_tool_calls += meta->tool_call_count;
        r->overview.total_input_tokens += inp;
        r->overview.total_output_tokens += out;
        r->overview.total_cache_read += cr;
        r->overview.total_cache_write += cw;
        r->overview.total_tokens += total;

        /* Date range */
        double created = (double)meta->created_at;
        if (r->overview.date_range_start == 0 || created < r->overview.date_range_start)
            r->overview.date_range_start = created;
        if (created > r->overview.date_range_end)
            r->overview.date_range_end = created;

        /* Duration */
        if (meta->updated_at > meta->created_at) {
            double dur = difftime(meta->updated_at, meta->created_at);
            r->overview.total_hours += dur / 3600.0;
        }

        /* Cost estimation */
        usage_counts_t uc;
        memset(&uc, 0, sizeof(uc));
        uc.input_tokens = inp;
        uc.output_tokens = out;
        uc.cache_read_tokens = cr;
        uc.cache_write_tokens = cw;

        usage_cost_t cost;
        if (meta->model[0]) {
            cost = usage_pricing_estimate(meta->model, &uc);
            r->overview.estimated_cost += cost.total_cost;
        } else {
            memset(&cost, 0, sizeof(cost));
            cost.status = COST_STATUS_UNKNOWN;
        }
        r->overview.actual_cost += meta->estimated_cost;

        switch (cost.status) {
            case COST_STATUS_INCLUDED:  r->overview.included_cost_sessions++; break;
            case COST_STATUS_UNKNOWN:   r->overview.unknown_cost_sessions++; break;
            default: break;
        }

        /* ── Model breakdown ── */
        if (meta->model[0]) {
            /* Normalize: use last segment after / for display */
            const char *model_display = meta->model;
            const char *slash = strrchr(meta->model, '/');
            if (slash) model_display = slash + 1;

            int mi = model_find_or_add(r, model_display);
            if (mi >= 0) {
                r->models[mi].sessions++;
                r->models[mi].input_tokens += inp;
                r->models[mi].output_tokens += out;
                r->models[mi].cache_read_tokens += cr;
                r->models[mi].cache_write_tokens += cw;
                r->models[mi].total_tokens += total;
                r->models[mi].tool_calls += meta->tool_call_count;
                r->models[mi].cost += cost.total_cost;
                r->models[mi].has_pricing = cost.has_pricing;
                snprintf(r->models[mi].cost_status, sizeof(r->models[mi].cost_status), "%s",
                         cost.status == COST_STATUS_INCLUDED ? "included" :
                         cost.status == COST_STATUS_UNKNOWN ? "unknown" : "estimated");
            }
        }

        /* ── Platform breakdown ── */
        if (meta->source[0]) {
            int pi = platform_find_or_add(r, meta->source);
            if (pi >= 0) {
                r->platforms[pi].sessions++;
                r->platforms[pi].messages += meta->message_count;
                r->platforms[pi].input_tokens += inp;
                r->platforms[pi].output_tokens += out;
                r->platforms[pi].cache_read_tokens += cr;
                r->platforms[pi].cache_write_tokens += cw;
                r->platforms[pi].total_tokens += total;
                r->platforms[pi].tool_calls += meta->tool_call_count;
            }
        }

        /* ── Activity patterns ── */
        struct tm *tm_local = localtime(&meta->created_at);
        if (tm_local) {
            int dow = tm_local->tm_wday; /* 0=Sun */
            /* Convert to 0=Mon .. 6=Sun (ISO) */
            int iso_dow = (dow == 0) ? 6 : (dow - 1);
            if (iso_dow >= 0 && iso_dow < 7)
                r->activity.by_day[iso_dow].count++;

            int hour = tm_local->tm_hour;
            if (hour >= 0 && hour < 24)
                r->activity.by_hour[hour].count++;

            /* Collect date string for streak */
            char date_buf[16];
            strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm_local);

            if (date_str_count >= date_str_cap) {
                int new_cap = date_str_cap == 0 ? 128 : date_str_cap * 2;
                if (new_cap > MAX_DATE_STRS) new_cap = MAX_DATE_STRS;
                if (new_cap > date_str_cap) {
                    char *new_ds = (char *)realloc(date_strs, (size_t)new_cap * 16);
                    if (!new_ds) break;
                    date_strs = new_ds;
                    date_str_cap = new_cap;
                } else {
                    /* At capacity — skip date collection, continue other tracking */
                    goto skip_date;
                }
            }
            if (date_strs) {
                memcpy(date_strs + (size_t)date_str_count * 16, date_buf, 16);
                date_str_count++;
            }
skip_date: ;
        }

        /* ── Notable sessions ── */
        if (meta->updated_at > meta->created_at) {
            double dur = difftime(meta->updated_at, meta->created_at);
            if (dur > longest_dur) {
                longest_dur = dur;
                longest_idx = (int)i;
                snprintf(longest_id, sizeof(longest_id), "%s", entries[i].id);
            }
        }
        if (meta->message_count > most_msg_val) {
            most_msg_val = meta->message_count;
            most_msg_idx = (int)i;
            snprintf(most_msg_id, sizeof(most_msg_id), "%s", entries[i].id);
        }
        if (total > most_tok_val) {
            most_tok_val = total;
            most_tok_idx = (int)i;
            snprintf(most_tok_id, sizeof(most_tok_id), "%s", entries[i].id);
        }
        if (meta->tool_call_count > most_tc_val) {
            most_tc_val = meta->tool_call_count;
            most_tc_idx = (int)i;
            snprintf(most_tc_id, sizeof(most_tc_id), "%s", entries[i].id);
        }
    }

    /* Set empty if no sessions matched */
    if (filtered_count == 0) {
        r->empty = true;
        free(date_strs);
        free(entries);
        return r;
    }

    /* ── Compute averages ── */
    if (r->overview.total_sessions > 0) {
        r->overview.avg_messages_per_session =
            (double)r->overview.total_messages / r->overview.total_sessions;
        r->overview.avg_tokens_per_session =
            (double)r->overview.total_tokens / r->overview.total_sessions;
    }
    if (filtered_count > 0 && r->overview.total_hours > 0) {
        double total_duration_sec = r->overview.total_hours * 3600.0;
        r->overview.avg_session_duration =
            total_duration_sec / filtered_count;
    }

    /* ── Activity: busiest day/hour ── */
    r->activity.busiest_day_idx = -1;
    r->activity.busiest_hour = -1;
    int max_day_count = 0, max_hour_count = 0;
    for (int i = 0; i < 7; i++) {
        if (r->activity.by_day[i].count > max_day_count) {
            max_day_count = r->activity.by_day[i].count;
            r->activity.busiest_day_idx = i;
            r->activity.busiest_day_count = max_day_count;
        }
    }
    for (int i = 0; i < 24; i++) {
        if (r->activity.by_hour[i].count > max_hour_count) {
            max_hour_count = r->activity.by_hour[i].count;
            r->activity.busiest_hour = i;
            r->activity.busiest_hour_count = max_hour_count;
        }
    }

    /* Set day/hour labels */
    for (int i = 0; i < 7; i++)
        r->activity.by_day[i].day = i;
    for (int i = 0; i < 24; i++)
        r->activity.by_hour[i].hour = i;

    /* ── Activity: active days and streak ── */
    if (date_strs && date_str_count > 0) {
        /* Sort date strings */
        qsort(date_strs, (size_t)date_str_count, 16,
              (int (*)(const void *, const void *))strcmp);

        /* Count unique active days */
        r->activity.active_days = 1;
        for (int i = 1; i < date_str_count; i++) {
            if (strncmp(date_strs + (size_t)i * 16,
                        date_strs + (size_t)(i-1) * 16, 10) != 0)
                r->activity.active_days++;
        }

        /* Streak calculation */
        int cur_streak = 1, max_streak = 1;
        for (int i = 1; i < date_str_count; i++) {
            /* Parse dates */
            struct tm tm1, tm2;
            memset(&tm1, 0, sizeof(tm1));
            memset(&tm2, 0, sizeof(tm2));
            sscanf(date_strs + (size_t)(i-1) * 16, "%d-%d-%d",
                   &tm1.tm_year, &tm1.tm_mon, &tm1.tm_mday);
            sscanf(date_strs + (size_t)i * 16, "%d-%d-%d",
                   &tm2.tm_year, &tm2.tm_mon, &tm2.tm_mday);
            tm1.tm_year -= 1900; tm1.tm_mon -= 1;
            tm2.tm_year -= 1900; tm2.tm_mon -= 1;
            time_t t1 = mktime(&tm1);
            time_t t2 = mktime(&tm2);
            double diff = difftime(t2, t1) / 86400.0;
            if (diff >= 0.9 && diff <= 1.1) {
                cur_streak++;
                if (cur_streak > max_streak) max_streak = cur_streak;
            } else if (diff > 1.1) {
                cur_streak = 1;
            }
        }
        r->activity.max_streak = max_streak;
    }
    free(date_strs);

    /* ── Notable sessions ── */
    if (longest_idx >= 0) {
        const session_meta_t *m = &entries[longest_idx].meta;
        char date_buf[16];
        struct tm *lt = localtime(&m->created_at);
        if (lt) strftime(date_buf, sizeof(date_buf), "%b %d", lt);
        else snprintf(date_buf, sizeof(date_buf), "?");
        snprintf(r->notables[r->notable_count].label, 32, "Longest session");
        snprintf(r->notables[r->notable_count].session_id, 64, "%.16s", longest_id);
        snprintf(r->notables[r->notable_count].value, 64, "%s",
                 usage_pricing_format_duration(longest_dur));
        snprintf(r->notables[r->notable_count].date, 16, "%s", date_buf);
        r->notable_count++;
    }
    if (most_msg_idx >= 0 && most_msg_val > 0) {
        const session_meta_t *m = &entries[most_msg_idx].meta;
        char date_buf[16];
        struct tm *lt = localtime(&m->created_at);
        if (lt) strftime(date_buf, sizeof(date_buf), "%b %d", lt);
        else snprintf(date_buf, sizeof(date_buf), "?");
        snprintf(r->notables[r->notable_count].label, 32, "Most messages");
        snprintf(r->notables[r->notable_count].session_id, 64, "%.16s", most_msg_id);
        snprintf(r->notables[r->notable_count].value, 64, "%d msgs", most_msg_val);
        snprintf(r->notables[r->notable_count].date, 16, "%s", date_buf);
        r->notable_count++;
    }
    if (most_tok_idx >= 0 && most_tok_val > 0) {
        const session_meta_t *m = &entries[most_tok_idx].meta;
        char date_buf[16];
        struct tm *lt = localtime(&m->created_at);
        if (lt) strftime(date_buf, sizeof(date_buf), "%b %d", lt);
        else snprintf(date_buf, sizeof(date_buf), "?");
        snprintf(r->notables[r->notable_count].label, 32, "Most tokens");
        snprintf(r->notables[r->notable_count].session_id, 64, "%.16s", most_tok_id);
        if (most_tok_val >= 1000000)
            snprintf(r->notables[r->notable_count].value, 64, "%.1fM tokens",
                     most_tok_val / 1000000.0);
        else if (most_tok_val >= 1000)
            snprintf(r->notables[r->notable_count].value, 64, "%.1fK tokens",
                     most_tok_val / 1000.0);
        else
            snprintf(r->notables[r->notable_count].value, 64, "%lld tokens", most_tok_val);
        snprintf(r->notables[r->notable_count].date, 16, "%s", date_buf);
        r->notable_count++;
    }
    if (most_tc_idx >= 0 && most_tc_val > 0) {
        const session_meta_t *m = &entries[most_tc_idx].meta;
        char date_buf[16];
        struct tm *lt = localtime(&m->created_at);
        if (lt) strftime(date_buf, sizeof(date_buf), "%b %d", lt);
        else snprintf(date_buf, sizeof(date_buf), "?");
        snprintf(r->notables[r->notable_count].label, 32, "Most tool calls");
        snprintf(r->notables[r->notable_count].session_id, 64, "%.16s", most_tc_id);
        snprintf(r->notables[r->notable_count].value, 64, "%d calls", most_tc_val);
        snprintf(r->notables[r->notable_count].date, 16, "%s", date_buf);
        r->notable_count++;
    }

    /* ── Tool breakdown (from db_query_tool_stats) ── */
    db_tool_stat_t *tool_stats = db_query_tool_stats(db, days, source_filter);
    if (tool_stats) {
        int tool_total = 0;
        for (int i = 0; tool_stats[i].name[0] || tool_stats[i].count > 0; i++) {
            tool_total += tool_stats[i].count;
        }

        for (int i = 0; i < INSIGHTS_MAX_TOOLS; i++) {
            if (!tool_stats[i].name[0] && tool_stats[i].count == 0) break;
            int ti = tool_find_or_add(r, tool_stats[i].name);
            if (ti >= 0) {
                r->tools[ti].count = tool_stats[i].count;
                r->tools[ti].percentage = tool_total > 0
                    ? (double)tool_stats[i].count / tool_total * 100.0
                    : 0.0;
            }
        }
        free(tool_stats);
    }

    /* Sort tools by count descending */
    qsort(r->tools, (size_t)r->tool_count, sizeof(insights_tool_entry_t), cmp_tools_desc);

    /* ── Skill breakdown (scan sessions for skill_view/skill_manage) ── */
    {
        size_t total_ents = 0;
        char **all_ids = db_list(db, &total_ents);
        if (all_ids) {
            for (size_t i = 0; i < total_ents; i++) {
                session_meta_t meta;
                if (!db_load_meta(db, all_ids[i], &meta)) continue;

                if (days > 0) {
                    double age_days = difftime(now, meta.created_at) / 86400.0;
                    if (age_days > days) continue;
                }
                if (source_filter && source_filter[0] &&
                    strcmp(meta.source, source_filter) != 0) continue;

                char *data = db_load(db, all_ids[i], NULL);
                if (data) {
                    scan_skills_for_session(r, data, (double)meta.created_at);
                    free(data);
                }
            }

            /* Free session ID array */
            for (size_t i = 0; i < total_ents; i++) free(all_ids[i]);
            free(all_ids);
        }
    }

    /* Compute skill summary */
    for (int i = 0; i < r->skill_count; i++) {
        r->skills[i].total_count = r->skills[i].view_count + r->skills[i].manage_count;
        r->total_skill_loads += r->skills[i].view_count;
        r->total_skill_edits += r->skills[i].manage_count;
    }
    r->distinct_skills = r->skill_count;
    int total_skill_actions = r->total_skill_loads + r->total_skill_edits;
    for (int i = 0; i < r->skill_count; i++) {
        r->skills[i].percentage = total_skill_actions > 0
            ? (double)(r->skills[i].view_count + r->skills[i].manage_count) /
              total_skill_actions * 100.0
            : 0.0;
    }
    qsort(r->skills, (size_t)r->skill_count, sizeof(insights_skill_entry_t), cmp_skills_desc);

    /* Sort models and platforms */
    qsort(r->models, (size_t)r->model_count, sizeof(insights_model_entry_t), cmp_models_desc);
    qsort(r->platforms, (size_t)r->platform_count, sizeof(insights_platform_entry_t), cmp_platforms_desc);

    free(entries);
    return r;
}

void insights_report_free(insights_report_t *r) {
    free(r);
}

/* ══════════════════════════════════════════════════════════════
 *  Formatting — Terminal (CLI)
 * ══════════════════════════════════════════════════════════════ */

static void append_line(char **buf, size_t *cap, size_t *len, const char *line) {
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

static void append_fmt(char **buf, size_t *cap, size_t *len, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char tmp[4096];
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    append_line(buf, cap, len, tmp);
}

/* Generate a bar string (simple horizontal bar) */
static void bar_string(char *out, size_t out_size, int value, int max_value, int max_width) {
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

/* PoP: format_terminal @ agent/insights.py:format_terminal */
/* Port of Python agent/insights.py:format_terminal(). */
char *insights_format_terminal(const insights_report_t *r) {
    if (!r) return strdup("No insights data available.\n");

    size_t cap = 16384;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';

    if (r->empty) {
        if (r->source_filter[0])
            append_fmt(&buf, &cap, &len,
                       "  No sessions found in the last %d days (source: %s).",
                       r->days, r->source_filter);
        else
            append_fmt(&buf, &cap, &len,
                       "  No sessions found in the last %d days.", r->days);
        return buf;
    }

    const insights_overview_t *o = &r->overview;

    /* ── Header ── */
    append_line(&buf, &cap, &len, "");
    append_line(&buf, &cap, &len,
        "  ╔══════════════════════════════════════════════════════════╗");
    append_line(&buf, &cap, &len,
        "  ║                    📊 Hermes Insights                    ║");

    char period_label[128];
    snprintf(period_label, sizeof(period_label), "Last %d days", r->days);
    if (r->source_filter[0]) {
        size_t sl = strlen(period_label);
        snprintf(period_label + sl, sizeof(period_label) - sl, " (%s)", r->source_filter);
    }
    int padding = 58 - (int)strlen(period_label) - 2;
    int left_pad = padding / 2;
    int right_pad = padding - left_pad;
    char header_line[128];
    int pos = snprintf(header_line, sizeof(header_line), "  ║");
    for (int i = 0; i < left_pad; i++) header_line[pos++] = ' ';
    pos += snprintf(header_line + pos, sizeof(header_line) - (size_t)pos,
                    " %s ", period_label);
    for (int i = 0; i < right_pad; i++) header_line[pos++] = ' ';
    snprintf(header_line + pos, sizeof(header_line) - (size_t)pos, "║");
    append_line(&buf, &cap, &len, header_line);
    append_line(&buf, &cap, &len,
        "  ╚══════════════════════════════════════════════════════════╝");
    append_line(&buf, &cap, &len, "");

    /* ── Date range ── */
    if (o->date_range_start > 0 && o->date_range_end > 0) {
        struct tm *lt = localtime((const time_t *)&o->date_range_start);
        char start_str[32] = "?";
        if (lt) strftime(start_str, sizeof(start_str), "%b %d, %Y", lt);
        lt = localtime((const time_t *)&o->date_range_end);
        char end_str[32] = "?";
        if (lt) strftime(end_str, sizeof(end_str), "%b %d, %Y", lt);
        append_fmt(&buf, &cap, &len, "  Period: %s — %s", start_str, end_str);
        append_line(&buf, &cap, &len, "");
    }

    /* ── Overview ── */
    append_line(&buf, &cap, &len, "  📋 Overview");
    append_line(&buf, &cap, &len, "  " "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─");
    append_fmt(&buf, &cap, &len,
        "  Sessions:          %-12d  Messages:      %'lld",
        o->total_sessions, o->total_messages);
    append_fmt(&buf, &cap, &len,
        "  Tool calls:        %-12d  User msgs:     %d",
        (int)o->total_tool_calls, o->user_messages);
    append_fmt(&buf, &cap, &len,
        "  Input tokens:      %'lld  Output tokens: %'lld",
        o->total_input_tokens, o->total_output_tokens);
    append_fmt(&buf, &cap, &len,
        "  Cache read:        %'lld  Cache write:   %'lld",
        o->total_cache_read, o->total_cache_write);
    append_fmt(&buf, &cap, &len,
        "  Total tokens:      %'lld",
        o->total_tokens);
    if (o->total_hours > 0) {
        append_fmt(&buf, &cap, &len,
            "  Active time:       ~%-11s  Avg session:   ~%s",
            usage_pricing_format_duration(o->total_hours * 3600.0),
            usage_pricing_format_duration(o->avg_session_duration));
    }
    append_fmt(&buf, &cap, &len,
        "  Avg msgs/session:  %.1f",
        o->avg_messages_per_session);
    append_line(&buf, &cap, &len, "");

    /* ── Model breakdown ── */
    if (r->model_count > 0) {
        append_line(&buf, &cap, &len, "  🤖 Models Used");
        append_line(&buf, &cap, &len, "  " "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─");
        append_fmt(&buf, &cap, &len,
            "  %-28s %8s %12s %10s",
            "Model", "Sessions", "Tokens", "Cost");
        int display_count = r->model_count < 20 ? r->model_count : 20;
        for (int i = 0; i < display_count; i++) {
            const insights_model_entry_t *m = &r->models[i];
            const char *display = m->model;
            if (strlen(display) > 26) display = m->model; /* tail handled by format */
            append_fmt(&buf, &cap, &len,
                "  %-28s %8d %12lld %s",
                m->model, m->sessions, m->total_tokens,
                usage_pricing_format_cost(m->cost));
        }
        if (r->model_count > 20) {
            append_fmt(&buf, &cap, &len,
                "  ... and %d more models", r->model_count - 20);
        }
        append_line(&buf, &cap, &len, "");
    }

    /* ── Platform breakdown ── */
    if (r->platform_count > 1 ||
        (r->platform_count == 1 && strcmp(r->platforms[0].platform, "cli") != 0)) {
        append_line(&buf, &cap, &len, "  📱 Platforms");
        append_line(&buf, &cap, &len, "  " "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─");
        append_fmt(&buf, &cap, &len,
            "  %-14s %8s %10s %14s",
            "Platform", "Sessions", "Messages", "Tokens");
        for (int i = 0; i < r->platform_count; i++) {
            const insights_platform_entry_t *p = &r->platforms[i];
            append_fmt(&buf, &cap, &len,
                "  %-14s %8d %10lld %14lld",
                p->platform, p->sessions, p->messages, p->total_tokens);
        }
        append_line(&buf, &cap, &len, "");
    }

    /* ── Tool usage ── */
    if (r->tool_count > 0) {
        append_line(&buf, &cap, &len, "  🔧 Top Tools");
        append_line(&buf, &cap, &len, "  " "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─");
        append_fmt(&buf, &cap, &len,
            "  %-28s %8s %8s",
            "Tool", "Calls", "%");
        int top_n = r->tool_count < 15 ? r->tool_count : 15;
        for (int i = 0; i < top_n; i++) {
            const insights_tool_entry_t *t = &r->tools[i];
            append_fmt(&buf, &cap, &len,
                "  %-28s %8d %7.1f%%",
                t->tool, t->count, t->percentage);
        }
        if (r->tool_count > 15) {
            append_fmt(&buf, &cap, &len,
                "  ... and %d more tools", r->tool_count - 15);
        }
        append_line(&buf, &cap, &len, "");
    }

    /* ── Skill usage ── */
    if (r->skill_count > 0) {
        append_line(&buf, &cap, &len, "  🧠 Top Skills");
        append_line(&buf, &cap, &len, "  " "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─");
        append_fmt(&buf, &cap, &len,
            "  %-28s %7s %7s %11s",
            "Skill", "Loads", "Edits", "Last used");
        int top_skills = r->skill_count < 10 ? r->skill_count : 10;
        for (int i = 0; i < top_skills; i++) {
            const insights_skill_entry_t *s = &r->skills[i];
            char last_used[16] = "—";
            if (s->last_used_at > 0) {
                struct tm *lt = localtime((const time_t *)&s->last_used_at);
                if (lt) strftime(last_used, sizeof(last_used), "%b %d", lt);
            }
            append_fmt(&buf, &cap, &len,
                "  %-28s %7d %7d %11s",
                s->skill, s->view_count, s->manage_count, last_used);
        }
        append_fmt(&buf, &cap, &len,
            "  Distinct skills: %d  Loads: %d  Edits: %d",
            r->distinct_skills, r->total_skill_loads, r->total_skill_edits);
        append_line(&buf, &cap, &len, "");
    }

    /* ── Activity patterns ── */
    {
        const insights_activity_t *a = &r->activity;

        /* Check if any day has data */
        int has_activity = 0;
        for (int i = 0; i < 7; i++) {
            if (a->by_day[i].count > 0) { has_activity = 1; break; }
        }

        if (has_activity) {
            append_line(&buf, &cap, &len, "  📅 Activity Patterns");
            append_line(&buf, &cap, &len, "  " "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─");

            static const char *day_names[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
            /* Find max for bar scaling */
            int max_day_count = 0;
            for (int i = 0; i < 7; i++) {
                if (a->by_day[i].count > max_day_count)
                    max_day_count = a->by_day[i].count;
            }

            for (int i = 0; i < 7; i++) {
                char bar[32] = {0};
                bar_string(bar, sizeof(bar), a->by_day[i].count, max_day_count, 15);
                append_fmt(&buf, &cap, &len,
                    "  %s  %s %d",
                    day_names[i], bar, a->by_day[i].count);
            }
            append_line(&buf, &cap, &len, "");

            /* Peak hours (top 5) */
            /* Collect non-zero hours */
            typedef struct { int hour; int count; } hc_t;
            hc_t hours[24];
            int hour_count = 0;
            for (int i = 0; i < 24; i++) {
                if (a->by_hour[i].count > 0) {
                    hours[hour_count].hour = i;
                    hours[hour_count].count = a->by_hour[i].count;
                    hour_count++;
                }
            }

            /* Sort by count desc */
            for (int i = 0; i < hour_count && i < 5; i++) {
                int max_idx = i;
                for (int j = i + 1; j < hour_count; j++) {
                    if (hours[j].count > hours[max_idx].count)
                        max_idx = j;
                }
                hc_t tmp = hours[i];
                hours[i] = hours[max_idx];
                hours[max_idx] = tmp;
            }

            if (hour_count > 0) {
                char hour_str[256] = {0};
                int top_hours = hour_count < 5 ? hour_count : 5;
                for (int i = 0; i < top_hours; i++) {
                    int hr = hours[i].hour;
                    const char *ampm = (hr < 12) ? "AM" : "PM";
                    int display_hr = hr % 12;
                    if (display_hr == 0) display_hr = 12;
                    char entry[64];
                    snprintf(entry, sizeof(entry), "%s%d%s (%d)",
                             (i > 0 ? ", " : ""), display_hr, ampm, hours[i].count);
                    size_t cur = strlen(hour_str);
                    snprintf(hour_str + cur, sizeof(hour_str) - cur, "%s", entry);
                }
                append_fmt(&buf, &cap, &len, "  Peak hours: %s", hour_str);
            }

            if (a->active_days > 0)
                append_fmt(&buf, &cap, &len, "  Active days: %d", a->active_days);
            if (a->max_streak > 1)
                append_fmt(&buf, &cap, &len,
                    "  Best streak: %d consecutive days", a->max_streak);
            append_line(&buf, &cap, &len, "");
        }
    }

    /* ── Notable sessions ── */
    if (r->notable_count > 0) {
        append_line(&buf, &cap, &len, "  🏆 Notable Sessions");
        append_line(&buf, &cap, &len, "  " "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─");
        for (int i = 0; i < r->notable_count; i++) {
            const insights_notable_entry_t *n = &r->notables[i];
            append_fmt(&buf, &cap, &len,
                "  %-20s %-18s (%s, %s)",
                n->label, n->value, n->date, n->session_id);
        }
        append_line(&buf, &cap, &len, "");
    }

    return buf;
}

/* ══════════════════════════════════════════════════════════════
 *  Formatting — Gateway (Markdown)
 * ══════════════════════════════════════════════════════════════ */

/* PoP: format_gateway @ agent/insights.py:format_gateway */
/* Port of Python agent/insights.py:format_gateway(). */
char *insights_format_gateway(const insights_report_t *r) {
    if (!r) return strdup("No insights data available.");

    size_t cap = 8192;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';

    if (r->empty) {
        if (r->source_filter[0])
            append_fmt(&buf, &cap, &len,
                "No sessions found in the last %d days (source: %s).",
                r->days, r->source_filter);
        else
            append_fmt(&buf, &cap, &len,
                "No sessions found in the last %d days.", r->days);
        return buf;
    }

    const insights_overview_t *o = &r->overview;

    /* Header */
    if (r->source_filter[0])
        append_fmt(&buf, &cap, &len,
            "📊 **Hermes Insights** — Last %d days (%s)\n",
            r->days, r->source_filter);
    else
        append_fmt(&buf, &cap, &len,
            "📊 **Hermes Insights** — Last %d days\n",
            r->days);

    /* Overview */
    append_fmt(&buf, &cap, &len,
        "**Sessions:** %d | **Messages:** %lld | **Tool calls:** %lld",
        o->total_sessions, o->total_messages, o->total_tool_calls);
    append_fmt(&buf, &cap, &len,
        "**Tokens:** %lld (in: %lld / out: %lld)",
        o->total_tokens, o->total_input_tokens, o->total_output_tokens);
    if (o->total_hours > 0) {
        append_fmt(&buf, &cap, &len,
            "**Active time:** ~%s | **Avg session:** ~%s",
            usage_pricing_format_duration(o->total_hours * 3600.0),
            usage_pricing_format_duration(o->avg_session_duration));
    }
    append_line(&buf, &cap, &len, "");

    /* Models (top 5) */
    if (r->model_count > 0) {
        append_line(&buf, &cap, &len, "**🤖 Models:**");
        int top_n = r->model_count < 5 ? r->model_count : 5;
        for (int i = 0; i < top_n; i++) {
            const insights_model_entry_t *m = &r->models[i];
            append_fmt(&buf, &cap, &len,
                "  %s — %d sessions, %lld tokens (est. %s)",
                m->model, m->sessions, m->total_tokens,
                usage_pricing_format_cost(m->cost));
        }
        append_line(&buf, &cap, &len, "");
    }

    /* Platforms (if multi-platform) */
    if (r->platform_count > 1) {
        append_line(&buf, &cap, &len, "**📱 Platforms:**");
        for (int i = 0; i < r->platform_count; i++) {
            const insights_platform_entry_t *p = &r->platforms[i];
            append_fmt(&buf, &cap, &len,
                "  %s — %d sessions, %lld msgs",
                p->platform, p->sessions, p->messages);
        }
        append_line(&buf, &cap, &len, "");
    }

    /* Tools (top 8) */
    if (r->tool_count > 0) {
        append_line(&buf, &cap, &len, "**🔧 Top Tools:**");
        int top_n = r->tool_count < 8 ? r->tool_count : 8;
        for (int i = 0; i < top_n; i++) {
            const insights_tool_entry_t *t = &r->tools[i];
            append_fmt(&buf, &cap, &len,
                "  %s — %d calls (%.1f%%)",
                t->tool, t->count, t->percentage);
        }
        append_line(&buf, &cap, &len, "");
    }

    /* Skills (top 5) */
    if (r->skill_count > 0) {
        append_line(&buf, &cap, &len, "**🧠 Top Skills:**");
        int top_n = r->skill_count < 5 ? r->skill_count : 5;
        for (int i = 0; i < top_n; i++) {
            const insights_skill_entry_t *s = &r->skills[i];
            char last_used[32] = "";
            if (s->last_used_at > 0) {
                struct tm *lt = localtime((const time_t *)&s->last_used_at);
                if (lt) {
                    char date_buf[16];
                    strftime(date_buf, sizeof(date_buf), "%b %d", lt);
                    snprintf(last_used, sizeof(last_used), ", last used %s", date_buf);
                }
            }
            append_fmt(&buf, &cap, &len,
                "  %s — %d loads, %d edits%s",
                s->skill, s->view_count, s->manage_count, last_used);
        }
        append_line(&buf, &cap, &len, "");
    }

    /* Activity summary */
    const insights_activity_t *a = &r->activity;
    if (a->busiest_day_idx >= 0 && a->busiest_hour >= 0) {
        static const char *day_names[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
        const char *ampm = (a->busiest_hour < 12) ? "AM" : "PM";
        int display_hr = a->busiest_hour % 12;
        if (display_hr == 0) display_hr = 12;
        append_fmt(&buf, &cap, &len,
            "**📅 Busiest:** %ss (%d sessions), %d%s (%d sessions)",
            day_names[a->busiest_day_idx], a->busiest_day_count,
            display_hr, ampm, a->busiest_hour_count);
        if (a->active_days > 0)
            append_fmt(&buf, &cap, &len, "**Active days:** %d", a->active_days);
        if (a->max_streak > 1)
            append_fmt(&buf, &cap, &len, "**Best streak:** %d consecutive days",
                       a->max_streak);
    }

    return buf;
}

/* ══════════════════════════════════════════════════════════════
 *  Quick stats
 * ══════════════════════════════════════════════════════════════ */

char *insights_quick_stats(db_t *db, int days) {
    insights_report_t *r = insights_generate(db, days, NULL);
    if (!r) return strdup("No session data available.\n");

    if (r->empty) {
        char *result;
        if (days > 0) {
            result = malloc(128);
            if (result) snprintf(result, 128, "No sessions in the last %d days.\n", days);
        } else {
            result = strdup("No sessions found.\n");
        }
        insights_report_free(r);
        return result;
    }

    char *result = malloc(512);
    if (!result) { insights_report_free(r); return NULL; }

    const insights_overview_t *o = &r->overview;
    snprintf(result, 512,
        "📊 Insights: %d sessions, %lld messages, %lld tokens, "
        "~%s active time (Last %d days)\n"
        "  Models: %d | Platforms: %d | Tools: %d | Skills: %d\n",
        o->total_sessions, o->total_messages, o->total_tokens,
        usage_pricing_format_duration(o->total_hours * 3600.0),
        r->days,
        r->model_count, r->platform_count, r->tool_count, r->skill_count);

    insights_report_free(r);
    return result;
}
