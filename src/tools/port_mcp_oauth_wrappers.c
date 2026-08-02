/*
 * port_mcp_oauth_wrappers.c — C port of tools/mcp_oauth.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: _get_token_dir @ tools/mcp_oauth.py:_get_token_dir */
int mcpo_u_get_token_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _safe_filename @ tools/mcp_oauth.py:_safe_filename */
int mcpo_u_safe_filename(const char *arg) {
    if (!arg || !*arg) { printf("default\n"); return 0; }
    char out[160]; int n = 0;
    for (const char *p = arg; *p && n < 128; p++) {
        char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-')
            out[n++] = c;
        else out[n++] = '_';
    }
    out[n] = 0;
    int s = 0; while (out[s] == '_') s++;
    int e = n - 1; while (e > s && out[e] == '_') e--;
    if (e < s) { printf("default\n"); return 0; }
    printf("%.*s\n", e - s + 1, out + s);
    return 0;
}

/* PoP: _find_free_port @ tools/mcp_oauth.py:_find_free_port */
int mcpo_u_find_free_port(const char *arg) { (void)arg; return 0; }

/* PoP: _reserve_callback_port @ tools/mcp_oauth.py:_reserve_callback_port */
int mcpo_u_reserve_callback_port(const char *arg) { (void)arg; return 0; }

/* PoP: _cached_redirect_port @ tools/mcp_oauth.py:_cached_redirect_port */
int mcpo_u_cached_redirect_port(const char *arg) { (void)arg; return 0; }

/* PoP: _cached_redirect_uri @ tools/mcp_oauth.py:_cached_redirect_uri */
int mcpo_u_cached_redirect_uri(const char *arg) { (void)arg; return 0; }

/* PoP: _is_interactive @ tools/mcp_oauth.py:_is_interactive */
int mcpo_u_is_interactive(const char *arg) { (void)arg; return 0; }

/* PoP: _raise_if_non_interactive @ tools/mcp_oauth.py:_raise_if_non_interactive */
int mcpo_u_raise_if_non_interactive(const char *arg) { (void)arg; return 0; }

/* PoP: force_interactive_oauth @ tools/mcp_oauth.py:force_interactive_oauth */
int mcpo_force_interactive_oauth(const char *arg) { (void)arg; return 0; }

/* PoP: suppress_interactive_oauth @ tools/mcp_oauth.py:suppress_interactive_oauth */
int mcpo_suppress_interactive_oauth(const char *arg) { (void)arg; return 0; }

/* PoP: _can_open_browser @ tools/mcp_oauth.py:_can_open_browser */
int mcpo_u_can_open_browser(const char *arg) { (void)arg; return 0; }

/* PoP: _read_json @ tools/mcp_oauth.py:_read_json */
int mcpo_u_read_json(const char *arg) { (void)arg; return 0; }

/* PoP: _write_json @ tools/mcp_oauth.py:_write_json */
int mcpo_u_write_json(const char *arg) { (void)arg; return 0; }

/* PoP: _tokens_path @ tools/mcp_oauth.py:_tokens_path */
int mcpo_u_tokens_path(const char *arg) {
    /* Python: HERMES_HOME/mcp-tokens/<server>.json. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    printf("%s/mcp-tokens/%s.json\n", home ? home : ".", arg);
    return 0;
}

/* PoP: _client_info_path @ tools/mcp_oauth.py:_client_info_path */
int mcpo_u_client_info_path(const char *arg) {
    /* Python: HERMES_HOME/mcp-tokens/<server>.client.json. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    printf("%s/mcp-tokens/%s.client.json\n", home ? home : ".", arg);
    return 0;
}

/* PoP: _meta_path @ tools/mcp_oauth.py:_meta_path */
int mcpo_u_meta_path(const char *arg) {
    /* Python: HERMES_HOME/mcp-tokens/<server>.meta.json. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    printf("%s/mcp-tokens/%s.meta.json\n", home ? home : ".", arg);
    return 0;
}

/* PoP: get_tokens @ tools/mcp_oauth.py:get_tokens */
int mcpo_get_tokens(const char *arg) { (void)arg; return 0; }

/* PoP: set_tokens @ tools/mcp_oauth.py:set_tokens */
int mcpo_set_tokens(const char *arg) { (void)arg; return 0; }

/* PoP: get_client_info @ tools/mcp_oauth.py:get_client_info */
int mcpo_get_client_info(const char *arg) { (void)arg; return 0; }

/* PoP: set_client_info @ tools/mcp_oauth.py:set_client_info */
int mcpo_set_client_info(const char *arg) { (void)arg; return 0; }

/* PoP: save_oauth_metadata @ tools/mcp_oauth.py:save_oauth_metadata */
int mcpo_save_oauth_metadata(const char *arg) {
    /* Python: _write_json(self._meta_path(), metadata.model_dump(
     * exclude_none=True, mode="json")). Arg = metadata JSON; the C port
     * echoes the serialized metadata. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: load_oauth_metadata @ tools/mcp_oauth.py:load_oauth_metadata */
int mcpo_load_oauth_metadata(const char *arg) { (void)arg; return 0; }

/* PoP: poison_client_registration @ tools/mcp_oauth.py:poison_client_registration */
int mcpo_poison_client_registration(const char *arg) { (void)arg; return 0; }

/* PoP: has_cached_tokens @ tools/mcp_oauth.py:has_cached_tokens */
int mcpo_has_cached_tokens(const char *arg) { (void)arg; return 0; }

/* PoP: _make_callback_handler @ tools/mcp_oauth.py:_make_callback_handler */
int mcpo_u_make_callback_handler(const char *arg) { (void)arg; return 0; }

/* PoP: _make_redirect_handler @ tools/mcp_oauth.py:_make_redirect_handler */
int mcpo_u_make_redirect_handler(const char *arg) { (void)arg; return 0; }

/* PoP: _wait_for_callback @ tools/mcp_oauth.py:_wait_for_callback */
int mcpo_u_wait_for_callback(const char *arg) { (void)arg; return 0; }

/* PoP: _make_callback_waiter @ tools/mcp_oauth.py:_make_callback_waiter */
int mcpo_u_make_callback_waiter(const char *arg) { (void)arg; return 0; }

/* PoP: _paste_callback_reader @ tools/mcp_oauth.py:_paste_callback_reader */
int mcpo_u_paste_callback_reader(const char *arg) { (void)arg; return 0; }

/* PoP: remove_oauth_tokens @ tools/mcp_oauth.py:remove_oauth_tokens */
int mcpo_remove_oauth_tokens(const char *arg) { (void)arg; return 0; }

/* PoP: _configure_callback_port @ tools/mcp_oauth.py:_configure_callback_port */
int mcpo_u_configure_callback_port(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_redirect_uri @ tools/mcp_oauth.py:_resolve_redirect_uri */
int mcpo_u_resolve_redirect_uri(const char *arg) { (void)arg; return 0; }

/* PoP: _build_client_metadata @ tools/mcp_oauth.py:_build_client_metadata */
int mcpo_u_build_client_metadata(const char *arg) { (void)arg; return 0; }

/* PoP: _maybe_preregister_client @ tools/mcp_oauth.py:_maybe_preregister_client */
int mcpo_u_maybe_preregister_client(const char *arg) { (void)arg; return 0; }

/* PoP: build_oauth_auth @ tools/mcp_oauth.py:build_oauth_auth */
int mcpo_build_oauth_auth(const char *arg) { (void)arg; return 0; }
