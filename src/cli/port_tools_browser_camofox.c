/*
 * port_tools_browser_camofox.c — C port of tools/browser_camofox.py
 *
 * Camofox service integration: remote browser control via Camofox API.
 * Provides URL resolution, availability checks, VNC access, tab management,
 * navigation, input simulation, snapshot, vision, and console access.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include "hermes_json.h"
#include "hermes_logger.h"
#include "base64.h"
#include "libwebsocket/websocket.h"

/* ── Camofox URL resolution ───────────────────────────────────── */

/* PoP: get_camofox_url @ tools/browser_camofox.py:get_camofox_url */
char *get_camofox_url(void) {
    const char *url = getenv("CAMEOFox_URL");
    if (url && url[0]) return strdup(url);
    /* Check CAMEOFox_HOST and CAMEOFox_PORT */
    const char *host = getenv("CAMEOFox_HOST");
    const char *port = getenv("CAMEOFox_PORT");
    if (!host || !host[0]) host = "127.0.0.1";
    int p = 8080;
    if (port) {
        int parsed = atoi(port);
        if (parsed > 0) p = parsed;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "http://%s:%d", host, p);
    return strdup(buf);
}

/* PoP: _config_cdp_url @ tools/browser_camofox.py:_config_cdp_url */
char *_config_cdp_url(const char *camofox_url) {
    if (!camofox_url || !camofox_url[0]) return NULL;
    char buf[512];
    snprintf(buf, sizeof(buf), "%s/cdp", camofox_url);
    return strdup(buf);
}

/* PoP: check_camofox_available @ tools/browser_camofox.py:check_camofox_available */
bool check_camofox_available(void) {
    char *url = get_camofox_url();
    if (!url) return false;
    /* Attempt a simple HTTP GET to /health or / */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "curl -s -o /dev/null -w '%%{http_code}' %s 2>/dev/null", url);
    FILE *fp = popen(cmd, "r");
    if (!fp) { free(url); return false; }
    char code[16] = {0};
    if (fgets(code, sizeof(code), fp)) {
        int http_code = atoi(code);
        pclose(fp);
        free(url);
        return http_code >= 200 && http_code < 400;
    }
    pclose(fp);
    free(url);
    return false;
}

/* PoP: get_vnc_url @ tools/browser_camofox.py:get_vnc_url */
char *get_vnc_url(void) {
    const char *host = getenv("CAMEOFox_HOST");
    const char *port = getenv("CAMEOFox_VNC_PORT");
    if (!host) host = "127.0.0.1";
    int p = 5900;
    if (port) {
        int parsed = atoi(port);
        if (parsed > 0) p = parsed;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "vnc://%s:%d", host, p);
    return strdup(buf);
}

/* ── Camofox config helpers ─────────────────────────────────── */

/* PoP: _get_camofox_config @ tools/browser_camofox.py:_get_camofox_config */
json_t *_get_camofox_config(void) {
    json_t *cfg = json_object();
    /* Load from CAMEOFox_ env vars */
    const char *debug = getenv("CAMEOFox_DEBUG");
    json_set(cfg, "debug", json_bool(debug && strcmp(debug, "1")==0));
    const char *timeout = getenv("CAMEOFox_TIMEOUT");
    int t = timeout ? atoi(timeout) : 30;
    json_set(cfg, "timeout", json_int(t));
    const char *max_sessions = getenv("CAMEOFox_MAX_SESSIONS");
    json_set(cfg, "max_sessions", json_int(max_sessions ? atoi(max_sessions) : 5));
    return cfg;
}

/* PoP: _managed_persistence_enabled @ tools/browser_camofox.py:_managed_persistence_enabled */
bool _managed_persistence_enabled(void) {
    const char *env = getenv("CAMEOFox_PERSISTENCE");
    return env && (strcmp(env, "1")==0 || strcmp(env, "true")==0 || strcmp(env, "enabled")==0);
}

/* PoP: _camofox_identity_override @ tools/browser_camofox.py:_camofox_identity_override */
char *_camofox_identity_override(void) {
    const char *id = getenv("CAMEOFox_IDENTITY");
    return id ? strdup(id) : NULL;
}

