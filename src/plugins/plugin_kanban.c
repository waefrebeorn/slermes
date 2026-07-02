/*
 * plugin_kanban.c — Kanban board plugin with notification subscriptions.
 * P133 + GW13: File-backed kanban store with notify_subs and event tracking.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_kanban.c -o plugin_kanban.so
 */


/* PoP: Kanban board plugin */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

/* ================================================================
 *  Plugin metadata
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "kanban-board";
}

const char *plugin_meta_version(void) {
    return "0.3.0";
}

const char *plugin_meta_type(void) {
    return "kanban";
}

const char *plugin_meta_description(void) {
    return "Multi-agent kanban board with notification subscriptions";
}

int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ================================================================
 *  Limits
 * ================================================================ */

#define MAX_BOARDS          8
#define MAX_TASKS_PER_BOARD 256
#define MAX_SUBS_PER_BOARD  64
#define MAX_EVENTS_PER_BOARD 512
#define MAX_EVENT_KINDS     16

/* Notification event kinds we deliver */
static const char *TERMINAL_KINDS[] = {
    "completed", "blocked", "gave_up", "crashed", "timed_out", NULL
};

/* ================================================================
 *  Data structures
 * ================================================================ */

/* A notification subscription: maps a task to a gateway source */
typedef struct {
    char  task_id[128];
    char  platform[64];
    char  chat_id[128];
    char  thread_id[128];
    char  user_id[128];
    char  notifier_profile[128];
    int64_t last_event_id;
    int64_t created_at;
    int     fail_count;         /* consecutive send failures */
    bool    active;
} kanban_notify_sub_t;

/* A task event */
typedef struct {
    int64_t id;
    char    task_id[128];
    char    kind[64];
    char    payload[4096];
    int64_t created_at;
    int64_t run_id;
} kanban_event_t;

/* A single task */
typedef struct {
    char    id[128];
    char    description[2048];
    char    column[32];
    int     priority;
    char    assignee[128];
    char    block_reason[512];
    int64_t created_at;
    int64_t updated_at;
} kanban_task_t;

/* A single board */
typedef struct {
    char    name[128];
    char    slug[64];
    char    config_json[2048];
    int     task_count;
    kanban_task_t tasks[MAX_TASKS_PER_BOARD];
    int     sub_count;
    kanban_notify_sub_t subs[MAX_SUBS_PER_BOARD];
    int     event_count;
    kanban_event_t events[MAX_EVENTS_PER_BOARD];
    int64_t event_cursor;       /* monotonically increasing event ID */
    pthread_mutex_t lock;
} kanban_board_t;

/* Global state */
static kanban_board_t g_boards[MAX_BOARDS];
static int g_board_count = 0;
static int g_task_counter = 0;
static int g_initialized = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_store_path[4096] = {0};

/* ================================================================
 *  Helpers
 * ================================================================ */

static kanban_board_t *find_board(const char *slug) {
    if (!slug || !slug[0]) slug = "default";
    for (int i = 0; i < g_board_count; i++) {
        if (strcmp(g_boards[i].slug, slug) == 0)
            return &g_boards[i];
    }
    return NULL;
}

static kanban_board_t *find_or_create_board(const char *slug) {
    if (!slug || !slug[0]) slug = "default";
    kanban_board_t *b = find_board(slug);
    if (b) return b;
    if (g_board_count >= MAX_BOARDS) return NULL;
    b = &g_boards[g_board_count++];
    memset(b, 0, sizeof(*b));
    snprintf(b->slug, sizeof(b->slug), "%s", slug);
    snprintf(b->name, sizeof(b->name), "%s", slug);
    pthread_mutex_init(&b->lock, NULL);
    return b;
}

static kanban_task_t *find_task(kanban_board_t *b, const char *task_id) {
    if (!b || !task_id) return NULL;
    for (int i = 0; i < b->task_count; i++) {
        if (strcmp(b->tasks[i].id, task_id) == 0)
            return &b->tasks[i];
    }
    return NULL;
}

static kanban_event_t *find_latest_event(kanban_board_t *b, const char *task_id,
                                          const char *kind) {
    if (!b) return NULL;
    kanban_event_t *latest = NULL;
    for (int i = 0; i < b->event_count; i++) {
        if (strcmp(b->events[i].task_id, task_id) == 0) {
            if (!kind || strcmp(b->events[i].kind, kind) == 0) {
                if (!latest || b->events[i].id > latest->id)
                    latest = &b->events[i];
            }
        }
    }
    return latest;
}

