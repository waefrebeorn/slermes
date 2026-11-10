/*
 * omarchy_usage.c -- Slermes' agent-integration stack, native.
 *
 * Ports omarchy's `omarchy-agent-usage-hermes` collector (the agents-panel
 * contract) into C11.  Omarchy's shell agents panel shows a usage record for
 * the Hermes agent by reading Hermes' local state.db; this module emits the
 * SAME JSON record from a Slermes (or Hermes) state.db, so the panel and any
 * other consumer of the omarchy contract see Slermes exactly where they see
 * Hermes.
 *
 * Reads (schema parity with hermes_state_open.c, which Slermes writes):
 *   sessions(id, started_at, model, billing_provider, billing_base_url)
 *   session_model_usage(session_id, model, billing_provider, billing_base_url,
 *                       input_tokens, output_tokens, cache_read_tokens,
 *                       cache_write_tokens)
 *   messages(session_id, role, timestamp)
 *
 * JSON contract (omarchy shell/plugins/agents/collectors.py + the hermes
 * collector): schemaVersion 1, id "hermes", name "Hermes", stats block with
 * todayPrompts/todaySessions/todayTotalTokens/todayTokensByModel/recentDays
 * (last 7 local days, oldest first)/totalPrompts/totalSessions/activeDays/
 * activeDates/modelUsage, and empty limits.
 */

#define _POSIX_C_SOURCE 200809L
#include "omarchy_usage.h"

#include <dirent.h>
#include <math.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ---- growable string (JSON out) -------------------------------------- */
typedef struct { char *s; size_t len, cap; } sbuf;
static void sput(sbuf *b, const char *s) {
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        size_t c = b->cap ? b->cap * 2 : 512;
        while (c < b->len + n + 1) c *= 2;
        b->s = realloc(b->s, c); b->cap = c;
    }
    memcpy(b->s + b->len, s, n); b->len += n; b->s[b->len] = 0;
}
static void sputll(sbuf *b, long long v) {
    char t[32]; snprintf(t, sizeof(t), "%lld", v); sput(b, t);
}

/* ---- agg structures --------------------------------------------------- */
static void stamp_day(double ts, char out[16]) {
    time_t t = (time_t)ts; struct tm tm; localtime_r(&t, &tm);
    strftime(out, 16, "%Y-%m-%d", &tm);
}
static void today_str(char out[16]) {
    time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm);
    strftime(out, 16, "%Y-%m-%d", &tm);
}
/* omarchy treats negative/non-numeric/absurd tokens as 0. */
static long long trusted(sqlite3_stmt *st, int col) {
    if (sqlite3_column_type(st, col) != SQLITE_INTEGER &&
        sqlite3_column_type(st, col) != SQLITE_FLOAT) return 0;
    double v = sqlite3_column_double(st, col);
    if (isnan(v) || v < 0 || v >= 1e12) return 0;
    return (long long)v;
}

typedef struct day { char date[16]; long long prompts; struct day *next; } day;
typedef struct mdl { char name[64]; long long all_tokens, today_tokens;
                     long long in_, out, cread, cwrite; struct mdl *next; } mdl;

typedef struct {
    long long today_prompts, total_prompts;
    long long today_sessions, total_sessions;
    long long today_tokens;
    day *days;            /* union of dates that carry ANY usage/session */
    mdl *models;
} agg;

/* returns the `day` node for a date, creating it (unsorted, dedup) */
static day *day_of(agg *a, const char *date) {
    for (day *d = a->days; d; d = d->next) if (strcmp(d->date, date) == 0) return d;
    day *d = calloc(1, sizeof(*d)); snprintf(d->date, sizeof(d->date), "%s", date);
    d->next = a->days; a->days = d; return d;
}
static mdl *model_of(agg *a, const char *name) {
    for (mdl *m = a->models; m; m = m->next) if (strcmp(m->name, name) == 0) return m;
    mdl *m = calloc(1, sizeof(*m));
    snprintf(m->name, sizeof(m->name), "%s", (name && name[0]) ? name : "hermes");
    m->next = a->models; a->models = m; return m;
}

/* omarchy route_owner: unrouted rows (no provider AND no url recorded) count
 * as first-request-owned (us); otherwise only empty/"nous" billing is us.
 * A recorded non-nous provider (deepseek, stepfun, ...) routes elsewhere. */
static int is_ours(const char *prov, const char *url) {
    const char *p = prov ? prov : "";
    const char *u = url ? url : "";
    if (!p[0] && !u[0]) return 1;           /* unrouted: first-request owner */
    return p[0] == 0 || strcmp(p, "nous") == 0;
}

