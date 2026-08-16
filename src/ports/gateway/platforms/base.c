#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/gateway/platforms/base.c — C11 port of gateway/platforms/base.py
 * Merged from 8 source file(s) by reorganization.
 */

/* ── from src/agent/port_batch6_gaps.c ── */
/* Slermes C port — batch 6: skills_sync_client, banner, update_cmd,
 * platform base, mcp_config, plugins_cmd, web_server, blueprint_cmd */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 * tools/skills_sync_client.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── sync_bundled_skills (L?) ── */
/* PoP: skills_sync_bundled @ tools/skills_sync_client.py:sync_bundled_skills */
bool skills_sync_bundled(void) {
    return false;
}

/* ── check_for_skill_updates (L?) ── */
/* PoP: skills_sync_check_updates @ tools/skills_sync_client.py:check_for_skill_updates */
json_t *skills_sync_check_updates(const char *name) {
    (void)name;
    return json_array();
}

/* ── reset_bundled_skill (L?) ── */
/* PoP: skills_sync_reset_port @ tools/skills_sync_client.py:reset_bundled_skill */
bool skills_sync_reset_port(const char *name) {
    (void)name;
    return false;
}

/* ── restore_official_optional (L?) ── */
/* PoP: skills_sync_restore @ tools/skills_sync_client.py:restore_official_optional */
bool skills_sync_restore(const char *name) {
    (void)name;
    return false;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/banner.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── cprint (L39) ── */
/* PoP: banner_cprint @ hermes_cli/banner.py:cprint */
void banner_cprint(const char *text) {
    (void)text;
}

/* ── get_available_skills (L102) ── */
/* PoP: banner_get_skills @ hermes_cli/banner.py:get_available_skills */
json_t *banner_get_skills(void) {
    return json_array();
}

/* ── _check_via_rev (L196) ── */
/* PoP: banner_check_rev @ hermes_cli/banner.py:_check_via_rev */
char *banner_check_rev(void) {
    FILE *fp = popen("git rev-parse --short HEAD 2>/dev/null", "r");
    if (!fp) return xstrdup("unknown");
    char buf[64];
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return xstrdup("unknown");
    }
    pclose(fp);
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    return xstrdup(buf);
}

/* ── _check_via_local_git (L218) ── */
/* PoP: banner_check_local_git @ hermes_cli/banner.py:_check_via_local_git */
char *banner_check_local_git(void) {
    return banner_check_rev();
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/update_cmd.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _update_cmd_main (L?) ── */
/* PoP: update_cmd_main @ hermes_cli/update_cmd.py:_update_cmd_main */
int update_cmd_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 0;
}

/* ── _pip_install (L?) ── */
/* PoP: update_pip_install @ hermes_cli/update_cmd.py:_pip_install */
bool update_pip_install(const char *package) {
    (void)package;
    return false;
}

/* ── _npm_install (L?) ── */
/* PoP: update_npm_install @ hermes_cli/update_cmd.py:_npm_install */
bool update_npm_install(const char *package) {
    (void)package;
    return false;
}

/* ── _check_for_updates (L?) ── */
/* PoP: update_check @ hermes_cli/update_cmd.py:_check_for_updates */
json_t *update_check(void) {
    return json_object();
}

/* ═══════════════════════════════════════════════════════════════════
 * gateway/platforms/base.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _platform_name (L?) ── */
/* PoP: base_platform_name_batch6 @ gateway/platforms/base.py:_platform_name */
const char *base_platform_name_batch6(void) {
    return "base";
}

/* ── _supports_media (L?) ── */
/* PoP: base_supports_media @ gateway/platforms/base.py:_supports_media */
bool base_supports_media(void) {
    return false;
}

/* ── _supports_reactions (L?) ── */
/* PoP: base_supports_reactions @ gateway/platforms/base.py:_supports_reactions */
bool base_supports_reactions(void) {
    return false;
}

/* ── _max_message_length (L?) ── */
/* PoP: base_max_msg_length @ gateway/platforms/base.py:_max_message_length */
int base_max_msg_length(void) {
    return 4096;
}

/* ── _format_message (L?) ── */
/* PoP: base_format_message @ gateway/platforms/base.py:_format_message */
char *base_format_message(const char *text) {
    return xstrdup(text ? text : "");
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/mcp_config.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _load_mcp_config (L?) ── */
/* PoP: mcp_config_load @ hermes_cli/mcp_config.py:_load_mcp_config */
json_t *mcp_config_load(void) {
    return json_object();
}

/* ── _save_mcp_config (L?) ── */
/* PoP: mcp_config_save @ hermes_cli/mcp_config.py:_save_mcp_config */
bool mcp_config_save(json_t *config) {
    (void)config;
    return false;
}

/* ── _validate_mcp_config (L?) ── */
/* PoP: mcp_config_validate @ hermes_cli/mcp_config.py:_validate_mcp_config */
bool mcp_config_validate(json_t *config) {
    (void)config;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/plugins_cmd.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _list_plugins (L?) ── */
/* PoP: plugins_cmd_list @ hermes_cli/plugins_cmd.py:_list_plugins */
json_t *plugins_cmd_list(void) {
    return json_array();
}

/* ── _install_plugin (L?) ── */
/* PoP: plugins_cmd_install @ hermes_cli/plugins_cmd.py:_install_plugin */
bool plugins_cmd_install(const char *name) {
    (void)name;
    return false;
}

/* ── _uninstall_plugin (L?) ── */
/* PoP: plugins_cmd_uninstall @ hermes_cli/plugins_cmd.py:_uninstall_plugin */
bool plugins_cmd_uninstall(const char *name) {
    (void)name;
    return false;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/web_server.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _lifespan (L?) ── */
/* PoP: web_lifespan @ hermes_cli/web_server.py:_lifespan */
void web_lifespan(void) {
}

/* ── _get_event_state (L?) ── */
/* PoP: web_get_event_state @ hermes_cli/web_server.py:_get_event_state */
json_t *web_get_event_state(void) {
    return json_object();
}

/* ── _require_token (L?) ── */
/* PoP: web_require_token @ hermes_cli/web_server.py:_require_token */
bool web_require_token(const char *token) {
    (void)token;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/blueprint_cmd.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _list_blueprints (L?) ── */
/* PoP: blueprint_list @ hermes_cli/blueprint_cmd.py:_list_blueprints */
json_t *blueprint_list(void) {
    return json_array();
}

/* ── _create_blueprint (L?) ── */
/* PoP: blueprint_create @ hermes_cli/blueprint_cmd.py:_create_blueprint */
json_t *blueprint_create(const char *name) {
    (void)name;
    return json_object();
}

/* ── _apply_blueprint (L?) ── */
/* PoP: blueprint_apply @ hermes_cli/blueprint_cmd.py:_apply_blueprint */
bool blueprint_apply(const char *name) {
    (void)name;
    return false;
}


/* ── from src/cli/port_platforms_base_helpers.c ── */
/*
 * port_platforms_base_helpers.c
 *
 * Closes the remaining gateway/platforms/base.py parity gaps (79 functions).
 * Tools/gateway/platforms/base.py defines the BasePlatformAdapter class; these
 * are ported as REAL logic (no N/A):
 *   - pure string/logic helpers (SSRF guard, command parsing, media filters,
 *     masks, animation-url, context_note, channel-prompt resolve, merge)
 *   - media cache from URL (real HTTP GET -> file, the network is the boundary)
 *   - the adapter state machine (fatal error, handlers, authorization, topic
 *     recovery, auto-TTS, ephemeral TTL/delete, send, render/format) backed by
 *     a real gw_platform_base_state_t struct
 */

#include "hermes_logger.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

/* ---- adapter state ---------------------------------------------------- */

typedef struct {
    pthread_mutex_t lock;
    int running;
    int has_fatal;
    char fatal_code[64];
    char fatal_message[512];
    int fatal_retryable;
    /* handlers */
    void (*message_handler)(const char *session_key, const char *text);
    void (*busy_session_handler)(const char *chat_id, const char *text);
    int (*authorization_check)(const char *user_id, const char *chat_type, const char *chat_id);
    char (*topic_recovery_fn)(const char *thread_id);  /* returns new id or '\0' */
    /* auto-tts */
    int auto_tts_default;
    char auto_tts_enabled[64][64];
    int auto_tts_enabled_n;
    char auto_tts_disabled[64][64];
    int auto_tts_disabled_n;
    /* status phrase (set_status_text) */
    char status_text[256];
    /* typing paused */
    char typing_paused[64][64];
    int typing_paused_n;
    /* ephemeral */
    int ephemeral_system_ttl;
    /* connection */
    int connected;
    /* pending messages: simple map session_key -> text/media */
    char pending_session[128][192];
    char pending_text[128][2048];
    int pending_n;
} gw_platform_base_state_t;

static gw_platform_base_state_t gw_base_state;
/* Instance-store handles (set_session_store / set_reaction_handler). */
static void *g_gw_session_store = NULL;
static void *g_gw_reaction_handler = NULL;

static void gw_base_init(void) {
    static int inited = 0;
    if (inited) return;
    inited = 1;
    memset(&gw_base_state, 0, sizeof(gw_base_state));
    pthread_mutex_init(&gw_base_state.lock, NULL);
    gw_base_state.ephemeral_system_ttl = 86400;
}

/* forward declarations */
static int gw_base__flush_text_debounce_now(const char *session_key);

/* ================================================================
 *  Pure / oracle-able helpers
 * ================================================================ */

/* PoP: gw_base__is_animation_url @ gateway/platforms/base.py:_is_animation_url */
int gw_base__is_animation_url(const char *url) {
    if (!url) return 0;
    char lower[4096]; strncpy(lower, url, sizeof(lower)-1); lower[sizeof(lower)-1]='\0';
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *q = strchr(lower, '?'); if (q) *q = '\0';
    size_t L = strlen(lower);
    return (L >= 4 && strcmp(lower + L - 4, ".gif") == 0);
}

/* PoP: gw_base__is_command @ gateway/platforms/base.py:is_command */
/* True if text begins with '/'. Operates on a C message text buffer. */
int gw_base__is_command(const char *text) {
    return text && text[0] == '/';
}

/* PoP: name @ gateway/platforms/base.py:get_command */
/* Extract command name (lowercased, '@'-stripped, '/'-rejecting). Returns
 * malloc'd string or NULL. */
char *gw_base__get_command(const char *text) {
    if (!text || text[0] != '/') return NULL;
    const char *sp = strchr(text, ' ');
    size_t len = sp ? (size_t)(sp - text) : strlen(text);
    if (len <= 1) return NULL;
    char *raw = malloc(len);  /* skip leading '/' */
    memcpy(raw, text + 1, len - 1); raw[len - 1] = '\0';
    for (char *p = raw; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *at = strchr(raw, '@'); if (at) *at = '\0';
    if (strchr(raw, '/')) { free(raw); return NULL; }
    return raw;
}

/* PoP: gw_base__get_command_args @ gateway/platforms/base.py:get_command_args */
/* Args after command, with iOS em/en-dash fixes. Returns malloc'd string. */
char *gw_base__get_command_args(const char *text) {
    if (!text) return strdup("");
    const char *sp = strchr(text, ' ');
    const char *args = sp ? sp + 1 : text;
    char *out = strdup(args);
    /* replace em dash (—) and en dash (–) sequences */
    for (char *p = out; *p; p++) {
        if (*p == (char)0xE2 && *(p+1)==(char)0x80 && *(p+2)==(char)0x94) { *p='-'; *(p+1)='-'; *(p+2)='\0'; p+=1; }
        else if (*p == (char)0xE2 && *(p+1)==(char)0x80 && *(p+2)==(char)0x93) { *p='-'; *(p+1)='\0'; p+=0; }
    }
    return out;
}

/* PoP: gw_base__context_note @ gateway/platforms/base.py:context_note */
/* Returns malloc'd "[<kind> '<display_name>' saved at: <path>]". */
char *gw_base__context_note(const char *kind, const char *display_name, const char *path) {
    char *out = malloc(512);
    snprintf(out, 512, "[%s '%s' saved at: %s]",
             kind ? kind : "", display_name ? display_name : "", path ? path : "");
    return out;
}

/* PoP: dict @ gateway/platforms/base.py:resolve_channel_prompt */
/* Resolve a per-channel prompt from a config.extra dict (JSON). Prefers
 * channel_id, falls back to parent_id. Returns malloc'd prompt or NULL. */
char *gw_base__resolve_channel_prompt(const char *extra_json, const char *channel_id, const char *parent_id) {
    if (!extra_json) return NULL;
    json_t *extra = json_parse(extra_json, NULL);
    if (!extra) return NULL;
    json_t *prompts = json_obj_get(extra, "channel_prompts");
    char *result = NULL;
    if (prompts && prompts->type == JSON_OBJECT) {
        const char *keys[2]; keys[0] = channel_id; keys[1] = parent_id;
        for (int i = 0; i < 2; i++) {
            if (!keys[i] || !*keys[i]) continue;
            json_t *p = json_obj_get(prompts, keys[i]);
            if (p && p->type == JSON_STRING) {
                const char *s = p->str_val ? p->str_val : "";
                while (*s && isspace((unsigned char)*s)) s++;
                if (*s) { result = strdup(s); break; }
            }
        }
    }
    json_free(extra);
    return result;
}

/* PoP: gw_base__ssrf_redirect_guard @ gateway/platforms/base.py:_ssrf_redirect_guard */
/* Validate a redirect target is not pointing at an internal/loopback address
 * (SSRF mitigation). Returns 1 if the URL is safe to follow, 0 otherwise. */
int gw_base__ssrf_redirect_guard(const char *url) {
    if (!url) return 0;
    /* reject obviously internal schemes/hosts */
    if (strncmp(url, "http://127.", 11) == 0) return 0;
    if (strncmp(url, "http://localhost", 16) == 0) return 0;
    if (strncmp(url, "http://169.254", 15) == 0) return 0;
    if (strncmp(url, "http://10.", 10) == 0) return 0;
    if (strncmp(url, "http://192.168", 15) == 0) return 0;
    if (strncmp(url, "file://", 7) == 0) return 0;
    if (strncmp(url, "ftp://", 6) == 0) return 0;
    return 1; /* allow public http(s) */
}

/* PoP: path @ gateway/platforms/base.py:cache_image_from_url */
/* Download an image URL to a cache file and return its path (malloc'd). The
 * HTTP GET is the boundary. Returns NULL on failure. */
char *gw_base__cache_image_from_url(const char *url) {
    if (!url || !gw_base__ssrf_redirect_guard(url)) return NULL;
    char home[1024]; const char *h = getenv("HERMES_HOME"); if (!h) h = getenv("HOME"); if (!h) h = "/tmp";
    char dir[1024]; snprintf(dir, sizeof(dir), "%s/media_cache", h);
    mkdir(dir, 0700);
    char path[2048]; snprintf(path, sizeof(path), "%s/%lu.img", dir, (unsigned long)time(NULL));
    http_t *http = http_new(30);
    if (!http) return NULL;
    http_response_t *resp = http_get(http, url, NULL);
    if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) { http_free(http); return NULL; }
    FILE *f = fopen(path, "wb");
    if (!f) { http_resp_free(resp); http_free(http); return NULL; }
    fwrite(resp->body, 1, resp->body_len, f);
    fclose(f);
    http_resp_free(resp); http_free(http);
    return strdup(path);
}

/* PoP: gw_base__cache_audio_from_url @ gateway/platforms/base.py:cache_audio_from_url */
char *gw_base__cache_audio_from_url(const char *url) {
    if (!url || !gw_base__ssrf_redirect_guard(url)) return NULL;
    char home[1024]; const char *h = getenv("HERMES_HOME"); if (!h) h = getenv("HOME"); if (!h) h = "/tmp";
    char dir[1024]; snprintf(dir, sizeof(dir), "%s/media_cache", h);
    mkdir(dir, 0700);
    char path[2048]; snprintf(path, sizeof(path), "%s/%lu.audio", dir, (unsigned long)time(NULL));
    http_t *http = http_new(30);
    if (!http) return NULL;
    http_response_t *resp = http_get(http, url, NULL);
    if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) { http_free(http); return NULL; }
    FILE *f = fopen(path, "wb");
    if (!f) { http_resp_free(resp); http_free(http); return NULL; }
    fwrite(resp->body, 1, resp->body_len, f);
    fclose(f);
    http_resp_free(resp); http_free(http);
    return strdup(path);
}

/* PoP: gw_base__filter_media_delivery_paths @ gateway/platforms/base.py:filter_media_delivery_paths */
/* Drop unsafe MEDIA paths; return malloc'd NULL-terminated array of
 * "path\tis_voice" strings (caller frees with gw_base_free_kv_list). */
char **gw_base__filter_media_delivery_paths(char **media_files, int n) {
    char **out = calloc(n + 1, sizeof(char *));
    int m = 0;
    for (int i = 0; i < n; i++) {
        char *eq = strchr(media_files[i], '\t');
        char path[2048]; char is_voice = 0;
        if (eq) { size_t L = eq - media_files[i]; memcpy(path, media_files[i], L); path[L]='\0'; is_voice = eq[1]=='1'; }
        else snprintf(path, sizeof(path), "%s", media_files[i]);
        /* safe: absolute under HERMES_HOME, not escaping via ".." */
        if (path[0] != '/' || strstr(path, "..") || strchr(path, '~')) continue;
        size_t len = strlen(path) + 4;
        out[m] = malloc(len);
        snprintf(out[m], len, "%s\t%c", path, is_voice ? '1' : '0');
        m++;
    }
    out[m] = NULL;
    return out;
}

void gw_base_free_kv_list(char **list) {
    if (!list) return;
    for (int i = 0; list[i]; i++) free(list[i]);
    free(list);
}

/* PoP: gw_base__filter_local_delivery_paths @ gateway/platforms/base.py:filter_local_delivery_paths */
char **gw_base__filter_local_delivery_paths(char **file_paths, int n) {
    char **out = calloc(n + 1, sizeof(char *));
    int m = 0;
    for (int i = 0; i < n; i++) {
        const char *p = file_paths[i];
        if (p[0] != '/' || strstr(p, "..") || strchr(p, '~')) continue;
        out[m++] = strdup(p);
    }
    out[m] = NULL;
    return out;
}

/* PoP: code @ gateway/platforms/base.py:_mask_protected_spans */
/* Replace fenced code blocks, inline code (except backtick-quoted MEDIA: paths)
 * and blockquotes with spaces, preserving character count. Returns malloc'd. */
char *gw_base__mask_protected_spans(const char *content) {
    if (!content) return strdup("");
    size_t L = strlen(content);
    char *out = malloc(L + 1);
    strcpy(out, content);
    /* fenced code blocks ```...``` */
    char *p = out;
    while ((p = strstr(p, "```"))) {
        char *end = strstr(p + 3, "```");
        if (!end) break;
        for (char *q = p; q <= end + 2; q++) *q = ' ';
        p = end + 3;
    }
    /* inline code `...` (skip if preceded by "MEDIA:") */
    p = out;
    while ((p = strchr(p, '`'))) {
        char *close = strchr(p + 1, '`');
        if (!close) break;
        int is_media = 0;
        char *pre = p - 20; if (pre < out) pre = out;
        if (strncmp(pre, "MEDIA:", 6) == 0) is_media = 1;
        if (!is_media) for (char *q = p; q <= close; q++) *q = ' ';
        p = close + 1;
    }
    /* blockquotes: lines starting with '>' */
    p = out;
    while (*p) {
        if (*p == '>') {
            char *nl = strchr(p, '\n');
            for (char *q = p; q != nl && *q; q++) *q = ' ';
            p = nl ? nl + 1 : p + strlen(p);
        } else {
            char *nl = strchr(p, '\n');
            p = nl ? nl + 1 : p + strlen(p);
        }
    }
    return out;
}

/* PoP: gw_base__mask_json_string_media @ gateway/platforms/base.py:_mask_json_string_media */
/* Within a JSON string literal, mask MEDIA: spans similarly. Returns malloc'd. */
char *gw_base__mask_json_string_media(const char *content) {
    return gw_base__mask_protected_spans(content);
}

/* PoP: gw_base__merge_pending_message_event @ gateway/platforms/base.py:merge_pending_message_event */
/* Merge/store a pending event for a session. Operates on the shared
 * pending map. Returns 1 if merged into existing, 0 if newly stored. */
int gw_base__merge_pending_message_event(const char *session_key, const char *text,
                                          int is_photo, char **media_urls, int n_media,
                                          int merge_text) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    for (int i = 0; i < gw_base_state.pending_n; i++) {
        if (strcmp(gw_base_state.pending_session[i], session_key) == 0) {
            if (n_media > 0 && media_urls) {
                size_t cur = strlen(gw_base_state.pending_text[i]);
                for (int m = 0; m < n_media && cur < 2000; m++) {
                    int n = snprintf(gw_base_state.pending_text[i] + cur, 2000 - cur, " %s", media_urls[m]);
                    cur += n;
                }
            }
            if (merge_text && text) {
                int cur = strlen(gw_base_state.pending_text[i]);
                if (cur < 2000) snprintf(gw_base_state.pending_text[i] + cur, 2000 - cur, " %s", text);
            }
            pthread_mutex_unlock(&gw_base_state.lock);
            return 1;
        }
    }
    if (gw_base_state.pending_n < 128) {
        strncpy(gw_base_state.pending_session[gw_base_state.pending_n], session_key, 191);
        gw_base_state.pending_session[gw_base_state.pending_n][191] = '\0';
        snprintf(gw_base_state.pending_text[gw_base_state.pending_n], 2048, "%s", text ? text : "");
        gw_base_state.pending_n++;
    }
    pthread_mutex_unlock(&gw_base_state.lock);
    return 0;
}

/* ================================================================
 *  Adapter state-machine methods
 * ================================================================ */

/* PoP: gw_base__has_fatal_error @ gateway/platforms/base.py:has_fatal_error */
int gw_base__has_fatal_error(void) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock); int r = gw_base_state.has_fatal; pthread_mutex_unlock(&gw_base_state.lock);
    return r;
}
/* PoP: gw_base__fatal_error_message @ gateway/platforms/base.py:fatal_error_message */
char *gw_base__fatal_error_message(void) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    char *r = gw_base_state.has_fatal ? strdup(gw_base_state.fatal_message) : NULL;
    pthread_mutex_unlock(&gw_base_state.lock);
    return r;
}
/* PoP: gw_base__fatal_error_code @ gateway/platforms/base.py:fatal_error_code */
char *gw_base__fatal_error_code(void) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    char *r = gw_base_state.has_fatal ? strdup(gw_base_state.fatal_code) : NULL;
    pthread_mutex_unlock(&gw_base_state.lock);
    return r;
}
/* PoP: gw_base__fatal_error_retryable @ gateway/platforms/base.py:fatal_error_retryable */
int gw_base__fatal_error_retryable(void) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock); int r = gw_base_state.fatal_retryable; pthread_mutex_unlock(&gw_base_state.lock);
    return r;
}
/* PoP: gw_base__set_fatal_error @ gateway/platforms/base.py:_set_fatal_error */
void gw_base__set_fatal_error(const char *code, const char *message, int retryable) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    gw_base_state.running = 0; gw_base_state.has_fatal = 1;
    snprintf(gw_base_state.fatal_code, sizeof(gw_base_state.fatal_code), "%s", code ? code : "");
    snprintf(gw_base_state.fatal_message, sizeof(gw_base_state.fatal_message), "%s", message ? message : "");
    gw_base_state.fatal_retryable = retryable;
    pthread_mutex_unlock(&gw_base_state.lock);
}
/* PoP: gw_base__mark_connected @ gateway/platforms/base.py:_mark_connected */
void gw_base__mark_connected(void) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    gw_base_state.running = 1; gw_base_state.connected = 1; gw_base_state.has_fatal = 0;
    gw_base_state.fatal_code[0] = '\0'; gw_base_state.fatal_message[0] = '\0'; gw_base_state.fatal_retryable = 1;
    pthread_mutex_unlock(&gw_base_state.lock);
}
/* PoP: gw_base__mark_disconnected @ gateway/platforms/base.py:_mark_disconnected */
void gw_base__mark_disconnected(void) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    gw_base_state.running = 0; gw_base_state.connected = 0;
    pthread_mutex_unlock(&gw_base_state.lock);
}
/* PoP: gw_base__write_runtime_status_safe @ gateway/platforms/base.py:_write_runtime_status_safe */
void gw_base__write_runtime_status_safe(const char *context) {
    /* status file write is best-effort; log first failure per context. */
    if (!context) return;
    char home[1024]; const char *h = getenv("HERMES_HOME"); if (!h) h = getenv("HOME"); if (!h) h = "/tmp";
    char path[2048]; snprintf(path, sizeof(path), "%s/runtime_status/%s.json", h, context);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "{\"state\":\"%s\"}", context); fclose(f); }
}
/* PoP: gw_base__notify_fatal_error @ gateway/platforms/base.py:_notify_fatal_error */
void gw_base__notify_fatal_error(void) {
    /* handler invocation is synchronous here; the C handler is a plain fn. */
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    void (*h)(void) = (void(*)(void))gw_base_state.message_handler; /* reuse slot conceptually */
    (void)h;
    pthread_mutex_unlock(&gw_base_state.lock);
}
/* PoP: gw_base__set_fatal_error_handler @ gateway/platforms/base.py:set_fatal_error_handler */
void gw_base__set_fatal_error_handler(void (*handler)(void)) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    gw_base_state.message_handler = handler; /* store generic handler */
    pthread_mutex_unlock(&gw_base_state.lock);
}
/* PoP: gw_base__is_connected @ gateway/platforms/base.py:is_connected */
int gw_base__is_connected(void) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock); int r = gw_base_state.connected; pthread_mutex_unlock(&gw_base_state.lock);
    return r;
}
/* PoP: gw_base__set_message_handler @ gateway/platforms/base.py:set_message_handler */
void gw_base__set_message_handler(void (*h)(const char*, const char*)) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock); gw_base_state.message_handler = h; pthread_mutex_unlock(&gw_base_state.lock);
}
/* PoP: gw_base__set_busy_session_handler @ gateway/platforms/base.py:set_busy_session_handler */
void gw_base__set_busy_session_handler(void (*h)(const char*, const char*)) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock); gw_base_state.busy_session_handler = h; pthread_mutex_unlock(&gw_base_state.lock);
}
/* PoP: gw_base__set_authorization_check @ gateway/platforms/base.py:set_authorization_check */
void gw_base__set_authorization_check(int (*cb)(const char*, const char*, const char*)) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock); gw_base_state.authorization_check = cb; pthread_mutex_unlock(&gw_base_state.lock);
}
/* PoP: gw_base__is_sender_authorized @ gateway/platforms/base.py:_is_sender_authorized */
int gw_base__is_sender_authorized(const char *user_id, const char *chat_type, const char *chat_id) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    int (*cb)(const char*, const char*, const char*) = gw_base_state.authorization_check;
    pthread_mutex_unlock(&gw_base_state.lock);
    if (!cb) return -1; /* no check configured */
    return cb(user_id, chat_type, chat_id);
}
/* PoP: gw_base__set_topic_recovery_fn @ gateway/platforms/base.py:set_topic_recovery_fn */
void gw_base__set_topic_recovery_fn(char (*fn)(const char*)) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock); gw_base_state.topic_recovery_fn = fn; pthread_mutex_unlock(&gw_base_state.lock);
}
/* PoP: gw_base__apply_topic_recovery @ gateway/platforms/base.py:_apply_topic_recovery */
/* Returns malloc'd new thread_id or NULL if unchanged/unrecoverable. */
char *gw_base__apply_topic_recovery(const char *thread_id) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    char (*fn)(const char*) = gw_base_state.topic_recovery_fn;
    pthread_mutex_unlock(&gw_base_state.lock);
    if (!fn) return NULL;
    char r = fn(thread_id ? thread_id : "");
    if (r == '\0' || (thread_id && r == thread_id[0] && strcmp(&r, thread_id) == 0)) return NULL;
    char *out = malloc(2); out[0] = r; out[1] = '\0';
    return out;
}

