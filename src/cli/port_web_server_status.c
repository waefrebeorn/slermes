/*
 * port_web_server_status.c — faithful C11 port of the dashboard operational
 * backbone in hermes_cli/web_server.py (the heavy dependency layer the rest of
 * the web_server routes depend on).
 *
 * Every function has REAL behavior — no stubs, no façade:
 *   - ws_probe_gateway_health        → real HTTP GET via libhttp
 *   - ws_count_active_sessions       → real read-only SQLite query on state.db
 *   - ws_record_error / _count       → real bounded in-memory ring
 *   - dashboard_runtime_*            → real opaque app.state analogue
 *   - ws_start_desktop_cron_ticker    → real periodic scheduler_run_job
 */

#include "web_server_status.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hermes_http.h"        /* http_new / http_get / http_resp_t */
#include "hermes_web_dashboard.h" /* ws_is_accepted_host */
#include "slermes_home.h"        /* slermes_home(), SLERMES_FILE_STATE_DB */
#include "sqlite3.h"
#include "cron_jobs.h"           /* cronjobs_get_due_jobs */
#include "hermes_json.h"         /* json_free, json_node_is_array, json_array_* */

/* cron scheduler runtime (real scheduler_run_job, finished earlier in port) */
#include "cron_scheduler_runtime.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>

/* ── gateway health env (mirrors web_server._GATEWAY_HEALTH_URL /
 *     _GATEWAY_HEALTH_TIMEOUT) ─────────────────────────────────────────── */
static const char *gateway_health_url(void) {
    const char *u = getenv("GATEWAY_HEALTH_URL");
    return u && u[0] ? u : NULL;
}
static int gateway_health_timeout(void) {
    const char *t = getenv("GATEWAY_HEALTH_TIMEOUT");
    if (t && t[0]) {
        int v = atoi(t);
        if (v > 0) return v;
    }
    return 5;
}

/* ── runtime state (Python app.state) ──────────────────────────────────── */
struct dashboard_runtime_t {
    pthread_mutex_t lock;
    /* PTY active-session-file map (channel -> malloc'd abs path). */
    struct pty_file { char *channel; char *path; struct pty_file *next; } *pty_files;
    /* error ring */
    struct ws_error_entry {
        char component[32];
        char message[256];
        time_t ts;
    } errors[WS_ERROR_RING_MAX];
    int error_head;   /* next write slot */
    int error_count;  /* total entries (saturates at WS_ERROR_RING_MAX) */
    /* desktop cron ticker */
    volatile int cron_running;
    pthread_t cron_thread;
};

static dashboard_runtime_t g_rt = {0};
static pthread_once_t g_rt_once = PTHREAD_ONCE_INIT;

static void rt_init(void) {
    pthread_mutex_init(&g_rt.lock, NULL);
    g_rt.pty_files = NULL;
    g_rt.error_head = 0;
    g_rt.error_count = 0;
    g_rt.cron_running = 0;
}

dashboard_runtime_t *dashboard_runtime_get(void) {
    pthread_once(&g_rt_once, rt_init);
    return &g_rt;
}

void dashboard_runtime_lock(dashboard_runtime_t *rt) {
    if (rt) pthread_mutex_lock(&rt->lock);
}
void dashboard_runtime_unlock(dashboard_runtime_t *rt) {
    if (rt) pthread_mutex_unlock(&rt->lock);
}

void dashboard_runtime_set_pty_file(dashboard_runtime_t *rt,
                                    const char *channel,
                                    const char *abs_path) {
    if (!rt || !channel || !abs_path) return;
    pthread_mutex_lock(&rt->lock);
    for (struct pty_file *p = rt->pty_files; p; p = p->next) {
        if (strcmp(p->channel, channel) == 0) {
            free(p->path);
            p->path = strdup(abs_path);
            pthread_mutex_unlock(&rt->lock);
            return;
        }
    }
    struct pty_file *n = malloc(sizeof(*n));
    n->channel = strdup(channel);
    n->path = strdup(abs_path);
    n->next = rt->pty_files;
    rt->pty_files = n;
    pthread_mutex_unlock(&rt->lock);
}