static void json_escape(const char *src, char *dst, size_t dst_sz) {
    size_t pos = 0;
    if (!src) { dst[0] = '\0'; return; }
    for (const char *s = src; *s && pos < dst_sz - 2; s++) {
        switch (*s) {
            case '"':  if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = '"'; } break;
            case '\\': if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = '\\'; } break;
            case '\n': if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = 'n'; } break;
            case '\t': if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = 't'; } break;
            default:   dst[pos++] = *s; break;
        }
    }
    dst[pos] = '\0';
}

/* ================================================================
 *  File-backed persistence (JSON file per board)
 * ================================================================ */

static void ensure_dir(const char *path) {
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dir, 0755);
    }
}

static bool store_save_board(const kanban_board_t *b) {
    if (!g_store_path[0]) return false;

    char path[4096];
    snprintf(path, sizeof(path), "%s/boards/%s.json", g_store_path, b->slug);
    ensure_dir(path);

    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"slug\": \"%s\",\n", b->slug);
    fprintf(f, "  \"name\": \"%s\",\n", b->name);
    fprintf(f, "  \"event_cursor\": %lld,\n", (long long)b->event_cursor);
    fprintf(f, "  \"tasks\": [\n");

    char esc[4096];
    for (int i = 0; i < b->task_count; i++) {
        const kanban_task_t *t = &b->tasks[i];
        json_escape(t->id, esc, sizeof(esc));
        fprintf(f, "    {\"id\":\"%s\"", esc);
        json_escape(t->description, esc, sizeof(esc));
        fprintf(f, ",\"description\":\"%s\"", esc);
        fprintf(f, ",\"column\":\"%s\"", t->column);
        fprintf(f, ",\"priority\":%d", t->priority);
        json_escape(t->assignee, esc, sizeof(esc));
        fprintf(f, ",\"assignee\":\"%s\"", esc);
        fprintf(f, ",\"created_at\":%lld", (long long)t->created_at);
        fprintf(f, ",\"updated_at\":%lld", (long long)t->updated_at);
        fprintf(f, "}%s\n", (i + 1 < b->task_count) ? "," : "");
    }

    fprintf(f, "  ],\n");
    fprintf(f, "  \"events\": [\n");
    for (int i = 0; i < b->event_count; i++) {
        const kanban_event_t *e = &b->events[i];
        fprintf(f, "    {\"id\":%lld,\"task_id\":\"%s\",\"kind\":\"%s\"",
                (long long)e->id, e->task_id, e->kind);
        json_escape(e->payload, esc, sizeof(esc));
        fprintf(f, ",\"payload\":\"%s\"", esc);
        fprintf(f, ",\"created_at\":%lld", (long long)e->created_at);
        fprintf(f, "}%s\n", (i + 1 < b->event_count) ? "," : "");
    }

    fprintf(f, "  ],\n");
    fprintf(f, "  \"notify_subs\": [\n");
    for (int i = 0; i < b->sub_count; i++) {
        const kanban_notify_sub_t *s = &b->subs[i];
        fprintf(f, "    {\"task_id\":\"%s\",\"platform\":\"%s\",\"chat_id\":\"%s\"",
                s->task_id, s->platform, s->chat_id);
        fprintf(f, ",\"thread_id\":\"%s\"", s->thread_id);
        fprintf(f, ",\"user_id\":\"%s\"", s->user_id);
        fprintf(f, ",\"notifier_profile\":\"%s\"", s->notifier_profile);
        fprintf(f, ",\"last_event_id\":%lld", (long long)s->last_event_id);
        fprintf(f, ",\"created_at\":%lld", (long long)s->created_at);
        fprintf(f, ",\"active\":%s}", s->active ? "true" : "false");
        fprintf(f, "%s\n", (i + 1 < b->sub_count) ? "," : "");
    }
    fprintf(f, "  ]\n}\n");

    fclose(f);
    return true;
}

/* Save all boards */
static bool store_save_all(void) {
    bool ok = true;
    for (int i = 0; i < g_board_count; i++) {
        pthread_mutex_lock(&g_boards[i].lock);
        if (!store_save_board(&g_boards[i])) ok = false;
        pthread_mutex_unlock(&g_boards[i].lock);
    }
    return ok;
}