/* ---- one state.db ----------------------------------------------------- */
static void collect_db(const char *path, agg *a, const char *today) {
    char uri[PATH_MAX * 2];
    sqlite3 *db = NULL;
    sqlite3_snprintf(sizeof(uri), uri, "file:%s?mode=ro", path);
    if (sqlite3_open_v2(uri, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db); return;
    }
    sqlite3_stmt *st = NULL;
    int has_usage = 0;
    sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' AND name='session_model_usage'", -1, &st, NULL);
    if (st && sqlite3_step(st) == SQLITE_ROW) has_usage = 1;
    if (st) { sqlite3_finalize(st); st = NULL; }

    if (has_usage) {
        const char *q =
          "SELECT smu.session_id, smu.model, smu.billing_provider, smu.billing_base_url, "
          "SUM(CASE WHEN typeof(smu.input_tokens) IN ('integer','real') AND smu.input_tokens>=0 THEN smu.input_tokens ELSE 0 END), "
          "SUM(CASE WHEN typeof(smu.output_tokens) IN ('integer','real') AND smu.output_tokens>=0 THEN smu.output_tokens ELSE 0 END), "
          "SUM(CASE WHEN typeof(smu.cache_read_tokens) IN ('integer','real') AND smu.cache_read_tokens>=0 THEN smu.cache_read_tokens ELSE 0 END), "
          "SUM(CASE WHEN typeof(smu.cache_write_tokens) IN ('integer','real') AND smu.cache_write_tokens>=0 THEN smu.cache_write_tokens ELSE 0 END), "
          "s.started_at "
          "FROM session_model_usage smu JOIN sessions s ON s.id=smu.session_id "
          "GROUP BY 1,2,3,4";
        if (sqlite3_prepare_v2(db, q, -1, &st, NULL) == SQLITE_OK)
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char *model = (const char *)sqlite3_column_text(st, 1);
                const char *prov = (const char *)sqlite3_column_text(st, 2);
                const char *url = (const char *)sqlite3_column_text(st, 3);
                if (!is_ours(prov, url)) continue;
                long long in_ = trusted(st, 4), o = trusted(st, 5),
                          cr = trusted(st, 6), cw = trusted(st, 7);
                mdl *m = model_of(a, model);
                m->all_tokens += in_ + o + cr + cw;
                m->in_ += in_; m->out += o; m->cread += cr; m->cwrite += cw;
                double started = sqlite3_column_double(st, 8);
                char dstr[16]; stamp_day(started, dstr);
                day_of(a, dstr);             /* a session day is an active date */
                if (strcmp(dstr, today) == 0) {
                    m->today_tokens += in_ + o + cr + cw;
                    a->today_tokens += in_ + o + cr + cw;
                }
            }
        if (st) { sqlite3_finalize(st); st = NULL; }
    }

    const char *qsess =
        "SELECT s.id, s.started_at, s.billing_provider, s.billing_base_url, "
        "s.model FROM sessions s";
    if (sqlite3_prepare_v2(db, qsess, -1, &st, NULL) == SQLITE_OK)
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *prov = (const char *)sqlite3_column_text(st, 2);
            const char *url = (const char *)sqlite3_column_text(st, 3);
            if (!is_ours(prov, url)) continue;
            a->total_sessions++;
            double started = sqlite3_column_double(st, 1);
            char dstr[16]; stamp_day(started, dstr);
            if (strcmp(dstr, today) == 0) a->today_sessions++;
            day_of(a, dstr);
        }
    if (st) { sqlite3_finalize(st); st = NULL; }

    const char *qp =
        "SELECT m.timestamp, COUNT(*) FROM messages m "
        "JOIN sessions s ON s.id=m.session_id WHERE m.role='user' AND "
        "(s.billing_provider IS NULL OR s.billing_provider='' "
        "OR s.billing_provider='nous') GROUP BY 1";
    if (sqlite3_prepare_v2(db, qp, -1, &st, NULL) == SQLITE_OK)
        while (sqlite3_step(st) == SQLITE_ROW) {
            double ts = sqlite3_column_double(st, 0);
            long long n = sqlite3_column_int64(st, 1);
            char dstr[16]; stamp_day(ts, dstr);
            day *d = day_of(a, dstr); d->prompts += n;
            a->total_prompts += n;
            if (strcmp(dstr, today) == 0) a->today_prompts += n;
        }
    if (st) { sqlite3_finalize(st); st = NULL; }

    sqlite3_close(db);
}