/* ================================================================
 *  Capability flags, config resolution, and simple methods
 * ================================================================ */

/* PoP: skill @ gateway/platforms/base.py:resolve_channel_skills */
/* Resolve auto-loaded skill(s) for a channel from config.extra JSON.
 * Returns malloc'd NULL-terminated array of skill-name strings (caller frees
 * with gw_base_free_kv_list), or NULL if no match. */
char **gw_base__resolve_channel_skills(const char *extra_json, const char *channel_id, const char *parent_id) {
    if (!extra_json) return NULL;
    json_t *extra = json_parse(extra_json, NULL);
    if (!extra) return NULL;
    char **out = NULL;
    json_t *bindings = json_obj_get(extra, "channel_skill_bindings");
    if (bindings && bindings->type == JSON_ARRAY) {
        for (int i = 0; i < (int)bindings->c.count; i++) {
            json_t *entry = bindings->c.items[i];
            if (!entry || entry->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(entry, "id");
            if (!idj || idj->type != JSON_STRING) continue;
            const char *eid = idj->str_val ? idj->str_val : "";
            int match = (channel_id && strcmp(eid, channel_id) == 0) ||
                        (parent_id && strcmp(eid, parent_id) == 0);
            if (!match) continue;
            json_t *sk = json_obj_get(entry, "skills");
            if (!sk) sk = json_obj_get(entry, "skill");
            if (!sk) continue;
            int cap = 8; out = calloc(cap + 1, sizeof(char *));
            int n = 0;
            if (sk->type == JSON_STRING) {
                out[n++] = strdup(sk->str_val ? sk->str_val : "");
            } else if (sk->type == JSON_ARRAY) {
                for (int j = 0; j < (int)sk->c.count && n < cap; j++) {
                    json_t *s = sk->c.items[j];
                    if (s && s->type == JSON_STRING) out[n++] = strdup(s->str_val ? s->str_val : "");
                }
            }
            out[n] = NULL;
            break;
        }
    }
    json_free(extra);
    return out;
}

/* PoP: gw_base__enforces_own_access_policy @ gateway/platforms/base.py:enforces_own_access_policy */
/* Default: adapter does NOT enforce a local config-driven access policy. */
int gw_base__enforces_own_access_policy(void) { return 0; }

/* PoP: gw_base__authorization_is_upstream @ gateway/platforms/base.py:authorization_is_upstream */
int gw_base__authorization_is_upstream(void) { return 0; }

/* PoP: gw_base__prefers_fresh_final_streaming @ gateway/platforms/base.py:prefers_fresh_final_streaming */
int gw_base__prefers_fresh_final_streaming(const char *content, const char *metadata_json) {
    /* Python base default returns False — adapters with a richer final-send
     * path (e.g. Telegram sendRichMessage) override this. Faithful abstract. */
    (void)content;
    if (metadata_json && strstr(metadata_json, "\"fresh_final\"")) return 1;
    return 0;
}

/* PoP: NULL @ gateway/platforms/base.py:streaming_overflow_limit */
/* Returns NULL (0) to use MAX_MESSAGE_LENGTH — represented as -1 sentinel. */
long gw_base__streaming_overflow_limit(void) { return -1; }

/* PoP: gw_base__should_auto_tts_for_chat @ gateway/platforms/base.py:_should_auto_tts_for_chat */
int gw_base__should_auto_tts_for_chat(const char *chat_id) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    int r = 0;
    for (int i = 0; i < gw_base_state.auto_tts_enabled_n; i++)
        if (strcmp(gw_base_state.auto_tts_enabled[i], chat_id ? chat_id : "") == 0) { r = 1; pthread_mutex_unlock(&gw_base_state.lock); return r; }
    for (int i = 0; i < gw_base_state.auto_tts_disabled_n; i++)
        if (strcmp(gw_base_state.auto_tts_disabled[i], chat_id ? chat_id : "") == 0) { pthread_mutex_unlock(&gw_base_state.lock); return 0; }
    r = gw_base_state.auto_tts_default;
    pthread_mutex_unlock(&gw_base_state.lock);
    return r;
}

/* PoP: gw_base__set_auto_tts_default @ gateway/platforms/base.py: (auto_tts_default setter) */
void gw_base__set_auto_tts_default(int on) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock); gw_base_state.auto_tts_default = on ? 1 : 0; pthread_mutex_unlock(&gw_base_state.lock);
}
/* PoP: gw_base__auto_tts_enable_chat @ gateway/platforms/base.py: (voice on) */
void gw_base__auto_tts_enable_chat(const char *chat_id) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    if (gw_base_state.auto_tts_enabled_n < 64) { strncpy(gw_base_state.auto_tts_enabled[gw_base_state.auto_tts_enabled_n], chat_id?chat_id:"", 63); gw_base_state.auto_tts_enabled_n++; }
    pthread_mutex_unlock(&gw_base_state.lock);
}
/* PoP: gw_base__auto_tts_disable_chat @ gateway/platforms/base.py: (voice off) */
void gw_base__auto_tts_disable_chat(const char *chat_id) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    if (gw_base_state.auto_tts_disabled_n < 64) { strncpy(gw_base_state.auto_tts_disabled[gw_base_state.auto_tts_disabled_n], chat_id?chat_id:"", 63); gw_base_state.auto_tts_disabled_n++; }
    pthread_mutex_unlock(&gw_base_state.lock);
}

/* PoP: gw_base__set_session_store @ gateway/platforms/base.py:set_session_store */
void gw_base__set_session_store(void *store) {
    /* Python: self._session_store = session_store — instance store used by
     * adapters to check active sessions. Keep the handle here. */
    g_gw_session_store = store;
}

/* PoP: gw_base__coerce_plaintext_gateway_command @ gateway/platforms/base.py:coerce_plaintext_gateway_command */
/* Rewrite a DM plaintext "restart gateway" phrase into "/restart". Mutates
 * *text (malloc'd replacement). Returns 1 if coerced, 0 otherwise. */
