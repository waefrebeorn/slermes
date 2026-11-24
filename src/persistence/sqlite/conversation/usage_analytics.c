/*
 * usage_analytics.c - real C11 port of upstream Hermes
 *   persistence/sqlite/conversation/usage_analytics.py statement shaping.
 * The statement text of every dashboard/insights read, the source-filter pair, the
 * unpinned/no-created_source rewrites and the read-wait rule are real here; the
 * sqlite3 connection itself is the seam.
 * PoP: w19_usageanaly_6ec515_scoped @ persistence/sqlite/conversation/usage_analytics.py:_scoped
 * PoP: w19_usageanaly_6ec515_local_surface @ persistence/sqlite/conversation/usage_analytics.py:_LOCAL_SURFACE
 * PoP: w19_usageanaly_6ec515_query_sql @ persistence/sqlite/conversation/usage_analytics.py:_InsightQueries.__init__
 * PoP: w19_usageanaly_6ec515_query_params @ persistence/sqlite/conversation/usage_analytics.py:_InsightQueries._query
 * PoP: w19_usageanaly_6ec515_insight_wait @ persistence/sqlite/conversation/usage_analytics.py:_insight_wait
 */
#include "noc_w19_usageanaly_6ec515.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* str.replace(pat, rep) over every occurrence. */
static char *str_replace(const char *s, const char *pat, const char *rep) {
    if (!s) return NULL;
    if (!pat || !*pat) return dupstr(s);
    size_t plen = strlen(pat), rlen = strlen(rep ? rep : ""), slen = strlen(s);
    size_t cap = slen + 1, count = 0;
    for (const char *p = s; (p = strstr(p, pat)) != NULL; p += plen) count++;
    if (rlen > plen) cap += count * (rlen - plen);
    char *out = (char *)malloc(cap + 1);
    if (!out) return NULL;
    char *o = out;
    const char *p = s;
    const char *hit;
    while ((hit = strstr(p, pat)) != NULL) {
        size_t n = (size_t)(hit - p);
        memcpy(o, p, n);
        o += n;
        memcpy(o, rep, rlen);
        o += rlen;
        p = hit + plen;
    }
    strcpy(o, p);
    return out;
}

typedef struct { const char *base; const char *all; const char *with_source; } sql_pair;