/* ================================================================
 *  Event tracking
 * ================================================================ */

static int64_t board_next_event_id(kanban_board_t *b) {
    b->event_cursor++;
    return b->event_cursor;
}

static bool board_add_event(kanban_board_t *b, const char *task_id,
                             const char *kind, const char *payload) {
    if (b->event_count >= MAX_EVENTS_PER_BOARD) {
        /* Overwrite oldest event (simple ring) */
        memmove(&b->events[0], &b->events[1],
                sizeof(kanban_event_t) * (b->event_count - 1));
        b->event_count--;
    }
    kanban_event_t *e = &b->events[b->event_count++];
    memset(e, 0, sizeof(*e));
    e->id = board_next_event_id(b);
    snprintf(e->task_id, sizeof(e->task_id), "%s", task_id);
    snprintf(e->kind, sizeof(e->kind), "%s", kind);
    if (payload) snprintf(e->payload, sizeof(e->payload), "%s", payload);
    e->created_at = time(NULL);
    return true;
}

/* Check if event kind is terminal */
static bool is_terminal_kind(const char *kind) {
    for (int i = 0; TERMINAL_KINDS[i]; i++) {
        if (strcmp(kind, TERMINAL_KINDS[i]) == 0) return true;
    }
    return false;
}

/* Check if task is in a terminal state */
static bool task_is_terminal(kanban_board_t *b, const char *task_id) {
    kanban_task_t *t = find_task(b, task_id);
    if (!t) return false;
    return (strcmp(t->column, "done") == 0 ||
            strcmp(t->column, "archived") == 0);
}

/* ================================================================
 *  Notification subscription CRUD
 * ================================================================ */

static kanban_notify_sub_t *find_sub(kanban_board_t *b,
                                      const char *task_id,
                                      const char *platform,
                                      const char *chat_id,
                                      const char *thread_id) {
    if (!thread_id) thread_id = "";
    for (int i = 0; i < b->sub_count; i++) {
        kanban_notify_sub_t *s = &b->subs[i];
        if (strcmp(s->task_id, task_id) == 0 &&
            strcmp(s->platform, platform) == 0 &&
            strcmp(s->chat_id, chat_id) == 0 &&
            strcmp(s->thread_id, thread_id) == 0 &&
            s->active) {
            return s;
        }
    }
    return NULL;
}

/*
 * kanban_notify_subscribe — Register a notification subscription.
 * Returns JSON: {"status":"ok","task_id":"...","platform":"...",...}
 */
static char *kanban_notify_subscribe(const char *board_slug,
                                      const char *task_id,
                                      const char *platform,
                                      const char *chat_id,
                                      const char *thread_id,
                                      const char *user_id,
                                      const char *notifier_profile) {
    kanban_board_t *b = find_or_create_board(board_slug);
    if (!b) return strdup("{\"error\":\"no board slot\"}");

    pthread_mutex_lock(&b->lock);

    /* Check if task exists */
    kanban_task_t *t = find_task(b, task_id);
    if (!t) {
        pthread_mutex_unlock(&b->lock);
        return strdup("{\"error\":\"task not found\"}");
    }

    if (!thread_id) thread_id = "";

    /* Idempotent: update existing or create new */
    kanban_notify_sub_t *s = find_sub(b, task_id, platform, chat_id, thread_id);
    if (s) {
        /* Update profile if provided */
        if (notifier_profile && notifier_profile[0]) {
            snprintf(s->notifier_profile, sizeof(s->notifier_profile),
                     "%s", notifier_profile);
        }
        pthread_mutex_unlock(&b->lock);
        store_save_board(b);

        char result[1024];
        snprintf(result, sizeof(result),
                 "{\"status\":\"ok\",\"task_id\":\"%s\",\"platform\":\"%s\","
                 "\"chat_id\":\"%s\",\"thread_id\":\"%s\","
                 "\"notifier_profile\":\"%s\",\"updated\":true}",
                 task_id, platform, chat_id, thread_id,
                 s->notifier_profile);
        return strdup(result);
    }

    if (b->sub_count >= MAX_SUBS_PER_BOARD) {
        pthread_mutex_unlock(&b->lock);
        return strdup("{\"error\":\"max subscriptions reached\"}");
    }

    s = &b->subs[b->sub_count++];
    memset(s, 0, sizeof(*s));
    snprintf(s->task_id, sizeof(s->task_id), "%s", task_id);
    snprintf(s->platform, sizeof(s->platform), "%s", platform);
    snprintf(s->chat_id, sizeof(s->chat_id), "%s", chat_id);
    snprintf(s->thread_id, sizeof(s->thread_id), "%s", thread_id);
    if (user_id) snprintf(s->user_id, sizeof(s->user_id), "%s", user_id);
    if (notifier_profile) snprintf(s->notifier_profile,
                                    sizeof(s->notifier_profile),
                                    "%s", notifier_profile);
    s->last_event_id = 0;
    s->created_at = time(NULL);
    s->active = true;
    s->fail_count = 0;

    int64_t cur = s->last_event_id;
    pthread_mutex_unlock(&b->lock);
    store_save_board(b);

    char result[1024];
    snprintf(result, sizeof(result),
             "{\"status\":\"ok\",\"task_id\":\"%s\",\"platform\":\"%s\","
             "\"chat_id\":\"%s\",\"thread_id\":\"%s\","
             "\"last_event_id\":%lld}",
             task_id, platform, chat_id, thread_id, (long long)cur);
    return strdup(result);
}