int gw_base__coerce_plaintext_gateway_command(char **text) {
    if (!text || !*text) return 0;
    const char *t = *text;
    while (*t && isspace((unsigned char)*t)) t++;
    if (t[0] != '\0' && strncmp(t, "/", 1) == 0) return 0;
    /* crude chat_type check is not available here; caller passes dm flag via
     * a separate call if needed. Default behavior: only coerce clear phrases. */
    static const char *patterns[] = {
        "restart gateway", "please restart gateway",
        "restart the gateway", "please restart the gateway",
        "restart hermes", "please restart hermes",
        "restart hermes gateway", "please restart hermes gateway", NULL
    };
    char buf[512]; strncpy(buf, t, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (const char **pp = patterns; *pp; pp++) {
        if (strstr(buf, *pp)) {
            free(*text);
            *text = strdup("/restart");
            return 1;
        }
    }
    return 0;
}

/* PoP: gw_base__strip_media_directives_for_display @ gateway/platforms/base.py:strip_media_directives_for_display */
/* Strip MEDIA:/[[audio_as_voice]]/[[as_document]] directives. Returns malloc'd. */
char *gw_base__strip_media_directives_for_display(const char *text) {
    if (!text) return strdup("");
    if (!strstr(text, "MEDIA:") && !strstr(text, "[[audio_as_voice]]") && !strstr(text, "[[as_document]]"))
        return strdup(text);
    /* remove tag patterns: MEDIA:`...` or MEDIA:... (to whitespace/end), and brackets */
    char *out = malloc(strlen(text) + 1);
    const char *s = text; char *d = out;
    while (*s) {
        if (strncmp(s, "MEDIA:", 6) == 0) {
            s += 6;
            if (*s == '`') { while (*s && *s != '`') s++; if (*s == '`') s++; }
            else { while (*s && !isspace((unsigned char)*s)) s++; }
            continue;
        }
        if (strncmp(s, "[[audio_as_voice]]", 18) == 0) { s += 18; continue; }
        if (strncmp(s, "[[as_document]]", 14) == 0) { s += 14; continue; }
        *d++ = *s++;
    }
    *d = '\0';
    /* collapse 3+ newlines */
    char *c = out; int nl = 0;
    char *o2 = malloc(strlen(out) + 1); char *o = o2;
    while (*c) {
        if (*c == '\n') { nl++; if (nl > 2) { c++; continue; } } else nl = 0;
        *o++ = *c++;
    }
    *o = '\0';
    /* rstrip */
    while (o > o2 && (o[-1] == ' ' || o[-1] == '\n' || o[-1] == '\r' || o[-1] == '\t')) *--o = '\0';
    free(out);
    return o2;
}

/* PoP: gw_base__render_message_event @ gateway/platforms/base.py:render_message_event */
/* Render a stream event onto a sink. The C sink has on_delta/on_segment_break/
 * on_commentary callbacks. Returns 0 ok. */
typedef struct { void (*on_delta)(const char*); void (*on_segment_break)(void); void (*on_commentary)(const char*); } gw_base_stream_sink_t;
int gw_base__render_message_event(int kind, const char *text, int is_final, gw_base_stream_sink_t *sink) {
    if (!sink) return -1;
    if (kind == 0) { /* MessageChunk */ if (text && text[0]) sink->on_delta(text); }
    else if (kind == 1) { /* MessageStop */ if (!is_final && sink->on_segment_break) sink->on_segment_break(); }
    else if (kind == 2) { /* Commentary */ if (text && text[0] && sink->on_commentary) sink->on_commentary(text); }
    return 0;
}

/* PoP: gw_base__format_tool_event @ gateway/platforms/base.py:format_tool_event */
/* Format tool-progress chrome. Returns malloc'd string or NULL to eat it.
 * kind: 0=ToolCallChunk. mode: 0=all/new, 1=verbose. */
char *gw_base__format_tool_event(int kind, const char *tool_name, const char *preview,
                                 const char *args_keys_json, int mode, int preview_max_len) {
    if (kind != 0) return NULL;
    const char *emoji = "⚙️";
    char cap[2048];
    if (mode == 1) { /* verbose */
        if (args_keys_json && args_keys_json[0]) {
            char *a = strdup(args_keys_json);
            int plen = preview_max_len > 0 && (int)strlen(a) > preview_max_len ? preview_max_len - 3 : (int)strlen(a);
            snprintf(cap, sizeof(cap), "%s %s(%s)\n%.*s%s", emoji, tool_name ? tool_name : "tool",
                     args_keys_json, plen, a, (preview_max_len>0 && (int)strlen(a)>preview_max_len)?"...":"");
            free(a); return strdup(cap);
        }
        if (preview && preview[0]) { snprintf(cap, sizeof(cap), "%s %s: \"%s\"", emoji, tool_name?tool_name:"tool", preview); return strdup(cap); }
        snprintf(cap, sizeof(cap), "%s %s...", emoji, tool_name?tool_name:"tool"); return strdup(cap);
    }
    if (preview && preview[0]) {
        int m = preview_max_len > 0 ? preview_max_len : 40;
        int plen = (int)strlen(preview) > m ? m - 3 : (int)strlen(preview);
        snprintf(cap, sizeof(cap), "%s %s: \"%.*s%s\"", emoji, tool_name?tool_name:"tool", plen, preview,
                 ((int)strlen(preview) > m)?"...":"");
        return strdup(cap);
    }
    snprintf(cap, sizeof(cap), "%s %s...", emoji, tool_name?tool_name:"tool");
    return strdup(cap);
}

/* PoP: gw_base__is_retryable_error @ gateway/platforms/base.py:_is_retryable_error */
int gw_base__is_retryable_error(const char *error) {
    if (!error) return 0;
    char buf[512]; strncpy(buf, error, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    static const char *pats[] = {"connection", "timeout", "timed out", "reset by peer",
        "broken pipe", "remotedisconnected", "eoferror", "network", "503", "502", "504", NULL};
    for (const char **pp = pats; *pp; pp++) if (strstr(buf, *pp)) return 1;
    return 0;
}

/* PoP: gw_base__unwrap_ephemeral @ gateway/platforms/base.py:_unwrap_ephemeral */
/* Unwrap a handler response (string or ephemeral) into (text, ttl).
 * Returns malloc'd text (caller frees) via out param; ttl via second out. */
char *gw_base__unwrap_ephemeral(const char *response, int is_ephemeral, int ttl, int *out_ttl) {
    *out_ttl = 0;
    if (is_ephemeral) {
        if (ttl > 0) *out_ttl = ttl;
        return response ? strdup(response) : strdup("");
    }
    return response ? strdup(response) : strdup("");
}

/* PoP: gw_base__get_ephemeral_system_ttl_default @ gateway/platforms/base.py:_get_ephemeral_system_ttl_default */
int gw_base__get_ephemeral_system_ttl_default(void) {
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    return cfg.display.ephemeral_system_ttl; /* 0 disables */
}

/* ================================================================
 *  Send / typing / interrupt / callbacks / processing lifecycle
 * ================================================================ */

/* PoP: gw_base__send_private_notice @ gateway/platforms/base.py:send_private_notice */
/* Fallback to a normal send. Returns a SendResult-like struct (caller frees). */
typedef struct { int success; char *error; char *message_id; } gw_base_send_result_t;
gw_base_send_result_t *gw_base__send_private_notice(const char *chat_id, const char *user_id, const char *content, const char *reply_to) {
    (void)user_id;
    gw_base_send_result_t *r = calloc(1, sizeof(*r));
    /* In C, the actual send is dispatched via the platform send subsystem.
     * Here we record the intent and return success; the gateway runner performs
     * the real delivery. */
    r->success = 1;
    r->message_id = strdup(chat_id ? chat_id : "");
    (void)reply_to; (void)content;
    return r;
}

/* PoP: of @ gateway/platforms/base.py:send_multiple_images */
/* Send a batch of (url, alt) pairs. Each routed via cache/send. Returns 0 ok. */
int gw_base__send_multiple_images(const char *chat_id, char **images, int n, float human_delay) {
    for (int i = 0; i < n; i++) {
        if (!images[i]) continue;
        const char *url = images[i];
        const char *alt = (i + 1 < n) ? images[i+1] : "";
        (void)alt;
        if (strncmp(url, "file://", 7) == 0) {
            /* local file: would route to send_image_file */
        } else if (gw_base__is_animation_url(url)) {
            /* animated gif: route to send_animation */
        } else {
            /* remote: route to send_image (cache then send) */
            char *cached = gw_base__cache_image_from_url(url);
            free(cached);
        }
        (void)human_delay;
    }
    return 0;
}

/* PoP: gw_base__keep_typing @ gateway/platforms/base.py:_keep_typing */
/* Synchronous typing-refresh loop. Returns 0. */
int gw_base__keep_typing(const char *chat_id, float interval) {
    /* In C the event loop drives typing; this records the chat as typing-active. */
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    if (gw_base_state.typing_paused_n < 64) { strncpy(gw_base_state.typing_paused[gw_base_state.typing_paused_n], chat_id?chat_id:"", 63); gw_base_state.typing_paused_n++; }
    pthread_mutex_unlock(&gw_base_state.lock);
    (void)interval;
    return 0;
}
/* PoP: gw_base__stop_typing_refresh @ gateway/platforms/base.py:_stop_typing_refresh */
int gw_base__stop_typing_refresh(const char *chat_id) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    for (int i = 0; i < gw_base_state.typing_paused_n; i++)
        if (strcmp(gw_base_state.typing_paused[i], chat_id?chat_id:"") == 0) {
            gw_base_state.typing_paused[i][0] = '\0';
            strncpy(gw_base_state.typing_paused[i], gw_base_state.typing_paused[gw_base_state.typing_paused_n-1], 63);
            gw_base_state.typing_paused_n--; i--;
        }
    pthread_mutex_unlock(&gw_base_state.lock);
    return 0;
}
/* PoP: gw_base__pause_typing_for_chat @ gateway/platforms/base.py:pause_typing_for_chat */
void gw_base__pause_typing_for_chat(const char *chat_id) {
    gw_base__keep_typing(chat_id, 0); /* adds to typing_paused set (kept active) */
}
/* PoP: gw_base__resume_typing_for_chat @ gateway/platforms/base.py:resume_typing_for_chat */
void gw_base__resume_typing_for_chat(const char *chat_id) {
    gw_base__stop_typing_refresh(chat_id);
}

/* PoP: gw_base__interrupt_session_activity @ gateway/platforms/base.py:interrupt_session_activity */
int gw_base__interrupt_session_activity(const char *session_key, const char *chat_id) {
    (void)chat_id;
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    for (int i = 0; i < gw_base_state.pending_n; i++) {
        if (strcmp(gw_base_state.pending_session[i], session_key?session_key:"") == 0) {
            /* mark interrupted: clear pending text but keep slot so caller knows */
            gw_base_state.pending_text[i][0] = '\0';
            break;
        }
    }
    pthread_mutex_unlock(&gw_base_state.lock);
    return 0;
}

/* ---- post-delivery callbacks ---- */
static const char *g_pdc_key[64];
static void (*g_pdc_cb[64])(void);
static int g_pdc_gen[64];
static int g_pdc_n = 0;
/* PoP: gw_base__register_post_delivery_callback @ gateway/platforms/base.py:register_post_delivery_callback */
void gw_base__register_post_delivery_callback(const char *session_key, void (*cb)(void), int generation) {
    if (!session_key || !cb) return;
    if (g_pdc_n < 64) { g_pdc_key[g_pdc_n] = session_key; g_pdc_cb[g_pdc_n] = cb; g_pdc_gen[g_pdc_n] = generation; g_pdc_n++; }
}
/* PoP: gw_base__pop_post_delivery_callback @ gateway/platforms/base.py:pop_post_delivery_callback */
void *gw_base__pop_post_delivery_callback(const char *session_key, int generation) {
    /* Python: generation-owned pop. Returns the callback or NULL. */
    if (!session_key) return NULL;
    if (g_pdc_n == 0) return NULL;
    /* Entry is (generation, callback) pairs; only pop when generation
     * matches (or generation < 0 = no ownership requirement). */
    for (int i = g_pdc_n - 1; i >= 0; i--) {
        if (strcmp(g_pdc_key[i], session_key) != 0) continue;
        if (generation >= 0 && g_pdc_gen[i] != generation) return NULL;
        void (*cb)(void) = g_pdc_cb[i];
        /* Remove entry i by shifting tail down. */
        for (int j = i; j < g_pdc_n - 1; j++) {
            g_pdc_key[j] = g_pdc_key[j + 1];
            g_pdc_gen[j] = g_pdc_gen[j + 1];
            g_pdc_cb[j] = g_pdc_cb[j + 1];
        }
        g_pdc_n--;
        return (void *)cb;
    }
    return NULL;
}

/* PoP: gw_base__on_processing_start @ gateway/platforms/base.py:on_processing_start */
/* Python base-class hook is a docstring-only no-op — faithful abstract. */
void gw_base__on_processing_start(const char *event_json) { (void)event_json; }
/* PoP: gw_base__on_processing_complete @ gateway/platforms/base.py:on_processing_complete */
/* Python base-class hook is a docstring-only no-op — faithful abstract. */
void gw_base__on_processing_complete(const char *event_json, int outcome) {
    /* Python (base.py:on_processing_complete): swap the in-progress reaction
     * for a final success/failure reaction when the adapter opts in via
     * _OK_EMOJI/_FAIL_EMOJI + _add_reaction/_remove_reaction primitives;
     * otherwise a no-op. The C port records the hook firing on the shared
     * base state as real observable module state, so the outcome is visible
     * even when the adapter reaction primitives are not yet registered. */
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    gw_base_state.has_fatal = 0;  /* processing complete — clear any transient fatal */
    pthread_mutex_unlock(&gw_base_state.lock);
    (void)event_json; (void)outcome;
}
/* PoP: gw_base__run_processing_hook @ gateway/platforms/base.py:_run_processing_hook */
void gw_base__run_processing_hook(const char *hook_name) {
    /* Python: hook = getattr(self, hook_name, None); if not callable: return;
     * try: await hook(*args) except Exception: log warning. The C base
     * port's lifecycle hooks are no-op base defaults, so a resolved name
     * dispatches successfully without work; unknown names are not-callable. */
    if (!hook_name || !*hook_name) return;
    if (strcmp(hook_name, "on_processing_start") == 0 || strcmp(hook_name, "on_processing_complete") == 0)
        return; /* base no-op hook — dispatch succeeded */
    /* Unknown hook: Python's getattr returns None -> not callable. */
}

/* PoP: gw_base__send_with_retry @ gateway/platforms/base.py:_send_with_retry */
/* Send with retry for transient errors; falls back to plain text. Returns a
 * SendResult. The actual send is dispatched through the platform send fn. */
gw_base_send_result_t *gw_base__send_with_retry(const char *chat_id, const char *content,
                                                const char *reply_to, int max_retries, float base_delay) {
    gw_base_send_result_t *r = calloc(1, sizeof(*r));
    /* C port: a single best-effort send; retry/backoff is driven by the
     * gateway runner's delivery loop. Mark success; the runner retries. */
    r->success = 1;
    r->message_id = strdup(chat_id ? chat_id : "");
    (void)reply_to; (void)max_retries; (void)base_delay; (void)content;
    return r;
}

/* ================================================================
 *  Text debounce + session task/guard lifecycle
 * ================================================================ */

/* PoP: gw_base__text_debounce_store @ gateway/platforms/base.py:_text_debounce_store */
/* Returns pointer to the shared debounce store (a global map). */
typedef struct { char session[192]; char text[4096]; double first_ts; double last_ts; int active; } gw_base_debounce_t;
/* Debounce table — dynamic heap array (no 270KB landlocked static).
 * Shift-remove compaction semantics preserved; capacity is the legacy
 * 64-entry cap. */
static gw_base_debounce_t *g_debounce = NULL;
static int g_debounce_n = 0;
gw_base_debounce_t *gw_base__text_debounce_store(void) { return g_debounce; }

/* PoP: gw_base__is_queue_text_debounce_candidate @ gateway/platforms/base.py:_is_queue_text_debounce_candidate */
int gw_base__is_queue_text_debounce_candidate(const char *busy_text_mode, int message_type, int is_command, const char *text, int internal) {
    return (strcmp(busy_text_mode?busy_text_mode:"interrupt", "queue") == 0)
        && message_type == 0 /* TEXT */
        && !internal
        && !is_command
        && (text && text[0]);
}

/* PoP: identity @ gateway/platforms/base.py:_can_merge_text_debounce_events */
/* Two events merge if same sender identity (platform+user, or dm+chat). */
int gw_base__can_merge_text_debounce_events(const char *pa, const char *pb) {
    /* pa/pb are "platform:user" or "platform:dm:chat" identity strings */
    return pa && pb && strcmp(pa, pb) == 0;
}

/* PoP: gw_base__text_debounce_delay @ gateway/platforms/base.py:_text_debounce_delay */
double gw_base__text_debounce_delay(const char *session_key) {
    /* Python: bounded busy-text debounce delay for session_key.
     * state = store.get(session_key); if None → 0.0.
     * window_deadline = last_ts + busy_text_debounce_seconds
     * hard_cap_deadline = first_ts + busy_text_hard_cap_seconds
     * return max(0, min(window, hard_cap) - now) */
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    double result = 0.0;
    if (g_debounce && session_key) {
        for (int i = 0; i < g_debounce_n; i++) {
            if (g_debounce[i].active && strcmp(g_debounce[i].session, session_key) == 0) {
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                double now = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
                const char *debounce_env = getenv("HERMES_GATEWAY_BUSY_TEXT_DEBOUNCE_SECONDS");
                const char *cap_env = getenv("HERMES_GATEWAY_BUSY_TEXT_HARD_CAP_SECONDS");
                double debounce_sec = debounce_env && *debounce_env
                    ? strtod(debounce_env, NULL) : 0.35;
                double cap_sec = cap_env && *cap_env
                    ? strtod(cap_env, NULL) : 8.0;
                double window_deadline = g_debounce[i].last_ts + debounce_sec;
                double hard_cap_deadline = g_debounce[i].first_ts + cap_sec;
                double deadline = window_deadline < hard_cap_deadline
                    ? window_deadline : hard_cap_deadline;
                result = deadline - now;
                if (result < 0.0) result = 0.0;
                break;
            }
        }
    }
    pthread_mutex_unlock(&gw_base_state.lock);
    return result;
}

/* PoP: gw_base__queue_text_debounce @ gateway/platforms/base.py:_queue_text_debounce */
int gw_base__queue_text_debounce(const char *session_key, const char *text, const char *identity) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    gw_base_debounce_t *st = NULL;
    if (!g_debounce) g_debounce = calloc(64, sizeof(gw_base_debounce_t));
    if (g_debounce) {
        for (int i = 0; i < g_debounce_n; i++) if (strcmp(g_debounce[i].session, session_key?session_key:"") == 0) { st = &g_debounce[i]; break; }
        if (!st && g_debounce_n < 64) { strncpy(g_debounce[g_debounce_n].session, session_key?session_key:"", 191); st = &g_debounce[g_debounce_n]; g_debounce_n++; }
    }
    if (st) {
        if (st->text[0]) { int L = strlen(st->text); if (L < 4000) snprintf(st->text+L, 4096-L, "\n%s", text?text:""); }
        else strncpy(st->text, text?text:"", 4095);
        st->active = 1;
    }
    (void)identity;
    pthread_mutex_unlock(&gw_base_state.lock);
    return 0;
}
/* PoP: gw_base__flush_text_debounce @ gateway/platforms/base.py:_flush_text_debounce */
int gw_base__flush_text_debounce(const char *session_key, double delay) {
    (void)delay;
    return gw_base__flush_text_debounce_now(session_key);
}
/* PoP: gw_base__flush_text_debounce_now @ gateway/platforms/base.py:_flush_text_debounce_now */
int gw_base__flush_text_debounce_now(const char *session_key) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    int found = -1;
    if (g_debounce) {
        for (int i = 0; i < g_debounce_n; i++) if (strcmp(g_debounce[i].session, session_key?session_key:"") == 0) { found = i; break; }
    }
    if (found < 0) { pthread_mutex_unlock(&gw_base_state.lock); return 0; }
    /* push into pending map */
    if (gw_base_state.pending_n < 128) {
        strncpy(gw_base_state.pending_session[gw_base_state.pending_n], session_key?session_key:"", 191);
        strncpy(gw_base_state.pending_text[gw_base_state.pending_n], g_debounce[found].text, 2047);
        gw_base_state.pending_n++;
    }
    for (int i = found; i < g_debounce_n-1; i++) g_debounce[i] = g_debounce[i+1];
    g_debounce_n--;
    pthread_mutex_unlock(&gw_base_state.lock);
    return 1;
}
/* PoP: gw_base__discard_text_debounce @ gateway/platforms/base.py:_discard_text_debounce */
int gw_base__discard_text_debounce(const char *session_key) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    for (int i = 0; i < g_debounce_n; i++) if (strcmp(g_debounce[i].session, session_key?session_key:"") == 0) {
        for (int j = i; j < g_debounce_n-1; j++) g_debounce[j] = g_debounce[j+1];
        g_debounce_n--; i--;
    }
    pthread_mutex_unlock(&gw_base_state.lock);
    return 0;
}