/* PoP: _env_flag @ tools/browser_camofox.py:_env_flag */
bool _env_flag(const char *flag_name, bool default_val) {
    const char *env = getenv(flag_name);
    if (!env) return default_val;
    return strcmp(env, "1")==0 || strcmp(env, "true")==0;
}

/* PoP: _adopt_existing_tab_enabled @ tools/browser_camofox.py:_adopt_existing_tab_enabled */
bool _adopt_existing_tab_enabled(void) {
    return _env_flag("CAMEOFox_ADOPT_TAB", true);
}

/* PoP: _loopback_rewrite_enabled @ tools/browser_camofox.py:_loopback_rewrite_enabled */
bool _loopback_rewrite_enabled(void) {
    return _env_flag("CAMEOFox_REWRITE_LOOPBACK", false);
}

/* PoP: _loopback_rewrite_host @ tools/browser_camofox.py:_loopback_rewrite_host */
char *_loopback_rewrite_host(void) {
    const char *h = getenv("CAMEOFox_LOOPBACK_HOST");
    return h ? strdup(h) : strdup("localhost");
}

/* PoP: _is_loopback_hostname @ tools/browser_camofox.py:_is_loopback_hostname */
bool _is_loopback_hostname(const char *hostname) {
    if (!hostname) return false;
    return strcmp(hostname, "localhost")==0 ||
           strcmp(hostname, "127.0.0.1")==0 ||
           strcmp(hostname, "::1")==0;
}

/* PoP: _rewrite_loopback_url_for_camofox @ tools/browser_camofox.py:_rewrite_loopback_url_for_camofox */
char *_rewrite_loopback_url_for_camofox(const char *url) {
    if (!url) return NULL;
    if (!_is_loopback_hostname(url)) return strdup(url);
    char *host = _loopback_rewrite_host();
    if (!host) return strdup(url);
    char buf[2048];
    snprintf(buf, sizeof(buf), "http://%s%s", host, strstr(url, "://") ?: url);
    free(host);
    return strdup(buf);
}

/* ── Tab management ───────────────────────────────────────────── */

/* PoP: _adopt_existing_tab @ tools/browser_camofox.py:_adopt_existing_tab */
char *_adopt_existing_tab(const char *camofox_url, const char *url) {
    if (!camofox_url || !url) return NULL;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -s -X POST %s/tabs/adopt -d '{\"url\":\"%s\"}' 2>/dev/null", camofox_url, url);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char buf[1024] = {0};
    if (fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return strdup(buf);
    }
    pclose(fp);
    return NULL;
}

/* PoP: _ensure_tab @ tools/browser_camofox.py:_ensure_tab */
char *_ensure_tab(const char *camofox_url, const char *url, bool new_window) {
    if (!camofox_url) return NULL;
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "curl -s -X POST %s/tabs/ensure -d '{\"url\":\"%s\",\"new_window\":%s}' 2>/dev/null",
             camofox_url, url, new_window ? "true" : "false");
    FILE *fp = popen(buf, "r");
    if (!fp) return NULL;
    char resp[1024] = {0};
    if (fgets(resp, sizeof(resp), fp)) {
        pclose(fp);
        return strdup(resp);
    }
    pclose(fp);
    return NULL;
}

/* PoP: _drop_session @ tools/browser_camofox.py:_drop_session */
void _drop_session(const char *camofox_url, const char *session_id) {
    if (!camofox_url || !session_id) return;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "curl -s -X DELETE %s/sessions/%s 2>/dev/null", camofox_url, session_id);
    system(cmd);
}

/* PoP: camofox_soft_cleanup @ tools/browser_camofox.py:camofox_soft_cleanup */
void camofox_soft_cleanup(const char *camofox_url) {
    if (!camofox_url) return;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "curl -s -X POST %s/sessions/cleanup 2>/dev/null", camofox_url);
    system(cmd);
}

/* ── Low-level HTTP helpers ─────────────────────────────────── */