const char *dashboard_runtime_get_pty_file(dashboard_runtime_t *rt,
                                           const char *channel) {
    if (!rt || !channel) return NULL;
    pthread_mutex_lock(&rt->lock);
    const char *found = NULL;
    for (struct pty_file *p = rt->pty_files; p; p = p->next) {
        if (strcmp(p->channel, channel) == 0) { found = p->path; break; }
    }
    pthread_mutex_unlock(&rt->lock);
    return found;
}

/* ── gateway health probe (Python _probe_gateway_health) ───────────────── */
/* PoP: ws_probe_gateway_health @ hermes_cli/web_server.py:_probe_gateway_health */
bool ws_probe_gateway_health(char **out_body) {
    if (out_body) *out_body = NULL;
    const char *raw = gateway_health_url();
    if (!raw || !raw[0]) return false;

    /* Normalise to base URL (strip trailing /health or /health/detailed). */
    char base[1024];
    snprintf(base, sizeof(base), "%s", raw);
    size_t blen = strlen(base);
    while (blen > 0 && base[blen - 1] == '/') base[--blen] = '\0';
    const char *detailed = "/health/detailed";
    const char *simple = "/health";
    if (blen >= strlen(detailed) &&
        strcmp(base + blen - strlen(detailed), detailed) == 0)
        base[blen - strlen(detailed)] = '\0';
    else if (blen >= strlen(simple) &&
             strcmp(base + blen - strlen(simple), simple) == 0)
        base[blen - strlen(simple)] = '\0';

    http_t *h = http_new(gateway_health_timeout());
    if (!h) return false;

    static const char *paths[] = {"/health/detailed", "/health"};
    bool alive = false;
    for (int i = 0; i < 2; i++) {
        char url[1536];
        snprintf(url, sizeof(url), "%s%s", base, paths[i]);
        http_resp_t *resp = http_get(h, url, NULL);
        if (resp && resp->status == 200 && resp->body) {
            alive = true;
            if (out_body) {
                *out_body = malloc(resp->body_len + 1);
                memcpy(*out_body, resp->body, resp->body_len);
                (*out_body)[resp->body_len] = '\0';
            }
            http_resp_free(resp);
            break;
        }
        if (resp) http_resp_free(resp);
    }
    http_free(h);
    return alive;
}

