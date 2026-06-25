/*
 * desktop_gui.c — Slermes Desktop GUI
 *
 * C11 SDL2 desktop application for Slermes Agent.
 * Uses SDL2 as a thin platform layer only — all widgets,
 * themes, and drawing are custom (gui_core.h/c).
 *
 * Features:
 * ✓ Scrollable sidebar + chat with themed scrollbars
 * ✓ Session age metadata (2h, 3d, 1w)
 * ✓ Disclosure carets on section headers
 * ✓ Composer model pill (shows current model)
 * ✓ Full hover coverage on ALL interactive elements
 * ✓ Proper session title truncation
 * ✓ Collapsible nav with hover/selection states
 * ✓ Profile section at sidebar bottom with connection state
 * ✓ Statusbar with real stats
 * ✓ Date separators for messages
 * ✓ Scroll reset on session change
 * ✓ Scroll wheel support (mouse + keyboard)
 * ✓ Theme toggle (t key)
 * ✓ Sidebar search (/ key, type to filter)
 * ✓ +New Chat functional
 * ✓ Disclosure section headers (clickable)
 * ✓ Message Copy/Edit actions on hover
 * ✓ Code block rendering with language label
 *
 * MIT License — Slermes Fork
 */
#define _GNU_SOURCE
#include "gui_core.h"
#include "chat_render.h"
#include "slermes_home.h"
#include "sqlite3.h"
#include "libhttp/http.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

/* ── Layout constants ──────────────────────────────────────────────── */
#define TITLEBAR_H       34
#define STATUSBAR_H      20
#define SIDEBAR_W        237
#define ITEM_H           30
#define SEARCH_H         28
#define SECTION_H        16
#define PADDING          16
#define BUBBLE_GAP       6
#define TOOL_SIZE        20
#define TOOL_GAP         4
#define CONTACT_X        10
#define BUBBLE_RAD       6
#define COMPOSER_H       44
#define PILL_H           20
#define MAX_SESSIONS     200
#define MAX_MESSAGES     500
#define MAX_ITEMS        200
#define SCROLLBAR_W      6
#define SCROLL_MUL       30
#define HIT_NONE         -1
#define HIT_NAV          0
#define HIT_SESSION      1
#define HIT_TITLEBAR     2
#define HIT_STATUSBAR    3
#define HIT_CHAT         4
#define HIT_TOOL         5
#define HIT_COMPOSER     6
#define HIT_NEWCHAT      7
#define HIT_PILL         8
#define HIT_PROFILE      9
#define HIT_SEARCH       10
#define HIT_SESSIONS_HDR 11
#define HIT_NAV_HDR      12
#define HIT_MESSAGE_COPY 13
#define HIT_MESSAGE_EDIT 14
#define HIT_SCROLL_BOTTOM 15
#define HIT_SESSION_HDR  16
#define HIT_SESSION_PIN  17
#define HIT_SESSION_DEL  18
#define HIT_SESSION_ARCH 19
#define HIT_LOAD_MORE    20
#define HIT_MODEL_PICKER 21

/* ── Data structures ───────────────────────────────────────────────── */
typedef struct {
    char id[64];
    char title[256];
    char source[32];
    char model[128];
    int  msg_count;
    int  tokens;
    long started_at;       /* unix epoch seconds */
} session_entry_t;

typedef struct {
    char role[32];
    char content[65536];
    long timestamp;        /* unix ms */
} message_entry_t;

typedef struct {
    const char *icon;
    const char *label;
    int view_id;
} nav_item_t;

static nav_item_t nav_items[] = {
    {"\xe2\x97\x8b", "Chat",        0},
    {"\xe2\x96\xb6", "Cmd Center",  1},
    {"\xe2\x9c\xa6", "Skills",      2},
    {"\xe2\x9d\x90", "Artifacts",   3},
    {"\xe2\x8c\x9a", "Cron",        4},
    {"\xe2\x99\xa0", "Profiles",    5},
    {"\xe2\x99\x9f", "Agents",      6},
    {"\xe2\x87\x84", "Messaging",   7},
    {"\xf0\x9f\x93\x81", "Files",   8},
    {"\xf0\x9f\x93\xab", "Snippets", 9},
    {NULL, NULL, 0}
};

static const char *model_list[] = {
    "deepseek/deepseek-v4-flash",
    "deepseek/deepseek-r1-distill-llama-70b",
    "nousresearch/hermes-3-405b",
    "meta-llama/llama-3.3-70b-instruct",
    "mistralai/mistral-large-2407",
    NULL
};

static const char *tool_chars[] = {
    "\xf0\x9f\x94\x8a", "\xe2\x8c\xa8", "\xe2\x9a\x99", "\xe2\x96\xa0",
};

typedef struct {
    sqlite3    *db;
    char        state_db_path[512];
    char        latest_model[128];
    bool        running;

    session_entry_t sessions[MAX_SESSIONS];
    int             session_count;
    int             selected_session;

    message_entry_t messages[MAX_MESSAGES];
    int             message_count;

    int  selected_nav;
    int  current_view;
    char current_view_name[64];

    char skill_names[MAX_ITEMS][128];
    int  skill_count;
    char profile_names[MAX_ITEMS][64];
    int  profile_count;
    char cron_names[MAX_ITEMS][128];
    int  cron_count;

    /* Hover for every interactive element */
    int hover_nav, hover_session, hover_tool;
    int hover_newchat, hover_pill, hover_profile, hover_search;
    bool mouse_in_sidebar, mouse_in_chat;
    bool mouse_in_titlebar, mouse_in_statusbar;
    bool composer_hover;

    /* Bubble position tracking for hover-detection */
    int bubble_y[MAX_MESSAGES], bubble_h[MAX_MESSAGES];
    int hover_message;
    int hover_action; /* 0=copy, 1=edit, -1=none */

    int  sidebar_scroll, sidebar_content_h;
    int  chat_scroll, chat_content_h;

    int  total_sessions;
    int  total_messages;

    /* Section expand state */
    bool sessions_expanded;
    bool nav_expanded;

    /* Sidebar search */
    char search_query[64];
    int  search_query_len;
    bool search_active;

    /* Theme */
    bool dark_mode;

    /* Sidebar collapsed state (titlebar toggle) */
    bool sidebar_collapsed;

    /* Haptics muted (titlebar toggle) */
    bool haptics_muted;

    gc_window_t *win;
    gc_theme_t   theme;

    /* Composer state */
    char        composer_buf[4096];
    int         composer_pos;
    bool        composer_focused;
    bool        api_busy;
    char        api_status[128];

    /* Session pinning */
    int  pinned_session_ids[MAX_SESSIONS];
    int  pinned_session_count;

    /* Model picker overlay */
    bool        show_model_picker;
    int         model_picker_hover;
    int         model_picker_scroll;

    /* Scroll-to-bottom button */
    bool        show_scroll_button;
    bool        scroll_button_hover;

    /* Session header */
    bool        show_session_menu;
    int         session_menu_hover; /* 0=pin, 1=delete, 2=archive */

    /* Load more */
    bool        sessions_loaded_all;
    int         sessions_page;

    /* Gateway status */
    bool        gateway_connected;
    char        gateway_status_text[64];

    /* Session export/import */
    char        export_path[512];
    bool        show_export_dialog;
    bool        show_import_dialog;
    char        import_path[512];
    int         import_path_len;

    /* Notification toast */
    char        toast_msg[256];
    int         toast_time;

    /* Petdex */
    bool        pet_active;
    int         pet_type;       /* 0=cat, 1=dragon, 2=owl, 3=blob */
    int         pet_frame;      /* animation frame */
    int         pet_frame_tick; /* frame timer */
    float       pet_x, pet_y;   /* position */
    float       pet_vx, pet_vy;  /* velocity */
    bool        pet_show_gallery;
    char        pet_names[16][32]; /* petdex catalog */
    int         pet_count;
    int         pet_selected;
    float       pet_scale;

    /* Voice */
    bool        voice_active;
    bool        voice_recording;
    int         voice_tts_pending;

    /* Command palette */
    bool        show_command_palette;
    char        command_palette_query[128];
    int         command_palette_query_len;
    int         command_palette_selected;
    int         command_palette_result_count;

    /* Image paste overlay */
    bool        image_paste_active;
    char        image_paste_path[512];
    char        image_paste_data[1048576]; /* base64 or raw */
    int         image_paste_data_len;
    bool        image_paste_is_base64;
} app_state_t;

static app_state_t app;

static void load_messages(int idx);
static int db_open(void);

static int win_w(void) { return gc_window_w(app.win); }
static int win_h(void) { return gc_window_h(app.win); }
static int sidebar_w(void) { return app.sidebar_collapsed ? 48 : SIDEBAR_W; }
static int chat_x(void) { return sidebar_w(); }
static int chat_w(void) { return win_w() - sidebar_w(); }
static int chat_y(void) { return TITLEBAR_H; }
static int chat_h(void) { return win_h() - TITLEBAR_H - STATUSBAR_H; }
static int sidebar_h(void) { return win_h() - TITLEBAR_H - STATUSBAR_H; }

/* ── API call: send messages to provider and get response ────────── */
static void api_send_message(void) {
    if (!app.db || app.api_busy || app.composer_buf[0] == '\0') return;
    if (app.session_count == 0) return;

    app.api_busy = true;
    snprintf(app.api_status, sizeof(app.api_status), "Sending...");

    /* Get current session */
    session_entry_t *s = &app.sessions[app.selected_session];

    /* Read API key and config from env */
    const char *api_key = getenv("NOUS_API_KEY");
    if (!api_key) api_key = getenv("OPENAI_API_KEY");
    if (!api_key) {
        snprintf(app.api_status, sizeof(app.api_status), "Error: no API key");
        app.api_busy = false;
        return;
    }

    /* API base URL: env var or default */
    const char *api_base = getenv("SLERMES_API_BASE");
    if (!api_base) api_base = "https://inference-api.nousresearch.com/v1";

    /* Model: from session or env or default */
    const char *model_name = s->model[0] ? s->model : NULL;
    if (!model_name) model_name = getenv("SLERMES_MODEL");
    if (!model_name) model_name = "deepseek/deepseek-v4-flash";

    /* Build messages JSON array */
    char msgs_json[65536] = "";
    int mlen = 0;
    mlen += snprintf(msgs_json + mlen, sizeof(msgs_json) - mlen, "[");
    for (int i = 0; i < app.message_count && mlen < (int)sizeof(msgs_json) - 128; i++) {
        char role_esc[128], content_esc[65536];
        /* Simple JSON escaping */
        const char *rp = app.messages[i].role;
        int ri = 0;
        while (*rp && ri < (int)sizeof(role_esc)-1) {
            if (*rp == '"' || *rp == '\\') role_esc[ri++] = '\\';
            role_esc[ri++] = *rp++;
        }
        role_esc[ri] = '\0';

        const char *cp = app.messages[i].content;
        int ci = 0;
        while (*cp && ci < (int)sizeof(content_esc)-1) {
            if (*cp == '"' || *cp == '\\') content_esc[ci++] = '\\';
            else if (*cp == '\n') { content_esc[ci++] = '\\'; content_esc[ci++] = 'n'; cp++; continue; }
            else if (*cp == '\t') { content_esc[ci++] = '\\'; content_esc[ci++] = 't'; cp++; continue; }
            content_esc[ci++] = *cp++;
        }
        content_esc[ci] = '\0';

        mlen += snprintf(msgs_json + mlen, sizeof(msgs_json) - mlen,
            "%s{\"role\":\"%s\",\"content\":\"%s\"}",
            i > 0 ? "," : "", role_esc, content_esc);
    }
    /* Add user's new message */
    {
        char content_esc[4096];
        const char *cp = app.composer_buf;
        int ci = 0;
        while (*cp && ci < (int)sizeof(content_esc)-1) {
            if (*cp == '"' || *cp == '\\') content_esc[ci++] = '\\';
            else if (*cp == '\n') { content_esc[ci++] = '\\'; content_esc[ci++] = 'n'; cp++; continue; }
            else if (*cp == '\t') { content_esc[ci++] = '\\'; content_esc[ci++] = 't'; cp++; continue; }
            content_esc[ci++] = *cp++;
        }
        content_esc[ci] = '\0';
        mlen += snprintf(msgs_json + mlen, sizeof(msgs_json) - mlen,
            ",{\"role\":\"user\",\"content\":\"%s\"}", content_esc);
    }
    mlen += snprintf(msgs_json + mlen, sizeof(msgs_json) - mlen, "]");

    /* Build full request JSON */
    char req_body[131072];
    snprintf(req_body, sizeof(req_body),
        "{\"model\":\"%s\",\"messages\":%s,\"stream\":false,\"max_tokens\":4096}",
        model_name, msgs_json);

    /* Save user message to DB first */
    double now_t = (double)time(NULL) * 1000;
    char *zErr = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT INTO messages (session_id, role, content, timestamp) VALUES ('%q', 'user', '%q', %f)",
        s->id, app.composer_buf, now_t);
    if (sql) {
        sqlite3_exec(app.db, sql, NULL, NULL, &zErr);
        sqlite3_free(sql);
        if (zErr) { sqlite3_free(zErr); zErr = NULL; }
    }

    /* Clear composer */
    app.composer_buf[0] = '\0';
    app.composer_pos = 0;

    /* Make API call */
    http_t *h = http_new(30);
    if (!h) {
        snprintf(app.api_status, sizeof(app.api_status), "Error: HTTP init failed");
        app.api_busy = false;
        return;
    }

    char auth_hdr[512];
    const char *ak = api_key;
    snprintf(auth_hdr, sizeof(auth_hdr),
        "Content-Type: application/json\r\nAuthorization: Bearer %s", ak);

    char api_url[512];
    snprintf(api_url, sizeof(api_url), "%s/chat/completions", api_base);

    snprintf(app.api_status, sizeof(app.api_status), "Calling API...");
    http_resp_t *resp = http_post_json_auth(h,
        api_url, req_body, auth_hdr);

    if (!resp || resp->status != 200) {
        snprintf(app.api_status, sizeof(app.api_status),
            "API error: %d", resp ? resp->status : -1);
        http_resp_free(resp);
        http_free(h);
        app.api_busy = false;
        return;
    }

    /* Parse response JSON */
    json_t *doc = json_parse(resp->body, NULL);
    char reply[65536] = "";
    if (doc) {
        json_t *choices = json_obj_get(doc, "choices");
        if (choices && choices->type == JSON_ARRAY && choices->c.count > 0) {
            json_t *first = choices->c.items[0];
            json_t *msg_node = json_obj_get(first, "message");
            if (msg_node) {
                const char *content = json_get_str(msg_node, "content", "");
                snprintf(reply, sizeof(reply), "%s", content);
            }
        }
        json_free(doc);
    }

    http_resp_free(resp);
    http_free(h);

    if (reply[0] == '\0') {
        snprintf(app.api_status, sizeof(app.api_status), "Empty response");
    } else {
        snprintf(app.api_status, sizeof(app.api_status), "Response received");
        /* Save assistant message to DB */
        double now_t2 = (double)time(NULL) * 1000;
        char *sql2 = sqlite3_mprintf(
            "INSERT INTO messages (session_id, role, content, timestamp) VALUES ('%q', 'assistant', '%q', %f)",
            s->id, reply, now_t2);
        if (sql2) {
            sqlite3_exec(app.db, sql2, NULL, NULL, &zErr);
            sqlite3_free(sql2);
            if (zErr) { sqlite3_free(zErr); zErr = NULL; }
        }
    }

    app.api_busy = false;
    snprintf(app.api_status, sizeof(app.api_status), "");

    /* Reload messages */
    load_messages(app.selected_session);
}