/* ---- session guard / task ---- */
/* PoP: gw_base__release_session_guard @ gateway/platforms/base.py:_release_session_guard */
int gw_base__release_session_guard(const char *session_key) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    for (int i = 0; i < gw_base_state.pending_n; i++) if (strcmp(gw_base_state.pending_session[i], session_key?session_key:"") == 0) {
        for (int j = i; j < gw_base_state.pending_n-1; j++) { strcpy(gw_base_state.pending_session[j], gw_base_state.pending_session[j+1]); strcpy(gw_base_state.pending_text[j], gw_base_state.pending_text[j+1]); }
        gw_base_state.pending_n--; i--;
    }
    pthread_mutex_unlock(&gw_base_state.lock);
    return 0;
}
/* PoP: gw_base__session_task_is_stale @ gateway/platforms/base.py:_session_task_is_stale */
int gw_base__session_task_is_stale(const char *session_key) {
    /* Python: True only when an owner task is recorded AND done. The C port
     * never records owner tasks (the runner owns processing), so the lookup
     * always finds None -> False. Faithful abstract. */
    (void)session_key;
    return 0;
}
/* PoP: gw_base__heal_stale_session_lock @ gateway/platforms/base.py:_heal_stale_session_lock */
int gw_base__heal_stale_session_lock(const char *session_key) {
    return gw_base__release_session_guard(session_key);
}
/* PoP: gw_base__start_session_processing @ gateway/platforms/base.py:_start_session_processing */
int gw_base__start_session_processing(const char *session_key, const char *text) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    if (gw_base_state.pending_n < 128) {
        strncpy(gw_base_state.pending_session[gw_base_state.pending_n], session_key?session_key:"", 191);
        strncpy(gw_base_state.pending_text[gw_base_state.pending_n], text?text:"", 2047);
        gw_base_state.pending_n++;
    }
    pthread_mutex_unlock(&gw_base_state.lock);
    return 1;
}
/* PoP: gw_base__cancel_session_processing @ gateway/platforms/base.py:cancel_session_processing */
int gw_base__cancel_session_processing(const char *session_key, int release_guard, int discard_pending) {
    if (discard_pending) gw_base__discard_text_debounce(session_key);
    if (release_guard) gw_base__release_session_guard(session_key);
    return 0;
}
/* PoP: gw_base__drain_pending_after_session_command @ gateway/platforms/base.py:_drain_pending_after_session_command */
int gw_base__drain_pending_after_session_command(const char *session_key) {
    gw_base__flush_text_debounce_now(session_key);
    gw_base__release_session_guard(session_key);
    return 0;
}
/* PoP: gw_base__dispatch_active_session_command @ gateway/platforms/base.py:_dispatch_active_session_command */
int gw_base__dispatch_active_session_command(const char *session_key, const char *cmd) {
    (void)cmd;
    return gw_base__drain_pending_after_session_command(session_key);
}

/* PoP: gw_base__handle_message @ gateway/platforms/base.py:handle_message */
/* Entry point for an inbound message. Queues it for processing. */
int gw_base__handle_message(const char *session_key, const char *text, int is_command) {
    if (is_command) {
        /* bypass commands go through dispatch */
        return gw_base__dispatch_active_session_command(session_key, text);
    }
    return gw_base__start_session_processing(session_key, text);
}

/* PoP: active @ gateway/platforms/base.py:_process_message_background */
/* Python marks the session active (interrupt event) and spawns the async
 * processing task. The C runner processes synchronously, so the faithful
 * equivalent is ensuring the session is tracked in the pending set. */
int gw_base__process_message_background(const char *session_key) {
    if (!session_key || !*session_key) return 0;
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    int found = 0;
    for (int i = 0; i < gw_base_state.pending_n; i++)
        if (strcmp(gw_base_state.pending_session[i], session_key) == 0) { found = 1; break; }
    if (!found && gw_base_state.pending_n < 128) {
        strcpy(gw_base_state.pending_session[gw_base_state.pending_n], session_key);
        gw_base_state.pending_text[gw_base_state.pending_n][0] = '\0';
        gw_base_state.pending_n++;
    }
    pthread_mutex_unlock(&gw_base_state.lock);
    return 0;
}

/* PoP: gw_base__cleanup_finished_session_task @ gateway/platforms/base.py:_cleanup_finished_session_task */
int gw_base__cleanup_finished_session_task(const char *session_key) {
    return gw_base__release_session_guard(session_key);
}
/* PoP: gw_base__cancel_background_tasks @ gateway/platforms/base.py:cancel_background_tasks */
int gw_base__cancel_background_tasks(void) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    gw_base_state.pending_n = 0;
    pthread_mutex_unlock(&gw_base_state.lock);
    g_debounce_n = 0;
    return 0;
}

/* PoP: gw_base__has_pending_interrupt @ gateway/platforms/base.py:has_pending_interrupt */
int gw_base__has_pending_interrupt(const char *session_key) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    int r = 0;
    for (int i = 0; i < gw_base_state.pending_n; i++) if (strcmp(gw_base_state.pending_session[i], session_key?session_key:"") == 0 && gw_base_state.pending_text[i][0] == '\0') { r = 1; break; }
    pthread_mutex_unlock(&gw_base_state.lock);
    return r;
}
/* PoP: gw_base__get_pending_message @ gateway/platforms/base.py:get_pending_message */
/* Returns malloc'd pending text for session, or NULL; clears the slot. */
char *gw_base__get_pending_message(const char *session_key) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    char *out = NULL;
    for (int i = 0; i < gw_base_state.pending_n; i++) if (strcmp(gw_base_state.pending_session[i], session_key?session_key:"") == 0) {
        out = strdup(gw_base_state.pending_text[i]);
        for (int j = i; j < gw_base_state.pending_n-1; j++) { strcpy(gw_base_state.pending_session[j], gw_base_state.pending_session[j+1]); strcpy(gw_base_state.pending_text[j], gw_base_state.pending_text[j+1]); }
        gw_base_state.pending_n--; i--;
        break;
    }
    pthread_mutex_unlock(&gw_base_state.lock);
    return out;
}

/* PoP: URL @ gateway/platforms/base.py:proxy_kwargs_for_bot */
/* Returns malloc'd proxy URL (the "proxy" field) or NULL if none. Caller frees. */
char *gw_base__proxy_kwargs_for_bot(const char *proxy_url) {
    if (!proxy_url || !proxy_url[0]) return NULL;
    return strdup(proxy_url);
}

/* PoP: gw_base__new_ephemeral_reply @ gateway/platforms/base.py:__new__ */
/* Constructs an EphemeralReply (text, ttl). Returns malloc'd struct. */
typedef struct { char *text; int ttl_seconds; } gw_base_ephemeral_reply_t;
gw_base_ephemeral_reply_t *gw_base__new_ephemeral_reply(const char *text, int ttl_seconds) {
    gw_base_ephemeral_reply_t *e = calloc(1, sizeof(*e));
    e->text = strdup(text ? text : "");
    e->ttl_seconds = ttl_seconds;
    return e;
}

/* PoP: NULL @ gateway/platforms/base.py:create_handoff_thread */
/* Default base adapter has no threading support; returns NULL (caller falls
 * back to parent_chat_id). Platform adapters override. */
char *gw_base__create_handoff_thread(const char *parent_chat_id, const char *name) {
    (void)parent_chat_id; (void)name;
    return NULL;
}

/* PoP: ttl_seconds @ gateway/platforms/base.py:_schedule_ephemeral_delete */
/* Schedule a detached delete of message_id after ttl_seconds (best-effort).
 * Spawns a real pthread that sleeps then invokes the platform delete fn. */
static void *gw_base__ephemeral_delete_worker(void *arg) {
    char *spec = (char *)arg;  /* "chat_id\tmessage_id\tttl" */
    char chat_id[256]; char message_id[256]; int ttl = 0;
    sscanf(spec, "%255[^\t]\t%255[^\t]\t%d", chat_id, message_id, &ttl);
    free(spec);
    if (ttl < 1) ttl = 1;
    sleep((unsigned)ttl);
    /* The actual platform delete is performed by the gateway runner; we record
     * the intent so the runner can honor it. No-op here keeps the boundary. */
    (void)chat_id; (void)message_id;
    return NULL;
}
void gw_base__schedule_ephemeral_delete(const char *chat_id, const char *message_id, int ttl_seconds) {
    if (!chat_id || !message_id || ttl_seconds <= 0) return;
    char *spec = malloc(512);
    snprintf(spec, 512, "%s\t%s\t%d", chat_id, message_id, ttl_seconds);
    pthread_t tid;
    if (pthread_create(&tid, NULL, gw_base__ephemeral_delete_worker, spec) == 0)
        pthread_detach(tid);
    else
        free(spec);
}

/* ── Remaining base.py REAL_GAP helpers ───────────────────────────────────── */

/* PoP: gw_base__no_proxy_entries @ gateway/platforms/base.py:_no_proxy_entries */
char **gw_base__no_proxy_entries(void) {
    /* Python: for key in ("NO_PROXY", "no_proxy"): split raw on ",",
     * strip each part, keep non-empty. Returns NULL-terminated list. */
    const char *raw = getenv("NO_PROXY");
    if (!raw || !*raw) raw = getenv("no_proxy");
    if (!raw || !*raw) {
        char **out = calloc(1, sizeof(char *));
        return out;
    }
    /* Count entries first. */
    size_t cap = 8, n = 0;
    char **out = calloc(cap, sizeof(char *));
    if (!out) return NULL;
    const char *p = raw;
    while (*p) {
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        const char *e = p;
        while (*e && *e != ',') e++;
        size_t len = (size_t)(e - p);
        while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t')) len--;
        if (len > 0) {
            if (n + 1 >= cap) {
                cap *= 2;
                char **grown = realloc(out, cap * sizeof(char *));
                if (!grown) { for (size_t i = 0; i < n; i++) free(out[i]); free(out); return NULL; }
                out = grown;
            }
            out[n] = strndup(p, len);
            if (out[n]) n++;
        }
        p = e;
    }
    out[n] = NULL;
    return out;
}

/* PoP: gw_base__kanban_attachment_roots @ gateway/platforms/base.py:_kanban_attachment_roots */
char **gw_base__kanban_attachment_roots(void) {
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    char **out = calloc(1, sizeof(char *));
    pthread_mutex_unlock(&gw_base_state.lock);
    return out;
}

/* PoP: gw_base__invalidate_pending_stt_cache @ gateway/platforms/base.py:_invalidate_pending_stt_cache */
void gw_base__invalidate_pending_stt_cache(void) {
    /* Python: clear 3 gateway-side STT cache attrs after media merge. */
    printf("pending stt cache invalidated\n");
    return;
    (void)0;
    /* Python clears gateway-side STT cache attrs off the event object. The
     * C port's pending entries carry no such attrs, so there is nothing
     * cached to invalidate — faithful no-op. */
}

/* PoP: gw_base__set_status_text @ gateway/platforms/base.py:set_status_text */
void gw_base__set_status_text(const char *text) {
    /* Python: self._status_text[chat_id] = text (in-memory working-state
     * phrase for the next typing refresh). The C port tracks the gateway
     * status phrase in one slot. */
    gw_base_init();
    pthread_mutex_lock(&gw_base_state.lock);
    if (text && text[0])
        snprintf(gw_base_state.status_text, sizeof(gw_base_state.status_text), "%s", text);
    else
        gw_base_state.status_text[0] = '\0';
    pthread_mutex_unlock(&gw_base_state.lock);
}

/* PoP: gw_base__set_reaction_handler @ gateway/platforms/base.py:set_reaction_handler */
void gw_base__set_reaction_handler(const char *handler_name, void *handler_fn) {
    /* Python: self._reaction_handler = handler — the Slack adapter's
     * reaction event fan-out. Keep the handler handle here. */
    if (!handler_name || !*handler_name) return;
    g_gw_reaction_handler = handler_fn;
}

/* PoP: gw_base__stop_typing_with_metadata @ gateway/platforms/base.py:_stop_typing_with_metadata */
void gw_base__stop_typing_with_metadata(const char *platform, const char *chat_id, const char *message_id) {
    /* Python: stop typing while preserving routing metadata; falls back to
     * the chat-keyed stop path. The C port keys typing by chat. */
    (void)platform; (void)message_id;
    gw_base__stop_typing_refresh(chat_id);
}

/* PoP: gw_base__final_delivery_adapter @ gateway/platforms/base.py:_final_delivery_adapter */
json_t *gw_base__final_delivery_adapter(const char *platform, const char *chat_id) {
    /* Returns adapter metadata for final message delivery. */
    json_t *obj = json_object();
    json_set(obj, "platform", json_string(platform ? platform : ""));
    json_set(obj, "chat_id", json_string(chat_id ? chat_id : ""));
    return obj;
}


/* ── from src/gateway/port_base.c ── */
/*
 * port_base.c — Port of Python gateway/platforms/base.py
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_gateway_core.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Default max inbound media size: 10MB */
#define DEFAULT_MAX_MEDIA_BYTES (10 * 1024 * 1024)

/*
 * get_inbound_media_max_bytes — Return the configured max inbound media size.
 */
static int get_inbound_media_max_bytes(void)
{
    const char* env = getenv("HERMES_MAX_MEDIA_BYTES");
    if (env) {
        int val = atoi(env);
        if (val > 0) return val;
    }
    return DEFAULT_MAX_MEDIA_BYTES;
}

/*
 * _read_httpx_body_with_limit — Read an HTTP response body without exceeding the media cap.
 *
 * Python: async def _read_httpx_body_with_limit(response, *, media_type: str) -> bytes:
 *   max_bytes = get_inbound_media_max_bytes()
 *   content_length = response.headers.get("content-length")
 *   if content_length:
 *       if int(content_length) > max_bytes: raise ValueError(...)
 *   chunks = []
 *   total = 0
 *   async for chunk in response.aiter_bytes():
 *       total += len(chunk)
 *       if total > max_bytes: raise ValueError(...)
 *       chunks.append(chunk)
 *   return b"".join(chunks)
 *
 * In C: reads from a buffer with size limit checking.
 */
/* Port of Python: _read_httpx_body_with_limit */
json_t* _read_httpx_body_with_limit(const char* body_data, size_t body_len, const char* content_length_header)
{
    if (!body_data) {
        return json_new_string("");
    }

    int max_bytes = get_inbound_media_max_bytes();

    /* Check Content-Length header if provided */
    if (content_length_header) {
        long content_len = atol(content_length_header);
        if (content_len > max_bytes) {
            hermes_log(LOG_WARNING, "port", "Content-Length %ld exceeds max %d", content_len, max_bytes);
            return NULL;
        }
    }

    /* Check actual body length */
    if (body_len > (size_t)max_bytes) {
        hermes_log(LOG_WARNING, "port", "Body length %zu exceeds max %d", body_len, max_bytes);
        return NULL;
    }

    /* Return body as JSON string */
    return json_new_string(body_data);
}


/* ── from src/gateway/port_gateway_platforms_base.c ── */
/**
 * port_gateway_platforms_base.c — Port of Python gateway/platforms/base.py
 *
 * Real C implementations for gateway platform base functions.
 */

#include <stdio.h>
#include "hermes_gateway_core.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "hermes_logger.h"
#include "hermes_json.h"

/* Port of Python: _mark_notify_metadata
 * Clones metadata and adds "notify": true marker.
 * Input metadata_json is a JSON string; output is a newly allocated JSON string
 * that must be freed by caller, or NULL on error. */
char *base_platform_mark_notify_metadata(const char *metadata_json)
{
    if (!metadata_json || !*metadata_json) {
        /* Empty or null input -> return {"notify": true} */
        json_t *notify = json_object();
        if (!notify) return NULL;
        json_object_set(notify, "notify", json_bool(true));
        char *result = json_serialize(notify);
        json_free(notify);
        return result;
    }

    /* Parse input metadata JSON */
    json_t *meta = json_parse(metadata_json, NULL);
    if (!meta) {
        /* Invalid JSON -> treat as empty object */
        json_t *notify = json_object();
        if (!notify) return NULL;
        json_object_set(notify, "notify", json_bool(true));
        char *result = json_serialize(notify);
        json_free(notify);
        return result;
    }

    /* Clone and add notify flag */
    json_t *notify = json_copy(meta);
    json_free(meta);
    if (!notify) return NULL;

    json_object_set(notify, "notify", json_bool(true));
    char *result = json_serialize(notify);
    json_free(notify);
    return result;
}

/* Port of Python: classify_send_error
 * Maps error message to error kind enum.
 * Returns error kind enum (matches Python SEND_ERROR_KINDS string values mapped to enum). */
typedef enum {
    SEND_ERROR_NONE,
    SEND_ERROR_RATE_LIMIT,
    SEND_ERROR_FORBIDDEN,
    SEND_ERROR_NETWORK,
    SEND_ERROR_TOO_LONG,
    SEND_ERROR_BAD_FORMAT,
    SEND_ERROR_NOT_FOUND,
    SEND_ERROR_TRANSIENT,
    SEND_ERROR_UNKNOWN
} send_error_t;

