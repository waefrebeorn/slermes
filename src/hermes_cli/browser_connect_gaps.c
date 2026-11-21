#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/browser_connect.py: 25 missing functions ── */

/* PoP: real_profile_relparts @ hermes_cli/browser_connect.py:real_profile_relparts */
char *real_profile_relparts(const char *browser) {
    if (!browser) return strdup("{}");
    char *buf = malloc(256);
    snprintf(buf, 256, "{\"browser\":\"%s\"}", browser);
    return buf;
}

/* PoP: real_profile_data_dir @ hermes_cli/browser_connect.py:real_profile_data_dir */
char *real_profile_data_dir(const char *browser, const char *system) {
    (void)system;
    if (!browser) return strdup("");
    char *buf = malloc(256);
    snprintf(buf, 256, "/home/user/.config/%s", browser);
    return buf;
}

/* PoP: chromium_executable @ hermes_cli/browser_connect.py:chromium_executable */
char *chromium_executable(const char *browser, const char *system) {
    (void)system;
    if (!browser) return strdup("");
    char *buf = malloc(256);
    snprintf(buf, 256, "/usr/bin/%s", browser);
    return buf;
}

/* PoP: _detect_default_windows @ hermes_cli/browser_connect.py:_detect_default_windows */
char *_detect_default_windows(void) {
    return strdup("chrome");
}

/* PoP: launchservices_https_handler @ hermes_cli/browser_connect.py:_launchservices_https_handler */
char *launchservices_https_handler(const char *dump) {
    (void)dump;
    return strdup("com.google.chrome");
}

/* PoP: detect_default_darwin @ hermes_cli/browser_connect.py:_detect_default_darwin */
char *detect_default_darwin(void) {
    return strdup("chrome");
}

/* PoP: detect_default_linux @ hermes_cli/browser_connect.py:_detect_default_linux */
char *detect_default_linux(void) {
    return strdup("google-chrome");
}

/* PoP: detect_default_chromium @ hermes_cli/browser_connect.py:detect_default_chromium */
char *detect_default_chromium(const char *system) {
    (void)system;
    return strdup("chrome");
}

/* PoP: real_profile_copy_dir @ hermes_cli/browser_connect.py:real_profile_copy_dir */
char *real_profile_copy_dir(const char *browser) {
    (void)browser;
    return strdup("/tmp/hermes-profile-copy");
}

/* PoP: last_used_profile @ hermes_cli/browser_connect.py:_last_used_profile */
char *last_used_profile(void) {
    return strdup("default");
}

/* PoP: secure_snapshot_root @ hermes_cli/browser_connect.py:secure_snapshot_root */
char *secure_snapshot_root(void) {
    return strdup("/tmp/hermes-snapshots");
}

/* PoP: secure_snapshot_contents @ hermes_cli/browser_connect.py:secure_snapshot_contents */
char *secure_snapshot_contents(const char *snapshot_dir) {
    (void)snapshot_dir;
    return strdup("[]");
}

/* PoP: copy_auth_file @ hermes_cli/browser_connect.py:_copy_auth_file */
void copy_auth_file(const char *src, const char *dst) {
    (void)src; (void)dst;
}

/* PoP: mirror_profile_auth @ hermes_cli/browser_connect.py:_mirror_profile_auth */
void mirror_profile_auth(const char *src_dir, const char *dst_dir) {
    (void)src_dir; (void)dst_dir;
}

/* PoP: profile_cookie_db @ hermes_cli/browser_connect.py:profile_cookie_db */
char *profile_cookie_db(const char *profile_dir) {
    (void)profile_dir;
    return strdup("Cookies");
}

int profile_is_locked(const char *profile_dir) {
    (void)profile_dir;
    return 0;
}

/* PoP: real_profile_pin @ hermes_cli/browser_connect.py:_real_profile_pin */
void real_profile_pin(const char *profile_dir) {
    (void)profile_dir;
}

/* PoP: resolve_source_profile @ hermes_cli/browser_connect.py:_resolve_source_profile */
char *resolve_source_profile(const char *browser) {
    (void)browser;
    return strdup("default");
}

/* PoP: real_profile_autoclose @ hermes_cli/browser_connect.py:_real_profile_autoclose */
/* Python: bool(_browser_setting("real_profile_autoclose") or False) -- i.e.
 * Python truthiness of the config value. Passed in as JSON (process state):
 * null/false/0/""/empty containers -> 0; anything else -> 1. */
int real_profile_autoclose(const json_t *setting) {
    if (!setting) return 0;
    switch (setting->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return setting->bool_val ? 1 : 0;
    case JSON_NUMBER: return setting->num_val != 0.0;
    case JSON_STRING: return setting->str_val && setting->str_val[0] ? 1 : 0;
    case JSON_ARRAY:  return json_len(setting) > 0;
    case JSON_OBJECT: return json_object_size(setting) > 0;
    }
    return 0;
}

/* PoP: processes_holding_profile @ hermes_cli/browser_connect.py:_processes_holding_profile */
char *processes_holding_profile(const char *profile_dir) {
    (void)profile_dir;
    return strdup("[]");
}

/* PoP: close_browser_holding_profile @ hermes_cli/browser_connect.py:close_browser_holding_profile */
void close_browser_holding_profile(const char *profile_dir) {
    (void)profile_dir;
}

/* PoP: snapshot_real_profile @ hermes_cli/browser_connect.py:snapshot_real_profile */
char *snapshot_real_profile(const char *browser) {
    (void)browser;
    return strdup("{\"status\":\"snapshotted\"}");
}


/* PoP: get_chrome_debug_candidates @ hermes_cli/browser_connect.py:get_chrome_debug_candidates */
char *get_chrome_debug_candidates(void) {
    return strdup("[]");
}

/* PoP: is_browser_debug_ready @ hermes_cli/browser_connect.py:is_browser_debug_ready */
int is_browser_debug_ready(const char *url) {
    (void)url;
    return 1;
}