/* ── Relative age formatting (like Electron: "2h", "3d", "1w") ──── */
static void format_age(long started_at, char *buf, size_t sz) {
    time_t now = time(NULL);
    double delta = difftime(now, (time_t)started_at);
    if (delta < 0) delta = 0;
    if (delta < 60) snprintf(buf, sz, "now");
    else if (delta < 3600) snprintf(buf, sz, "%dm", (int)(delta / 60));
    else if (delta < 86400) snprintf(buf, sz, "%dh", (int)(delta / 3600));
    else if (delta < 604800) snprintf(buf, sz, "%dd", (int)(delta / 86400));
    else if (delta < 2592000) snprintf(buf, sz, "%dw", (int)(delta / 604800));
    else snprintf(buf, sz, "%dmo", (int)(delta / 2592000));
}

/* ── Database ──────────────────────────────────────────────────────── */
static int db_open(void) {
    if (!slermes_initialized()) slermes_init();
    snprintf(app.state_db_path, sizeof(app.state_db_path), "%s/%s",
             slermes_home(), SLERMES_FILE_STATE_DB);
    if (access(app.state_db_path, R_OK) != 0) {
        fprintf(stderr, "state.db not found at %s\n", app.state_db_path);
        return -1;
    }
    int rc = sqlite3_open_v2(app.state_db_path, &app.db,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to open state.db: %s\n", sqlite3_errmsg(app.db));
        return -1;
    }
    sqlite3_exec(app.db, "PRAGMA journal_mode=WAL; PRAGMA temp_store=memory;", NULL, NULL, NULL);
    return 0;
}

static void db_close(void) {
    if (app.db) { sqlite3_close(app.db); app.db = NULL; }
}

typedef int (*db_cb_t)(void*,int,char**,char**);

static int db_query(const char *sql, db_cb_t cb, void *user) {
    if (!app.db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(app.db, sql, cb, user, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL: %s\nErr: %s\n", sql, err ? err : "?");
        sqlite3_free(err);
    }
    return rc;
}

static int cb_sessions(void *u, int argc, char **argv, char **cn) {
    (void)u;
    if (app.session_count >= MAX_SESSIONS) return 0;
    session_entry_t *s = &app.sessions[app.session_count];
    memset(s, 0, sizeof(*s));
    for (int i = 0; i < argc; i++) {
        if (!argv[i]) continue;
        if (!strcmp(cn[i],"id")) snprintf(s->id,sizeof(s->id),"%s",argv[i]);
        else if (!strcmp(cn[i],"title")) snprintf(s->title,sizeof(s->title),"%s",argv[i]);
        else if (!strcmp(cn[i],"source")) snprintf(s->source,sizeof(s->source),"%s",argv[i]);
        else if (!strcmp(cn[i],"model")) snprintf(s->model,sizeof(s->model),"%s",argv[i]);
        else if (!strcmp(cn[i],"message_count")) s->msg_count = atoi(argv[i]);
        else if (!strcmp(cn[i],"input_tokens")) s->tokens = atoi(argv[i]);
        else if (!strcmp(cn[i],"started_at")) s->started_at = (long)atof(argv[i]);
    }
    app.session_count++;
    return 0;
}

static void load_sessions(void) {
    app.session_count = 0;
    char buf[1024];
    snprintf(buf, sizeof(buf),
        "SELECT id, COALESCE(NULLIF(title,''),id) AS title, "
        "COALESCE(source,'cli') AS source, COALESCE(model,'') AS model, "
        "COALESCE(message_count,0) AS message_count, "
        "COALESCE(input_tokens,0) AS input_tokens, "
        "COALESCE(started_at,0) AS started_at "
        "FROM sessions WHERE parent_session_id IS NULL "
        "ORDER BY started_at DESC LIMIT %d", MAX_SESSIONS);
    db_query(buf, cb_sessions, NULL);
    if (app.session_count > 0)
        snprintf(app.latest_model, sizeof(app.latest_model), "%s", app.sessions[0].model);
}

static int cb_messages(void *u, int argc, char **argv, char **cn) {
    (void)u;
    if (app.message_count >= MAX_MESSAGES) return 0;
    message_entry_t *m = &app.messages[app.message_count];
    memset(m, 0, sizeof(*m));
    for (int i = 0; i < argc; i++) {
        if (!argv[i]) continue;
        if (!strcmp(cn[i],"role")) snprintf(m->role,sizeof(m->role),"%s",argv[i]);
        else if (!strcmp(cn[i],"content")) snprintf(m->content,sizeof(m->content),"%s",argv[i]);
        else if (!strcmp(cn[i],"timestamp")) m->timestamp = (long)(atof(argv[i])*1000);
    }
    app.message_count++;
    return 0;
}

static void load_messages(int idx) {
    app.message_count = 0;
    if (idx < 0 || idx >= app.session_count) return;
    char *sql = sqlite3_mprintf(
        "SELECT role,COALESCE(content,'') AS content,"
        "COALESCE(timestamp,0) AS timestamp "
        "FROM messages WHERE session_id='%q' "
        "ORDER BY timestamp ASC,id ASC LIMIT %d",
        app.sessions[idx].id, MAX_MESSAGES);
    if (sql) { db_query(sql, cb_messages, NULL); sqlite3_free(sql); }
}

static int cb_count(void *u, int c, char **v, char **cn) {
    (void)cn; int *out = (int*)u;
    if (c>0 && v[0]) *out = atoi(v[0]);
    return 0;
}

static void load_skills(void) {
    app.skill_count = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", slermes_home(), SLERMES_DIR_SKILLS);
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) && app.skill_count < MAX_ITEMS) {
        if (de->d_name[0] == '.') continue;
        snprintf(app.skill_names[app.skill_count], sizeof(app.skill_names[0]), "%s", de->d_name);
        app.skill_count++;
    }
    closedir(d);
}

static void load_profiles(void) {
    app.profile_count = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", slermes_home(), SLERMES_DIR_PROFILES);
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) && app.profile_count < MAX_ITEMS) {
        if (de->d_name[0] == '.') continue;
        snprintf(app.profile_names[app.profile_count], sizeof(app.profile_names[0]), "%s", de->d_name);
        app.profile_count++;
    }
    closedir(d);
}

static void load_cron(void) {
    app.cron_count = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", slermes_home(), SLERMES_FILE_CRON);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        const char *p = strstr(line, "\"name\"");
        if (!p) continue;
        p = strchr(p+6, '"'); if (!p) continue;
        p++;
        const char *end = strchr(p, '"'); if (!end) continue;
        if (app.cron_count >= MAX_ITEMS) break;
        size_t len = end - p; if (len > 120) len = 120;
        memcpy(app.cron_names[app.cron_count], p, len);
        app.cron_names[app.cron_count][len] = '\0';
        app.cron_count++;
    }
    fclose(f);
}

static void load_stats(void) {
    db_query("SELECT COUNT(*) FROM sessions WHERE parent_session_id IS NULL", cb_count, &app.total_sessions);
    db_query("SELECT COUNT(*) FROM messages", cb_count, &app.total_messages);
}

/* ── Hit testing — covers ALL interactive elements ──────────────── */
typedef struct { int type; int index; } hit_t;

static hit_t hit_test(int mx, int my) {
    hit_t r = { HIT_NONE, -1 };
    int w = win_w(), h = win_h();
    int cw = chat_w();

    /* Titlebar tools (right side) */
    if (my < TITLEBAR_H) {
        int tx = w - PADDING - TOOL_SIZE;
        for (int i = 3; i >= 0; i--) {
            if (mx >= tx && mx < tx + TOOL_SIZE + TOOL_GAP) { r.type = HIT_TOOL; r.index = i; return r; }
            tx -= TOOL_SIZE + TOOL_GAP;
        }
        r.type = HIT_TITLEBAR; return r;
    }
    if (my >= h - STATUSBAR_H) { r.type = HIT_STATUSBAR; return r; }

    /* Composer / Model pill */
    int comp_y = h - STATUSBAR_H - COMPOSER_H - 12;
    if (mx >= chat_x() + 20 && mx < chat_x() + cw - 20 && my >= comp_y && my < comp_y + COMPOSER_H) {
        r.type = HIT_COMPOSER; return r;
    }
    /* Model pill above composer */
    if (mx >= chat_x() + 24 && mx < chat_x() + cw - 24 && my >= comp_y - PILL_H - 6 && my < comp_y - 2) {
        r.type = HIT_PILL; return r;
    }

    /* Sidebar */
    if (mx < chat_x()) {
        int y = TITLEBAR_H;

        /* Search bar */
        if (my >= y + 8 && my < y + 8 + SEARCH_H) { r.type = HIT_SEARCH; return r; }
        y += 8 + SEARCH_H + 8;

        /* SESSIONS section header */
        if (my >= y && my < y + SECTION_H + 4) { r.type = HIT_SESSIONS_HDR; return r; }
        y += SECTION_H + 4;

        /* Session items */
        for (int i = 0; i < app.session_count; i++) {
            if (!app.sessions_expanded) break;
            if (my >= y && my < y + ITEM_H) { r.type = HIT_SESSION; r.index = i; return r; }
            y += ITEM_H;
        }
        if (!app.sessions_expanded) y += ITEM_H; /* show 1 item as hint */

        y += 4;
        /* +New Chat */
        if (my >= y && my < y + 26) { r.type = HIT_NEWCHAT; return r; }
        y += 26 + 12;

        /* NAVIGATION section header */
        if (my >= y && my < y + SECTION_H + 4) { r.type = HIT_NAV_HDR; return r; }
        y += SECTION_H + 4;

        if (app.nav_expanded) {
            for (int i = 0; nav_items[i].label; i++) {
                if (my >= y && my < y + ITEM_H) { r.type = HIT_NAV; r.index = i; return r; }
                y += ITEM_H;
            }
        } else {
            if (my >= y && my < y + ITEM_H) { r.type = HIT_NAV; r.index = app.selected_nav; return r; }
            y += ITEM_H;
        }

        /* Profile at bottom */
        int pr_y = TITLEBAR_H + sidebar_h() - ITEM_H - 14;
        int pr_h = ITEM_H + 10;
        if (my >= pr_y && my < pr_y + pr_h) { r.type = HIT_PROFILE; return r; }

        return r;
    }

    r.type = HIT_CHAT;
    return r;
}

/* ── Draw helpers ──────────────────────────────────────────────────── */
static void draw_scrollbar(int x, int y, int h, int content_h, int scroll) {
    if (content_h <= h) return;
    gc_theme_t *t = &app.theme;
    int track_x = x - SCROLLBAR_W;
    int thumb_h = (h * h) / content_h;
    if (thumb_h < 16) thumb_h = 16;
    int thumb_y = y + (scroll * (h - thumb_h)) / (content_h - h);
    if (thumb_y < y) thumb_y = y;
    if (thumb_y + thumb_h > y + h) thumb_y = y + h - thumb_h;
    gc_fill_rect(app.win, gc_rect(track_x, y, SCROLLBAR_W, h), GC_RGBA(255,255,255,4));
    gc_fill_round_rect(app.win, gc_rect(track_x, thumb_y, SCROLLBAR_W, thumb_h), 3, GC_RGBA(255,255,255,18));
}

static void draw_message_actions(int bx, int by, int bw, int msg_idx) {
    gc_theme_t *t = &app.theme;
    if (msg_idx < 0 || msg_idx != app.hover_message) return;
    int ax = bx + bw - 90, ay = by + 12;
    gc_rect_t abg = {ax, ay, 80, 20};
    gc_fill_round_rect(app.win, abg, 4, GC_RGBA(40,40,48,220));
    gc_draw_rect(app.win, abg, 1, GC_RGBA(255,255,255,20));
    /* Copy button — highlight if hovered */
    bool copy_hov = (app.hover_action == 0);
    gc_color_t cc = copy_hov ? t->text : t->text_secondary;
    if (copy_hov) gc_fill_round_rect(app.win, gc_rect(ax, ay, 38, 20), 4, GC_RGBA(255,255,255,30));
    gc_draw_text(app.win, gc_get_font_small(app.win), "\\xf0\\x9f\\x93\\x8b Copy", ax+4, ay+2, cc);
    /* Edit button — highlight if hovered */
    bool edit_hov = (app.hover_action == 1);
    gc_color_t ec = edit_hov ? t->text : t->text_secondary;
    if (edit_hov) gc_fill_round_rect(app.win, gc_rect(ax+42, ay, 38, 20), 4, GC_RGBA(255,255,255,30));
    gc_draw_text(app.win, gc_get_font_small(app.win), "\\xe2\\x9c\\x8f\\xef\\xb8\\x8f Edit", ax+42, ay+2, ec);
}