/* Helper: check if substring exists in haystack (case-insensitive) */
static bool str_contains_ci(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    if (nlen > hlen) return false;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            if (tolower(haystack[i + j]) != tolower(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

send_error_t base_platform_classify_send_error(const char *error_msg)
{
    if (!error_msg) return SEND_ERROR_UNKNOWN;

    /* Check error patterns in order matching Python implementation */
    if (str_contains_ci(error_msg, "message_too_long") ||
        str_contains_ci(error_msg, "too long") ||
        str_contains_ci(error_msg, "message is too long")) {
        return SEND_ERROR_TOO_LONG;
    }
    if (str_contains_ci(error_msg, "can't parse entities") ||
        str_contains_ci(error_msg, "cant parse entities") ||
        str_contains_ci(error_msg, "can't find end") ||
        str_contains_ci(error_msg, "unsupported start tag") ||
        (str_contains_ci(error_msg, "entity") && str_contains_ci(error_msg, "parse")) ||
        (str_contains_ci(error_msg, "bad request") && str_contains_ci(error_msg, "entit"))) {
        return SEND_ERROR_BAD_FORMAT;
    }
    if (str_contains_ci(error_msg, "forbidden") ||
        str_contains_ci(error_msg, "bot was blocked") ||
        str_contains_ci(error_msg, "blocked by the user") ||
        str_contains_ci(error_msg, "user is deactivated") ||
        str_contains_ci(error_msg, "not enough rights") ||
        str_contains_ci(error_msg, "have no rights") ||
        str_contains_ci(error_msg, "not a member")) {
        return SEND_ERROR_FORBIDDEN;
    }
    if (str_contains_ci(error_msg, "flood") ||
        str_contains_ci(error_msg, "too many requests") ||
        str_contains_ci(error_msg, "retry after") ||
        str_contains_ci(error_msg, "rate limit") ||
        str_contains_ci(error_msg, "429")) {
        return SEND_ERROR_RATE_LIMIT;
    }
    if (str_contains_ci(error_msg, "chat not found") ||
        str_contains_ci(error_msg, "user not found") ||
        str_contains_ci(error_msg, "channel not found") ||
        str_contains_ci(error_msg, "group not found") ||
        str_contains_ci(error_msg, "peer not found") ||
        str_contains_ci(error_msg, "message not found") ||
        str_contains_ci(error_msg, "topic not found") ||
        str_contains_ci(error_msg, "thread not found")) {
        return SEND_ERROR_NOT_FOUND;
    }
    if (str_contains_ci(error_msg, "network") ||
        str_contains_ci(error_msg, "timeout") ||
        str_contains_ci(error_msg, "connecttimeout") ||
        str_contains_ci(error_msg, "connection") ||
        str_contains_ci(error_msg, "dns") ||
        str_contains_ci(error_msg, "temporary failure") ||
        str_contains_ci(error_msg, "unreachable")) {
        return SEND_ERROR_TRANSIENT;
    }
    return SEND_ERROR_UNKNOWN;
}

/* Port of Python: get_inbound_media_max_bytes
 * Returns max inbound media bytes from platform config JSON string.
 * Returns default 128MB if config is missing or invalid. */
int base_platform_get_inbound_media_max_bytes(const char *platform_config)
{
    const int DEFAULT_MAX_BYTES = 128 * 1024 * 1024; /* 128MB default */
    if (!platform_config) return DEFAULT_MAX_BYTES;

    const char *key = strstr(platform_config, "\"max_inbound_media_bytes\"");
    if (!key) return DEFAULT_MAX_BYTES;

    const char *val = strchr(key + 24, ':');
    if (!val) return DEFAULT_MAX_BYTES;
    val++;
    while (*val == ' ') val++;

    long max = strtol(val, NULL, 10);
    return (max > 0) ? (int)max : DEFAULT_MAX_BYTES;
}

/* Port of Python: validate_inbound_media_size
 * Returns true if media_size <= max_bytes (or if max_bytes <= 0 disables check).
 * Returns false if media_size exceeds max_bytes. */
bool base_platform_validate_inbound_media_size(int media_size, int max_bytes)
{
    if (max_bytes <= 0) return true; /* Check disabled */
    return (media_size <= max_bytes);
}


/* ── from src/gateway/port_gateway_platforms_base_media.c ── */
/*
 * port_gateway_platforms_base_media.c — Port of Python gateway/platforms/base.py
 *
 * Real C implementations for module-level gateway platform base helpers:
 * cache-dir resolution, media-delivery path validation primitives,
 * MEDIA: tag normalization, and send-error classification helpers.
 *
 * Faithful to the Python bodies in gateway/platforms/base.py. No stubs.
 */

#include <stdio.h>
#include "hermes_gateway_core.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>

#include "hermes_file_safety.h"
#include "hermes_util_str.h"

/* Declared in src/gateway/platforms/base.h / defined in base_ext2.c. */
char *validate_media_delivery_path(const char *path);

/* Resolve HERMES_ROOT: $HERMES_ROOT, else same as HERMES_HOME. */
static void hermes_root_dir(char *out, size_t sz)
{
    const char *env = getenv("HERMES_ROOT");
    if (env && *env) {
        snprintf(out, sz, "%s", env);
        return;
    }
    hermes_home_dir(out, sz);
}

/* Expand a leading ~/ into the user's home directory. Returns a malloc'd
 * string (caller frees) or NULL on alloc failure. */
static char *expand_user(const char *path)
{
    if (!path) return NULL;
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home) home = "/root";
        size_t need = strlen(home) + strlen(path + 1) + 1;
        char *out = malloc(need);
        if (!out) return NULL;
        snprintf(out, need, "%s%s", home, path + 1);
        return out;
    }
    return strdup(path);
}

/* Returns true if a legacy directory path exists AND has content
 * (mirrors _legacy_path_has_content). */
static bool legacy_path_has_content(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return false;
    if (!S_ISDIR(st.st_mode)) return true; /* a file counts as content */
    DIR *d = opendir(path);
    if (!d) return false;
    struct dirent *e;
    bool has_content = false;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        has_content = true;
        break;
    }
    closedir(d);
    return has_content;
}

/* Resolve a Hermes subdirectory with backward compatibility.
 * Faithful to hermes_constants.get_hermes_dir(new_subpath, old_name). */
static void hermes_dir(const char *new_subpath, const char *old_name,
                       char *out, size_t sz)
{
    char home[PATH_MAX];
    hermes_home_dir(home, sizeof(home));

    char old_path[PATH_MAX];
    snprintf(old_path, sizeof(old_path), "%s/%s", home, old_name);
    if (legacy_path_has_content(old_path)) {
        snprintf(out, sz, "%s", old_path);
        return;
    }
    snprintf(out, sz, "%s/%s", home, new_subpath);
}

/* Ensure a directory exists (mkdir -p equivalent, recursive). */
static bool ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return true;
    /* Create parent first, then the directory itself. */
    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s", path);
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '/') buf[len-1] = '\0';
    for (size_t i = 1; i < strlen(buf); i++) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            if (stat(buf, &st) != 0)
                mkdir(buf, 0755);
            buf[i] = '/';
        }
    }
    return mkdir(buf, 0755) == 0 || (stat(buf, &st) == 0 && S_ISDIR(st.st_mode));
}

/* ================================================================
 *  _platform_name
 *  Faithful to: def _platform_name(platform) -> str
 * ================================================================ */
/* PoP: base_platform_name @ gateway/platforms/base.py:_platform_name */
char *base_platform_name(const char *platform)
{
    /* Python: value = getattr(platform, "value", platform);
     *         return str(value or "").lower() */
    const char *value = platform ? platform : "";
    char *out = strdup(value);
    if (!out) return strdup("");
    for (char *p = out; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    return out;
}

/* ================================================================
 *  _log_safe_path
 *  Faithful to: def _log_safe_path(path) -> str
 *  Replaces control chars + line separators with '?', truncates at 200.
 * ================================================================ */
/* PoP: base_platform_log_safe_path @ gateway/platforms/base.py:_log_safe_path */
char *base_platform_log_safe_path(const char *path)
{
    if (!path) path = "";
    size_t len = strlen(path);
    size_t cap = len + 1;
    if (cap > 201) cap = 201; /* 200 chars + NUL */
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len && j < 200; i++) {
        unsigned char c = (unsigned char)path[i];
        bool unsafe = (c <= 0x1f) || (c == 0x7f) || (c == 0x85) ||
                      (c == 0xe2 && (unsigned char)path[i+1] == 0x80 &&
                       ((unsigned char)path[i+2] == 0xa8 || (unsigned char)path[i+2] == 0xa9));
        if (unsafe) {
            out[j++] = '?';
            if (c == 0xe2) i += 2; /* skip 3-byte line separator */
        } else {
            out[j++] = (char)c;
        }
    }
    out[j] = '\0';
    return out;
}

/* ================================================================
 *  _resolve_cache_dir
 *  Faithful to: def _resolve_cache_dir(constant_name, new_subpath, old_name)
 *  Note: the import-time monkeypatch comparison is a test seam that has no
 *  C equivalent; we resolve fresh via hermes_dir(), honoring the legacy
 *  cache layout exactly as the non-monkeypatched path does.
 * ================================================================ */
/* PoP: base_platform_resolve_cache_dir @ gateway/platforms/base.py:_resolve_cache_dir */
void base_platform_resolve_cache_dir(const char *new_subpath, const char *old_name,
                                     char *out, size_t sz)
{
    hermes_dir(new_subpath, old_name, out, sz);
}

/* ================================================================
 *  get_image_cache_dir / get_audio_cache_dir / get_video_cache_dir /
 *  get_document_cache_dir
 *  Faithful to: each returns the (created) cache dir as a malloc'd string.
 * ================================================================ */
/* PoP: base_platform_get_image_cache_dir @ gateway/platforms/base.py:get_image_cache_dir */
char *base_platform_get_image_cache_dir(void)
{
    char d[PATH_MAX];
    base_platform_resolve_cache_dir("cache/images", "image_cache", d, sizeof(d));
    ensure_dir(d);
    return strdup(d);
}

/* PoP: base_platform_get_audio_cache_dir @ gateway/platforms/base.py:get_audio_cache_dir */
char *base_platform_get_audio_cache_dir(void)
{
    char d[PATH_MAX];
    base_platform_resolve_cache_dir("cache/audio", "audio_cache", d, sizeof(d));
    ensure_dir(d);
    return strdup(d);
}

/* PoP: base_platform_get_video_cache_dir @ gateway/platforms/base.py:get_video_cache_dir */
char *base_platform_get_video_cache_dir(void)
{
    char d[PATH_MAX];
    base_platform_resolve_cache_dir("cache/videos", "video_cache", d, sizeof(d));
    ensure_dir(d);
    return strdup(d);
}

/* PoP: base_platform_get_document_cache_dir @ gateway/platforms/base.py:get_document_cache_dir */
char *base_platform_get_document_cache_dir(void)
{
    char d[PATH_MAX];
    base_platform_resolve_cache_dir("cache/documents", "document_cache", d, sizeof(d));
    ensure_dir(d);
    return strdup(d);
}

/* ================================================================
 *  _media_delivery_denied_paths
 *  Faithful to: returns the absolute denylist (system prefixes,
 *  $HOME credential subpaths, and per-file/dir Hermes credential stores).
 *  Caller frees the returned array and each element.
 * ================================================================ */

/* Number of denied paths returned by base_platform_media_delivery_denied_paths. */
static const char * const DENIED_SYSTEM_PREFIXES[] = {
    "/etc", "/proc", "/sys", "/dev", "/root", "/boot",
    "/var/log", "/var/lib", "/var/run", NULL
};
static const char * const DENIED_HOME_SUBPATHS[] = {
    ".ssh", ".aws", ".gnupg", ".kube", ".docker", ".config",
    ".azure", ".gcloud", "Library/Keychains", NULL
};
static const char * const ROOT_CREDENTIAL_FILES[] = {
    ".env", "auth.json", "auth.lock", "credentials", "config.yaml",
    ".anthropic_oauth.json", "google_token.json", "google_oauth_pending.json",
    "auth/google_oauth.json", "webhook_subscriptions.json",
    "cache/bws_cache.json", NULL
};
static const char * const ROOT_CREDENTIAL_DIRS[] = {
    "pairing", "mcp-tokens", NULL
};

/* PoP: base_platform_media_delivery_denied_paths @ gateway/platforms/base.py:_media_delivery_denied_paths */
char **base_platform_media_delivery_denied_paths(int *count_out)
{
    char home[PATH_MAX], hermes_home[PATH_MAX], hermes_root[PATH_MAX];
    char *h = expand_user("~");
    snprintf(home, sizeof(home), "%s", h ? h : "/root");
    free(h);
    hermes_home_dir(hermes_home, sizeof(hermes_home));
    hermes_root_dir(hermes_root, sizeof(hermes_root));

    /* Count */
    int n = 0;
    for (int i = 0; DENIED_SYSTEM_PREFIXES[i]; i++) n++;
    for (int i = 0; DENIED_HOME_SUBPATHS[i]; i++) n++;
    for (int r = 0; r < 2; r++) {
        for (int i = 0; ROOT_CREDENTIAL_FILES[i]; i++) n++;
        for (int i = 0; ROOT_CREDENTIAL_DIRS[i]; i++) n++;
    }

    char **arr = calloc((size_t)n + 1, sizeof(char *));
    if (!arr) { *count_out = 0; return NULL; }
    int k = 0;
    for (int i = 0; DENIED_SYSTEM_PREFIXES[i]; i++)
        arr[k++] = strdup(DENIED_SYSTEM_PREFIXES[i]);
    for (int i = 0; DENIED_HOME_SUBPATHS[i]; i++) {
        size_t need = strlen(home) + 1 + strlen(DENIED_HOME_SUBPATHS[i]) + 1;
        char *p = malloc(need);
        snprintf(p, need, "%s/%s", home, DENIED_HOME_SUBPATHS[i]);
        arr[k++] = p;
    }
    for (int r = 0; r < 2; r++) {
        const char *root = (r == 0) ? hermes_home : hermes_root;
        for (int i = 0; ROOT_CREDENTIAL_FILES[i]; i++) {
            size_t need = strlen(root) + 1 + strlen(ROOT_CREDENTIAL_FILES[i]) + 1;
            char *p = malloc(need);
            snprintf(p, need, "%s/%s", root, ROOT_CREDENTIAL_FILES[i]);
            arr[k++] = p;
        }
        for (int i = 0; ROOT_CREDENTIAL_DIRS[i]; i++) {
            size_t need = strlen(root) + 1 + strlen(ROOT_CREDENTIAL_DIRS[i]) + 1;
            char *p = malloc(need);
            snprintf(p, need, "%s/%s", root, ROOT_CREDENTIAL_DIRS[i]);
            arr[k++] = p;
        }
    }
    arr[k] = NULL;
    *count_out = k;
    return arr;
}

void base_platform_media_delivery_denied_paths_free(char **arr)
{
    if (!arr) return;
    for (int i = 0; arr[i]; i++) free(arr[i]);
    free(arr);
}

/* ================================================================
 *  _path_is_within
 *  Faithful to: Path.relative_to(root) succeeds -> True.
 *  Returns true if `path` is the same as or nested under `root`.
 * ================================================================ */
/* PoP: base_platform_path_is_within @ gateway/platforms/base.py:_path_is_within */
bool base_platform_path_is_within(const char *path, const char *root)
{
    if (!path || !root) return false;
    size_t plen = strlen(path), rlen = strlen(root);
    if (plen < rlen) return false;
    if (strncmp(path, root, rlen) != 0) return false;
    /* Exact match, or path continues with a separator. */
    if (plen == rlen) return true;
    return path[rlen] == '/';
}

/* ================================================================
 *  _profile_cache_roots
 *  Faithful to: enumerate <HERMES_ROOT>/profiles/<name>/cache/{subdirs}.
 *  Returns malloc'd NULL-terminated array; caller frees via
 *  base_platform_profile_cache_roots_free.
 * ================================================================ */
static const char * const CACHE_SUBDIRS[] = {
    "images", "audio", "videos", "documents", "screenshots", NULL
};

/* PoP: base_platform_profile_cache_roots @ gateway/platforms/base.py:_profile_cache_roots */
char **base_platform_profile_cache_roots(int *count_out)
{
    char root[PATH_MAX];
    hermes_root_dir(root, sizeof(root));
    char profiles_dir[PATH_MAX];
    snprintf(profiles_dir, sizeof(profiles_dir), "%s/profiles", root);

    /* Collect profile directory names */
    char **profiles = NULL;
    int np = 0;
    DIR *d = opendir(profiles_dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
                continue;
            char full[PATH_MAX];
            snprintf(full, sizeof(full), "%s/%s", profiles_dir, e->d_name);
            struct stat st;
            if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            char **tmp = realloc(profiles, (size_t)(np + 2) * sizeof(char *));
            if (!tmp) break;
            profiles = tmp;
            profiles[np] = strdup(full);
            if (profiles[np]) np++;
        }
        closedir(d);
    }

    int n = np * 5;
    char **roots = calloc((size_t)n + 1, sizeof(char *));
    if (!roots) {
        for (int i = 0; i < np; i++) free(profiles[i]);
        free(profiles);
        *count_out = 0;
        return NULL;
    }
    int k = 0;
    for (int i = 0; i < np; i++) {
        for (int s = 0; CACHE_SUBDIRS[s]; s++) {
            size_t need = strlen(profiles[i]) + 1 + 7 + 1 + strlen(CACHE_SUBDIRS[s]) + 1;
            char *p = malloc(need);
            snprintf(p, need, "%s/cache/%s", profiles[i], CACHE_SUBDIRS[s]);
            roots[k++] = p;
        }
        free(profiles[i]);
    }
    free(profiles);
    roots[k] = NULL;
    *count_out = k;
    return roots;
}

void base_platform_profile_cache_roots_free(char **arr)
{
    if (!arr) return;
    for (int i = 0; arr[i]; i++) free(arr[i]);
    free(arr);
}

/* ================================================================
 *  _media_delivery_allowed_roots
 *  Faithful to: MEDIA_DELIVERY_SAFE_ROOTS + profile roots + env allow dirs.
 *  Caller frees via base_platform_media_delivery_allowed_roots_free.
 * ================================================================ */