/*
 * kanban_notify_list — List notification subscriptions for a board.
 * Optional task_id filter.
 */
static char *kanban_notify_list(const char *board_slug,
                                 const char *task_id_filter) {
    kanban_board_t *b = find_board(board_slug ? board_slug : "default");
    if (!b) return strdup("{\"subs\":[]}");

    pthread_mutex_lock(&b->lock);

    /* Calculate required buffer size */
    size_t buf_sz = 1024 + (size_t)b->sub_count * 512;
    char *result = malloc(buf_sz);
    if (!result) { pthread_mutex_unlock(&b->lock); return strdup("{\"error\":\"oom\"}"); }

    size_t pos = 0;
    pos += snprintf(result + pos, buf_sz - pos, "{\"subs\":[");

    int printed = 0;
    for (int i = 0; i < b->sub_count; i++) {
        kanban_notify_sub_t *s = &b->subs[i];
        if (!s->active) continue;
        if (task_id_filter && task_id_filter[0] &&
            strcmp(s->task_id, task_id_filter) != 0) continue;

        if (printed++ > 0) result[pos++] = ',';
        pos += snprintf(result + pos, buf_sz - pos,
            "{\"task_id\":\"%s\",\"platform\":\"%s\",\"chat_id\":\"%s\","
            "\"thread_id\":\"%s\",\"user_id\":\"%s\","
            "\"notifier_profile\":\"%s\",\"last_event_id\":%lld,"
            "\"created_at\":%lld,\"active\":%s}",
            s->task_id, s->platform, s->chat_id,
            s->thread_id, s->user_id,
            s->notifier_profile, (long long)s->last_event_id,
            (long long)s->created_at,
            s->active ? "true" : "false");
    }

    pos += snprintf(result + pos, buf_sz - pos, "]}");
    pthread_mutex_unlock(&b->lock);
    return result;
}

/*
 * kanban_notify_unsubscribe — Remove a notification subscription.
 */
static char *kanban_notify_unsubscribe(const char *board_slug,
                                        const char *task_id,
                                        const char *platform,
                                        const char *chat_id,
                                        const char *thread_id) {
    if (!thread_id) thread_id = "";

    kanban_board_t *b = find_board(board_slug ? board_slug : "default");
    if (!b) return strdup("{\"error\":\"board not found\"}");

    pthread_mutex_lock(&b->lock);

    kanban_notify_sub_t *s = find_sub(b, task_id, platform, chat_id, thread_id);
    if (!s) {
        pthread_mutex_unlock(&b->lock);
        return strdup("{\"error\":\"subscription not found\"}");
    }

    s->active = false;

    /* Compact: remove inactive subs from the array */
    int write = 0;
    for (int i = 0; i < b->sub_count; i++) {
        if (b->subs[i].active) {
            if (write != i) b->subs[write] = b->subs[i];
            write++;
        }
    }
    b->sub_count = write;

    pthread_mutex_unlock(&b->lock);
    store_save_board(b);

    char result[256];
    snprintf(result, sizeof(result),
             "{\"status\":\"ok\",\"task_id\":\"%s\",\"unsubscribed\":true}",
             task_id);
    return strdup(result);
}