static int draw_bubble(int x, int y, int w, const char *role,
                        const char *msg, gc_color_t role_color,
                        bool is_user, bool draw, int msg_idx) {
    gc_theme_t *t = &app.theme;
    int small_h = gc_font_height(gc_get_font_small(app.win));
    int body_h  = gc_font_height(gc_get_font(app.win));
    int mono_h  = gc_font_height(gc_get_font_mono(app.win));
    gc_font_t *mono = gc_get_font_mono(app.win);
    if (!mono) mono = gc_get_font(app.win);
    int pad = 10, line_h = body_h + 3;

    chat_rendered_msg_t *rm = chat_render_message(msg, role);
    if (!rm) {
        /* Fallback: plain text */
        int text_h = gc_draw_text_wrapped(app.win, gc_get_font(app.win), msg,
                                           x+pad, draw ? y+pad+small_h+6 : 0,
                                           w-pad*2, line_h, t->text);
        if (text_h < line_h) text_h = line_h;
        int total_h = pad + small_h + 6 + text_h + pad;
        if (draw) {
            gc_rect_t bubble = {x, y, w, total_h};
            gc_fill_round_rect(app.win, bubble, BUBBLE_RAD,
                is_user ? GC_RGBA(0,83,253,10) : GC_RGBA(255,255,255,6));
            gc_draw_rect(app.win, bubble, 1, t->border_subtle);
            gc_draw_text(app.win, gc_get_font_small(app.win), role, x+pad, y+pad+2, role_color);
            draw_message_actions(x, y, w, msg_idx);
        }
        return total_h;
    }

    /* ── Walk tokens, build text segments, handle code blocks ── */
    int cur_y = y + pad + small_h + 6;
    int content_x = x + pad;
    int content_w = w - pad * 2;

    /* Helper lambda-like macro: draw accumulated text buffer */
    char text_buf[65536];
    int  text_len = 0;
#define FLUSH_TEXT \
    do { \
        if (text_len > 0) { \
            text_buf[text_len] = '\0'; \
            if (draw) { \
                int used = gc_draw_text_wrapped(app.win, gc_get_font(app.win), \
                    text_buf, content_x, cur_y, content_w, line_h, t->text); \
                cur_y += used; \
            } else { \
                int tw = gc_text_width(gc_get_font(app.win), text_buf); \
                cur_y += ((tw / content_w) + 1) * line_h; \
            } \
            text_len = 0; \
        } \
    } while(0)

    for (int i = 0; i < rm->token_count; i++) {
        chat_render_token_t *tok = &rm->tokens[i];
        switch (tok->type) {

        case TOKEN_TEXT:
        case TOKEN_LIST_ITEM:
        case TOKEN_HEADING:
        case TOKEN_LINK_TEXT:
        case TOKEN_BLOCKQUOTE:
        case TOKEN_CODE_INLINE:
            if (tok->text) {
                int rem = (int)sizeof(text_buf) - 1 - text_len;
                snprintf(text_buf + text_len, rem, "%s", tok->text);
                text_len = (int)strlen(text_buf);
            }
            break;

        case TOKEN_NEWLINE:
            text_buf[text_len++] = '\n';
            text_buf[text_len] = '\0';
            break;

        case TOKEN_BOLD_START:
            /* Mark bold with * for now — proper bold rendering later */
            if (text_len + 2 < (int)sizeof(text_buf)) {
                text_buf[text_len++] = '*'; text_buf[text_len] = '\0';
            }
            break;

        case TOKEN_BOLD_END:
            if (text_len + 2 < (int)sizeof(text_buf)) {
                text_buf[text_len++] = '*'; text_buf[text_len] = '\0';
            }
            break;

        case TOKEN_CODE_BLOCK_START: {
            FLUSH_TEXT;

            /* Collect language from next inline text */
            char lang[64] = "";
            if (tok->text && tok->text[0]) {
                snprintf(lang, sizeof(lang), "%.63s", tok->text);
            } else if (i+1 < rm->token_count && rm->tokens[i+1].type == TOKEN_TEXT && rm->tokens[i+1].text) {
                snprintf(lang, sizeof(lang), "%.63s", rm->tokens[i+1].text);
            }

            /* Collect code text until TOKEN_CODE_BLOCK_END */
            char code_buf[65536] = "";
            int  code_len = 0;
            int j;
            for (j = i + 1; j < rm->token_count; j++) {
                if (rm->tokens[j].type == TOKEN_CODE_BLOCK_END) break;
                if (rm->tokens[j].type == TOKEN_TEXT && rm->tokens[j].text) {
                    int rem2 = (int)sizeof(code_buf) - 1 - code_len;
                    snprintf(code_buf + code_len, rem2, "%s", rm->tokens[j].text);
                    code_len = (int)strlen(code_buf);
                }
            }
            i = j; /* advance past TOKEN_CODE_BLOCK_END */

            /* Calculate code block height */
            int code_lines = 1;
            for (const char *p = code_buf; *p; p++) if (*p == '\n') code_lines++;
            int code_block_h = (lang[0] ? small_h + 4 : 0) + code_lines * (mono_h + 2) + pad;

            if (draw) {
                /* Background for code block */
                gc_rect_t cbr = {content_x, cur_y, content_w, code_block_h};
                gc_fill_round_rect(app.win, cbr, 4, GC_RGBA(0,0,0,80));
                gc_draw_rect(app.win, cbr, 1, t->border_subtle);

                /* Language label */
                int top = cur_y + 2;
                if (lang[0]) {
                    gc_rect_t lang_bg = {content_x+6, top, gc_text_width(gc_get_font_small(app.win), lang)+8, small_h};
                    gc_fill_round_rect(app.win, lang_bg, 3, GC_RGBA(0,83,253,60));
                    gc_draw_text(app.win, gc_get_font_small(app.win), lang,
                                 content_x+10, top+2, GC_RGB(0xb1,0xb1,0xbb));
                    top += small_h + 4;
                }

                /* Code lines */
                char line_buf[1024];
                int line_idx = 0;
                for (const char *p = code_buf; ; p++) {
                    if (*p == '\n' || *p == '\0') {
                        line_buf[line_idx] = '\0';
                        if (line_idx > 0) {
                            gc_draw_text(app.win, mono, line_buf,
                                         content_x+8, top, t->text_secondary);
                        }
                        top += mono_h + 2;
                        line_idx = 0;
                        if (*p == '\0') break;
                    } else {
                        if (line_idx < (int)sizeof(line_buf)-1)
                            line_buf[line_idx++] = *p;
                    }
                }
                /* Copy button on code block */
                int ax = content_x + content_w - 52;
                gc_rect_t cpy_bg = {ax, cur_y+2, 44, 18};
                gc_fill_round_rect(app.win, cpy_bg, 3, GC_RGBA(40,40,48,180));
                gc_draw_text(app.win, gc_get_font_small(app.win), "\\xf0\\x9f\\x93\\x8b Copy",
                             ax+4, cur_y+3, t->text_dim);
            }

            cur_y += code_block_h + 4;
            break;
        }

        case TOKEN_CODE_BLOCK_END:
            /* Should be handled by the start case; skip if orphaned */
            break;

        case TOKEN_TOOL_CALL_START:
        case TOKEN_TOOL_CALL_END:
        case TOKEN_THINKING_START:
        case TOKEN_THINKING_END:
        case TOKEN_LINK:
        default:
            break;
        }
    }

    FLUSH_TEXT;
#undef FLUSH_TEXT

    int content_h = cur_y - (y + pad + small_h + 6);
    if (content_h < line_h) content_h = line_h;
    int total_h = pad + small_h + 6 + content_h + pad;

    if (draw) {
        gc_rect_t bubble = {x, y, w, total_h};
        gc_fill_round_rect(app.win, bubble, BUBBLE_RAD,
            is_user ? GC_RGBA(0,83,253,10) : GC_RGBA(255,255,255,6));
        gc_draw_rect(app.win, bubble, 1, t->border_subtle);
        gc_draw_text(app.win, gc_get_font_small(app.win), role, x+pad, y+pad+2, role_color);
        draw_message_actions(x, y, w, msg_idx);
    }

    chat_render_free(rm);
    return total_h;
}

/* ── Titlebar ──────────────────────────────────────────────────────── */
static void draw_titlebar(void) {
    gc_theme_t *t = &app.theme;
    int w = win_w();
    gc_fill_rect(app.win, gc_rect(0,0,w,TITLEBAR_H), t->bg);
    gc_draw_hline(app.win, 0, TITLEBAR_H-1, w, t->border);
    int ty = (TITLEBAR_H-TOOL_SIZE)/2;

    /* ── Left tool cluster: sidebar toggle, flip panes, app label ── */
    int lx = CONTACT_X;

    /* Sidebar toggle (☰) */
    gc_draw_text(app.win, gc_get_font(app.win), "\xe2\x98\xb0", lx, ty,
                 (app.mouse_in_titlebar && app.hover_tool==0) ? t->text : t->text_secondary);
    lx += TOOL_SIZE+TOOL_GAP;

    /* Flip panes (⟷) */
    gc_draw_text(app.win, gc_get_font(app.win), "\xe2\x9f\xb7", lx, ty,
                 (app.mouse_in_titlebar && app.hover_tool==1) ? t->text : t->text_secondary);
    lx += TOOL_SIZE+TOOL_GAP+8;

    /* App label */
    gc_draw_text(app.win, gc_get_font(app.win), "Slermes", lx, ty, t->text);
    lx += gc_text_width(gc_get_font(app.win),"Slermes")+12;
    gc_draw_text(app.win, gc_get_font_small(app.win), "\xe2\x97\x8b Slermes v1.0", lx, ty+2, t->success);

    /* ── Right tool cluster: haptics, keybinds, settings ── */
    int rx = w-PADDING;
    for (int i=3; i>=0; i--) {
        rx -= TOOL_SIZE;
        gc_draw_text(app.win, gc_get_font_small(app.win), tool_chars[i], rx, ty+1,
                     (app.mouse_in_titlebar && app.hover_tool==i) ? t->text : t->text_secondary);
        rx -= TOOL_GAP;
    }
}

/* ── Session header (title + chevron dropdown) ────────────────────── */
static void draw_session_header(void) {
    if (app.current_view != 0) return;
    gc_theme_t *t = &app.theme;
    int cx = chat_x(), cw = chat_w();
    int hy = chat_y();
    int fh = gc_font_height(gc_get_font_small(app.win));

    /* Session title area */
    const char *title = "New session";
    if (app.session_count > 0 && app.selected_session < app.session_count) {
        title = app.sessions[app.selected_session].title;
    }

    /* Background */
    gc_fill_rect(app.win, gc_rect(cx, hy, cw, fh + 12), t->bg);

    /* Title text (truncated) */
    gc_font_t *font = gc_get_font(app.win);
    int title_w = gc_text_width(font, title);
    int title_x = cx + 16;
    if (title_w > cw - 60) {
        gc_draw_text_clipped(app.win, font, title, title_x, hy + 4, cw - 60, t->text);
    } else {
        gc_draw_text(app.win, font, title, title_x, hy + 4, t->text);
    }

    /* Chevron dropdown */
    int chev_x = cx + cw - 28;
    gc_draw_text(app.win, gc_get_font_small(app.win), app.show_session_menu ? "\xe2\x96\xb2" : "\xe2\x96\xbc",
                 chev_x, hy + 6, t->text_secondary);

    /* Session actions dropdown */
    if (app.show_session_menu) {
        gc_color_t menu_bg = GC_RGBA(30, 30, 36, 240);
        int menu_x = cx + cw - 120, menu_y = hy + fh + 12;
        gc_rect_t mb = {menu_x, menu_y, 110, 90};
        gc_fill_round_rect(app.win, mb, 4, menu_bg);
        gc_draw_rect(app.win, mb, 1, t->border);

        const char *labels[] = {"\xe2\x9c\xa8 Pin", "\xf0\x9f\x97\x91  Delete", "\xf0\x9f\x96\xbc  Archive"};
        gc_color_t colors[] = {t->text_secondary, GC_RGBA(255,80,80,255), t->text_secondary};
        for (int i = 0; i < 3; i++) {
            gc_rect_t row = {menu_x + 2, menu_y + 2 + i * 30, 106, 28};
            if (app.session_menu_hover == i) {
                gc_fill_round_rect(app.win, row, 3, GC_RGBA(255,255,255,20));
            }
            gc_draw_text(app.win, gc_get_font_small(app.win), labels[i],
                         menu_x + 8, menu_y + 6 + i * 30, colors[i]);
        }
    }
}