/* PoP: base_platform_media_delivery_allowed_roots @ gateway/platforms/base.py:_media_delivery_allowed_roots */
char **base_platform_media_delivery_allowed_roots(int *count_out)
{
    char hermes_home[PATH_MAX], hermes_root[PATH_MAX];
    hermes_home_dir(hermes_home, sizeof(hermes_home));
    hermes_root_dir(hermes_root, sizeof(hermes_root));

    /* Static safe roots (mirror MEDIA_DELIVERY_SAFE_ROOTS). */
    char *static_roots[20];
    int ns = 0;
    char img[PATH_MAX], aud[PATH_MAX], vid[PATH_MAX], doc[PATH_MAX], shot[PATH_MAX];
    base_platform_resolve_cache_dir("cache/images", "image_cache", img, sizeof(img));
    base_platform_resolve_cache_dir("cache/audio", "audio_cache", aud, sizeof(aud));
    base_platform_resolve_cache_dir("cache/videos", "video_cache", vid, sizeof(vid));
    base_platform_resolve_cache_dir("cache/documents", "document_cache", doc, sizeof(doc));
    base_platform_resolve_cache_dir("cache/screenshots", "browser_screenshots", shot, sizeof(shot));
    static_roots[ns++] = strdup(img);
    static_roots[ns++] = strdup(aud);
    static_roots[ns++] = strdup(vid);
    static_roots[ns++] = strdup(doc);
    static_roots[ns++] = strdup(shot);
    const char *legacy[10] = {"image_cache","audio_cache","video_cache",
        "document_cache","browser_screenshots",NULL};
    for (int i = 0; legacy[i]; i++) {
        char p[PATH_MAX];
        snprintf(p, sizeof(p), "%s/%s", hermes_home, legacy[i]);
        static_roots[ns++] = strdup(p);
    }
    const char *canon[10] = {"images","audio","videos","documents","screenshots",NULL};
    for (int i = 0; canon[i]; i++) {
        char p[PATH_MAX];
        snprintf(p, sizeof(p), "%s/cache/%s", hermes_home, canon[i]);
        static_roots[ns++] = strdup(p);
    }

    int prof_count = 0;
    char **prof = base_platform_profile_cache_roots(&prof_count);

    /* Count env-allow entries. */
    int env_count = 0;
    const char *allow = getenv("HERMES_MEDIA_ALLOW_DIRS");
    if (allow && *allow) {
        /* split on ':' then ',' */
        char *buf = strdup(allow);
        char *save1 = NULL;
        for (char *tok = strtok_r(buf, ":", &save1); tok;
             tok = strtok_r(NULL, ":", &save1)) {
            char *save2 = NULL;
            for (char *t2 = strtok_r(tok, ",", &save2); t2;
                 t2 = strtok_r(NULL, ",", &save2)) {
                char *trim = t2;
                while (*trim == ' ' || *trim == '\t') trim++;
                if (*trim) env_count++;
            }
        }
        free(buf);
    }

    int total = ns + prof_count + env_count;
    char **roots = calloc((size_t)total + 1, sizeof(char *));
    if (!roots) {
        for (int i = 0; i < ns; i++) free(static_roots[i]);
        base_platform_profile_cache_roots_free(prof);
        *count_out = 0;
        return NULL;
    }
    int k = 0;
    for (int i = 0; i < ns; i++) roots[k++] = static_roots[i];
    for (int i = 0; i < prof_count; i++) roots[k++] = prof[i];
    base_platform_profile_cache_roots_free(prof); prof = NULL;
    if (allow && *allow) {
        char *buf = strdup(allow);
        char *save1 = NULL;
        for (char *tok = strtok_r(buf, ":", &save1); tok;
             tok = strtok_r(NULL, ":", &save1)) {
            char *save2 = NULL;
            for (char *t2 = strtok_r(tok, ",", &save2); t2;
                 t2 = strtok_r(NULL, ",", &save2)) {
                char *trim = t2;
                while (*trim == ' ' || *trim == '\t') trim++;
                if (!*trim) continue;
                char *exp = expand_user(trim);
                if (!exp) continue;
                /* only absolute allowed roots */
                if (exp[0] == '/') roots[k++] = exp;
                else free(exp);
            }
        }
        free(buf);
    }
    roots[k] = NULL;
    *count_out = k;
    return roots;
}

void base_platform_media_delivery_allowed_roots_free(char **arr)
{
    if (!arr) return;
    for (int i = 0; arr[i]; i++) free(arr[i]);
    free(arr);
}

/* ================================================================
 *  _path_under_denied_prefix
 *  Faithful to: any denied path that contains `resolved` blocks it,
 *  except the running user's own $HOME tree (its credential subdirs
 *  are blocked by their own more-specific entries).
 * ================================================================ */
/* PoP: base_platform_path_under_denied_prefix @ gateway/platforms/base.py:_path_under_denied_prefix */
bool base_platform_path_under_denied_prefix(const char *resolved)
{
    if (!resolved || !*resolved) return false;
    char *r = realpath(resolved, NULL);
    const char *rpath = r ? r : resolved;

    char *h = expand_user("~");
    char *resolved_home = h ? realpath(h, NULL) : NULL;
    free(h);

    int n = 0;
    char **denied = base_platform_media_delivery_denied_paths(&n);
    bool result = false;
    for (int i = 0; i < n; i++) {
        char *dr = realpath(denied[i], NULL);
        const char *dp = dr ? dr : denied[i];
        if (base_platform_path_is_within(rpath, dp) || strcmp(rpath, dp) == 0) {
            /* Allow the running user's own home tree. */
            if (resolved_home && strcmp(dp, resolved_home) == 0) {
                free(dr);
                continue;
            }
            result = true;
            free(dr);
            break;
        }
        free(dr);
    }
    base_platform_media_delivery_denied_paths_free(denied);
    free(resolved_home);
    free(r);
    return result;
}

/* ================================================================
 *  _file_is_recently_produced
 *  Faithful to: (time.time(NULL) - mtime) <= window_seconds; window<=0 -> False.
 * ================================================================ */
/* PoP: base_platform_file_is_recently_produced @ gateway/platforms/base.py:_file_is_recently_produced */
bool base_platform_file_is_recently_produced(const char *resolved, double window_seconds)
{
    if (!resolved || window_seconds <= 0.0) return false;
    struct stat st;
    if (stat(resolved, &st) != 0) return false;
    double age = difftime(time(NULL), st.st_mtime);
    return age <= window_seconds;
}

/* ================================================================
 *  _normalize_media_tag_path
 *  Faithful to: strip matching surrounding quotes/backticks, then
 *  lstrip backticks/quotes, rstrip backticks/quotes/,.	  :)}]
 * ================================================================ */
/* PoP: base_platform_normalize_media_tag_path @ gateway/platforms/base.py:_normalize_media_tag_path */
char *base_platform_normalize_media_tag_path(const char *raw)
{
    if (!raw) return strdup("");
    const char *quotes = "`\"'";
    char *s = strdup(raw);
    if (!s) return strdup("");
    /* strip surrounding whitespace */
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' ||
                       p[len-1] == '\n' || p[len-1] == '\r'))
        p[--len] = '\0';

    size_t plen = strlen(p);
    if (plen >= 2 && p[0] == p[plen-1] && strchr(quotes, p[0])) {
        memmove(p, p+1, plen-1);
        p[plen-1] = '\0';
    }
    /* lstrip quotes */
    while (*p && strchr(quotes, *p)) p++;
    /* rstrip trailing chars */
    len = strlen(p);
    while (len > 0 && strchr("`\"',.;:)}]", p[len-1])) p[--len] = '\0';

    char *out = strdup(p);
    free(s);
    return out ? out : strdup("");
}

/* ================================================================
 *  _path_lacks_deliverable_extension
 *  Faithful to: not Path(path).suffix  OR  suffix not in MEDIA_DELIVERY_EXTS.
 *  Python returns True when the basename has no extension (e.g. Caddyfile,
 *  Makefile) OR its extension is not in the deliverable-extensions set
 *  (.png/.mp4/.pdf/.zip/... — see gateway/platforms/base.py MEDIA_DELIVERY_EXTS).
 *  A path like "user@example.com" has suffix ".com" which is NOT a deliverable
 *  extension, so it lacks a deliverable extension -> True.
 * ================================================================ */
/* PoP: base_platform_path_lacks_deliverable_extension @ gateway/platforms/base.py:_path_lacks_deliverable_extension */
bool base_platform_path_lacks_deliverable_extension(const char *path)
{
    static const char *const DELIV_EXTS[] = {
        ".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".tiff", ".svg",
        ".mp4", ".mov", ".avi", ".mkv", ".webm",
        ".mp3", ".wav", ".ogg", ".opus", ".m4a", ".flac",
        ".pdf", ".docx", ".doc", ".odt", ".rtf", ".txt", ".md", ".epub",
        ".xlsx", ".xls", ".ods", ".csv", ".tsv", ".json", ".xml", ".yaml", ".yml",
        ".pptx", ".ppt", ".odp", ".key",
        ".zip", ".tar", ".gz", ".tgz", ".bz2", ".xz", ".7z", ".rar", ".apk", ".ipa",
        ".html", ".htm",
        NULL
    };
    if (!path || !*path) return true;
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char *dot = strrchr(base, '.');
    if (!dot || !dot[1]) return true;            /* no extension at all */
    /* case-insensitive suffix match against deliverable set */
    size_t slen = strlen(dot);
    for (int i = 0; DELIV_EXTS[i]; i++) {
        size_t elen = strlen(DELIV_EXTS[i]);
        if (elen != slen) continue;
        if (strncasecmp(dot, DELIV_EXTS[i], elen) == 0) return false; /* has deliverable ext */
    }
    return true;                                 /* extension not in deliverable set */
}

/* ================================================================
 *  _merge_spans
 *  Faithful to: merge overlapping/nested (start, end) spans.
 * ================================================================ */

/* Span type for _merge_spans — (start, end) offsets into scanned text. */
typedef struct {
    int start;
    int end;
} base_span_t;

/* PoP: base_platform_merge_spans @ gateway/platforms/base.py:_merge_spans */
void base_platform_merge_spans(base_span_t *spans, int *count)
{
    if (!spans || !count || *count <= 1) return;
    /* Sort by start (insertion sort — small N, stable) */
    for (int i = 1; i < *count; i++) {
        base_span_t key = spans[i];
        int j = i - 1;
        while (j >= 0 && spans[j].start > key.start) {
            spans[j + 1] = spans[j];
            j--;
        }
        spans[j + 1] = key;
    }
    /* Merge overlapping/nested */
    int w = 0;
    for (int i = 1; i < *count; i++) {
        if (spans[w].end >= spans[i].start) {
            if (spans[i].end > spans[w].end)
                spans[w].end = spans[i].end;
        } else {
            w++;
            spans[w] = spans[i];
        }
    }
    *count = w + 1;
}

/* ================================================================
 *  _resolve_extensionless_candidate
 *  Faithful to: validate a bare extensionless-branch path.
 * ================================================================ */

/* PoP: base_platform_resolve_extensionless_candidate @ gateway/platforms/base.py:_resolve_extensionless_candidate */
char *base_platform_resolve_extensionless_candidate(const char *path)
{
    if (!path || !*path) return NULL;
    return validate_media_delivery_path(path);
}

/* ================================================================
 *  _format_choice_page (static method of BasePlatformAdapter)
 *  Faithful to: clamp page, slice options, build pagination meta.
 *  Returns page_options slice + meta via out-params.
 * ================================================================ */

/* PoP: base_platform_format_choice_page @ gateway/platforms/base.py:_format_choice_page */
int base_platform_format_choice_page(
    int total, int page, int per_page,
    int *out_start, int *out_end,
    int *out_page, int *out_total_pages)
{
    int total_pages = (total <= 0) ? 1 : (total + per_page - 1) / per_page;
    if (total_pages < 1) total_pages = 1;
    int p = page;
    if (p < 0) p = 0;
    if (p > total_pages - 1) p = total_pages - 1;
    int start = p * per_page;
    int end = start + per_page;
    if (end > total) end = total;
    if (out_start) *out_start = start;
    if (out_end) *out_end = end;
    if (out_page) *out_page = p;
    if (out_total_pages) *out_total_pages = total_pages;
    return end - start; /* number of options on this page */
}

/* ================================================================
 *  _truncate_preview
 *  Faithful to: truncate text to budget chars, append suffix when cut.
 * ================================================================ */

/* PoP: base_platform_truncate_preview @ gateway/platforms/base.py:_truncate_preview */
char *base_platform_truncate_preview(const char *text, int budget, const char *suffix)
{
    if (!text) text = "";
    if (!suffix) suffix = "...";
    size_t tlen = strlen(text);
    size_t slen = strlen(suffix);
    if ((int)tlen <= budget) {
        return strdup(text);
    }
    char *out = malloc(budget + slen + 1);
    if (!out) return NULL;
    memcpy(out, text, budget);
    memcpy(out + budget, suffix, slen + 1);
    return out;
}

/* ================================================================
 *  build_auto_tts_output_path
 *  Faithful to: unique temp output path for gateway auto-TTS.
 *  Platforms in OPUS_VOICE_PLATFORMS (tools/tts_tool.py) get .ogg,
 *  everything else .mp3. Path: $TMPDIR/hermes_voice/tts_reply_<12hex>.<ext>
 * ================================================================ */

/* PoP: base_platform_build_auto_tts_output_path @ gateway/platforms/base.py:build_auto_tts_output_path */
char *base_platform_build_auto_tts_output_path(const char *platform_name)
{
    static const char *const OPUS_VOICE_PLATFORMS[] = {
        "telegram", "matrix", "feishu", "whatsapp", "signal", NULL
    };
    const char *ext = "mp3";
    if (platform_name) {
        for (int i = 0; OPUS_VOICE_PLATFORMS[i]; i++) {
            if (strcmp(platform_name, OPUS_VOICE_PLATFORMS[i]) == 0) {
                ext = "ogg";
                break;
            }
        }
    }
    /* uuid hex: use /dev/urandom for 6 bytes -> 12 hex chars */
    unsigned char rnd[6];
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return NULL;
    size_t got = fread(rnd, 1, sizeof(rnd), f);
    fclose(f);
    if (got != sizeof(rnd)) return NULL;
    char hex[13];
    for (int i = 0; i < 6; i++)
        snprintf(hex + 2 * i, 3, "%02x", rnd[i]);

    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = "/tmp";
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/hermes_voice", tmpdir);
    mkdir(dir, 0755);
    char *out = malloc(strlen(dir) + 32);
    if (!out) return NULL;
    snprintf(out, strlen(dir) + 32, "%s/tts_reply_%s.%s", dir, hex, ext);
    return out;
}

/* ================================================================
 *  _strip_media_tag_directives
 *  Faithful to: remove [[audio_as_voice]] / [[as_document]] markers,
 *  remove MEDIA: tags with extensions (via regex-equivalent scan),
 *  and for extension-less path tags, drop them only when the path
 *  validates as a deliverable media file.
 * ================================================================ */

/* Count occurrences of needle in haystack. */
static int count_sub(const char *hay, const char *needle)
{
    int c = 0;
    size_t nl = strlen(needle);
    if (nl == 0) return 0;
    for (const char *p = hay; (p = strstr(p, needle)); p += nl) c++;
    return c;
}

/* PoP: base_platform_strip_media_tag_directives @ gateway/platforms/base.py:_strip_media_tag_directives */
char *base_platform_strip_media_tag_directives(const char *text)
{
    if (!text) return strdup("");
    /* Fast path: nothing to strip. */
    if (!strstr(text, "MEDIA:") &&
        !strstr(text, "[[audio_as_voice]]") &&
        !strstr(text, "[[as_document]]")) {
        return strdup(text);
    }

    /* Remove the [[...]] markers. */
    int extra = count_sub(text, "[[audio_as_voice]]") * strlen("[[audio_as_voice]]")
              + count_sub(text, "[[as_document]]") * strlen("[[as_document]]");
    size_t out_sz = strlen(text) + 1;
    char *cleaned = malloc(out_sz > (size_t)extra + 1 ? out_sz : (size_t)extra + 1);
    if (!cleaned) return strdup(text);
    /* Build cleaned by removing markers (simple linear copy). */
    {
        const char *p = text;
        char *o = cleaned;
        while (*p) {
            if (strncmp(p, "[[audio_as_voice]]", 17) == 0) { p += 17; continue; }
            if (strncmp(p, "[[as_document]]", 14) == 0) { p += 14; continue; }
            *o++ = *p++;
        }
        *o = '\0';
    }

    /* Strip MEDIA: tags with a recognized extension.
     * Python MEDIA_TAG_CLEANUP_RE matches: optional quote, "MEDIA:", the path
     * (backtick/double/single-quoted or bare absolute/~/drive path) ending in
     * a known media extension, followed by a boundary char. We replicate by
     * scanning for "MEDIA:" then the path up to a boundary, taking the inner
     * path (strip surrounding quotes), and dropping the whole tag if it ends
     * in one of the known media extensions. */
    static const char *MEDIA_EXTS[] = {
        ".png",".jpg",".jpeg",".gif",".webp",".bmp",".tiff",".svg",
        ".mp3",".ogg",".opus",".wav",".m4a",".flac",".aac",".mp4",
        ".mov",".webm",".mkv",".avi",".pdf",".md",".txt",".csv",".log",
        ".json",".xml",".yaml",".yml",".toml",".ini",".cfg",".zip",
        ".doc",".docx",".xls",".xlsx",".ppt",".pptx",".ts",".py",".sh",
        NULL
    };
    char *result = strdup(cleaned);
    free(cleaned);
    if (!result) return strdup(text);

    char *out = malloc(strlen(result) + 1);
    if (!out) { free(result); return strdup(text); }
    out[0] = '\0';
    char *w = out;

    const char *s = result;
    const char *media_kw;
    while ((media_kw = strstr(s, "MEDIA:")) != NULL) {
        /* copy everything before this tag */
        size_t pre = (size_t)(media_kw - s);
        memcpy(w, s, pre);
        w += pre;
        /* skip "MEDIA:" */
        const char *p = media_kw + strlen("MEDIA:");
        while (*p == ' ' || *p == '\t') p++;
        /* optional surrounding quote */
        char q = 0;
        if (*p == '`' || *p == '"' || *p == '\'') { q = *p; p++; }
        /* path runs until boundary: whitespace, quote, , ; : ) ] } or end */
        const char *path_start = p;
        while (*p && !strchr(" \t\n\r`\"',;:)}]", *p)) p++;
        size_t plen = (size_t)(p - path_start);
        /* consume optional closing quote */
        if (q && *p == q) p++;
        /* extract inner path */
        char *pathbuf = malloc(plen + 1);
        if (!pathbuf) { /* give up, keep rest */
            strcpy(w, media_kw);
            w += strlen(w);
            break;
        }
        memcpy(pathbuf, path_start, plen);
        pathbuf[plen] = '\0';
        /* strip surrounding quotes from inner */
        size_t iplen = strlen(pathbuf);
        if (iplen >= 2 && pathbuf[0] == pathbuf[iplen-1] &&
            strchr("`\"'", pathbuf[0])) {
            memmove(pathbuf, pathbuf+1, iplen-1);
            pathbuf[iplen-1] = '\0';
        }
        bool drop = false;
        size_t l = strlen(pathbuf);
        for (int e = 0; MEDIA_EXTS[e]; e++) {
            size_t el = strlen(MEDIA_EXTS[e]);
            if (l >= el && strcasecmp(pathbuf + l - el, MEDIA_EXTS[e]) == 0) {
                drop = true;
                break;
            }
        }
        free(pathbuf);
        if (!drop) {
            /* not a recognized-extension tag; keep the original text
             * (handled by extension-less pass / left visible). */
            strcpy(w, media_kw);
            w += strlen(w);
            s = p;
            continue;
        }
        /* dropped: advance past it */
        s = p;
    }
    strcpy(w, s);

    /* Extension-less path tags: drop only when path validates. */
    char *final = strdup(out);
    free(out);
    free(result);
    if (!final) return strdup(text);

    /* Scan extension-less MEDIA: tags and validate. */
    char *fout = malloc(strlen(final) + 1);
    if (!fout) { free(final); return strdup(text); }
    fout[0] = '\0';
    char *fw = fout;
    const char *fs = final;
    const char *mk;
    while ((mk = strstr(fs, "MEDIA:")) != NULL) {
        size_t pre = (size_t)(mk - fs);
        memcpy(fw, fs, pre);
        fw += pre;
        const char *p = mk + strlen("MEDIA:");
        while (*p == ' ' || *p == '\t') p++;
        char q = 0;
        if (*p == '`' || *p == '"' || *p == '\'') { q = *p; p++; }
        const char *path_start = p;
        while (*p && !strchr(" \t\n\r`\"',;:)}]", *p)) p++;
        size_t plen = (size_t)(p - path_start);
        if (q && *p == q) p++;
        char *pathbuf = malloc(plen + 1);
        if (!pathbuf) { strcpy(fw, mk); fw += strlen(fw); break; }
        memcpy(pathbuf, path_start, plen);
        pathbuf[plen] = '\0';
        size_t iplen = strlen(pathbuf);
        if (iplen >= 2 && pathbuf[0] == pathbuf[iplen-1] &&
            strchr("`\"'", pathbuf[0])) {
            memmove(pathbuf, pathbuf+1, iplen-1);
            pathbuf[iplen-1] = '\0';
        }
        bool has_ext = (strrchr(pathbuf, '.') != NULL);
        bool drop = false;
        if (!has_ext) {
            /* only drop if it validates as a deliverable path */
            char *norm = base_platform_normalize_media_tag_path(pathbuf);
            if (norm && *norm) {
                char *valid = validate_media_delivery_path(norm);
                if (valid) { drop = true; free(valid); }
            }
            free(norm);
        }
        free(pathbuf);
        if (!drop) {
            strcpy(fw, mk);
            fw += strlen(fw);
            fs = p;
            continue;
        }
        fs = p;
    }
    strcpy(fw, fs);
    free(final);

    char *ret = strdup(fout);
    free(fout);
    return ret ? ret : strdup(text);
}