/* ---- emit the omarchy JSON record -------------------------------------- */
int omarchy_usage_scan(const char *home, char *out, size_t cap) {
    agg a = {0};
    char today[16]; today_str(today);

    char root[PATH_MAX];
    snprintf(root, sizeof(root), "%s", home ? home : "");
    if (!home || !home[0]) snprintf(root, sizeof(root), "%s/.hermes", getenv("HOME") ? getenv("HOME") : "/tmp");

    char dbpath[PATH_MAX];
    snprintf(dbpath, sizeof(dbpath), "%s/state.db", root);
    collect_db(dbpath, &a, today);

    /* named profiles under <root>/profiles/<name>/state.db */
    char profdir[PATH_MAX];
    snprintf(profdir, sizeof(profdir), "%s/profiles", root);
    DIR *d = opendir(profdir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            snprintf(dbpath, sizeof(dbpath), "%s/%s/state.db", profdir, e->d_name);
            collect_db(dbpath, &a, today);
        }
        closedir(d);
    }

    /* activeDates: sorted.  Bubble-small; day counts stay small. */
    int nact = 0; for (day *p = a.days; p; p = p->next) nact++;
    char (*dates)[16] = nact ? calloc(nact, 16) : NULL;
    int ids = 0;
    for (day *p = a.days; p; p = p->next) { snprintf(dates[ids++], 16, "%s", p->date); }
    /* insertion sort ascending */
    for (int i = 1; i < nact; i++) {
        char k[16]; memcpy(k, dates[i], 16); int j = i - 1;
        while (j >= 0 && strcmp(dates[j], k) > 0) { memcpy(dates[j + 1], dates[j], 16); j--; }
        if (j + 1 != i) memcpy(dates[j + 1], k, 16);
    }

    sbuf b = {0};
    sput(&b, "{\"schemaVersion\":1,\"id\":\"hermes\",\"name\":\"Hermes\",\"updatedAt\":\"");
    time_t now = time(NULL); struct tm utc; gmtime_r(&now, &utc);
    char ts[40]; strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &utc);
    sput(&b, ts);
    sput(&b, "Z\",\"ready\":true,\"hasLocalStats\":true,\"hasPromptStats\":true,\"tierLabel\":\"\",\"usageStatusText\":\"\",\"authHelpText\":\"\",\"limits\":[],\"limitsStale\":false,\"limitsFetchedAt\":0,");

    sput(&b, "\"todayPrompts\":"); sputll(&b, a.today_prompts); sput(&b, ",");
    sput(&b, "\"todaySessions\":"); sputll(&b, a.today_sessions); sput(&b, ",");
    sput(&b, "\"todayTotalTokens\":"); sputll(&b, a.today_tokens); sput(&b, ",");
    sput(&b, "\"todayTokensByModel\":{");
    int first = 1;
    for (mdl *m = a.models; m; m = m->next)
        if (m->today_tokens) {
            if (!first) sput(&b, ","); first = 0;
            sput(&b, "\""); sput(&b, m->name); sput(&b, "\":"); sputll(&b, m->today_tokens);
        }
    sput(&b, "},\"recentDays\":[");
    for (int off = 6; off >= 0; off--) {   /* last 7 local days, oldest first */
        time_t t = time(NULL) - (time_t)off * 86400;
        struct tm tm; localtime_r(&t, &tm);
        char dstr[16]; strftime(dstr, 16, "%Y-%m-%d", &tm);
        long long mc = 0; day *dd = a.days; for (; dd; dd = dd->next) if (strcmp(dd->date, dstr) == 0) { mc = dd->prompts; break; }
        if (off < 6) sput(&b, ",");
        sput(&b, "{\"date\":\""); sput(&b, dstr); sput(&b, "\",\"messageCount\":"); sputll(&b, mc); sput(&b, "}");
    }
    sput(&b, "],\"totalPrompts\":"); sputll(&b, a.total_prompts); sput(&b, ",");
    sput(&b, "\"totalSessions\":"); sputll(&b, a.total_sessions); sput(&b, ",");
    sput(&b, "\"activeDays\":"); sputll(&b, (long long)nact); sput(&b, ",\"activeDates\":[");
    for (int i = 0; i < nact; i++) { if (i) sput(&b, ","); sput(&b, "\""); sput(&b, dates[i]); sput(&b, "\""); }
    sput(&b, "],\"modelUsage\":{");
    first = 1;
    for (mdl *m = a.models; m; m = m->next) {
        if (!first) sput(&b, ","); first = 0;
        sput(&b, "\""); sput(&b, m->name);
        sput(&b, "\":{\"inputTokens\":"); sputll(&b, m->in_);
        sput(&b, ",\"outputTokens\":"); sputll(&b, m->out);
        sput(&b, ",\"cacheReadInputTokens\":"); sputll(&b, m->cread);
        sput(&b, ",\"cacheCreationInputTokens\":"); sputll(&b, m->cwrite);
        sput(&b, "}");
    }
    sput(&b, "}}");

    free(dates);
    if (out && cap) { snprintf(out, cap, "%s", b.s ? b.s : "{}"); }
    free(b.s);
    return 0;
}