/* ── Model picker dropdown ────────────────────────────────────────── */
static void draw_model_picker(void) {
    if (!app.show_model_picker) return;
    gc_theme_t *t = &app.theme;
    int cx = chat_x(), cy = chat_y(), ch = chat_h(), cw = chat_w();

    /* Position above composer */
    int comp_y = cy + ch - COMPOSER_H - 12;
    int picker_w = 300, picker_h = 280;
    int picker_x = cx + 24, picker_y = comp_y - picker_h - PILL_H - 6;

    gc_rect_t pr = {picker_x, picker_y, picker_w, picker_h};
    gc_fill_round_rect(app.win, pr, 6, GC_RGBA(22, 22, 26, 240));
    gc_draw_rect(app.win, pr, 1, t->border);

    /* Title */
    gc_draw_text(app.win, gc_get_font_small(app.win), "Switch Model", picker_x + 12, picker_y + 8, t->text_dim);

    /* Search bar */
    gc_rect_t sr = {picker_x + 8, picker_y + 24, picker_w - 16, 22};
    gc_fill_round_rect(app.win, sr, 3, GC_RGBA(255,255,255,5));

    /* Provider groups */
    const char *providers[] = {"OpenRouter", "Nous Research", "Anthropic"};
    const char *models[][3] = {
        {"openrouter/owl-alpha", "openrouter/claude-sonnet-4", "openrouter/gpt-4o-mini"},
        {"nousresearch/hermes-3-405b", "nousresearch/hermes-4-mid", NULL},
        {"claude-sonnet-4-20250514", "claude-3-5-haiku-20241022", NULL}
    };
    int model_counts[] = {3, 2, 2};
    int py = picker_y + 52;

    for (int p = 0; p < 3; p++) {
        gc_draw_text(app.win, gc_get_font_small(app.win), providers[p], picker_x + 12, py, t->text_dim);
        py += 18;
        for (int m = 0; m < model_counts[p]; m++) {
            bool hov = (app.model_picker_hover == p * 10 + m);
            bool sel = !strcmp(models[p][m], app.latest_model);
            gc_rect_t mr = {picker_x + 4, py - 2, picker_w - 8, 24};
            if (hov) gc_fill_round_rect(app.win, mr, 3, GC_RGBA(255,255,255,12));
            if (sel) gc_fill_round_rect(app.win, mr, 3, GC_RGBA(0,83,253,30));
            gc_color_t mc = sel ? t->accent : (hov ? t->text : t->text_secondary);
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win), models[p][m],
                                 picker_x + 12, py, picker_w - 24, mc);
            if (sel) {
                int ck_w = gc_text_width(gc_get_font_small(app.win), "\xe2\x9c\x93");
                gc_draw_text(app.win, gc_get_font_small(app.win), "\xe2\x9c\x93",
                             picker_x + picker_w - ck_w - 8, py, t->accent);
            }
            py += 24;
        }
        py += 4;
    }

    /* Refresh button */
    gc_draw_text(app.win, gc_get_font_small(app.win), "\xe2\x86\xb3 Refresh Models",
                 picker_x + 12, picker_y + picker_h - 18, t->text_dim);
}

/* ── Scroll-to-bottom button ──────────────────────────────────────── */
static void draw_scroll_button(void) {
    if (!app.show_scroll_button) return;
    gc_theme_t *t = &app.theme;
    int cx = chat_x(), cw = chat_w(), cy = chat_y(), ch = chat_h();
    int bx = cx + cw / 2 - 20, by = cy + ch - COMPOSER_H - 80;
    gc_rect_t bb = {bx, by, 40, 28};
    gc_fill_round_rect(app.win, bb, 14, app.scroll_button_hover ? GC_RGBA(0,83,253,180) : GC_RGBA(0,83,253,120));
    gc_draw_text(app.win, gc_get_font_small(app.win), "\xe2\x96\xbc", bx + 12, by + 5, GC_RGBA(255,255,255,220));
}

/* ── Sidebar — full depth parity ──────────────────────────────────── */
static void draw_sidebar(void) {
    gc_theme_t *t = &app.theme;
    int sy = TITLEBAR_H, sh = sidebar_h();
    int sw = sidebar_w();
    int cw = sw - PADDING*2;
    gc_fill_rect(app.win, gc_rect(0, sy, sw, sh), t->bg_secondary);

    /* Collapsed: show only nav icons, no text */
    if (app.sidebar_collapsed) {
        int icon_y = sy + 8;
        /* New chat icon */
        gc_draw_text(app.win, gc_get_font(app.win), "+", 14, icon_y, t->text_secondary);
        icon_y += 36;
        /* Nav item icons only */
        for (int i = 0; nav_items[i].label; i++) {
            gc_color_t nc = (i == app.selected_nav) ? t->text : t->text_secondary;
            gc_draw_text(app.win, gc_get_font_small(app.win), nav_items[i].icon, 14, icon_y, nc);
            icon_y += ITEM_H;
        }
        gc_draw_vline(app.win, sw-1, sy, sh, t->border_subtle);
        return;
    }

    int y = sy + 8 - app.sidebar_scroll;
    int first_y = y;
    int sfh = gc_font_height(gc_get_font_small(app.win));

    /* ── Search bar ── */
    {
        gc_rect_t sr = {PADDING, y, cw, SEARCH_H};
        bool srch_hov = app.hover_search >= 0;
        gc_fill_round_rect(app.win, sr, 4, srch_hov ? GC_RGBA(255,255,255,12) : GC_RGBA(255,255,255,6));
        gc_draw_text(app.win, gc_get_font_small(app.win), "\xf0\x9f\x94\x8d", PADDING+6, y+4, t->text_dim);
        if (app.search_active && app.search_query_len > 0) {
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win),
                                 app.search_query, PADDING+22, y+4, cw-28, t->text);
        } else {
            gc_draw_text(app.win, gc_get_font_small(app.win),
                         app.search_active ? "Type to search..." : "Search",
                         PADDING+22, y+4, srch_hov ? t->text_secondary : t->text_dim);
        }
    }
    y += SEARCH_H + 8;

    /* ── SESSIONS section header (with disclosure caret) ── */
    gc_draw_text(app.win, gc_get_font_small(app.win),
                 app.sessions_expanded ? "\xe2\x96\xbc" : "\xe2\x96\xb6", PADDING, y, t->text_dim);
    gc_draw_text(app.win, gc_get_font_small(app.win), "SESSIONS", PADDING+12, y, t->text_dim);
    y += SECTION_H + 4;

    /* Session items (filtered by search) */
    if (app.sessions_expanded) {
        for (int i=0; i<app.session_count; i++) {
            /* Search filter */
            if (app.search_active && app.search_query_len > 0 &&
                strcasestr(app.sessions[i].title, app.search_query) == NULL &&
                strcasestr(app.sessions[i].source, app.search_query) == NULL &&
                strcasestr(app.sessions[i].model, app.search_query) == NULL)
                continue;
            gc_rect_t ir = {PADDING, y, cw, ITEM_H};
            bool sel = (i==app.selected_session);
            bool hov = (i==app.hover_session && app.mouse_in_sidebar);
            if (sel) gc_fill_round_rect(app.win, ir, 4, GC_RGBA(0,83,253,30));
            else if (hov) gc_fill_round_rect(app.win, ir, 4, GC_RGBA(255,255,255,10));

            /* Pin indicator */
            bool is_pinned = false;
            for (int pi = 0; pi < app.pinned_session_count; pi++) {
                if (app.pinned_session_ids[pi] == i) { is_pinned = true; break; }
            }
            if (is_pinned) {
                gc_draw_text(app.win, gc_get_font_small(app.win), "\xe2\x9c\xa8", PADDING+cw-14, y+2, t->accent);
            }

            /* Title (truncated) */
            gc_color_t tc = sel ? t->text : (hov ? t->text : t->text_secondary);
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win),
                                 app.sessions[i].title, PADDING+6, y+2, cw-12, tc);

            /* Age + message count metadata */
            char meta[64];
            char age[16];
            format_age(app.sessions[i].started_at, age, sizeof(age));
            snprintf(meta, sizeof(meta), "%s  %d msgs", age, app.sessions[i].msg_count);
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win),
                                 meta, PADDING+6, y+16, cw-12, t->text_dim);
            y += ITEM_H;
        }
    } else {
        /* Show just the selected session as a hint when collapsed */
        if (app.session_count > 0 && app.selected_session < app.session_count) {
            int idx = app.selected_session;
            gc_rect_t ir = {PADDING, y, cw, ITEM_H};
            gc_fill_round_rect(app.win, ir, 4, GC_RGBA(0,83,253,30));
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win),
                                 app.sessions[idx].title, PADDING+6, y+2, cw-12, t->text);
            char meta[64];
            char age[16];
            format_age(app.sessions[idx].started_at, age, sizeof(age));
            snprintf(meta, sizeof(meta), "%s  %d msgs", age, app.sessions[idx].msg_count);
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win),
                                 meta, PADDING+6, y+16, cw-12, t->text_dim);
            y += ITEM_H;
        }
    }
    y += 4;

    /* ── +New Chat ── */
    {
        bool nc_hov = (app.hover_newchat >= 0);
        gc_draw_text(app.win, gc_get_font_small(app.win), "+ New Chat",
                     PADDING, y, nc_hov ? t->text : t->text_secondary);
        if (nc_hov) {
            gc_rect_t nr = {PADDING, y-2, cw, 26};
            gc_fill_round_rect(app.win, nr, 4, GC_RGBA(255,255,255,10));
        }
    }
    y += 26 + 12;

    /* ── Load more sessions ── */
    if (!app.sessions_loaded_all && app.session_count >= 20) {
        gc_draw_text(app.win, gc_get_font_small(app.win), "  Load more...", PADDING, y, t->accent);
        y += 20;
    }

    /* ── NAVIGATION section header (with disclosure caret) ── */
    gc_draw_text(app.win, gc_get_font_small(app.win),
                 app.nav_expanded ? "\xe2\x96\xbc" : "\xe2\x96\xb6", PADDING, y, t->text_dim);
    gc_draw_text(app.win, gc_get_font_small(app.win), "NAVIGATION", PADDING+12, y, t->text_dim);
    y += SECTION_H + 4;

    /* Nav items */
    if (app.nav_expanded) {
        for (int i=0; nav_items[i].label; i++) {
            gc_rect_t ir = {PADDING, y, cw, ITEM_H};
            bool sel = (i==app.selected_nav);
            bool hov = (i==app.hover_nav && app.mouse_in_sidebar);
            if (sel) gc_fill_round_rect(app.win, ir, 4, GC_RGBA(0,83,253,30));
            else if (hov) gc_fill_round_rect(app.win, ir, 4, GC_RGBA(255,255,255,10));
            gc_color_t nc = sel ? t->text : (hov ? t->text : t->text_secondary);
            gc_draw_text(app.win, gc_get_font_small(app.win), nav_items[i].icon, PADDING+4, y+2, nc);
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win), nav_items[i].label, PADDING+24, y+2, cw-28, nc);
            y += ITEM_H;
        }
    } else {
        /* Show only selected nav item */
        int i = app.selected_nav;
        if (nav_items[i].label) {
            gc_rect_t ir = {PADDING, y, cw, ITEM_H};
            gc_fill_round_rect(app.win, ir, 4, GC_RGBA(0,83,253,30));
            gc_draw_text(app.win, gc_get_font_small(app.win), nav_items[i].icon, PADDING+4, y+2, t->text);
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win), nav_items[i].label, PADDING+24, y+2, cw-28, t->text);
            y += ITEM_H;
        }
    }

    /* ── Profile at bottom ── */
    {
        int p_y = sy + sh - ITEM_H - 14;
    gc_draw_hline(app.win, 0, p_y-4, sw, t->border_subtle);
        gc_rect_t pr = {PADDING, p_y, cw, ITEM_H};
        bool pr_hov = (app.hover_profile >= 0);
        gc_fill_round_rect(app.win, pr, 4, pr_hov ? GC_RGBA(255,255,255,10) : GC_RGBA(255,255,255,4));
        gc_draw_text(app.win, gc_get_font_small(app.win), "\xf0\x9f\x91\xa4", PADDING+4, p_y+2, t->text);
        gc_draw_text(app.win, gc_get_font_small(app.win), "wubu", PADDING+22, p_y+2, t->text);
        char status[32];
        snprintf(status, sizeof(status), "%d sess", app.total_sessions);
        int gw = gc_text_width(gc_get_font_small(app.win), status);
        gc_draw_text(app.win, gc_get_font_small(app.win), status, PADDING+cw-gw, p_y+2, t->success);
    }

    /* Content height + scrollbar */
    app.sidebar_content_h = y - first_y + 20;
    draw_scrollbar(sw, sy, sh, app.sidebar_content_h, app.sidebar_scroll);
    gc_draw_vline(app.win, sw-1, sy, sh, t->border_subtle);
}