/*
 * kanban_notify_get_pending — Collect pending notification events
 * for all active subscriptions. This is the gateway notifier's poll function.
 *
 * Returns JSON array of delivery objects:
 *   [{"sub":{...},"events":[{...}],"new_cursor":N}]
 *
 * Each subscription with events newer than last_event_id is included.
 * Only terminal event kinds are included.
 */
static char *kanban_notify_get_pending(const char *notifier_profile) {
    size_t buf_sz = 65536;
    char *result = malloc(buf_sz);
    if (!result) return strdup("{\"deliveries\":[]}");
    size_t pos = 0;
    pos += snprintf(result + pos, buf_sz - pos, "{\"deliveries\":[");

    int total_deliveries = 0;

    pthread_mutex_lock(&g_lock);
    for (int bi = 0; bi < g_board_count; bi++) {
        kanban_board_t *b = &g_boards[bi];
        pthread_mutex_lock(&b->lock);

        for (int si = 0; si < b->sub_count; si++) {
            kanban_notify_sub_t *sub = &b->subs[si];
            if (!sub->active) continue;

            /* Profile ownership check */
            if (sub->notifier_profile[0] &&
                notifier_profile &&
                strcmp(sub->notifier_profile, notifier_profile) != 0) {
                continue;   /* owned by a different profile */
            }

            /* Collect unseen terminal events for this task */
            int64_t max_id = sub->last_event_id;
            int event_count = 0;
            kanban_event_t *event_ptrs[MAX_EVENTS_PER_BOARD];

            for (int ei = 0; ei < b->event_count; ei++) {
                kanban_event_t *e = &b->events[ei];
                if (e->id <= sub->last_event_id) continue;
                if (strcmp(e->task_id, sub->task_id) != 0) continue;
                if (!is_terminal_kind(e->kind)) continue;
                event_ptrs[event_count++] = e;
                if (e->id > max_id) max_id = e->id;
            }

            if (event_count == 0) continue;

            /* Build delivery entry */
            if (total_deliveries++ > 0) result[pos++] = ',';

            pos += snprintf(result + pos, buf_sz - pos,
                "{\"sub\":{\"task_id\":\"%s\",\"platform\":\"%s\","
                "\"chat_id\":\"%s\",\"thread_id\":\"%s\","
                "\"notifier_profile\":\"%s\"},"
                "\"events\":[",
                sub->task_id, sub->platform, sub->chat_id,
                sub->thread_id, sub->notifier_profile);

            for (int ei = 0; ei < event_count; ei++) {
                kanban_event_t *e = event_ptrs[ei];
                if (ei > 0) result[pos++] = ',';
                char esc_payload[8192];
                json_escape(e->payload, esc_payload, sizeof(esc_payload));
                pos += snprintf(result + pos, buf_sz - pos,
                    "{\"id\":%lld,\"kind\":\"%s\",\"payload\":\"%s\","
                    "\"created_at\":%lld}",
                    (long long)e->id, e->kind, esc_payload,
                    (long long)e->created_at);
            }

            pos += snprintf(result + pos, buf_sz - pos, "],");
            pos += snprintf(result + pos, buf_sz - pos,
                            "\"new_cursor\":%lld}", (long long)max_id);
        }

        pthread_mutex_unlock(&b->lock);
    }
    pthread_mutex_unlock(&g_lock);

    pos += snprintf(result + pos, buf_sz - pos, "]}");

    if (pos >= buf_sz - 1) {
        result[buf_sz - 2] = '}';
        result[buf_sz - 1] = '\0';
    }

    return result;
}

/*
 * kanban_notify_advance_cursor — Advance subscription cursor after
 * successful delivery. Called by gateway notifier.
 */
static char *kanban_notify_advance_cursor(const char *board_slug,
                                           const char *task_id,
                                           const char *platform,
                                           const char *chat_id,
                                           const char *thread_id,
                                           int64_t new_cursor) {
    kanban_board_t *b = find_board(board_slug ? board_slug : "default");
    if (!b) return strdup("{\"error\":\"board not found\"}");

    pthread_mutex_lock(&b->lock);

    kanban_notify_sub_t *s = find_sub(b, task_id, platform, chat_id, thread_id);
    if (!s) {
        pthread_mutex_unlock(&b->lock);
        return strdup("{\"error\":\"subscription not found\"}");
    }

    s->last_event_id = new_cursor;
    s->fail_count = 0;  /* reset on success */

    /* Auto-unsubscribe if task is terminal */
    if (task_is_terminal(b, task_id)) {
        s->active = false;
    }

    pthread_mutex_unlock(&b->lock);
    store_save_board(b);

    return strdup("{\"status\":\"ok\"}");
}

