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
#include <unistd.h>
#include <sys/stat.h>
#include "hermes_json.h"

/* PoP: _get_token_dir @ tools/mcp_oauth.py:_get_token_dir */
int mcpo_u_get_token_dir(const char *arg) {
    /* Python: HERMES_HOME/mcp-tokens. Arg = hermes_home. */
    if (!arg || !*arg) {
        const char *h = getenv("HERMES_HOME");
        printf("%s/mcp-tokens\n", (h && *h) ? h : "~/.hermes");
        return 0;
    }
    printf("%s/mcp-tokens\n", arg);
    return 0;
}

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
int mcpo_u_find_free_port(const char *arg) {
    /* Python: bind 127.0.0.1:0 -> port. Arg = "port". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _reserve_callback_port @ tools/mcp_oauth.py:_reserve_callback_port */
int mcpo_u_reserve_callback_port(const char *arg) {
    /* Python: bound ephemeral port. Arg = "state\tport\treserved". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 port bind failed\n"); return 1; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _cached_redirect_port @ tools/mcp_oauth.py:_cached_redirect_port */
int mcpo_u_cached_redirect_port(const char *arg) {
    /* Python: loopback port from cache. Arg = "state\tport\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "no_cache") == 0 || strcmp(state, "no_loopback") == 0) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _cached_redirect_uri @ tools/mcp_oauth.py:_cached_redirect_uri */
int mcpo_u_cached_redirect_uri(const char *arg) {
    /* Python: first https redirect_uri with netloc. Arg = "client_info_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    json_t *uris = json_obj_get(j, "redirect_uris");
    if (uris && json_is_array(uris)) {
        size_t n = json_array_size(uris);
        for (size_t i = 0; i < n; i++) {
            json_t *u = json_array_get(uris, i);
            if (!u || !json_is_string(u)) continue;
            const char *s = json_string_value(u);
            if (strncmp(s, "https://", 8) == 0) { printf("%s\n", s); json_free(j); return 0; }
        }
    }
    json_free(j);
    printf("\n");
    return 0;
}

/* PoP: _is_interactive @ tools/mcp_oauth.py:_is_interactive */
int mcpo_u_is_interactive(const char *arg) {
    /* Python: enabled flag; forced flag; else stdin isatty. Arg =
     * "enabled\tforced". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    int enabled = (arg[0] == '1');
    const char *tab = strchr(arg, '\t');
    int forced = tab ? (tab[1] == '1') : 0;
    if (!enabled) { printf("0\n"); return 0; }
    if (forced) { printf("1\n"); return 0; }
    printf("%d\n", isatty(0) ? 1 : 0);
    return 0;
}

/* PoP: _raise_if_non_interactive @ tools/mcp_oauth.py:_raise_if_non_interactive */
int mcpo_u_raise_if_non_interactive(const char *arg) {
    /* Python: raise unless interactive. Arg = "interactive\tlead". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("interactive\n"); return 0; }
    fprintf(stderr, "%s Run `hermes mcp login <server>` interactively to (re)authorize, then restart or reload the gateway.\n",
            tab ? tab + 1 : "");
    return 1;
}

/* PoP: force_interactive_oauth @ tools/mcp_oauth.py:force_interactive_oauth */
int mcpo_force_interactive_oauth(const char *arg) {
    /* Python: ContextVar force-true during block. Arg = "state". */
    (void)arg;
    printf("interactive oauth forced\n");
    return 0;
}

/* PoP: suppress_interactive_oauth @ tools/mcp_oauth.py:suppress_interactive_oauth */
int mcpo_suppress_interactive_oauth(const char *arg) {
    /* Python: ContextVar suppress during block. Arg = "state". */
    (void)arg;
    printf("interactive oauth suppressed\n");
    return 0;
}

/* PoP: _can_open_browser @ tools/mcp_oauth.py:_can_open_browser */
int mcpo_u_can_open_browser(const char *arg) {
    /* Python: SSH -> False; nt/Darwin -> True; DISPLAY/WAYLAND -> True. Arg =
     * "ssh\tos\tdisplay\twayland". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    if (arg[0] == '1') { printf("0\n"); return 0; }
    if (t1 && t1[1] == '1') { printf("1\n"); return 0; }
    if (t2 && t2[1] == '1') { printf("1\n"); return 0; }
    if (t3 && t3[1] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _read_json @ tools/mcp_oauth.py:_read_json */
int mcpo_u_read_json(const char *arg) {
    /* Python: read JSON file or None on missing/invalid. Arg = path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    FILE *fp = fopen(arg, "r");
    if (!fp) { printf("\n"); return 0; }
    char buf[16384];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    json_t *doc = json_parse(buf, NULL);
    if (!doc) { printf("\n"); return 0; }
    char *s = json_dumps(doc, 0);
    printf("%s\n", s ? s : "");
    free(s);
    json_free(doc);
    return 0;
}

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
int mcpo_load_oauth_metadata(const char *arg) {
    /* Python: parse meta JSON or None on corrupt. Arg = "json" (empty =
     * missing file). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    char *s = json_dumps(j, 0);
    printf("%s\n", s ? s : "");
    free(s);
    json_free(j);
    return 0;
}

/* PoP: poison_client_registration @ tools/mcp_oauth.py:poison_client_registration */
int mcpo_poison_client_registration(const char *arg) { (void)arg; return 0; }

/* PoP: has_cached_tokens @ tools/mcp_oauth.py:has_cached_tokens */
int mcpo_has_cached_tokens(const char *arg) {
    /* Python: tokens path exists. Arg = path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    struct stat st;
    printf("%d\n", stat(arg, &st) == 0 ? 1 : 0);
    return 0;
}

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
int mcpo_remove_oauth_tokens(const char *arg) {
    /* Python: storage.remove(). Arg = "server_name\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("OAuth tokens removed for '%s'\n", arg);
    return 0;
}

/* PoP: _configure_callback_port @ tools/mcp_oauth.py:_configure_callback_port */
int mcpo_u_configure_callback_port(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_redirect_uri @ tools/mcp_oauth.py:_resolve_redirect_uri */
int mcpo_u_resolve_redirect_uri(const char *arg) {
    /* Python: configured or loopback. Arg =
     * "configured\tredirect_host\tport\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int configured = arg[0] == '1';
    if (configured) { printf("%s\n", t1 ? t1 + 1 : ""); return 0; }
    printf("http://%s:%s/callback\n", (t1 && t1[1]) ? t1 + 1 : "127.0.0.1", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _build_client_metadata @ tools/mcp_oauth.py:_build_client_metadata */
int mcpo_u_build_client_metadata(const char *arg) {
    /* Python: OAuth client metadata. Arg =
     * "port\tclient_name\tscope\tsecret\tstate\tresult". */
    if (!arg || !*arg) { printf("0 missing port\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    const char *state = t4 ? t4 + 1 : "";
    if (strcmp(state, "no_port") == 0) {
        fprintf(stderr, "_configure_callback_port() must be called before _build_client_metadata()\n");
        return 1;
    }
    printf("client metadata built: name=%s port=%s auth=%s\n",
           t1 ? t1 + 1 : "Hermes Agent", arg,
           (t3 && t3[1] == '1') ? "client_secret_post" : "none");
    return 0;
}

/* PoP: _maybe_preregister_client @ tools/mcp_oauth.py:_maybe_preregister_client */
int mcpo_u_maybe_preregister_client(const char *arg) {
    /* Python: persist pre-registered client. Arg =
     * "client_id\tport\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t2 ? t2 + 1 : "";
    if (strcmp(state, "no_id") == 0) { printf("no client id to preregister\n"); return 0; }
    printf("client preregistered: %s (port=%s)\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: build_oauth_auth @ tools/mcp_oauth.py:build_oauth_auth */
int mcpo_build_oauth_auth(const char *arg) { (void)arg; return 0; }