/* ── Nav views ──────────────────────────────────────────────────────── */
static void draw_nav_view(int view_id) {
    gc_theme_t *t = &app.theme;
    int cx = chat_x(), cy = chat_y(), cw = chat_w();
    int y = cy+16, x = cx+24, max_w = cw-48;
    int fh = gc_font_height(gc_get_font(app.win));
    int sfh = gc_font_height(gc_get_font_small(app.win));
    (void)cy;

    switch (view_id) {
    case 0: return;
    case 1: {
        gc_draw_text(app.win, gc_get_font(app.win), "Command Center", x, y, t->text); y+=fh+12;
        const char *cmds[] = {"/help","/skills","/cron","/session","/config","/model",
            "/retry","/reset","/archive","/export","/fork","/summarize","/undo","/settings","/search",NULL};
        gc_font_t *mono = gc_get_font_mono(app.win); if (!mono) mono = gc_get_font(app.win);
        for (int i=0; cmds[i]; i++) { gc_draw_text(app.win, mono, cmds[i], x, y, t->text_secondary); y+=fh+4; }
        break;
    }
    case 2: {
        gc_draw_text(app.win, gc_get_font(app.win), "Skills", x, y, t->text); y+=fh+4;
        for (int i=0; i<app.skill_count; i++) {
            gc_draw_text(app.win, gc_get_font_small(app.win), "\xe2\x97\x8b", x, y, t->text_secondary);
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win), app.skill_names[i], x+16, y, max_w-16, t->text_secondary);
            y+=sfh+2;
        }
        break;
    }
    case 3: gc_draw_text(app.win, gc_get_font(app.win), "Artifacts", x, y, t->text); y+=fh+8;
        gc_draw_text(app.win, gc_get_font_small(app.win), "No artifacts available.", x, y, t->text_dim); break;
    case 4: {
        gc_draw_text(app.win, gc_get_font(app.win), "Cron", x, y, t->text); y+=fh+4;
        for (int i=0; i<app.cron_count; i++) {
            gc_draw_text(app.win, gc_get_font_small(app.win), "\xe2\x97\x8b", x, y, t->text_secondary);
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win), app.cron_names[i], x+16, y, max_w-16, t->text_secondary);
            y+=sfh+2;
        }
        break;
    }
    case 5: {
        gc_draw_text(app.win, gc_get_font(app.win), "Profiles", x, y, t->text); y+=fh+4;
        for (int i=0; i<app.profile_count; i++) {
            gc_draw_text(app.win, gc_get_font_small(app.win), "\xe2\x97\x8b", x, y, t->text_secondary);
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win), app.profile_names[i], x+16, y, max_w-16, t->text_secondary);
            y+=sfh+2;
        }
        break;
    }
    case 6: gc_draw_text(app.win, gc_get_font(app.win), "Agents", x, y, t->text); y+=fh+8;
        gc_draw_text(app.win, gc_get_font_small(app.win), "No active agents.", x, y, t->text_dim); break;
    case 7: {
        gc_draw_text(app.win, gc_get_font(app.win), "Messaging", x, y, t->text); y+=fh+8;
        const char *plats[] = {"Telegram","CLI","Cron",NULL};
        for (int i=0; plats[i]; i++) {
            gc_draw_text(app.win, gc_get_font_small(app.win), "\xe2\x97\x8b", x, y, t->text_secondary);
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win), plats[i], x+16, y, max_w-16, t->text_secondary);
            y+=sfh+2;
        }
        break;
    }
    case 8: {
        /* ── File Browser ── */
        gc_draw_text(app.win, gc_get_font(app.win), "Files", x, y, t->text); y+=fh+8;
        char dir_path[512];
        snprintf(dir_path, sizeof(dir_path), "%s", slermes_home());
        DIR *d = opendir(dir_path);
        if (d) {
            struct dirent *de;
            int file_count = 0;
            while ((de = readdir(d)) && file_count < 30) {
                if (de->d_name[0] == '.') continue;
                char fpath[1024];
                snprintf(fpath, sizeof(fpath), "%s/%s", dir_path, de->d_name);
                struct stat st;
                if (stat(fpath, &st) != 0) continue;
                bool is_dir = S_ISDIR(st.st_mode);
                gc_draw_text(app.win, gc_get_font_small(app.win),
                    is_dir ? "\xf0\x9f\x91\x91" : "\xf0\x9f\x93\x84", x, y, t->text_secondary);
                gc_draw_text_clipped(app.win, gc_get_font_small(app.win),
                    de->d_name, x+16, y, max_w-16, t->text_secondary);
                y+=sfh+2;
                file_count++;
            }
            closedir(d);
            if (file_count == 0) {
                gc_draw_text(app.win, gc_get_font_small(app.win), "(empty)", x+16, y, t->text_dim);
            }
        } else {
            gc_draw_text(app.win, gc_get_font_small(app.win), "(cannot open)", x+16, y, t->text_dim);
        }
        break;
    }
    case 9: {
        /* ── Prompt Snippets ── */
        gc_draw_text(app.win, gc_get_font(app.win), "Prompt Snippets", x, y, t->text); y+=fh+8;
        const char *snippets[] = {
            "Explain this code",
            "Write a test for",
            "Refactor to be more efficient",
            "Add documentation",
            "Find bugs in",
            "Summarize conversation",
            "Generate API docs",
            NULL
        };
        for (int i=0; snippets[i]; i++) {
            gc_draw_text(app.win, gc_get_font_small(app.win), "\xf0\x9f\x93\x8b", x, y, t->text_secondary);
            gc_draw_text_clipped(app.win, gc_get_font_small(app.win), snippets[i], x+16, y, max_w-16, t->text_secondary);
            y+=sfh+2;
        }
        gc_draw_text(app.win, gc_get_font_small(app.win), "+ New snippet...", x+16, y, t->accent);
        break;
    }
    default: break;
    }
}

/* ── Chat area — scroll + scrollbar + date separators ──────────────── */
static void draw_chat_area(void) {
    gc_theme_t *t = &app.theme;
    int cx = chat_x(), cw = chat_w(), cy = chat_y(), ch = chat_h();

    if (app.current_view != 0) { draw_nav_view(app.current_view); return; }

    int ml = cx+24, bubble_w = cw-48;
    int chat_bottom = cy + ch - COMPOSER_H - 20;
    int fh_small = gc_font_height(gc_get_font_small(app.win));

    if (app.message_count == 0) {
        int y = cy+8 - app.chat_scroll;
        if (y+20 >= cy && y < chat_bottom) {
            if (app.session_count > 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Session: %s", app.sessions[app.selected_session].title);
                gc_draw_text(app.win, gc_get_font(app.win), msg, ml, y, t->text_secondary);
                gc_draw_text(app.win, gc_get_font_small(app.win),
                    "No messages loaded. Select a conversation to begin.", ml, y+22, t->text_dim);
            } else {
                gc_draw_text(app.win, gc_get_font_small(app.win),
                    "No sessions found.", ml, y, t->text_dim);
            }
        }
        app.chat_content_h = ch;
        draw_scrollbar(cx+cw, cy, ch, app.chat_content_h, app.chat_scroll);
        goto composer;
    }

    int render_y = cy+8 - app.chat_scroll;

    for (int i=0; i<app.message_count; i++) {
        message_entry_t *m = &app.messages[i];
        bool is_user = !strcmp(m->role, "user");
        const char *label = is_user ? "You" : "Slermes Agent";
        gc_color_t rc = is_user ? t->accent : t->cyan;

        /* Date separator if gap > 5min */
        if (i>0 && (m->timestamp - app.messages[i-1].timestamp) > 300000) {
            if (render_y+fh_small+6 >= cy && render_y < chat_bottom) {
                char ts[32]; time_t t_s = (time_t)(m->timestamp/1000);
                struct tm lt_buf;
                struct tm *lt = localtime_r(&t_s, &lt_buf);
                if (lt) {
                    strftime(ts,sizeof(ts),"%b %d %H:%M",lt);
                    int tw = gc_text_width(gc_get_font_small(app.win), ts);
                    gc_draw_text(app.win, gc_get_font_small(app.win), ts, cx+(cw-tw)/2, render_y, t->text_dim);
                }
            }
            render_y += fh_small+6;
        }

        /* Always compute accurate height; draw only if visible */
        bool visible = (render_y+200 >= cy && render_y < chat_bottom);
        int bh = draw_bubble(ml, render_y, bubble_w, label, m->content, rc, is_user, visible, i);
        app.bubble_y[i] = render_y;
        app.bubble_h[i] = bh;
        render_y += bh + BUBBLE_GAP + 6;
        if (visible && render_y < chat_bottom)
            gc_fill_rect(app.win, gc_rect(ml, render_y-BUBBLE_GAP-5, bubble_w, 1), t->border_subtle);
    }

    app.chat_content_h = render_y - (cy+8) + app.chat_scroll + 40;
    if (app.chat_content_h < ch) app.chat_content_h = ch;
    draw_scrollbar(cx+cw, cy, ch, app.chat_content_h, app.chat_scroll);

composer:
    /* Composer area */
    {
        int comp_y = cy+ch-COMPOSER_H-12;
        gc_rect_t comp = {cx+20, comp_y, cw-40, COMPOSER_H};

        /* Model pill above composer */
        gc_rect_t pill = {cx+24, comp_y-PILL_H-6, 180, PILL_H};
        gc_fill_round_rect(app.win, pill, 10,
            app.hover_pill>=0 ? GC_RGBA(255,255,255,14) : GC_RGBA(255,255,255,6));
        gc_draw_rect(app.win, pill, 1, t->border_subtle);
        gc_draw_text(app.win, gc_get_font_small(app.win), app.latest_model[0]?app.latest_model:"no model",
                     pill.x+8, pill.y+3, app.hover_pill>=0 ? t->text : t->text_secondary);
        gc_draw_text(app.win, gc_get_font_small(app.win), "\xef\x83\x97",
                     pill.x+pill.w-16, pill.y+3, t->text_dim); /* ↓ chevron */

        /* Composer input */
        if (app.composer_hover || app.composer_focused)
            gc_fill_round_rect(app.win, comp, 8, GC_RGBA(255,255,255,12));
        else
            gc_fill_round_rect(app.win, comp, 8, GC_RGBA(255,255,255,4));
        gc_draw_rect(app.win, comp, 1, t->border);
        if (app.composer_focused && app.composer_buf[0]) {
            /* Show typed text with cursor */
            char display[4096];
            int cursor_visible = (SDL_GetTicks() / 500) % 2; /* blinking cursor */
            snprintf(display, sizeof(display), "%s%c",
                     app.composer_buf, cursor_visible ? '|' : ' ');
            gc_draw_text_clipped(app.win, gc_get_font(app.win), display,
                                 comp.x+12, comp.y+10, comp.w-24, t->text);
        } else if (app.composer_focused) {
            /* Empty but focused — show cursor */
            gc_draw_text(app.win, gc_get_font(app.win),
                         (SDL_GetTicks() / 500) % 2 ? "|" : " ",
                         comp.x+12, comp.y+10, t->text);
        } else {
            gc_draw_text(app.win, gc_get_font_small(app.win), "Send a message...",
                         comp.x+12, comp.y+13,
                         app.composer_hover ? t->text_secondary : t->text_dim);
        }
        /* API status indicator */
        if (app.api_status[0]) {
            gc_draw_text(app.win, gc_get_font_small(app.win), app.api_status,
                         comp.x + comp.w - 120, comp.y + 2, t->text_dim);
        }
    }
}

/* ── Statusbar ──────────────────────────────────────────────────────── */
static void draw_statusbar(void) {
    gc_theme_t *t = &app.theme;
    int w = win_w(), h = win_h(), sy = h-STATUSBAR_H;
    gc_fill_rect(app.win, gc_rect(0,sy,w,STATUSBAR_H), t->bg_secondary);
    gc_draw_hline(app.win, 0, sy, w, t->border);

    /* Left: gateway status dot + model pill */
    int lx = 6;
    gc_color_t gw_color = app.gateway_connected ? t->success : GC_RGBA(255,80,80,255);
    gc_draw_text(app.win, gc_get_font_small(app.win), app.gateway_connected ? "\xe2\x97\x8f" : "\xe2\x97\x8b", lx, sy+2, gw_color);
    lx += 12;

    /* Model name as clickable pill */
    const char *model_short = app.latest_model[0] ? app.latest_model : "none";
    int model_w = gc_text_width(gc_get_font_small(app.win), model_short) + 16;
    gc_rect_t mp = {lx, sy+1, model_w, 16};
    gc_fill_round_rect(app.win, mp, 3, GC_RGBA(255,255,255,5));
    gc_draw_text(app.win, gc_get_font_small(app.win), model_short, lx+8, sy+2, t->text_secondary);
    lx += model_w + 4;

    /* Session count */
    char info[128];
    snprintf(info, sizeof(info), "sessions: %d  msgs: %d  loaded: %d",
             app.total_sessions, app.total_messages, app.session_count);
    gc_draw_text(app.win, gc_get_font_small(app.win), info, lx, sy+2, t->text_secondary);
}

/* ── Petdex: floating pet with animation ───────────────────────────── */
static const char *pet_sprites[][4] = {
    /* cat frames */
    {"[cat1]", "[cat2]", "[cat3]", "[cat4]"},
    /* dragon frames */
    {"[drg1]", "[drg2]", "[drg3]", "[drg4]"},
    /* owl frames */
    {"[owl1]", "[owl2]", "[owl3]", "[owl4]"},
    /* blob frames */
    {"[blb1]", "[blb2]", "[blb3]", "[blb4]"},
};
static const int pet_frame_counts[] = {4, 4, 4, 4};

static void draw_pet(void) {
    if (!app.pet_active || app.show_command_palette) return;
    gc_theme_t *t = &app.theme;

    /* Animate pet position */
    app.pet_frame_tick++;
    if (app.pet_frame_tick >= 30) {
        app.pet_frame_tick = 0;
        app.pet_frame = (app.pet_frame + 1) % pet_frame_counts[app.pet_type];
    }
    app.pet_x += app.pet_vx;
    app.pet_y += app.pet_vy;
    /* Bounce off edges */
    if (app.pet_x < 50 || app.pet_x > win_w() - 100) app.pet_vx = -app.pet_vx;
    if (app.pet_y < 50 || app.pet_y > win_h() - 100) app.pet_vy = -app.pet_vy;
    if (app.pet_x < 50) app.pet_x = 50;
    if (app.pet_x > win_w() - 100) app.pet_x = win_w() - 100;
    if (app.pet_y < 50) app.pet_y = 50;
    if (app.pet_y > win_h() - 100) app.pet_y = win_h() - 100;

    int sz = (int)(36 * app.pet_scale);
    const char *sprite = pet_sprites[app.pet_type][app.pet_frame];

    /* Shadow */
    gc_draw_text(app.win, gc_get_font(app.win), " ",
                 (int)app.pet_x + 2, (int)app.pet_y + sz - 4,
                 GC_RGBA(0,0,0,60));
    /* Pet sprite */
    gc_draw_text(app.win, gc_get_font(app.win), sprite,
                 (int)app.pet_x, (int)app.pet_y,
                 GC_RGBA(255,255,255,255));
    /* Name label */
    if (app.pet_scale > 0.8f) {
        gc_draw_text(app.win, gc_get_font_small(app.win), app.pet_names[app.pet_type],
                     (int)app.pet_x, (int)app.pet_y - 14, t->text_secondary);
    }
}