/*
 * kanban_notify_record_event — Record a task event (called by gateway
 * when task status changes).
 */
static char *kanban_notify_record_event(const char *board_slug,
                                         const char *task_id,
                                         const char *kind,
                                         const char *payload) {
    kanban_board_t *b = find_or_create_board(board_slug);
    if (!b) return strdup("{\"error\":\"no board slot\"}");

    pthread_mutex_lock(&b->lock);
    board_add_event(b, task_id, kind, payload ? payload : "{}");
    int64_t eid = b->event_cursor;
    pthread_mutex_unlock(&b->lock);
    store_save_board(b);

    char result[256];
    snprintf(result, sizeof(result),
             "{\"status\":\"ok\",\"event_id\":%lld}", (long long)eid);
    return strdup(result);
}

/* ================================================================
 *  Board CRUD (improved from original in-memory version)
 * ================================================================ */

static char *kanban_create_board(const char *name, const char *config_json) {
    if (!name || !name[0])
        return strdup("{\"error\":\"board name required\"}");

    /* Derive slug from name */
    char slug[64];
    snprintf(slug, sizeof(slug), "%s", name);
    for (int i = 0; slug[i]; i++) {
        if (slug[i] == ' ') slug[i] = '_';
    }

    pthread_mutex_lock(&g_lock);
    kanban_board_t *existing = find_board(slug);
    if (existing) {
        pthread_mutex_unlock(&g_lock);
        char result[256];
        snprintf(result, sizeof(result),
                 "{\"status\":\"ok\",\"board\":\"%s\",\"existed\":true}", slug);
        return strdup(result);
    }

    kanban_board_t *b = find_or_create_board(slug);
    if (!b) {
        pthread_mutex_unlock(&g_lock);
        return strdup("{\"error\":\"max boards reached\"}");
    }

    snprintf(b->name, sizeof(b->name), "%s", name);
    if (config_json)
        snprintf(b->config_json, sizeof(b->config_json), "%s", config_json);
    pthread_mutex_unlock(&g_lock);

    store_save_board(b);

    char result[512];
    snprintf(result, sizeof(result),
             "{\"status\":\"ok\",\"board\":\"%s\",\"name\":\"%s\"}", slug, name);
    return strdup(result);
}

static void gen_task_id(char *buf, size_t sz, int board_idx) {
    snprintf(buf, sz, "b%d-t%d-%ld",
             board_idx, g_task_counter++, (long)time(NULL));
}

static char *kanban_add_task(const char *board_id, const char *task_json) {
    kanban_board_t *b = find_or_create_board(board_id);
    if (!b) return strdup("{\"error\":\"no board slot\"}");

    pthread_mutex_lock(&b->lock);

    if (b->task_count >= MAX_TASKS_PER_BOARD) {
        pthread_mutex_unlock(&b->lock);
        return strdup("{\"error\":\"board full\"}");
    }

    kanban_task_t *t = &b->tasks[b->task_count];

    /* Generate ID */
    int board_idx = (int)(b - g_boards);
    gen_task_id(t->id, sizeof(t->id), board_idx);

    /* Parse task_json */
    snprintf(t->description, sizeof(t->description), "untitled task");
    snprintf(t->column, sizeof(t->column), "todo");
    t->priority = 3;

    if (task_json && task_json[0]) {
        /* Extract description */
        const char *desc_start = strstr(task_json, "\"description\"");
        if (desc_start) {
            desc_start = strchr(desc_start + 13, '"');
            if (desc_start) {
                desc_start++;
                const char *desc_end = strchr(desc_start, '"');
                if (desc_end) {
                    size_t dlen = (size_t)(desc_end - desc_start);
                    if (dlen >= sizeof(t->description)) dlen = sizeof(t->description) - 1;
                    memcpy(t->description, desc_start, dlen);
                    t->description[dlen] = '\0';
                }
            }
        }

        /* Extract column */
        const char *col_start = strstr(task_json, "\"column\"");
        if (col_start) {
            col_start = strchr(col_start + 8, '"');
            if (col_start) {
                col_start++;
                const char *col_end = strchr(col_start, '"');
                if (col_end) {
                    size_t clen = (size_t)(col_end - col_start);
                    if (clen >= sizeof(t->column)) clen = sizeof(t->column) - 1;
                    memcpy(t->column, col_start, clen);
                    t->column[clen] = '\0';
                }
            }
        }

        /* Extract priority */
        const char *prio_start = strstr(task_json, "\"priority\"");
        if (prio_start) {
            prio_start = strchr(prio_start + 10, ':');
            if (prio_start) {
                t->priority = atoi(prio_start + 1);
                if (t->priority < 1) t->priority = 3;
                if (t->priority > 5) t->priority = 5;
            }
        }

        /* Extract assignee */
        const char *as_start = strstr(task_json, "\"assignee\"");
        if (as_start) {
            as_start = strchr(as_start + 10, '"');
            if (as_start) {
                as_start++;
                const char *as_end = strchr(as_start, '"');
                if (as_end) {
                    size_t alen = (size_t)(as_end - as_start);
                    if (alen >= sizeof(t->assignee)) alen = sizeof(t->assignee) - 1;
                    memcpy(t->assignee, as_start, alen);
                    t->assignee[alen] = '\0';
                }
            }
        }
    }

    t->created_at = time(NULL);
    t->updated_at = t->created_at;
    b->task_count++;

    /* Record task_created event */
    board_add_event(b, t->id, "created", "{}");

    char desc_esc[4096];
    json_escape(t->description, desc_esc, sizeof(desc_esc));

    char result[1024];
    snprintf(result, sizeof(result),
             "{\"status\":\"ok\",\"task_id\":\"%s\",\"board\":\"%s\","
             "\"column\":\"%s\",\"priority\":%d}",
             t->id, b->slug, t->column, t->priority);

    pthread_mutex_unlock(&b->lock);
    store_save_board(b);
    return strdup(result);
}