/* PoP: _get_raw @ tools/browser_camofox.py:_get_raw */
char *_get_raw(const char *camofox_url, const char *path) {
    if (!camofox_url || !path) return NULL;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -s %s%s 2>/dev/null", camofox_url, path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char buf[8192] = {0};
    if (fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return strdup(buf);
    }
    pclose(fp);
    return NULL;
}

/* ── Navigation ─────────────────────────────────────────────── */

/* PoP: camofox_navigate @ tools/browser_camofox.py:camofox_navigate */
char *camofox_navigate(const char *camofox_url, const char *session_id, const char *url) {
    if (!camofox_url || !url) return NULL;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST %s/sessions/%s/navigate -d '{\"url\":\"%s\"}' 2>/dev/null",
             camofox_url, session_id, url);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char resp[512] = {0};
    if (fgets(resp, sizeof(resp), fp)) {
        pclose(fp);
        return strdup(resp);
    }
    pclose(fp);
    return NULL;
}

/* ── Input simulation ─────────────────────────────────────────── */

/* PoP: camofox_click @ tools/browser_camofox.py:camofox_click */
char *camofox_click(const char *camofox_url, const char *session_id, int x, int y, bool right_click) {
    if (!camofox_url || !session_id) return NULL;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST %s/sessions/%s/click -d '{\"x\":%d,\"y\":%d,\"button\":\"%s\"}' 2>/dev/null",
             camofox_url, session_id, x, y, right_click ? "right" : "left");
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char resp[512] = {0};
    if (fgets(resp, sizeof(resp), fp)) {
        pclose(fp);
        return strdup(resp);
    }
    pclose(fp);
    return NULL;
}

/* PoP: camofox_type @ tools/browser_camofox.py:camofox_type */
char *camofox_type(const char *camofox_url, const char *session_id, const char *text) {
    if (!camofox_url || !session_id || !text) return NULL;
    char cmd[1536];
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST %s/sessions/%s/type -d '{\"text\":\"%s\"}' 2>/dev/null",
             camofox_url, session_id, text);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char resp[512] = {0};
    if (fgets(resp, sizeof(resp), fp)) {
        pclose(fp);
        return strdup(resp);
    }
    pclose(fp);
    return NULL;
}

/* PoP: camofox_scroll @ tools/browser_camofox.py:camofox_scroll */
char *camofox_scroll(const char *camofox_url, const char *session_id, int dx, int dy) {
    if (!camofox_url || !session_id) return NULL;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST %s/sessions/%s/scroll -d '{\"dx\":%d,\"dy\":%d}' 2>/dev/null",
             camofox_url, session_id, dx, dy);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char resp[512] = {0};
    if (fgets(resp, sizeof(resp), fp)) {
        pclose(fp);
        return strdup(resp);
    }
    pclose(fp);
    return NULL;
}

/* ── Navigation controls ─────────────────────────────────────── */

/* PoP: camofox_back @ tools/browser_camofox.py:camofox_back */
char *camofox_back(const char *camofox_url, const char *session_id) {
    if (!camofox_url || !session_id) return NULL;
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST %s/sessions/%s/back 2>/dev/null", camofox_url, session_id);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char resp[256] = {0};
    if (fgets(resp, sizeof(resp), fp)) {
        pclose(fp);
        return strdup(resp);
    }
    pclose(fp);
    return NULL;
}

/* PoP: camofox_press @ tools/browser_camofox.py:camofox_press */
char *camofox_press(const char *camofox_url, const char *session_id, const char *key) {
    if (!camofox_url || !session_id || !key) return NULL;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST %s/sessions/%s/press -d '{\"key\":\"%s\"}' 2>/dev/null",
             camofox_url, session_id, key);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char resp[256] = {0};
    if (fgets(resp, sizeof(resp), fp)) {
        pclose(fp);
        return strdup(resp);
    }
    pclose(fp);
    return NULL;
}

/* ── Session control ─────────────────────────────────────────── */