static void draw_pet_gallery(void) {
    if (!app.pet_show_gallery) return;
    gc_theme_t *t = &app.theme;
    int w = win_w(), h = win_h();
    /* Overlay */
    gc_fill_rect(app.win, gc_rect(0, 0, w, h), GC_RGBA(0, 0, 0, 180));
    /* Panel */
    int pw = 420, ph = 340;
    int px = (w - pw) / 2, py = (h - ph) / 2;
    gc_rect_t panel = {px, py, pw, ph};
    gc_fill_round_rect(app.win, panel, 12, GC_RGBA(22, 22, 28, 250));
    gc_draw_rect(app.win, panel, 1, t->border);
    gc_draw_text(app.win, gc_get_font(app.win), "Petdex Gallery", px + 16, py + 12, t->text);
    gc_draw_text(app.win, gc_get_font_small(app.win), "Select your companion", px + 16, py + 34, t->text_dim);

    /* Grid of pets */
    int cell_w = 90, cell_h = 90;
    int cols = 4, start_x = px + (pw - cols * cell_w) / 2 + 10;
    int start_y = py + 60;
    for (int i = 0; i < app.pet_count; i++) {
        int cx = start_x + (i % cols) * cell_w;
        int cy = start_y + (i / cols) * cell_h;
        gc_rect_t cell = {cx, cy, cell_w - 8, cell_h - 8};
        bool sel = (i == app.pet_selected);
        if (sel) gc_fill_round_rect(app.win, cell, 8, GC_RGBA(0, 83, 253, 40));
        gc_draw_rect(app.win, cell, 1, sel ? t->accent : t->border_subtle);
        gc_draw_text(app.win, gc_get_font(app.win), pet_sprites[i % 4][0],
                     cx + cell_w / 2 - 20, cy + 10, t->text);
        gc_draw_text(app.win, gc_get_font_small(app.win), app.pet_names[i],
                     cx + 4, cy + 50, t->text_secondary);
    }
    /* Scale slider */
    gc_draw_text(app.win, gc_get_font_small(app.win), "Scale:", px + 16, py + ph - 40, t->text_dim);
    gc_draw_hline(app.win, px + 70, py + ph - 33, pw - 100, t->border_subtle);
    int slider_x = px + 70 + (int)((app.pet_scale - 0.5f) / 1.5f * (pw - 100));
    gc_fill_round_rect(app.win, gc_rect(slider_x - 4, py + ph - 38, 8, 12), 4, t->accent);
}

/* ── Voice indicator ──────────────────────────────────────────────── */
static void draw_voice_indicator(void) {
    if (!app.voice_active) return;
    gc_theme_t *t = &app.theme;
    int w = win_w();
    int vx = w - 160, vy = win_h() - STATUSBAR_H - 30;
    gc_rect_t bg = {vx, vy, 140, 22};
    gc_fill_round_rect(app.win, bg, 11,
        app.voice_recording ? GC_RGBA(255, 60, 60, 200) : GC_RGBA(0, 83, 253, 180));
    gc_draw_text(app.win, gc_get_font_small(app.win),
        app.voice_recording ? "⏺ Recording..." : "🎤 Voice mode",
        vx + 8, vy + 3, GC_RGBA(255, 255, 255, 240));
}

/* ── Command palette (Ctrl+K) ─────────────────────────────────────── */
static const char *command_palette_items[] = {
    "New Chat", "Toggle Theme", "Toggle Sidebar", "Search Sessions",
    "Model Picker", "Pet Gallery", "Voice Toggle", "Export Session",
    "Import Session", "Settings", "Keyboard Shortcuts", "About",
    NULL
};

static void draw_command_palette(void) {
    if (!app.show_command_palette) return;
    gc_theme_t *t = &app.theme;
    int w = win_w(), h = win_h();
    gc_fill_rect(app.win, gc_rect(0, 0, w, h), GC_RGBA(0, 0, 0, 160));

    int pw = 480, ph = 380;
    int px = (w - pw) / 2, py = (h - ph) / 2;
    gc_rect_t panel = {px, py, pw, ph};
    gc_fill_round_rect(app.win, panel, 12, GC_RGBA(22, 22, 28, 250));
    gc_draw_rect(app.win, panel, 1, t->border);

    /* Search input */
    gc_rect_t inp = {px + 12, py + 12, pw - 24, 28};
    gc_fill_round_rect(app.win, inp, 6, GC_RGBA(255, 255, 255, 6));
    const char *q = app.command_palette_query_len > 0 ? app.command_palette_query : "Type a command...";
    gc_color_t qc = app.command_palette_query_len > 0 ? t->text : t->text_dim;
    gc_draw_text(app.win, gc_get_font_small(app.win), q, px + 20, py + 18, qc);
    /* Blinking cursor */
    if ((SDL_GetTicks() / 500) % 2) {
        int cw = gc_text_width(gc_get_font_small(app.win), q) + 2;
        gc_draw_vline(app.win, px + 20 + cw, py + 18, 14, t->accent);
    }

    /* Results */
    int ry = py + 52;
    int idx = 0;
    app.command_palette_result_count = 0;
    for (int i = 0; command_palette_items[i]; i++) {
        if (app.command_palette_query_len > 0 &&
            strcasestr(command_palette_items[i], app.command_palette_query) == NULL)
            continue;
        app.command_palette_result_count++;
        bool sel = (idx == app.command_palette_selected);
        if (sel) {
            gc_rect_t row = {px + 8, ry - 2, pw - 16, 24};
            gc_fill_round_rect(app.win, row, 4, GC_RGBA(0, 83, 253, 30));
        }
        gc_draw_text(app.win, gc_get_font_small(app.win), command_palette_items[i],
                     px + 16, ry, sel ? t->text : t->text_secondary);
        ry += 24;
        idx++;
        if (ry > py + ph - 30) break;
    }
    gc_draw_text(app.win, gc_get_font_small(app.win),
                 "↑↓ navigate  ⏎ select  esc close", px + 16, py + ph - 24, t->text_dim);
}

/* ── Image paste overlay ──────────────────────────────────────────── */
static void draw_image_paste_overlay(void) {
    if (!app.image_paste_active) return;
    gc_theme_t *t = &app.theme;
    int w = win_w(), h = win_h();
    gc_fill_rect(app.win, gc_rect(0, 0, w, h), GC_RGBA(0, 0, 0, 160));

    int pw = 400, ph = 200;
    int px = (w - pw) / 2, py = (h - ph) / 2;
    gc_rect_t panel = {px, py, pw, ph};
    gc_fill_round_rect(app.win, panel, 12, GC_RGBA(22, 22, 28, 250));
    gc_draw_rect(app.win, panel, 1, t->border);

    gc_draw_text(app.win, gc_get_font(app.win), "Image Paste", px + 16, py + 12, t->text);
    gc_draw_text(app.win, gc_get_font_small(app.win),
                 "Image detected in clipboard or dropped file.", px + 16, py + 40, t->text_secondary);
    if (app.image_paste_path[0]) {
        gc_draw_text(app.win, gc_get_font_small(app.win), app.image_paste_path,
                     px + 16, py + 64, t->text_dim);
    }
    /* Action buttons */
    gc_rect_t attach_btn = {px + 16, py + ph - 40, 100, 28};
    gc_fill_round_rect(app.win, attach_btn, 6, GC_RGBA(0, 83, 253, 200));
    gc_draw_text(app.win, gc_get_font_small(app.win), "Attach", attach_btn.x + 20, attach_btn.y + 4, t->text);

    gc_rect_t cancel_btn = {px + pw - 116, py + ph - 40, 100, 28};
    gc_fill_round_rect(app.win, cancel_btn, 6, GC_RGBA(60, 60, 68, 200));
    gc_draw_text(app.win, gc_get_font_small(app.win), "Cancel", cancel_btn.x + 20, cancel_btn.y + 4, t->text);
}

/* ── Toast notification ───────────────────────────────────────────── */
static void draw_toast(void) {
    if (app.toast_time <= 0) return;
    gc_theme_t *t = &app.theme;
    int alpha = app.toast_time > 30 ? 200 : app.toast_time * 7;
    int tw = gc_text_width(gc_get_font_small(app.win), app.toast_msg) + 32;
    int tx = (win_w() - tw) / 2, ty = win_h() - STATUSBAR_H - 50;
    gc_rect_t bg = {tx, ty, tw, 28};
    gc_fill_round_rect(app.win, bg, 14, GC_RGBA(40, 40, 48, alpha));
    gc_draw_text(app.win, gc_get_font_small(app.win), app.toast_msg,
                 tx + 16, ty + 4, GC_RGBA(255, 255, 255, alpha));
    app.toast_time--;
}

/* ── Session export/import ────────────────────────────────────────── */
static void export_session_to_file(int session_idx, const char *path) {
    if (session_idx < 0 || session_idx >= app.session_count) return;
    FILE *f = fopen(path, "w");
    if (!f) return;
    session_entry_t *s = &app.sessions[session_idx];
    fprintf(f, "# Slermes Session Export\n");
    fprintf(f, "id: %s\n", s->id);
    fprintf(f, "title: %s\n", s->title);
    fprintf(f, "source: %s\n", s->source);
    fprintf(f, "model: %s\n", s->model);
    fprintf(f, "started_at: %ld\n", s->started_at);
    fprintf(f, "---\n");
    /* Reload messages for this session */
    int prev_count = app.message_count;
    load_messages(session_idx);
    for (int i = 0; i < app.message_count; i++) {
        fprintf(f, "[%s] %s\n", app.messages[i].role, app.messages[i].content);
    }
    if (session_idx == app.selected_session) {
        /* Restore previous message count */
        app.message_count = prev_count;
    }
    fclose(f);
    snprintf(app.toast_msg, sizeof(app.toast_msg), "Exported to %s", path);
    app.toast_time = 120;
}

static void show_import_dialog(void) {
    app.show_import_dialog = true;
    /* Use a default path */
    snprintf(app.import_path, sizeof(app.import_path), "%s/import-session.txt", slermes_home());
    app.import_path_len = (int)strlen(app.import_path);
}

static void import_session_from_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(app.toast_msg, sizeof(app.toast_msg), "Cannot open %s", path);
        app.toast_time = 120;
        return;
    }
    if (!app.db) {
        fclose(f);
        return;
    }
    /* Parse simple format */
    char line[65536];
    char session_id[64] = "";
    char session_title[256] = "Imported Session";
    char session_source[32] = "cli";
    char session_model[128] = "";
    long started_at = (long)time(NULL);
    bool in_header = true;
    char content[65536] = "";
    char role[32] = "assistant";
    int content_len = 0;

    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        if (in_header) {
            if (line[0] == '#' || line[0] == '\0') continue;
            if (strcmp(line, "---") == 0) {
                in_header = false;
                /* Create session in DB */
                char *err = NULL;
                char *sql = sqlite3_mprintf(
                    "INSERT INTO sessions (id, title, source, model, started_at, message_count) "
                    "VALUES ('%q', '%q', '%q', '%q', %f, 0)",
                    session_id, session_title, session_source, session_model, (double)started_at * 1000);
                if (sql) {
                    sqlite3_exec(app.db, sql, NULL, NULL, &err);
                    sqlite3_free(sql);
                    if (err) sqlite3_free(err);
                }
                continue;
            }
            if (strncmp(line, "id:", 3) == 0) snprintf(session_id, sizeof(session_id), "%s", line + 3);
            else if (strncmp(line, "title:", 6) == 0) snprintf(session_title, sizeof(session_title), "%s", line + 6);
            else if (strncmp(line, "source:", 7) == 0) snprintf(session_source, sizeof(session_source), "%s", line + 7);
            else if (strncmp(line, "model:", 6) == 0) snprintf(session_model, sizeof(session_model), "%s", line + 6);
            else if (strncmp(line, "started_at:", 11) == 0) started_at = atol(line + 11);
        } else {
            if (line[0] == '[') {
                /* Role line: [user] or [assistant] */
                char *end = strchr(line + 1, ']');
                if (end) {
                    snprintf(role, sizeof(role), "%.*s", (int)(end - line - 1), line + 1);
                    content_len = 0;
                }
            } else {
                /* Content line */
                if (content_len > 0 && content_len < (int)sizeof(content) - 2)
                    content[content_len++] = '\n';
                int rem = (int)sizeof(content) - content_len - 1;
                if (rem > 0) {
                    snprintf(content + content_len, rem, "%s", line);
                    content_len = (int)strlen(content);
                }
                /* Save message */
                if (content_len > 0 && session_id[0]) {
                    char *err = NULL;
                    char *sql = sqlite3_mprintf(
                        "INSERT INTO messages (session_id, role, content, timestamp) "
                        "VALUES ('%q', '%q', '%q', %f)",
                        session_id, role, content, (double)(time(NULL) * 1000));
                    if (sql) {
                        sqlite3_exec(app.db, sql, NULL, NULL, &err);
                        sqlite3_free(sql);
                        if (err) sqlite3_free(err);
                    }
                }
                content[0] = 0;
                content_len = 0;
            }
        }
    }
    fclose(f);
    /* Reload sessions */
    load_sessions();
    snprintf(app.toast_msg, sizeof(app.toast_msg), "Imported: %s", session_title);
    app.toast_time = 120;
}