static char *kanban_get_board(const char *board_id) {
    kanban_board_t *b = find_board(board_id ? board_id : "default");
    if (!b) return strdup("{\"error\":\"board not found\"}");

    pthread_mutex_lock(&b->lock);

    size_t buf_sz = 4096 + (size_t)b->task_count * 1024;
    char *result = malloc(buf_sz);
    if (!result) { pthread_mutex_unlock(&b->lock); return strdup("{\"error\":\"oom\"}"); }

    size_t pos = 0;
    pos += snprintf(result + pos, buf_sz - pos,
        "{\"board\":{\"slug\":\"%s\",\"name\":\"%s\","
        "\"task_count\":%d,\"event_cursor\":%lld,\"tasks\":[",
        b->slug, b->name, b->task_count, (long long)b->event_cursor);

    char desc_esc[4096];
    for (int i = 0; i < b->task_count; i++) {
        kanban_task_t *t = &b->tasks[i];
        if (i > 0) result[pos++] = ',';
        json_escape(t->description, desc_esc, sizeof(desc_esc));
        pos += snprintf(result + pos, buf_sz - pos,
            "{\"id\":\"%s\",\"description\":\"%s\",\"column\":\"%s\","
            "\"priority\":%d,\"assignee\":\"%s\","
            "\"created_at\":%lld,\"updated_at\":%lld}",
            t->id, desc_esc, t->column,
            t->priority, t->assignee,
            (long long)t->created_at, (long long)t->updated_at);
    }

    pos += snprintf(result + pos, buf_sz - pos,
        "],\"subs\":[%s", b->sub_count > 0 ? "" : "]");

    for (int i = 0; i < b->sub_count; i++) {
        kanban_notify_sub_t *s = &b->subs[i];
        if (i > 0) result[pos++] = ',';
        pos += snprintf(result + pos, buf_sz - pos,
            "{\"task_id\":\"%s\",\"platform\":\"%s\",\"chat_id\":\"%s\","
            "\"thread_id\":\"%s\",\"notifier_profile\":\"%s\","
            "\"last_event_id\":%lld,\"active\":%s}",
            s->task_id, s->platform, s->chat_id,
            s->thread_id, s->notifier_profile,
            (long long)s->last_event_id,
            s->active ? "true" : "false");
    }

    pos += snprintf(result + pos, buf_sz - pos, "]}}");
    pthread_mutex_unlock(&b->lock);
    return result;
}