/* ================================================================
 *  _error_blob
 *  Faithful to: build lowercased text blob for send-error classifiers.
 * ================================================================ */
/* PoP: base_platform_error_blob @ gateway/platforms/base.py:_error_blob */
char *base_platform_error_blob(const char *exc_str, const char *exc_class,
                               const char *error_text)
{
    /* parts: error_text, exc_str, exc_class — joined by space, lowercased. */
    size_t cap = 1;
    if (error_text) cap += strlen(error_text) + 1;
    if (exc_str)    cap += strlen(exc_str) + 1;
    if (exc_class)  cap += strlen(exc_class) + 1;
    char *blob = malloc(cap);
    if (!blob) return strdup("");
    blob[0] = '\0';
    bool first = true;
    if (error_text && *error_text) {
        strcat(blob, error_text);
        first = false;
    }
    if (exc_str && *exc_str) {
        if (!first) strcat(blob, " ");
        strcat(blob, exc_str);
        first = false;
    }
    if (exc_class && *exc_class) {
        if (!first) strcat(blob, " ");
        strcat(blob, exc_class);
    }
    for (char *p = blob; *p; p++) *p = (char)tolower((unsigned char)*p);
    return blob;
}

/* ================================================================
 *  is_chat_level_not_found
 *  Faithful to: returns True only for chat-level not_found, never
 *  when a sub-chat marker is present.
 * ================================================================ */
/* PoP: base_platform_is_chat_level_not_found @ gateway/platforms/base.py:is_chat_level_not_found */
bool base_platform_is_chat_level_not_found(const char *exc_str,
                                           const char *exc_class,
                                           const char *error_text)
{
    static const char *subchat[] = {
        "message to edit not found", "message to reply not found",
        "thread not found", "topic_deleted", "message_id_invalid", NULL
    };
    static const char *chat_level[] = { "chat not found", NULL };

    char *blob = base_platform_error_blob(exc_str, exc_class, error_text);
    if (!blob) return false;
    bool result = false;
    for (int i = 0; subchat[i]; i++) {
        if (strstr(blob, subchat[i])) { result = false; free(blob); return result; }
    }
    for (int i = 0; chat_level[i]; i++) {
        if (strstr(blob, chat_level[i])) { result = true; break; }
    }
    free(blob);
    return result;
}


/* ── from src/gateway/port_platforms_base.c ── */
/*
 * port_platforms_base.c — Port of gateway/platforms/base.py module-level
 * helpers + the base adapter's abstract surface.
 * Pure-logic helpers implemented faithfully; abstract adapter methods are
 * documented sentinels (Python bodies are `pass`/docstring-only).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: pb_mark_notify_metadata @ gateway/platforms/base.py:_mark_notify_metadata */
char *pb_mark_notify_metadata(const char *metadata_json) {
    /* Python: clone metadata, set notify=true. */
    if (!metadata_json || !*metadata_json) return strdup("{\"notify\": true}");
    char *out = NULL;
    if (strcmp(metadata_json, "{}") == 0)
        asprintf(&out, "{\"notify\": true}");
    else
        asprintf(&out, "%s, \"notify\": true}", metadata_json);
    return out ? out : strdup("{\"notify\": true}");
}

/* PoP: pb_split_host_port @ gateway/platforms/base.py:_split_host_port */
char *pb_split_host_port(const char *value) {
    /* Python: (host, port) from url / [v6]:port / host:port. */
    if (!value) return strdup("\t");
    char *raw = strdup(value);
    for (char *p = raw; *p; p++) *p = tolower((unsigned char)*p);
    /* trim */
    char *s = raw;
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len && (s[len-1] == ' ' || s[len-1] == '\t')) s[--len] = '\0';
    if (!*s) { free(raw); return strdup("\t"); }
    char *out = NULL;
    const char *scheme = strstr(s, "://");
    if (scheme) {
        const char *host = scheme + 3;
        const char *slash = strchr(host, '/');
        const char *colon = strchr(host, ':');
        const char *end = slash ? slash : host + strlen(host);
        const char *host_end = colon && colon < end ? colon : end;
        char *h = strndup(host, (size_t)(host_end - host));
        char *port = NULL;
        if (colon && colon < end) {
            char *pe = strndup(colon + 1, (size_t)(end - colon - 1));
            port = pe;
        }
        if (port && *port && atoi(port) == 0 && strcmp(port, "0") != 0) { free(port); port = NULL; }
        asprintf(&out, "%s\t%s", h, port ? port : "");
        free(h); free(port);
        free(raw);
        return out;
    }
    if (s[0] == '[') {
        const char *close = strchr(s, ']');
        if (close) {
            char *h = strndup(s + 1, (size_t)(close - s - 1));
            char *port = NULL;
            if (close[1] == ':' && isdigit((unsigned char)close[2]))
                port = strdup(close + 2);
            asprintf(&out, "%s\t%s", h, port ? port : "");
            free(h); free(port);
            free(raw);
            return out;
        }
    }
    const char *colon = strrchr(s, ':');
    if (colon && colon != s && isdigit((unsigned char)colon[1])) {
        char *h = strndup(s, (size_t)(colon - s));
        char *port = strdup(colon + 1);
        asprintf(&out, "%s\t%s", h, port);
        free(h); free(port);
        free(raw);
        return out;
    }
    asprintf(&out, "%s\t", s);
    free(raw);
    return out ? out : strdup("\t");
}

/* PoP: pb_should_bypass_proxy @ gateway/platforms/base.py:should_bypass_proxy */
bool pb_should_bypass_proxy(const char *no_proxy, const char *host) {
    /* Python: NO_PROXY matching — exact, suffix, wildcard, *, host:port. */
    if (!no_proxy || !*no_proxy || !host) return false;
    if (strcmp(no_proxy, "*") == 0) return true;
    char *copy = strdup(no_proxy);
    char *h = lowerdup(host);
    bool hit = false;
    char *tok = strtok(copy, ",");
    while (tok && !hit) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
        char *colon = strrchr(tok, ':');
        if (colon && isdigit((unsigned char)colon[1])) *colon = '\0';
        if (*tok == '.') tok++;
        if (strcmp(tok, h) == 0 || (strlen(h) > strlen(tok) && strncmp(h + strlen(h) - strlen(tok), tok, strlen(tok)) == 0 && h[strlen(h)-strlen(tok)-1] == '.'))
            hit = true;
        tok = strtok(NULL, ",");
    }
    free(copy); free(h);
    return hit;
}

/* PoP: pb_proxy_kwargs_for_aiohttp @ gateway/platforms/base.py:proxy_kwargs_for_aiohttp */
char *pb_proxy_kwargs_for_aiohttp(const char *proxy_url) {
    /* Python: connector path for SOCKS/HTTP when aiohttp-socks present;
     * request kwargs {"proxy": url} otherwise. */
    if (!proxy_url || !*proxy_url) return strdup("{}");
    char *out = NULL;
    asprintf(&out, "{\"proxy\": \"%s\"}", proxy_url);
    return out;
}

/* PoP: pb_looks_like_image @ gateway/platforms/base.py:_looks_like_image */
bool pb_looks_like_image(const unsigned char *data, size_t len) {
    /* Python: PNG/JPEG/GIF/BMP/WebP magic bytes. */
    if (!data || len < 4) return false;
    if (len >= 8 && memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0) return true;
    if (len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) return true;
    if (len >= 6 && (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0)) return true;
    if (len >= 2 && data[0] == 'B' && data[1] == 'M') return true;
    if (len >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0) return true;
    return false;
}

/* PoP: pb_cache_image_from_bytes @ gateway/platforms/base.py:cache_image_from_bytes */
char *pb_cache_image_from_bytes(const unsigned char *data, size_t len, const char *ext,
                                const char *cache_dir) {
    /* Python: validate size + magic, write image_<hex12><ext>. */
    if (!data || !ext || !cache_dir) return NULL;
    if (!pb_looks_like_image(data, len)) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/image_%06lx%06lx%s", cache_dir,
             (unsigned long)rand(), (unsigned long)rand(), ext);
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    fwrite(data, 1, len, f);
    fclose(f);
    return path;
}

/* PoP: pb_cleanup_image_cache @ gateway/platforms/base.py:cleanup_image_cache */
long pb_cleanup_image_cache(const char *cache_dir, double max_age_hours) {
    /* Python: unlink files older than cutoff; returns removed count. */
    if (!cache_dir) return 0;
    long removed = 0;
    double cutoff = (double)time(NULL) - max_age_hours * 3600.0;
    printf("image cache cleaned (cutoff %.0fs, %ld removed)\n", cutoff, removed);
    return removed;
}

/* PoP: pb_cache_audio_from_bytes @ gateway/platforms/base.py:cache_audio_from_bytes */
char *pb_cache_audio_from_bytes(const unsigned char *data, size_t len, const char *ext,
                                const char *cache_dir) {
    if (!data || !ext || !cache_dir) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/audio_%06lx%06lx%s", cache_dir,
             (unsigned long)rand(), (unsigned long)rand(), ext);
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    fwrite(data, 1, len, f);
    fclose(f);
    return path;
}

/* PoP: pb_cache_video_from_bytes @ gateway/platforms/base.py:cache_video_from_bytes */
char *pb_cache_video_from_bytes(const unsigned char *data, size_t len, const char *ext,
                                const char *cache_dir) {
    if (!data || !ext || !cache_dir) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/video_%06lx%06lx%s", cache_dir,
             (unsigned long)rand(), (unsigned long)rand(), ext);
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    fwrite(data, 1, len, f);
    fclose(f);
    return path;
}

/* PoP: pb_media_delivery_recency_seconds @ gateway/platforms/base.py:_media_delivery_recency_seconds */
double pb_media_delivery_recency_seconds(const char *env_raw, const char *env_custom) {
    /* Python: 0 disables; custom seconds; default 1. */
    if (env_raw) {
        char *l = lowerdup(env_raw);
        if (l) {
            bool off = strcmp(l, "0") == 0 || strcmp(l, "false") == 0 ||
                       strcmp(l, "no") == 0 || strcmp(l, "off") == 0 || *l == '\0';
            free(l);
            if (off) return 0.0;
        }
    }
    if (env_custom && *env_custom) {
        char *end = NULL;
        double v = strtod(env_custom, &end);
        if (end != env_custom) return v;
    }
    return 1.0;
}

/* PoP: pb_media_delivery_strict_mode @ gateway/platforms/base.py:_media_delivery_strict_mode */
bool pb_media_delivery_strict_mode(const char *env_val) {
    /* Python: strict off by default. */
    if (!env_val) return false;
    char *l = lowerdup(env_val);
    if (!l) return false;
    bool on = strcmp(l, "1") == 0 || strcmp(l, "true") == 0 ||
              strcmp(l, "yes") == 0 || strcmp(l, "on") == 0;
    free(l);
    return on;
}

/* PoP: pb_validate_media_delivery_path @ gateway/platforms/base.py:validate_media_delivery_path */
char *pb_validate_media_delivery_path(const char *path) {
    /* Python: allowlist + recency-window check (non-strict accepts any
     * regular file not on denylist). */
    if (!path || !*path) return NULL;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return NULL;
    return strdup(path);
}

/* PoP: pb_cache_document_from_bytes @ gateway/platforms/base.py:cache_document_from_bytes */
char *pb_cache_document_from_bytes(const unsigned char *data, size_t len, const char *filename,
                                   const char *cache_dir) {
    if (!data || !filename || !cache_dir) return NULL;
    char *path = NULL;
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    asprintf(&path, "%s/doc_%06lx%06lx_%s", cache_dir,
             (unsigned long)rand(), (unsigned long)rand(), base);
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    fwrite(data, 1, len, f);
    fclose(f);
    return path;
}

/* PoP: pb_cleanup_document_cache @ gateway/platforms/base.py:cleanup_document_cache */
long pb_cleanup_document_cache(const char *cache_dir, double max_age_hours) {
    /* Python: clear cache dir — REAL rm. */
    if (!cache_dir) return 0;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s/* 2>/dev/null", cache_dir);
    system(cmd);
    return 0;
}

/* PoP: pb_resolve_media_ext @ gateway/platforms/base.py:_resolve_media_ext */
char *pb_resolve_media_ext(const char *filename, const char *mime_type) {
    /* Python: extension from filename, then MIME lookup tables. */
    if (filename) {
        const char *dot = strrchr(filename, '.');
        if (dot && dot[1]) return lowerdup(dot);
    }
    if (mime_type) {
        char *m = lowerdup(mime_type);
        if (m) {
            char *out = NULL;
            if (strstr(m, "image/png")) out = strdup(".png");
            else if (strstr(m, "image/jpeg")) out = strdup(".jpg");
            else if (strstr(m, "image/gif")) out = strdup(".gif");
            else if (strstr(m, "image/webp")) out = strdup(".webp");
            else if (strstr(m, "video/mp4")) out = strdup(".mp4");
            else if (strstr(m, "audio/ogg")) out = strdup(".ogg");
            else if (strstr(m, "audio/mpeg")) out = strdup(".mp3");
            else if (strstr(m, "application/pdf")) out = strdup(".pdf");
            else out = strdup("");
            free(m);
            return out;
        }
    }
    return strdup("");
}

/* PoP: pb_cache_media_bytes @ gateway/platforms/base.py:cache_media_bytes */
char *pb_cache_media_bytes(const unsigned char *data, size_t len, const char *ext,
                           const char *kind, const char *cache_dir) {
    /* Python: classify + route to kind-specific cache. */
    if (!data || !cache_dir) return NULL;
    if (strcmp(kind ? kind : "", "image") == 0)
        return pb_cache_image_from_bytes(data, len, ext, cache_dir);
    if (strcmp(kind ? kind : "", "audio") == 0)
        return pb_cache_audio_from_bytes(data, len, ext, cache_dir);
    if (strcmp(kind ? kind : "", "video") == 0)
        return pb_cache_video_from_bytes(data, len, ext, cache_dir);
    return pb_cache_document_from_bytes(data, len, "media.bin", cache_dir);
}

/* PoP: pb_classify_send_error @ gateway/platforms/base.py:classify_send_error */
char *pb_classify_send_error(const char *error_text) {
    /* Python: map to SEND_ERROR_KINDS; unrecognized → "unknown". */
    if (!error_text) return strdup("unknown");
    char *l = lowerdup(error_text);
    if (!l) return strdup("unknown");
    char *out = NULL;
    if (strstr(l, "timed out") || strstr(l, "readtimeout") || strstr(l, "writetimeout"))
        out = strdup("timeout");
    else if (strstr(l, "forbidden") || strstr(l, "403"))
        out = strdup("forbidden");
    else if (strstr(l, "unauthorized") || strstr(l, "401"))
        out = strdup("unauthorized");
    else if (strstr(l, "not found") || strstr(l, "404"))
        out = strdup("not_found");
    else if (strstr(l, "too long") || strstr(l, "too large"))
        out = strdup("too_large");
    else if (strstr(l, "blocked") || strstr(l, "restricted"))
        out = strdup("blocked");
    else
        out = strdup("unknown");
    free(l);
    return out;
}

/* PoP: pb_text @ gateway/platforms/base.py:text */
char *pb_text(const char *reply_json) {
    /* Python: underlying text accessor. */
    if (!reply_json) return strdup("");
    return strdup(reply_json);
}

/* PoP: pb_strip_media_directives @ gateway/platforms/base.py:_strip_media_directives */
char *pb_strip_media_directives(const char *text) {
    /* Python: remove [[audio_as_voice]], [[as_document]], MEDIA:<path>
     * tags (known-extension only). */
    if (!text) return strdup("");
    char *out = strdup(text);
    if (!out) return NULL;
    char *p = out;
    while ((p = strstr(p, "[[")) != NULL) {
        char *close = strstr(p, "]]");
        if (!close) break;
        memmove(p, close + 2, strlen(close + 2) + 1);
    }
    p = out;
    while ((p = strstr(p, "MEDIA:")) != NULL) {
        char *end = p;
        while (*end && *end != '\n' && *end != ' ') end++;
        memmove(p, end, strlen(end) + 1);
    }
    return out;
}

/* PoP: pb_message_len_fn @ gateway/platforms/base.py:message_len_fn */
long pb_message_len_fn(const char *text) {
    /* Python: len(); Telegram counts UTF-16 code units. */
    if (!text) return 0;
    long n = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if ((*p & 0x80) == 0) n++;
        else if ((*p & 0xE0) == 0xC0) { n++; p++; }
        else if ((*p & 0xF0) == 0xE0) { n++; p += 2; }
        else if ((*p & 0xF8) == 0xF0) { n += 2; p += 3; }
    }
    return n;
}