/* ══════════════════════════════════════════════════════════════════════
 *  Main
 * ══════════════════════════════════════════════════════════════════════ */
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if (!getenv("SDL_VIDEODRIVER")) SDL_SetHint(SDL_HINT_VIDEODRIVER, "wayland,x11");

    if (db_open() < 0) {
        fprintf(stderr, "Cannot open state.db\n");
    } else {
        load_sessions();
        load_skills();
        load_profiles();
        load_cron();
        load_stats();
        if (app.session_count > 0) load_messages(0);
        fprintf(stderr, "%d sessions, %d messages\n", app.session_count, app.message_count);
    }

    if (gc_init() < 0) { fprintf(stderr,"Init failed\n"); db_close(); return 1; }
    app.win = gc_create_window("Slermes Agent", 1200, 760, &gc_theme_dark);
    if (!app.win) { gc_quit(); db_close(); return 1; }

    app.running = true;
    app.selected_session = 0;
    app.selected_nav = 0;
    app.current_view = 0;
    app.hover_nav = app.hover_session = app.hover_tool = -1;
    app.hover_newchat = app.hover_pill = app.hover_profile = app.hover_search = -1;
    app.hover_message = -1;
    app.hover_action = -1;
    app.sidebar_collapsed = false;
    app.haptics_muted = false;
    app.pinned_session_count = 0;
    app.show_model_picker = false;
    app.model_picker_hover = -1;
    app.model_picker_scroll = 0;
    app.show_scroll_button = false;
    app.scroll_button_hover = false;
    app.show_session_menu = false;
    app.session_menu_hover = -1;
    app.sessions_loaded_all = false;
    app.sessions_page = 1;
    app.gateway_connected = false;
    snprintf(app.gateway_status_text, sizeof(app.gateway_status_text), "disconnected");
    app.sidebar_scroll = app.chat_scroll = 0;
    app.sidebar_content_h = app.chat_content_h = 0;
    app.sessions_expanded = true;
    app.nav_expanded = true;
    app.search_active = false;
    app.search_query[0] = 0;
    app.search_query_len = 0;
    app.dark_mode = true;
    app.composer_buf[0] = '\0';
    app.composer_pos = 0;
    app.composer_focused = false;
    app.api_busy = false;
    app.api_status[0] = '\0';
    snprintf(app.current_view_name, sizeof(app.current_view_name), "Chat");
    app.theme = gc_get_theme(app.win);
    gc_set_fps(app.win, 60);

    /* Petdex init */
    app.pet_active = true;
    app.pet_type = 0;
    app.pet_frame = 0;
    app.pet_frame_tick = 0;
    app.pet_x = 100.0f;
    app.pet_y = 200.0f;
    app.pet_vx = 0.5f;
    app.pet_vy = 0.3f;
    app.pet_show_gallery = false;
    app.pet_count = 8;
    app.pet_selected = 0;
    app.pet_scale = 1.0f;
    snprintf(app.pet_names[0], 32, "Whiskers");
    snprintf(app.pet_names[1], 32, "Ember");
    snprintf(app.pet_names[2], 32, "Hoot");
    snprintf(app.pet_names[3], 32, "Blobby");
    snprintf(app.pet_names[4], 32, "Sparky");
    snprintf(app.pet_names[5], 32, "Shadow");
    snprintf(app.pet_names[6], 32, "Pepper");
    snprintf(app.pet_names[7], 32, "Zephyr");

    /* Voice */
    app.voice_active = false;
    app.voice_recording = false;
    app.voice_tts_pending = 0;

    /* Command palette */
    app.show_command_palette = false;
    app.command_palette_query[0] = 0;
    app.command_palette_query_len = 0;
    app.command_palette_selected = 0;
    app.command_palette_result_count = 0;

    /* Image paste */
    app.image_paste_active = false;
    app.image_paste_path[0] = '\0';
    app.image_paste_data_len = 0;
    app.image_paste_is_base64 = false;

    /* Toast */
    app.toast_msg[0] = 0;
    app.toast_time = 0;

    while (app.running) {
        gc_event_t ev;
        while (gc_poll_event(app.win, &ev)) {
            /* Reset hover for elements that change on mouse move */
            if (ev.type != GC_EV_MOUSE_MOVE) {
                // keep existing values for non-move events
            }

            switch (ev.type) {
            case GC_EV_QUIT: app.running = false; break;
            case GC_EV_KEY_DOWN:
                /* Command palette: Ctrl+K or Cmd+K */
                if (ev.key == SDLK_k && (ev.mod & (KMOD_CTRL | KMOD_GUI))) {
                    app.show_command_palette = !app.show_command_palette;
                    app.command_palette_query[0] = 0;
                    app.command_palette_query_len = 0;
                    app.command_palette_selected = 0;
                    break;
                }
                /* Pet gallery: Ctrl+P */
                if (ev.key == SDLK_p && (ev.mod & KMOD_CTRL)) {
                    app.pet_show_gallery = !app.pet_show_gallery;
                    break;
                }
                /* Voice toggle: Ctrl+V */
                if (ev.key == SDLK_v && (ev.mod & KMOD_CTRL)) {
                    app.voice_active = !app.voice_active;
                    app.voice_recording = false;
                    if (app.voice_active) {
                        /* Check SDL clipboard for image */
                        if (SDL_HasClipboardText()) {
                            char *clip = SDL_GetClipboardText();
                            if (clip) {
                                if (strncmp(clip, "data:image", 10) == 0) {
                                    /* Base64 image in clipboard */
                                    app.image_paste_is_base64 = true;
                                    app.image_paste_data_len = 0;
                                    snprintf(app.image_paste_path, sizeof(app.image_paste_path), "clipboard");
                                    app.image_paste_active = true;
                                } else if (strncmp(clip, "file://", 7) == 0) {
                                    /* File path in clipboard */
                                    snprintf(app.image_paste_path, sizeof(app.image_paste_path), "%s", clip + 7);
                                    app.image_paste_is_base64 = false;
                                    app.image_paste_active = true;
                                }
                                SDL_free(clip);
                            }
                        }
                        if (!app.image_paste_active) {
                            snprintf(app.toast_msg, sizeof(app.toast_msg), "Voice mode enabled");
                            app.toast_time = 60;
                        }
                    }
                    break;
                }
                /* Export session: Ctrl+S */
                if (ev.key == SDLK_s && (ev.mod & KMOD_CTRL)) {
                    if (app.session_count > 0) {
                        char path[512];
                        snprintf(path, sizeof(path), "%s/export-%ld.txt",
                                 slermes_home(), (long)time(NULL));
                        export_session_to_file(app.selected_session, path);
                    }
                    break;
                }
                /* Import session: Ctrl+I */
                if (ev.key == SDLK_i && (ev.mod & KMOD_CTRL)) {
                    import_session_from_file(app.import_path);
                    break;
                }
                /* Command palette navigation */
                if (app.show_command_palette) {
                    if (ev.key == SDLK_ESCAPE) {
                        app.show_command_palette = false;
                    } else if (ev.key == SDLK_UP) {
                        if (app.command_palette_selected > 0) app.command_palette_selected--;
                    } else if (ev.key == SDLK_DOWN) {
                        if (app.command_palette_selected < app.command_palette_result_count - 1)
                            app.command_palette_selected++;
                    } else if (ev.key == SDLK_RETURN) {
                        /* Execute selected command */
                        int idx = 0;
                        for (int i = 0; command_palette_items[i]; i++) {
                            if (app.command_palette_query_len > 0 &&
                                strcasestr(command_palette_items[i], app.command_palette_query) == NULL)
                                continue;
                            if (idx == app.command_palette_selected) {
                                if (i == 0) { /* New Chat */
                                    /* Trigger new chat via existing mechanism */
                                } else if (i == 1) { /* Toggle Theme */
                                    app.dark_mode = !app.dark_mode;
                                    gc_set_theme(app.win, app.dark_mode ? &gc_theme_dark : &gc_theme_light);
                                    app.theme = gc_get_theme(app.win);
                                } else if (i == 2) { /* Toggle Sidebar */
                                    app.sidebar_collapsed = !app.sidebar_collapsed;
                                } else if (i == 3) { /* Search */
                                    app.search_active = true;
                                } else if (i == 4) { /* Model Picker */
                                    app.show_model_picker = !app.show_model_picker;
                                } else if (i == 5) { /* Pet Gallery */
                                    app.pet_show_gallery = !app.pet_show_gallery;
                                } else if (i == 6) { /* Voice Toggle */
                                    app.voice_active = !app.voice_active;
                                } else if (i == 7) { /* Export */
                                    if (app.session_count > 0) {
                                        char path[512];
                                        snprintf(path, sizeof(path), "%s/export-%ld.txt",
                                                 slermes_home(), (long)time(NULL));
                                        export_session_to_file(app.selected_session, path);
                                    }
                                } else if (i == 8) { /* Import */
                                    import_session_from_file(app.import_path);
                                } else if (i == 9) { /* Settings */
                                    snprintf(app.toast_msg, sizeof(app.toast_msg), "Settings not yet implemented");
                                    app.toast_time = 60;
                                } else if (i == 10) { /* Keyboard Shortcuts */
                                    snprintf(app.toast_msg, sizeof(app.toast_msg),
                                             "Ctrl+K: Cmd Palette  Ctrl+S: Export  Ctrl+I: Import  Ctrl+V: Voice");
                                    app.toast_time = 120;
                                } else if (i == 11) { /* About */
                                    snprintf(app.toast_msg, sizeof(app.toast_msg),
                                             "Slermes v1.0.0 — C11 Desktop Parity");
                                    app.toast_time = 60;
                                }
                                break;
                            }
                            idx++;
                        }
                        app.show_command_palette = false;
                    } else if (ev.key == SDLK_BACKSPACE && app.command_palette_query_len > 0) {
                        app.command_palette_query[--app.command_palette_query_len] = 0;
                        app.command_palette_selected = 0;
                    } else if (ev.key >= 32 && ev.key <= 126 && app.command_palette_query_len < 126) {
                        app.command_palette_query[app.command_palette_query_len++] = (char)ev.key;
                        app.command_palette_query[app.command_palette_query_len] = 0;
                        app.command_palette_selected = 0;
                    }
                    break;
                }
                /* Composer takes priority when focused */
                if (app.composer_focused) {
                    if (ev.key == SDLK_ESCAPE) {
                        app.composer_focused = false;
                        SDL_StopTextInput();
                    } else if (ev.key == SDLK_RETURN && app.composer_buf[0] != '\0' && !app.api_busy) {
                        api_send_message();
                    } else if (ev.key == SDLK_BACKSPACE && app.composer_pos > 0) {
                        app.composer_buf[--app.composer_pos] = '\0';
                    }
                    break;
                }
                if (ev.key == SDLK_ESCAPE || ev.key == SDLK_q) app.running = false;
                /* Theme toggle */
                if (ev.key == 't') {
                    app.dark_mode = !app.dark_mode;
                    gc_set_theme(app.win, app.dark_mode ? &gc_theme_dark : &gc_theme_light);
                    app.theme = gc_get_theme(app.win);
                }
                /* Search: activate with /, escape to cancel */
                if (app.search_active) {
                    if (ev.key == SDLK_ESCAPE || ev.key == SDLK_RETURN) {
                        app.search_active = false;
                        app.search_query[0] = 0;
                        app.search_query_len = 0;
                    } else if (ev.key == SDLK_BACKSPACE && app.search_query_len > 0) {
                        app.search_query[--app.search_query_len] = 0;
                    } else if (ev.key >= 32 && ev.key <= 126 && app.search_query_len < 62) {
                        app.search_query[app.search_query_len++] = (char)ev.key;
                        app.search_query[app.search_query_len] = 0;
                    }
                } else {
                    if (ev.key == '/') {
                        app.search_active = true;
                        app.search_query[0] = 0;
                        app.search_query_len = 0;
                    }
                }
                if (ev.key == SDLK_UP) {
                    if (app.current_view == 0) { app.sidebar_scroll -= ITEM_H*2; if (app.sidebar_scroll<0) app.sidebar_scroll=0; }
                    else { app.chat_scroll -= 20; if (app.chat_scroll<0) app.chat_scroll=0; }
                }
                if (ev.key == SDLK_DOWN) {
                    if (app.current_view == 0) app.sidebar_scroll += ITEM_H*2;
                    else app.chat_scroll += 20;
                }
                break;
            case GC_EV_TEXT_INPUT:
                if (app.composer_focused && !app.api_busy) {
                    int rem = (int)sizeof(app.composer_buf) - 1 - app.composer_pos;
                    snprintf(app.composer_buf + app.composer_pos, rem, "%s", ev.text);
                    app.composer_pos = (int)strlen(app.composer_buf);
                }
                break;
            case GC_EV_MOUSE_WHEEL: {
                int amt = ev.wheel_delta * SCROLL_MUL;
                if (app.mouse_in_sidebar) {
                    int max = app.sidebar_content_h - sidebar_h();
                    if (max < 0) max = 0;
                    app.sidebar_scroll += amt;
                    if (app.sidebar_scroll < 0) app.sidebar_scroll = 0;
                    if (app.sidebar_scroll > max) app.sidebar_scroll = max;
                } else if (app.current_view == 0 && app.mouse_in_chat) {
                    int max = app.chat_content_h - chat_h();
                    if (max < 0) max = 0;
                    app.chat_scroll += amt;
                    if (app.chat_scroll < 0) app.chat_scroll = 0;
                    if (app.chat_scroll > max) app.chat_scroll = max;
                }
                break;
            }
            case GC_EV_MOUSE_MOVE: {
                int sw = sidebar_w();
                app.mouse_in_sidebar = (ev.x < sw);
                app.mouse_in_chat = (ev.x >= sw);
                app.mouse_in_titlebar = (ev.y < TITLEBAR_H);
                app.mouse_in_statusbar = (ev.y >= win_h() - STATUSBAR_H);
                int cy2 = chat_y(), ch2 = chat_h();
                app.composer_hover = (ev.x >= chat_x()+20 && ev.x < chat_x()+chat_w()-20 &&
                    ev.y >= win_h()-STATUSBAR_H-COMPOSER_H-12 && ev.y < win_h()-STATUSBAR_H-12);
                hit_t hr = hit_test(ev.x, ev.y);
                app.hover_nav = (hr.type == HIT_NAV) ? hr.index : -1;
                app.hover_session = (hr.type == HIT_SESSION) ? hr.index : -1;
                app.hover_tool = (hr.type == HIT_TOOL) ? hr.index : -1;
                app.hover_newchat = (hr.type == HIT_NEWCHAT) ? 1 : -1;
                app.hover_pill = (hr.type == HIT_PILL) ? 1 : -1;
                app.hover_profile = (hr.type == HIT_PROFILE) ? 1 : -1;
                app.hover_search = (hr.type == HIT_SEARCH) ? 1 : -1;
                /* Detect which message bubble is hovered */
                app.hover_message = -1;
                app.hover_action = -1;
                app.scroll_button_hover = false;
                app.model_picker_hover = -1;

                /* Scroll-to-bottom button hover */
                if (app.show_scroll_button) {
                    int sbx = chat_x() + chat_w() / 2 - 20, sby = chat_y() + chat_h() - COMPOSER_H - 80;
                    if (ev.x >= sbx && ev.x < sbx + 40 && ev.y >= sby && ev.y < sby + 28) {
                        app.scroll_button_hover = true;
                    }
                }

                /* Model picker hover */
                if (app.show_model_picker) {
                    int comp_y = chat_y() + chat_h() - COMPOSER_H - 12;
                    int picker_x = chat_x() + 24, picker_y = comp_y - 280 - PILL_H - 6;
                    int picker_w = 300;
                    if (ev.x >= picker_x && ev.x < picker_x + picker_w && ev.y >= picker_y && ev.y < picker_y + 280) {
                        int rel_y = ev.y - (picker_y + 52);
                        if (rel_y >= 0) {
                            int idx = rel_y / 24;
                            if (idx < 8) app.model_picker_hover = idx;
                        }
                    }
                }

                /* Session menu hover */
                app.session_menu_hover = -1;
                if (app.show_session_menu) {
                    int cx = chat_x(), cw = chat_w();
                    int hy = chat_y();
                    int fh = gc_font_height(gc_get_font_small(app.win));
                    int menu_x = cx + cw - 120, menu_y = hy + fh + 12;
                    if (ev.x >= menu_x && ev.x < menu_x + 110 && ev.y >= menu_y && ev.y < menu_y + 90) {
                        app.session_menu_hover = (ev.y - menu_y) / 30;
                    }
                }
                if (app.mouse_in_chat && app.current_view == 0 && app.message_count > 0) {
                    for (int mi = 0; mi < app.message_count; mi++) {
                        int by = app.bubble_y[mi], bh = app.bubble_h[mi];
                        if (by <= 0 && mi > 0) continue; /* uninitialized */
                        int top = by, bot = by + bh;
                        if (ev.y >= top && ev.y < bot) {
                            app.hover_message = mi;
                            /* Check if mouse is on action bar (right side of bubble) */
                            int bx = chat_x() + 24;
                            int bw = chat_w() - 48;
                            int ax = bx + bw - 90, ay = by + 12;
                            if (ev.x >= ax && ev.x < ax + 80 && ev.y >= ay && ev.y < ay + 20) {
                                app.hover_action = (ev.x < ax + 38) ? 0 : 1;
                            }
                            break;
                        }
                    }
                }
                (void)cy2; (void)ch2;
                break;
            }
            case GC_EV_MOUSE_DOWN:
            case GC_EV_MOUSE_UP: {
                /* Image paste overlay buttons */
                if (app.image_paste_active && ev.type == GC_EV_MOUSE_DOWN) {
                    int w = win_w(), h = win_h();
                    int pw = 400, ph = 200;
                    int px = (w - pw) / 2, py = (h - ph) / 2;
                    gc_rect_t attach_btn = {px + 16, py + ph - 40, 100, 28};
                    gc_rect_t cancel_btn = {px + pw - 116, py + ph - 40, 100, 28};
                    if (ev.x >= attach_btn.x && ev.x < attach_btn.x + attach_btn.w &&
                        ev.y >= attach_btn.y && ev.y < attach_btn.y + attach_btn.h) {
                        /* Attach image to composer */
                        if (app.image_paste_is_base64) {
                            snprintf(app.composer_buf, sizeof(app.composer_buf),
                                     "[image:%d bytes]", app.image_paste_data_len);
                        } else {
                            snprintf(app.composer_buf, sizeof(app.composer_buf),
                                     "[image:%s]", app.image_paste_path);
                        }
                        app.composer_pos = (int)strlen(app.composer_buf);
                        app.image_paste_active = false;
                        snprintf(app.toast_msg, sizeof(app.toast_msg), "Image attached");
                        app.toast_time = 60;
                    } else if (ev.x >= cancel_btn.x && ev.x < cancel_btn.x + cancel_btn.w &&
                               ev.y >= cancel_btn.y && ev.y < cancel_btn.y + cancel_btn.h) {
                        app.image_paste_active = false;
                    }
                    break;
                }
                /* Pet gallery selection */
                if (app.pet_show_gallery && ev.type == GC_EV_MOUSE_DOWN) {
                    int w = win_w(), h = win_h();
                    int pw = 420, ph = 340;
                    int px = (w - pw) / 2, py = (h - ph) / 2;
                    int cell_w = 90, cell_h = 90;
                    int cols = 4, start_x = px + (pw - cols * cell_w) / 2 + 10;
                    int start_y = py + 60;
                    for (int i = 0; i < app.pet_count; i++) {
                        int cx = start_x + (i % cols) * cell_w;
                        int cy = start_y + (i / cols) * cell_h;
                        if (ev.x >= cx && ev.x < cx + cell_w - 8 &&
                            ev.y >= cy && ev.y < cy + cell_h - 8) {
                            app.pet_type = i % 4;
                            app.pet_selected = i;
                            app.pet_show_gallery = false;
                            snprintf(app.toast_msg, sizeof(app.toast_msg),
                                     "Selected: %s", app.pet_names[i]);
                            app.toast_time = 60;
                            break;
                        }
                    }
                    /* Scale slider */
                    int slider_y = py + ph - 38;
                    if (ev.y >= slider_y - 8 && ev.y <= slider_y + 12) {
                        int slider_x = px + 70;
                        int slider_w = pw - 100;
                        if (ev.x >= slider_x && ev.x <= slider_x + slider_w) {
                            float t = (float)(ev.x - slider_x) / slider_w;
                            app.pet_scale = 0.5f + t * 1.5f;
                        }
                    }
                    break;
                }
                hit_t hr = hit_test(ev.x, ev.y);
                if (hr.type == HIT_NAV) {
                    app.selected_nav = hr.index;
                    app.current_view = nav_items[hr.index].view_id;
                    snprintf(app.current_view_name, sizeof(app.current_view_name), "%s", nav_items[hr.index].label);
                }
                if (hr.type == HIT_SESSION) {
                    app.selected_session = hr.index;
                    app.selected_nav = 0;
                    app.current_view = 0;
                    app.chat_scroll = 0;  /* Reset scroll on session change */
                    snprintf(app.current_view_name, sizeof(app.current_view_name), "Chat");
                    load_messages(hr.index);
                }
                if (hr.type == HIT_NEWCHAT) {
                    /* New chat: try to insert into DB, or add locally */
                    fprintf(stderr, "New Chat clicked\n");
                    if (app.db) {
                        /* Add a minimal placeholder session entry locally */
                        if (app.session_count < MAX_SESSIONS) {
                            session_entry_t *s = &app.sessions[app.session_count];
                            memset(s, 0, sizeof(*s));
                            snprintf(s->id, sizeof(s->id), "new_%ld", (long)time(NULL));
                            snprintf(s->title, sizeof(s->title), "New Chat");
                            s->started_at = (long)time(NULL);
                            s->msg_count = 0;
                            app.session_count++;
                            app.selected_session = app.session_count - 1;
                            app.selected_nav = 0;
                            app.current_view = 0;
                            app.chat_scroll = 0;
                            app.message_count = 0;
                            load_messages(app.selected_session);
                        }
                    }
                }
                if (hr.type == HIT_PILL) {
                    /* Model pill click — toggle model picker */
                    app.show_model_picker = !app.show_model_picker;
                    app.model_picker_hover = -1;
                }
                /* Titlebar left cluster: sidebar toggle (tool 0), flip panes (tool 1) */
                if (hr.type == HIT_TOOL && app.mouse_in_titlebar) {
                    if (hr.index == 0) {
                        app.sidebar_collapsed = !app.sidebar_collapsed;
                    } else if (hr.index == 1) {
                        fprintf(stderr, "Flip panes clicked\n");
                    } else if (hr.index == 2) {
                        app.haptics_muted = !app.haptics_muted;
                    } else if (hr.index == 3) {
                        fprintf(stderr, "Settings clicked\n");
                    }
                }

                /* Session header chevron click */
                if (ev.type == GC_EV_MOUSE_DOWN && app.mouse_in_chat && app.current_view == 0) {
                    int cx = chat_x(), cw = chat_w(), hy = chat_y();
                    int fh = gc_font_height(gc_get_font_small(app.win));
                    if (ev.x >= cx && ev.x < cx + cw && ev.y >= hy && ev.y < hy + fh + 12) {
                        app.show_session_menu = !app.show_session_menu;
                    }
                }

                /* Session menu actions */
                if (ev.type == GC_EV_MOUSE_DOWN && app.show_session_menu && app.session_menu_hover >= 0) {
                    if (app.session_menu_hover == 0) {
                        if (app.session_count > 0 && app.selected_session < app.session_count) {
                            int idx = app.selected_session;
                            bool already_pinned = false;
                            for (int pi = 0; pi < app.pinned_session_count; pi++) {
                                if (app.pinned_session_ids[pi] == idx) {
                                    already_pinned = true;
                                    for (int pj = pi; pj < app.pinned_session_count - 1; pj++)
                                        app.pinned_session_ids[pj] = app.pinned_session_ids[pj + 1];
                                    app.pinned_session_count--;
                                    break;
                                }
                            }
                            if (!already_pinned && app.pinned_session_count < MAX_SESSIONS) {
                                app.pinned_session_ids[app.pinned_session_count++] = idx;
                            }
                        }
                    } else if (app.session_menu_hover == 1) {
                        fprintf(stderr, "Delete session %d\n", app.selected_session);
                    } else if (app.session_menu_hover == 2) {
                        fprintf(stderr, "Archive session %d\n", app.selected_session);
                    }
                    app.show_session_menu = false;
                }

                /* Model picker item selection */
                if (ev.type == GC_EV_MOUSE_DOWN && app.show_model_picker && app.model_picker_hover >= 0) {
                    const char *models[] = {
                        "openrouter/owl-alpha", "openrouter/claude-sonnet-4", "openrouter/gpt-4o-mini",
                        "nousresearch/hermes-3-405b", "nousresearch/hermes-4-mid",
                        "claude-sonnet-4-20250514", "claude-3-5-haiku-20241022"
                    };
                    if (app.model_picker_hover < 7) {
                        snprintf(app.latest_model, sizeof(app.latest_model), "%s", models[app.model_picker_hover]);
                        if (app.selected_session >= 0 && app.selected_session < app.session_count) {
                            snprintf(app.sessions[app.selected_session].model, sizeof(app.sessions[0].model), "%s", models[app.model_picker_hover]);
                        }
                        snprintf(app.api_status, sizeof(app.api_status), "Model: %s", models[app.model_picker_hover]);
                    }
                    app.show_model_picker = false;
                }

                /* Close model picker on outside click */
                if (app.show_model_picker && ev.type == GC_EV_MOUSE_DOWN && hr.type != HIT_PILL) {
                    int comp_y2 = chat_y() + chat_h() - COMPOSER_H - 12;
                    int px = chat_x() + 24, py = comp_y2 - 280 - PILL_H - 6;
                    if (ev.x < px || ev.x >= px + 300 || ev.y < py || ev.y >= py + 280) {
                        app.show_model_picker = false;
                    }
                }

                /* Scroll-to-bottom button */
                if (ev.type == GC_EV_MOUSE_DOWN && app.scroll_button_hover) {
                    app.chat_scroll = 0;
                }
                    int mi = app.hover_message;
                    if (app.hover_action == 0) {
                        /* Copy message to clipboard */
                        SDL_SetClipboardText(app.messages[mi].content);
                        snprintf(app.api_status, sizeof(app.api_status), "Copied!");
                    } else if (app.hover_action == 1) {
                        /* Edit message: copy content into composer */
                        snprintf(app.composer_buf, sizeof(app.composer_buf), "%s", app.messages[mi].content);
                        app.composer_pos = (int)strlen(app.composer_buf);
                        app.composer_focused = true;
                        SDL_StartTextInput();
                        snprintf(app.api_status, sizeof(app.api_status), "Editing...");
                    }
                if (hr.type == HIT_SEARCH) {
                    app.search_active = true;
                    app.search_query[0] = 0;
                    app.search_query_len = 0;
                }
                if (hr.type == HIT_SESSIONS_HDR) {
                    app.sessions_expanded = !app.sessions_expanded;
                }
                if (hr.type == HIT_NAV_HDR) {
                    app.nav_expanded = !app.nav_expanded;
                }
                /* Composer click-to-focus */
                if (ev.type == GC_EV_MOUSE_DOWN && hr.type == HIT_COMPOSER) {
                    app.composer_focused = true;
                    SDL_StartTextInput();
                } else if (ev.type == GC_EV_MOUSE_DOWN && hr.type != HIT_COMPOSER &&
                           hr.type != HIT_CHAT && hr.type != HIT_SESSION && hr.type != HIT_NAV) {
                    if (app.composer_focused) {
                        app.composer_focused = false;
                        SDL_StopTextInput();
                    }
                }
                break;
            }
            case GC_EV_RESIZE: break;
            default: break;
            }
        }

        gc_begin_frame(app.win);
        app.theme = gc_get_theme(app.win);
        draw_titlebar();
        draw_sidebar();
        draw_session_header();
        draw_chat_area();
        draw_model_picker();
        draw_scroll_button();
        draw_statusbar();
        draw_pet();
        draw_pet_gallery();
        draw_voice_indicator();
        draw_command_palette();
        draw_image_paste_overlay();
        draw_toast();
        gc_end_frame(app.win);

        static bool first_frame = true;
        if (first_frame) { first_frame = false; gc_save_screenshot(app.win, "/tmp/slermes-gui.png"); }
    }

    gc_destroy_window(app.win);
    gc_quit();
    db_close();
    return 0;
}