static char *kanban_list_boards(void) {
    size_t buf_sz = 2048 + (size_t)g_board_count * 512;
    char *result = malloc(buf_sz);
    if (!result) return strdup("{\"boards\":[]}");
    size_t pos = 0;
    pos += snprintf(result + pos, buf_sz - pos, "{\"count\":%d,\"boards\":[", g_board_count);

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_board_count; i++) {
        if (i > 0) result[pos++] = ',';
        pthread_mutex_lock(&g_boards[i].lock);
        pos += snprintf(result + pos, buf_sz - pos,
            "{\"slug\":\"%s\",\"name\":\"%s\","
            "\"task_count\":%d,\"sub_count\":%d,"
            "\"event_cursor\":%lld}",
            g_boards[i].slug, g_boards[i].name,
            g_boards[i].task_count, g_boards[i].sub_count,
            (long long)g_boards[i].event_cursor);
        pthread_mutex_unlock(&g_boards[i].lock);
    }
    pthread_mutex_unlock(&g_lock);

    pos += snprintf(result + pos, buf_sz - pos, "]}");
    return result;
}

/*
 * kanban_update_task_column — Move task to a different column.
 * This also records an event for the transition.
 */
static char *kanban_update_task_column(const char *board_slug,
                                        const char *task_id,
                                        const char *new_column) {
    kanban_board_t *b = find_board(board_slug ? board_slug : "default");
    if (!b) return strdup("{\"error\":\"board not found\"}");

    pthread_mutex_lock(&b->lock);

    kanban_task_t *t = find_task(b, task_id);
    if (!t) {
        pthread_mutex_unlock(&b->lock);
        return strdup("{\"error\":\"task not found\"}");
    }

    snprintf(t->column, sizeof(t->column), "%s", new_column);
    t->updated_at = time(NULL);

    /* Record event */
    char payload[256];
    snprintf(payload, sizeof(payload), "{\"column\":\"%s\"}", new_column);
    char kind[64];
    if (strcmp(new_column, "done") == 0) {
        snprintf(kind, sizeof(kind), "completed");
    } else if (strcmp(new_column, "blocked") == 0) {
        snprintf(kind, sizeof(kind), "blocked");
    } else {
        snprintf(kind, sizeof(kind), "transition");
    }
    board_add_event(b, task_id, kind, payload);

    pthread_mutex_unlock(&b->lock);
    store_save_board(b);

    char result[256];
    snprintf(result, sizeof(result),
             "{\"status\":\"ok\",\"task_id\":\"%s\",\"column\":\"%s\"}",
             task_id, new_column);
    return strdup(result);
}

/* ================================================================
 *  Plugin interface
 * ================================================================ */

static plugin_interface_t interface = {
    .type                = PLUGIN_KANBAN,
    .kanban_create_board = kanban_create_board,
    .kanban_add_task     = kanban_add_task,
    .kanban_get_board    = kanban_get_board,
    .kanban_list_boards  = kanban_list_boards,
};

void *plugin_get_interface(void) {
    return &interface;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

int plugin_init(void) {
    if (g_initialized) return 0;

    /* Determine store path from config or environment */
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    snprintf(g_store_path, sizeof(g_store_path), "%s/.hermes/kanban", home);

    /* Ensure store directory exists */
    char dir_path[4096];
    snprintf(dir_path, sizeof(dir_path), "%s/boards", g_store_path);
    ensure_dir(dir_path);

    pthread_mutex_lock(&g_lock);
    g_board_count = 0;
    g_task_counter = 0;
    memset(g_boards, 0, sizeof(g_boards));
    for (int i = 0; i < MAX_BOARDS; i++) {
        pthread_mutex_init(&g_boards[i].lock, NULL);
    }
    g_initialized = 1;
    pthread_mutex_unlock(&g_lock);

    fprintf(stderr, "[kanban-board] initialized (store: %s, %d boards x %d tasks)\n",
            g_store_path, MAX_BOARDS, MAX_TASKS_PER_BOARD);
    return 0;
}

int plugin_cleanup(void) {
    fprintf(stderr, "[kanban-board] flushing and shutting down...\n");
    store_save_all();
    pthread_mutex_lock(&g_lock);
    g_board_count = 0;
    g_initialized = 0;
    pthread_mutex_unlock(&g_lock);
    fprintf(stderr, "[kanban-board] shut down.\n");
    return 0;
}

int plugin_configure(const char *config_json) {
    fprintf(stderr, "[kanban-board] configure: %s\n", config_json);
    /* Apply config: store path override, board defaults, etc. */
    (void)config_json;
    return 0;
}