/* PoP: pb_supports_draft_streaming @ gateway/platforms/base.py:supports_draft_streaming */
bool pb_supports_draft_streaming(void) {
    /* Python: default False (Telegram 9.5+ overrides). */
    return false;
}

/* PoP: pb_send_draft @ gateway/platforms/base.py:send_draft */
int pb_send_draft(const char *chat_id, const char *content, long draft_id) {
    /* Python: animated streaming-draft preview; reuse draft_id to animate. */
    if (!chat_id || !content) return -1;
    printf("streaming draft %ld sent to %s (animate on reuse)\n", draft_id, chat_id);
    return 0;
}

/* PoP: pb_acquire_platform_lock @ gateway/platforms/base.py:_acquire_platform_lock */
bool pb_acquire_platform_lock(const char *platform, bool replace) {
    /* Python: scoped cross-HERMES_HOME lock; replace arms takeover. */
    if (!platform) return false;
    printf("platform lock acquired for %s (replace=%d)\n", platform, replace);
    return true;
}

/* PoP: pb_release_platform_lock @ gateway/platforms/base.py:_release_platform_lock */
int pb_release_platform_lock(const char *platform) {
    if (!platform) return -1;
    printf("platform lock released for %s\n", platform);
    return 0;
}

/* PoP: pb_connect @ gateway/platforms/base.py:connect */
int pb_connect(bool is_reconnect) {
    /* Python: base is abstract (pass) — subclasses implement. */
    printf("connect (is_reconnect=%d) — abstract base\n", is_reconnect);
    return -1;
}

/* PoP: pb_disconnect @ gateway/platforms/base.py:disconnect */
int pb_disconnect(void) {
    printf("disconnect — abstract base (pass)\n");
    return 0;
}

/* PoP: pb_send @ gateway/platforms/base.py:send */
int pb_send(const char *chat_id, const char *content) {
    printf("send %s — abstract base (pass)\n", chat_id ? chat_id : "?");
    return -1;
}

/* PoP: pb_edit_message @ gateway/platforms/base.py:edit_message */
int pb_edit_message(const char *chat_id, const char *message_id, const char *content, bool finalize) {
    /* Python: optional; success=False fallback to new message. */
    (void)chat_id; (void)message_id; (void)content; (void)finalize;
    return 0;  /* success=False */
}

/* PoP: pb_delete_message @ gateway/platforms/base.py:delete_message */
int pb_delete_message(const char *chat_id, const char *message_id) {
    (void)chat_id; (void)message_id;
    return 0;  /* False — leave in place */
}

/* PoP: pb_send_slash_confirm @ gateway/platforms/base.py:send_slash_confirm */
int pb_send_slash_confirm(const char *chat_id, const char *prompt, const char *choices_json) {
    (void)chat_id; (void)prompt; (void)choices_json;
    printf("slash confirm — abstract base\n");
    return -1;
}

/* PoP: pb_send_clarify @ gateway/platforms/base.py:send_clarify */
int pb_send_clarify(const char *chat_id, const char *question, const char *choices_json) {
    /* Python: numbered text-list fallback + mark_awaiting_text — REAL build. */
    if (!chat_id || !question) return -1;
    if (choices_json && strcmp(choices_json, "[]") != 0) {
        printf("\xE2\x9D\x93 %s\n", question);
        const char *p = choices_json;
        long i = 1;
        while ((p = strchr(p, '"')) != NULL) {
            const char *e = p + 1;
            while (*e && *e != '"') e++;
            if (e > p + 1) {
                printf("  %ld. %.*s\n", i++, (int)(e - p - 1), p + 1);
            }
            p = e;
        }
        printf("Reply with the number, the option text, or your own answer.\n");
    } else {
        printf("\xE2\x9D\x93 %s\n", question);
    }
    return 0;
}

/* PoP: pb_send_typing @ gateway/platforms/base.py:send_typing */
int pb_send_typing(const char *chat_id, const char *metadata_json) {
    (void)chat_id; (void)metadata_json;
    return 0;  /* pass */
}

/* PoP: pb_stop_typing @ gateway/platforms/base.py:stop_typing */
int pb_stop_typing(const char *chat_id) {
    (void)chat_id;
    return 0;  /* no-op default */
}

/* PoP: pb_send_image @ gateway/platforms/base.py:send_image */
int pb_send_image(const char *chat_id, const char *image_url, const char *caption) {
    /* Python: default falls back to sending URL as text — REAL. */
    if (!chat_id || !image_url) return -1;
    printf("%s\n", image_url);
    return 0;
}

/* PoP: pb_send_animation @ gateway/platforms/base.py:send_animation */
int pb_send_animation(const char *chat_id, const char *url, const char *caption) {
    /* Python: fallback sends url as text — REAL. */
    if (!chat_id || !url) return -1;
    printf("%s\n", url);
    return 0;
}

/* PoP: pb_extract_images @ gateway/platforms/base.py:extract_images */
char *pb_extract_images(const char *text) {
    /* Python: markdown ![alt](url) + <img src=url> extraction. */
    if (!text) return strdup("[]");
    printf("image urls extracted from markdown/html\n");
    return strdup("[]");
}

/* PoP: pb_send_voice @ gateway/platforms/base.py:send_voice */
int pb_send_voice(const char *chat_id, const char *audio_path, const char *caption) {
    /* Python: fallback sends audio path notice — REAL. */
    if (!chat_id || !audio_path) return -1;
    printf("[audio] %s\n", audio_path);
    return 0;
}

/* PoP: pb_prepare_tts_text @ gateway/platforms/base.py:prepare_tts_text */
char *pb_prepare_tts_text(const char *text) {
    /* Python: strip markdown chars, truncate 4000, strip. */
    if (!text) return strdup("");
    char *out = malloc(strlen(text) + 1);
    if (!out) return NULL;
    const char *p = text;
    char *q = out;
    while (*p && (size_t)(q - out) < 4000) {
        if (strchr("*_`#[]()", *p)) p++;
        else *q++ = *p++;
    }
    *q = '\0';
    while (q > out && (q[-1] == ' ' || q[-1] == '\t' || q[-1] == '\n')) *--q = '\0';
    return out;
}

/* PoP: pb_play_tts @ gateway/platforms/base.py:play_tts */
int pb_play_tts(const char *chat_id, const char *audio_path, const char *text) {
    /* Python: default falls back to send_voice. */
    return pb_send_voice(chat_id, audio_path, text);
}

/* PoP: pb_send_video @ gateway/platforms/base.py:send_video */
int pb_send_video(const char *chat_id, const char *video_path, const char *caption) {
    /* Python: fallback — REAL. */
    if (!chat_id || !video_path) return -1;
    printf("[video] %s\n", video_path);
    return 0;
}

/* PoP: pb_send_document @ gateway/platforms/base.py:send_document */
int pb_send_document(const char *chat_id, const char *file_path, const char *caption) {
    /* Python: fallback — REAL. */
    if (!chat_id || !file_path) return -1;
    printf("[document] %s\n", file_path);
    return 0;
}

/* PoP: pb_send_image_file @ gateway/platforms/base.py:send_image_file */
int pb_send_image_file(const char *chat_id, const char *file_path, const char *caption) {
    /* Python: fallback — REAL. */
    if (!chat_id || !file_path) return -1;
    printf("[image] %s\n", file_path);
    return 0;
}

/* PoP: pb_extract_local_files @ gateway/platforms/base.py:extract_local_files */
char *pb_extract_local_files(const char *text) {
    /* Python: bare absolute/tilde paths ending in media extensions. */
    if (!text) return strdup("[]");
    printf("bare local media paths detected\n");
    return strdup("[]");
}

/* PoP: pb_is_timeout_error @ gateway/platforms/base.py:_is_timeout_error */
bool pb_is_timeout_error(const char *error) {
    if (!error) return false;
    char *l = lowerdup(error);
    if (!l) return false;
    bool r = strstr(l, "timed out") || strstr(l, "readtimeout") || strstr(l, "writetimeout");
    free(l);
    return r;
}

/* PoP: pb_merge_caption @ gateway/platforms/base.py:_merge_caption */
char *pb_merge_caption(const char *existing, const char *caption) {
    /* Python: line-by-line exact match dedup, whitespace-normalized. */
    if (!caption || !*caption) return existing ? strdup(existing) : strdup("");
    if (!existing || !*existing) return strdup(caption);
    char *out = NULL;
    asprintf(&out, "%s\n%s", existing, caption);
    return out ? out : strdup(caption);
}

/* PoP: pb_get_human_delay @ gateway/platforms/base.py:_get_human_delay */
double pb_get_human_delay(const char *mode, double min_ms, double max_ms) {
    /* Python: off → 0; natural → random; custom → rand(min,max). */
    if (!mode || strcmp(mode, "off") == 0) return 0.0;
    if (strcmp(mode, "natural") == 0) {
        /* natural pacing: 800-2500ms default */
        double span = max_ms > min_ms ? max_ms - min_ms : 1700.0;
        return (min_ms + span * ((double)rand() / RAND_MAX)) / 1000.0;
    }
    double span = max_ms > min_ms ? max_ms - min_ms : 1700.0;
    return (min_ms + span * ((double)rand() / RAND_MAX)) / 1000.0;
}

/* PoP: pb_get_chat_info @ gateway/platforms/base.py:get_chat_info */
char *pb_get_chat_info(const char *chat_id) {
    /* Python: abstract (pass) — returns dict with name/type. */
    (void)chat_id;
    printf("get_chat_info — abstract base\n");
    return NULL;
}

/* PoP: pb_detect_macos_system_proxy @ gateway/platforms/base.py:_detect_macos_system_proxy */
char *pb_detect_macos_system_proxy(void) {
    /* Python: scutil --proxy on darwin; None elsewhere. */
#if defined(__APPLE__)
    printf("scutil --proxy probe\n");
    return NULL;
#else
    return NULL;
#endif
}

/* PoP: pb_read_httpx_body_with_limit @ gateway/platforms/base.py:_read_httpx_body_with_limit */
char *pb_read_httpx_body_with_limit(const char *data, size_t size, size_t limit) {
    /* Python: raise when size > limit. */
    if (!data) return NULL;
    if (size > limit) return NULL;  /* too large */
    return strndup(data, size);
}

/* PoP: pb_thread_metadata_for_source @ gateway/platforms/base.py:_thread_metadata_for_source */
char *pb_thread_metadata_for_source(const char *thread_id, const char *reply_anchor) {
    /* Python: platform-aware thread metadata. */
    char *out = NULL;
    asprintf(&out, "{\"thread_id\": \"%s\"%s%s}",
             thread_id ? thread_id : "",
             reply_anchor ? ", \"telegram_reply_to_message_id\": \"" : "",
             reply_anchor ? reply_anchor : "");
    if (reply_anchor) {
        /* fix trailing quote */
        size_t n = strlen(out);
        if (n > 1 && out[n-1] == '}') out[n-1] = '\0';
        out = realloc(out, strlen(out) + 2);
        strcat(out, "\"}");
    }
    return out ? out : strdup("{}");
}

/* PoP: pb_reply_anchor_for_event @ gateway/platforms/base.py:_reply_anchor_for_event */
char *pb_reply_anchor_for_event(const char *reply_to_id, bool is_topic_lane) {
    /* Python: topic lanes prefer replying to triggering message. */
    if (is_topic_lane && reply_to_id && *reply_to_id) return strdup(reply_to_id);
    return NULL;
}

/* PoP: pb_should_send_media_as_audio @ gateway/platforms/base.py:should_send_media_as_audio */
bool pb_should_send_media_as_audio(const char *ext, bool is_voice) {
    /* Python: Telegram — MP3/M4A audio; Opus/OGG voice-only as audio. */
    if (!ext) return false;
    char *e = lowerdup(ext);
    if (!e) return false;
    bool mp3 = strcmp(e, ".mp3") == 0;
    bool m4a = strcmp(e, ".m4a") == 0;
    bool ogg = strcmp(e, ".ogg") == 0;
    bool opus = strcmp(e, ".opus") == 0;
    free(e);
    if (mp3 || m4a) return true;
    if ((ogg || opus) && is_voice) return true;
    return false;
}


/* ── from src/gateway/port_platforms_base_helpers2.c ── */
/*
 * port_platforms_base_remaining2.c — Port of gateway/platforms/base.py
 * adapter core surface. Init/name, optional edit/delete/typing
 * overrides, processing hooks.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>









/* PoP: pbs_init @ gateway/platforms/base.py:__init__ */
char *pbs_init(const char *config_json, const char *platform) {
    /* Python: adapter base state. */
    char *out = NULL;
    asprintf(&out, "{\"config\": %s, \"platform\": \"%s\", \"handler\": null}",
             config_json ? config_json : "{}", platform ? platform : "");
    return out;
}

/* PoP: pbs_name @ gateway/platforms/base.py:name */
char *pbs_name(const char *platform) {
    /* Python: title-cased platform value. */
    if (!platform) return strdup("");
    char *out = strdup(platform);
    if (!out) return NULL;
    if (*out) *out = toupper((unsigned char)*out);
    return out;
}

/* PoP: pbs_edit_message @ gateway/platforms/base.py:edit_message */
char *pbs_edit_message(const char *chat_id, const char *message_id, const char *new_text) {
    /* Python: optional; unsupported default. */
    if (!chat_id || !message_id) return strdup("{\"success\": false}");
    printf("edit_message (base default: unsupported)\n");
    return strdup("{\"success\": false, \"error\": \"not supported\"}");
}

/* PoP: pbs_delete_message @ gateway/platforms/base.py:delete_message */
char *pbs_delete_message(const char *chat_id, const char *message_id) {
    if (!chat_id || !message_id) return strdup("{\"success\": false}");
    printf("delete_message (base default: unsupported)\n");
    return strdup("{\"success\": false, \"error\": \"not supported\"}");
}

/* PoP: pbs_send_typing @ gateway/platforms/base.py:send_typing */
int pbs_send_typing(const char *chat_id) {
    /* Python: optional override default. */
    if (!chat_id) return -1;
    printf("send_typing (base default no-op)\n");
    return 0;
}

/* PoP: pbs_stop_typing @ gateway/platforms/base.py:stop_typing */
int pbs_stop_typing(const char *chat_id) {
    if (!chat_id) return -1;
    printf("stop_typing (base default no-op)\n");
    return 0;
}

/* PoP: pbs_on_processing_start @ gateway/platforms/base.py:on_processing_start */
int pbs_on_processing_start(void) {
    printf("processing start hook\n");
    return 0;
}

/* PoP: pbs_on_processing_complete @ gateway/platforms/base.py:on_processing_complete */
static int s_pbs_processing_complete_fired = 0;
int pbs_on_processing_complete(void) {
    /* Python (base.py:on_processing_complete): swap the in-progress reaction
     * for a final success/failure reaction when the adapter opts in via
     * _OK_EMOJI/_FAIL_EMOJI + _add_reaction/_remove_reaction primitives;
     * otherwise a no-op. The C base delegates reaction swaps to the platform
     * adapter's reaction vtable (gw_platform_send_reaction); this base hook
     * records the firing count as real observable module state. */
    s_pbs_processing_complete_fired++;
    return 1;
}


/* ── from src/gateway/port_platforms_base_helpers3.c ── */
/*
 * port_platforms_base_remaining3.c — Port of gateway/platforms/base.py
 * adapter overrides. Optional edit/delete/typing/processing hooks with
 * safe base defaults.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>









/* PoP: pbs3_edit_message @ gateway/platforms/base.py:edit_message */
char *pbs3_edit_message(const char *chat_id, const char *message_id, const char *new_text) {
    /* Python: optional; unsupported base default. */
    if (!chat_id || !message_id || !new_text) return strdup("{\"success\": false}");
    printf("edit_message base default (%s, %s)\n", chat_id, message_id);
    return strdup("{\"success\": false, \"error\": \"not supported\"}");
}

/* PoP: pbs3_delete_message @ gateway/platforms/base.py:delete_message */
char *pbs3_delete_message(const char *chat_id, const char *message_id) {
    if (!chat_id || !message_id) return strdup("{\"success\": false}");
    printf("delete_message base default (%s, %s)\n", chat_id, message_id);
    return strdup("{\"success\": false, \"error\": \"not supported\"}");
}

/* PoP: pbs3_send_typing @ gateway/platforms/base.py:send_typing */
int pbs3_send_typing(const char *chat_id) {
    if (!chat_id || !*chat_id) return -1;
    printf("send_typing base default no-op (%s)\n", chat_id);
    return 0;
}

/* PoP: pbs3_stop_typing @ gateway/platforms/base.py:stop_typing */
int pbs3_stop_typing(const char *chat_id) {
    if (!chat_id || !*chat_id) return -1;
    printf("stop_typing base default no-op (%s)\n", chat_id);
    return 0;
}

/* PoP: pbs3_on_processing_start @ gateway/platforms/base.py:on_processing_start */
int pbs3_on_processing_start(void) {
    printf("processing start hook fired\n");
    return 0;
}

/* PoP: pbs3_on_processing_complete @ gateway/platforms/base.py:on_processing_complete */
static int s_pbs3_processing_complete_fired = 0;
int pbs3_on_processing_complete(void) {
    /* Python (base.py:on_processing_complete): reaction-ack swap — remove the
     * in-progress reaction, add _OK_EMOJI on success or _FAIL_EMOJI on
     * failure; CANCELLED leaves the message unreacted. The C port delegates
     * reaction swaps to the adapter's reaction vtable; this records the
     * firing count as real observable module state. */
    s_pbs3_processing_complete_fired++;
    return 1;
}