static const sql_pair SQL_PAIRS[] = {
    {"_GET_SESSIONS",
     "SELECT id, source, model, started_at, ended_at, message_count, tool_call_count, input_tokens, output_tokens, cache_read_tokens, cache_write_tokens, billing_provider, billing_base_url, billing_mode, estimated_cost_usd, actual_cost_usd, cost_status, cost_source, api_call_count FROM sessions WHERE started_at >= ? ORDER BY started_at DESC",
     "SELECT id, source, model, started_at, ended_at, message_count, tool_call_count, input_tokens, output_tokens, cache_read_tokens, cache_write_tokens, billing_provider, billing_base_url, billing_mode, estimated_cost_usd, actual_cost_usd, cost_status, cost_source, api_call_count FROM sessions WHERE started_at >= ? AND (source = ? OR (source = 'local' AND created_source = ?)) ORDER BY started_at DESC"},
    {"_GET_TOOL_CALLS",
     "SELECT m.session_id, m.tool_calls FROM messages m INDEXED BY idx_messages_assistant_calls_by_session JOIN sessions s ON s.id = m.session_id WHERE s.started_at >= ? AND m.role = 'assistant' AND m.tool_calls IS NOT NULL",
     "SELECT m.session_id, m.tool_calls FROM messages m INDEXED BY idx_messages_assistant_calls_by_session JOIN sessions s ON s.id = m.session_id WHERE s.started_at >= ? AND (s.source = ? OR (s.source = 'local' AND s.created_source = ?)) AND m.role = 'assistant' AND m.tool_calls IS NOT NULL"},
    {"_GET_SKILL_CALLS",
     "SELECT m.tool_calls, m.timestamp FROM messages m INDEXED BY idx_messages_assistant_calls_by_session JOIN sessions s ON s.id = m.session_id WHERE s.started_at >= ? AND m.role = 'assistant' AND m.tool_calls IS NOT NULL AND (instr(m.tool_calls, 'skill_view') > 0 OR instr(m.tool_calls, 'skill_manage') > 0)",
     "SELECT m.tool_calls, m.timestamp FROM messages m INDEXED BY idx_messages_assistant_calls_by_session JOIN sessions s ON s.id = m.session_id WHERE s.started_at >= ? AND (s.source = ? OR (s.source = 'local' AND s.created_source = ?)) AND m.role = 'assistant' AND m.tool_calls IS NOT NULL AND (instr(m.tool_calls, 'skill_view') > 0 OR instr(m.tool_calls, 'skill_manage') > 0)"},
    {"_GET_TOOL_NAMES",
     "SELECT m.session_id, m.tool_name, COUNT(*) as count\n                   FROM messages m\n                   JOIN sessions s ON s.id = m.session_id\n                   WHERE s.started_at >= ?\n                     AND m.role = 'tool' AND m.tool_name IS NOT NULL\n                   GROUP BY m.session_id, m.tool_name",
     "SELECT m.session_id, m.tool_name, COUNT(*) as count\n                   FROM messages m\n                   JOIN sessions s ON s.id = m.session_id\n                   WHERE s.started_at >= ? AND (s.source = ? OR (s.source = 'local' AND s.created_source = ?))\n                     AND m.role = 'tool' AND m.tool_name IS NOT NULL\n                   GROUP BY m.session_id, m.tool_name"},
    {"_GET_MESSAGE_STATS",
     "SELECT\n                     COUNT(*) as total_messages,\n                     SUM(CASE WHEN m.role = 'user' THEN 1 ELSE 0 END) as user_messages,\n                     SUM(CASE WHEN m.role = 'assistant' THEN 1 ELSE 0 END) as assistant_messages,\n                     SUM(CASE WHEN m.role = 'tool' THEN 1 ELSE 0 END) as tool_messages\n                   FROM messages m\n                   JOIN sessions s ON s.id = m.session_id\n                   WHERE s.started_at >= ?",
     "SELECT\n                     COUNT(*) as total_messages,\n                     SUM(CASE WHEN m.role = 'user' THEN 1 ELSE 0 END) as user_messages,\n                     SUM(CASE WHEN m.role = 'assistant' THEN 1 ELSE 0 END) as assistant_messages,\n                     SUM(CASE WHEN m.role = 'tool' THEN 1 ELSE 0 END) as tool_messages\n                   FROM messages m\n                   JOIN sessions s ON s.id = m.session_id\n                   WHERE s.started_at >= ? AND (s.source = ? OR (s.source = 'local' AND s.created_source = ?))"},
    {"_GET_MODEL_USAGE",
     "SELECT u.session_id, u.model, u.billing_provider, u.billing_base_url, u.api_call_count, u.input_tokens, u.output_tokens, u.cache_read_tokens, u.cache_write_tokens, u.reasoning_tokens, u.estimated_cost_usd, u.actual_cost_usd, u.cost_status, u.cost_source, u.billing_mode FROM session_model_usage u JOIN sessions s ON s.id = u.session_id WHERE s.started_at >= ?",
     "SELECT u.session_id, u.model, u.billing_provider, u.billing_base_url, u.api_call_count, u.input_tokens, u.output_tokens, u.cache_read_tokens, u.cache_write_tokens, u.reasoning_tokens, u.estimated_cost_usd, u.actual_cost_usd, u.cost_status, u.cost_source, u.billing_mode FROM session_model_usage u JOIN sessions s ON s.id = u.session_id WHERE s.started_at >= ? AND (s.source = ? OR (s.source = 'local' AND s.created_source = ?))"},
    {NULL, NULL, NULL}
};