/* PoP: camofox_close @ tools/browser_camofox.py:camofox_close */
char *camofox_close(const char *camofox_url, const char *session_id) {
    if (!camofox_url || !session_id) return NULL;
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST %s/sessions/%s/close 2>/dev/null", camofox_url, session_id);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char resp[256] = {0};
    if (fgets(resp, sizeof(resp), fp)) {
        pclose(fp);
        return strdup(resp);
    }
    pclose(fp);
    return NULL;
}

/* ── Snapshot & media ─────────────────────────────────────────── */

/* PoP: camofox_snapshot @ tools/browser_camofox.py:camofox_snapshot */
char *camofox_snapshot(const char *camofox_url, const char *session_id) {
    if (!camofox_url || !session_id) return NULL;
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "curl -s %s/sessions/%s/screenshot 2>/dev/null", camofox_url, session_id);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char resp[8192] = {0};
    if (fgets(resp, sizeof(resp), fp)) {
        pclose(fp);
        return strdup(resp);
    }
    pclose(fp);
    return NULL;
}

/* PoP: camofox_get_images @ tools/browser_camofox.py:camofox_get_images */
json_t *camofox_get_images(const char *camofox_url, const char *session_id) {
    if (!camofox_url || !session_id) return NULL;
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "curl -s %s/sessions/%s/images 2>/dev/null", camofox_url, session_id);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char resp[4096] = {0};
    if (fgets(resp, sizeof(resp), fp)) {
        pclose(fp);
        char *err = NULL;
        json_t *j = json_parse(resp, &err);
        if (err) { free(err); return NULL; }
        return j ? j : json_array();
    }
    pclose(fp);
    return json_array();
}

/* ── Vision ───────────────────────────────────────────────────── */

/* PoP: camofox_vision @ tools/browser_camofox.py:camofox_vision */
json_t *camofox_vision(const char *camofox_url, const char *session_id, const char *prompt) {
    if (!camofox_url || !session_id) return NULL;
    char cmd[1536];
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST %s/sessions/%s/vision -d '{\"prompt\":\"%s\"}' 2>/dev/null",
             camofox_url, session_id, prompt ? prompt : "");
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char resp[4096] = {0};
    if (fgets(resp, sizeof(resp), fp)) {
        pclose(fp);
        char *err = NULL;
        json_t *j = json_parse(resp, &err);
        if (err) { free(err); return NULL; }
        return j ? j : json_object();
    }
    pclose(fp);
    return json_object();
}

/* ── Console access ─────────────────────────────────────────── */

/* PoP: camofox_console @ tools/browser_camofox.py:camofox_console */
char *camofox_console(const char *camofox_url, const char *session_id, const char *cmd) {
    if (!camofox_url || !session_id) return NULL;
    char http_cmd[1536];
    snprintf(http_cmd, sizeof(http_cmd),
             "curl -s -X POST %s/sessions/%s/evaluate -d '{\"code\":\"%s\"}' 2>/dev/null",
             camofox_url, session_id, cmd ? cmd : "");
    FILE *fp = popen(http_cmd, "r");
    if (!fp) return NULL;
    char resp[4096] = {0};
    if (fgets(resp, sizeof(resp), fp)) {
        pclose(fp);
        return strdup(resp);
    }
    pclose(fp);
    return NULL;
}

/* ── Private page block helper ───────────────────────────────── */

/* PoP: _camofox_private_page_block @ tools/browser_camofox.py:_camofox_private_page_block */
bool _camofox_private_page_block(const char *url) {
    if (!url) return false;
    /* Block known tracking domains and private IP ranges */
    const char *blocked[] = {
        "google-analytics.com", "analytics.google.com", "facebook.com/tr",
        "doubleclick.net", "admantX.com", "taboola.com", "outbrain.com",
        "crwdcoin.com", "scorecardresearch.com", "quantserve.com",
        "snapchat.com", "tiktok.com", "pinterest.com", NULL
    };
    for (int i = 0; blocked[i]; i++) {
        if (strstr(url, blocked[i])) return true;
    }
    return false;
}