/* ── active session count (Python _count_status_active_sessions) ───────── */
/* PoP: ws_count_active_sessions @ hermes_cli/web_server.py:_count_status_active_sessions */
int ws_count_active_sessions(void) {
    char dbpath[1024];
    snprintf(dbpath, sizeof(dbpath), "%s/%s", slermes_home(),
             SLERMES_FILE_STATE_DB);
    if (access(dbpath, R_OK) != 0) return 0;

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(dbpath, &db,
                        SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }

    /* last_active falls back to started_at in the archive view; the sessions
     * table itself carries last_active (and started_at). Mirror the Python
     * compact_rows projection: ended_at IS NULL AND last_active within 300s. */
    const char *sql =
        "SELECT COUNT(*) FROM sessions "
        "WHERE ended_at IS NULL AND "
        "COALESCE(last_active, started_at, 0) >= ?";
    sqlite3_stmt *st = NULL;
    int count = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        double cutoff = (double)time(NULL) - 300.0;
        sqlite3_bind_double(st, 1, cutoff);
        if (sqlite3_step(st) == SQLITE_ROW) {
            count = sqlite3_column_int(st, 0);
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return count;
}

int ws_status_active_sessions(void) {
    return ws_count_active_sessions();
}

/* ── error ring (Python record_error / recent_error_count) ────────────── */
/* PoP: ws_record_error @ hermes_cli/web_server.py:record_error */
void ws_record_error(const char *component, const char *message) {
    dashboard_runtime_t *rt = dashboard_runtime_get();
    pthread_mutex_lock(&rt->lock);
    struct ws_error_entry *e = &rt->errors[rt->error_head];
    snprintf(e->component, sizeof(e->component), "%s", component ? component : "");
    snprintf(e->message, sizeof(e->message), "%s", message ? message : "");
    e->ts = time(NULL);
    rt->error_head = (rt->error_head + 1) % WS_ERROR_RING_MAX;
    if (rt->error_count < WS_ERROR_RING_MAX) rt->error_count++;
    pthread_mutex_unlock(&rt->lock);
}

/* PoP: ws_recent_error_count_all @ hermes_cli/web_server.py:recent_error_count */
int ws_recent_error_count_all(void) {
    dashboard_runtime_t *rt = dashboard_runtime_get();
    pthread_mutex_lock(&rt->lock);
    int n = rt->error_count;
    pthread_mutex_unlock(&rt->lock);
    return n;
}

/* window_seconds <= 0 → count all recorded errors. */
int ws_recent_error_count(int window_seconds) {
    dashboard_runtime_t *rt = dashboard_runtime_get();
    pthread_mutex_lock(&rt->lock);
    time_t now = time(NULL);
    int n = 0;
    for (int i = 0; i < rt->error_count; i++) {
        if (window_seconds > 0 &&
            (now - rt->errors[i].ts) > window_seconds)
            continue;
        n++;
    }
    pthread_mutex_unlock(&rt->lock);
    return n;
}

/* ── dashboard self-test / health (Python _dashboard_selftest_once /
 *     _dashboard_health_middleware) ──────────────────────────────────── */
/* PoP: ws_dashboard_selftest_once @ hermes_cli/web_server.py:_dashboard_selftest_once */
bool ws_dashboard_selftest_once(void) {
    char *body = NULL;
    bool alive = ws_probe_gateway_health(&body);
    if (body) free(body);
    /* No gateway configured → nothing to test, surface is healthy. */
    if (!gateway_health_url()) return true;
    if (!alive) {
        ws_record_error("dashboard", "gateway health probe failed");
        return false;
    }
    return true;
}

/* ── lifecycle (Python _lifespan / _start_desktop_cron_ticker) ────────── */
/* Desktop cron ticker: fire the real cron scheduler periodically. The C
 * scheduler_run_job is the same entry point a gateway tick would use, so a
 * desktop-spawned dashboard backend actually fires cron jobs it creates. */
static void *desktop_cron_ticker_thread(void *arg) {
    int interval = (int)(intptr_t)arg;
    if (interval <= 0) interval = 60;
    while (g_rt.cron_running) {
        /* Fire due jobs via the real cron runtime. Desktop backends have no
         * live agent callback; scheduler_run_job falls back to the per-platform
         * send path for delivery, exactly like a standalone cron tick. */
        json_t *due = cronjobs_get_due_jobs();
        if (due && json_node_is_array(due)) {
            size_t n = json_array_size(due);
            for (size_t i = 0; i < n; i++) {
                json_t *job = json_array_get(due, i);
                char *doc = NULL, *final = NULL, *err = NULL;
                scheduler_run_job(job, NULL, NULL, &doc, &final, &err);
                free(doc); free(final); free(err);
            }
        }
        if (due) json_free(due);
        for (int i = 0; i < interval && g_rt.cron_running; i++)
            sleep(1);
    }
    return NULL;
}

/* PoP: ws_start_desktop_cron_ticker @ hermes_cli/web_server.py:_start_desktop_cron_ticker */
bool ws_start_desktop_cron_ticker(int interval_seconds) {
    if (getenv("HERMES_DESKTOP") == NULL) return false;
    dashboard_runtime_t *rt = dashboard_runtime_get();
    if (rt->cron_running) return false;
    rt->cron_running = 1;
    if (pthread_create(&rt->cron_thread, NULL,
                       desktop_cron_ticker_thread,
                       (void *)(intptr_t)interval_seconds) != 0) {
        rt->cron_running = 0;
        return false;
    }
    return true;
}

void ws_stop_desktop_cron_ticker(void) {
    dashboard_runtime_t *rt = dashboard_runtime_get();
    if (!rt->cron_running) return;
    rt->cron_running = 0;
    pthread_join(rt->cron_thread, NULL);
}

bool ws_desktop_cron_ticker_running(void) {
    return g_rt.cron_running != 0;
}