static const sql_pair *find_pair(const char *base) {
    if (!base) return NULL;
    for (int i = 0; SQL_PAIRS[i].base; i++)
        if (!strcmp(SQL_PAIRS[i].base, base)) return &SQL_PAIRS[i];
    return NULL;
}

static const char *LOCAL_SURFACE = " OR ({a}source = 'local' AND {a}created_source = ?)";

char *w19_usageanaly_6ec515_local_surface(const char *alias) {
    const char *a = alias ? alias : "";
    char buf[512];
    const char *p = LOCAL_SURFACE;
    size_t o = 0;
    while (*p && o + strlen(a) + 1 < sizeof(buf)) {
        if (p[0] == '{' && p[1] == 'a' && p[2] == '}') {
            strcpy(buf + o, a);
            o += strlen(a);
            p += 3;
            continue;
        }
        buf[o++] = *p++;
    }
    buf[o] = '\0';
    return dupstr(buf);
}

json_t *w19_usageanaly_6ec515_scoped(const char *before, const char *after, const char *alias) {
    const char *b = before ? before : "";
    const char *aft = after ? after : "";
    const char *al = alias ? alias : "s.";
    char *surface = w19_usageanaly_6ec515_local_surface(al);
    char src[1024];
    snprintf(src, sizeof(src), " AND (%ssource = ?%s)", al, surface);
    free(surface);

    size_t need = strlen(b) + strlen(aft) + 1;
    char *all = (char *)malloc(need);
    if (!all) return json_null();
    snprintf(all, need, "%s%s", b, aft);

    size_t need2 = strlen(b) + strlen(src) + strlen(aft) + 1;
    char *filtered = (char *)malloc(need2);
    if (!filtered) { free(all); return json_null(); }
    snprintf(filtered, need2, "%s%s%s", b, src, aft);

    json_t *out = json_object();
    json_set(out, "all", json_string(all));
    json_set(out, "with_source", json_string(filtered));
    free(all);
    free(filtered);
    return out;
}

json_t *w19_usageanaly_6ec515_query_sql(const char *base, int has_index, int local_surfaces) {
    const sql_pair *pair = find_pair(base);
    if (!pair) return json_null();
    char *all = dupstr(pair->all);
    char *with_source = dupstr(pair->with_source);
    if (!all || !with_source) { free(all); free(with_source); return json_null(); }

    if (!has_index) {
        /* strip = f" INDEXED BY {index}" */
        char *stripped = str_replace(all, " INDEXED BY idx_messages_assistant_calls_by_session", "");
        free(all);
        all = stripped;
        stripped = str_replace(with_source, " INDEXED BY idx_messages_assistant_calls_by_session", "");
        free(with_source);
        with_source = stripped;
    }
    if (!local_surfaces) {
        const char *aliases[2] = {"", "s."};
        for (int i = 0; i < 2; i++) {
            char *pat = w19_usageanaly_6ec515_local_surface(aliases[i]);
            char *tmp = str_replace(with_source, pat, "");
            free(pat);
            free(with_source);
            with_source = tmp;
        }
    }
    json_t *out = json_object();
    json_set(out, "all", json_string(all));
    json_set(out, "with_source", json_string(with_source));
    free(all);
    free(with_source);
    return out;
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

json_t *w19_usageanaly_6ec515_query_params(json_t *cutoff, json_t *source, int local_surfaces) {
    json_t *out = json_array();
    json_append(out, cutoff ? json_copy(cutoff) : json_null());
    if (!py_truthy(source)) return out;
    json_append(out, json_copy(source));
    if (local_surfaces) json_append(out, json_copy(source));
    return out;
}

json_t *w19_usageanaly_6ec515_insight_wait(int read_only, double busy_timeout) {
    return json_number(read_only ? busy_timeout : 1.0);
}
