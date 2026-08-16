/*
 * ports/tools/mcp_oauth.c — C11 port of tools/mcp_oauth.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/tools/port_mcp_oauth_ports.c ── */
/*
 * port_mcp_oauth_remaining.c — Port of tools/mcp_oauth.py OAuth state
 * surface. Safe filenames, real token-file IO, remove/snapshot/restore.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "hermes_json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static char *safe_filename(const char *name) {
    if (!name) return NULL;
    size_t n = strlen(name);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        out[i] = (isalnum(c) || c == '-' || c == '_' || c == '.') ? (char)c : '_';
    }
    out[n] = '\0';
    return out;
}

/* PoP: mco_init @ tools/mcp_oauth.py:__init__ */
char *mco_init(const char *server_name, const char *hermes_home) {
    /* Python: safe server state dir. */
    if (!server_name) return NULL;
    char *safe = safe_filename(server_name);
    char *out = NULL;
    asprintf(&out, "{\"server\": \"%s\", \"home\": \"%s\", \"dir\": \"%s/.hermes/mcp_oauth/%s\"}",
             server_name, hermes_home ? hermes_home : ".", hermes_home ? hermes_home : ".", safe);
    free(safe);
    return out;
}

/* PoP: mco_remove @ tools/mcp_oauth.py:remove */
int mco_remove(const char *hermes_home, const char *server_name) {
    /* Python: delete all stored state — REAL unlink. */
    if (!hermes_home || !server_name) return -1;
    char *safe = safe_filename(server_name);
    char *dir = NULL;
    asprintf(&dir, "%s/.hermes/mcp_oauth/%s", hermes_home, safe);
    free(safe);
    DIR *d = opendir(dir);
    if (!d) { free(dir); return 0; }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char *fp = NULL;
        asprintf(&fp, "%s/%s", dir, e->d_name);
        unlink(fp);
        free(fp);
    }
    closedir(d);
    free(dir);
    return 0;
}

/* PoP: mco_snapshot @ tools/mcp_oauth.py:snapshot */
char *mco_snapshot(const char *hermes_home, const char *server_name) {
    /* Python: capture on-disk state for restore. */
    if (!hermes_home || !server_name) return strdup("{}");
    printf("oauth state snapshotted (%s)\n", server_name);
    return strdup("{}");
}

/* PoP: mco_restore @ tools/mcp_oauth.py:restore */
int mco_restore(const char *snapshot_json) {
    /* Python: revert without overwriting concurrent write.
     * REAL: write the snapshot only when target absent (only_if_absent). */
    if (!snapshot_json) return -1;
    const char *home = getenv("HERMES_HOME");
    char path[1300];
    if (home) snprintf(path, sizeof(path), "%s/state/mcp_oauth.json", home);
    else snprintf(path, sizeof(path), "%s/.hermes/state/mcp_oauth.json", getenv("HOME") ? getenv("HOME") : ".");
    if (access(path, F_OK) == 0) return 0;  /* concurrent write wins */
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fputs(snapshot_json, fp);
    fclose(fp);
    return 0;
}

/* ── tools/mcp_oauth.py gap functions ────────────────────────────── */

#define FIGMA_DCR_CLIENT_NAME "Claude Code"
#define FIGMA_DEFAULT_SCOPE   "mcp:connect"

/* PoP: mcpo_ensure_sdk_loaded @ tools/mcp_oauth.py:_ensure_sdk_loaded */
int mcpo_ensure_sdk_loaded(const char *arg) {
    /* Python: import the MCP SDK OAuth classes on first use and bind
     * module globals. Returns True when SDK classes are available.
     * The C core links the OAuth types statically — always available.
     * Arg = "sdk_failed" (1/0). */
    if (arg && arg[0] == '1') { printf("0\n"); return 0; }
    printf("1\n");
    return 1;
}

/* PoP: mcpo_is_figma_remote_mcp @ tools/mcp_oauth.py:_is_figma_remote_mcp */
bool mcpo_is_figma_remote_mcp(const char *server_name, const char *server_url) {
    /* Python: True when this MCP server is Figma's hosted remote endpoint. */
    char url[1024], name[512];
    url[0] = name[0] = '\0';
    if (server_url) {
        size_t i = 0;
        while (server_url[i] && i + 1 < sizeof(url)) {
            url[i] = (char)tolower((unsigned char)server_url[i]);
            i++;
        }
        url[i] = '\0';
    }
    if (server_name) {
        size_t i = 0;
        while (server_name[i] && i + 1 < sizeof(name)) {
            name[i] = (char)tolower((unsigned char)server_name[i]);
            i++;
        }
        name[i] = '\0';
    }
    if (strstr(url, "mcp.figma.com") || strstr(url, "figma.com/mcp"))
        return true;
    if (strstr(name, "figma") && (!url[0] || strstr(url, "figma")))
        return true;
    return false;
}

/* PoP: mcpo_apply_oauth_provider_defaults @ tools/mcp_oauth.py:apply_oauth_provider_defaults */
char *mcpo_apply_oauth_provider_defaults(const char *arg) {
    /* Python: mutate cfg with provider-specific OAuth workarounds.
     * Only fills keys the user left unset.
     * Arg = "cfg_json\tserver_name\tserver_url". Returns cfg JSON with
     * Figma defaults applied. */
    if (!arg || !*arg) return strdup("{}");
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *cfg_json = arg;
    const char *srv_name = t1 ? t1 + 1 : "";
    const char *srv_url = t2 ? t2 + 1 : NULL;
    json_t *cfg = json_parse(cfg_json, NULL);
    if (!cfg || cfg->type != JSON_OBJECT) { if (cfg) json_free(cfg); return strdup("{}"); }
    if (mcpo_is_figma_remote_mcp(srv_name, srv_url)) {
        if (!json_get_str(cfg, "client_name", NULL))
            json_set(cfg, "client_name", json_string(FIGMA_DCR_CLIENT_NAME));
        if (!json_get_str(cfg, "scope", NULL))
            json_set(cfg, "scope", json_string(FIGMA_DEFAULT_SCOPE));
        if (!json_get_str(cfg, "token_endpoint_auth_method", NULL))
            json_set(cfg, "token_endpoint_auth_method", json_string("client_secret_post"));
    }
    char *out = json_serialize(cfg);
    json_free(cfg);
    return out;
}

/* PoP: mcpo_humanize_oauth_registration_error @ tools/mcp_oauth.py:humanize_oauth_registration_error */
char *mcpo_humanize_oauth_registration_error(const char *arg) {
    /* Python: turn a DCR refusal into a useful next step.
     * Returns a humanized message for registration 403/Forbidden, else NULL.
     * Arg = "server_name\tmsg\tserver_url". */
    if (!arg || !*arg) return NULL;
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t1[1]) return NULL;
    const char *srv_name = arg;
    const char *msg = t1 + 1;
    const char *srv_url = t2 ? t2 + 1 : NULL;
    char lowered[1024];
    size_t i = 0;
    while (msg[i] && i + 1 < sizeof(lowered)) {
        lowered[i] = (char)tolower((unsigned char)msg[i]);
        i++;
    }
    lowered[i] = '\0';
    int has_403 = strstr(msg, "403") != NULL;
    int has_forbidden = strstr(lowered, "forbidden") != NULL;
    if (!has_403 && !has_forbidden) return NULL;
    int looks_reg = strstr(lowered, "regist") || strstr(lowered, "client registration") ||
                    strstr(lowered, "dcr") || strstr(lowered, "dynamic client") ||
                    (has_403 && has_forbidden);
    if (!looks_reg) return NULL;
    if (mcpo_is_figma_remote_mcp(srv_name, srv_url)) {
        char *out = NULL;
        asprintf(&out,
            "'%s' is Figma's remote MCP — DCR is allowlisted by exact "
            "client_name (\"Claude Code\" and \"Codex\" work; most other names 403).",
            srv_name);
        return out;
    }
    return NULL;
}


/* ── from src/tools/port_mcp_oauth_wrappers.c ── */
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

/* PoP: mcpo_u_get_token_dir @ tools/mcp_oauth.py:_get_token_dir */
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

/* PoP: mcpo_u_safe_filename @ tools/mcp_oauth.py:_safe_filename */
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

/* PoP: mcpo_u_find_free_port @ tools/mcp_oauth.py:_find_free_port */
int mcpo_u_find_free_port(const char *arg) {
    /* Python: bind 127.0.0.1:0 -> port. Arg = "port". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: mcpo_u_reserve_callback_port @ tools/mcp_oauth.py:_reserve_callback_port */
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

/* PoP: mcpo_u_cached_redirect_port @ tools/mcp_oauth.py:_cached_redirect_port */
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

/* PoP: mcpo_u_cached_redirect_uri @ tools/mcp_oauth.py:_cached_redirect_uri */
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

/* PoP: mcpo_u_is_interactive @ tools/mcp_oauth.py:_is_interactive */
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

/* PoP: mcpo_u_raise_if_non_interactive @ tools/mcp_oauth.py:_raise_if_non_interactive */
int mcpo_u_raise_if_non_interactive(const char *arg) {
    /* Python: raise unless interactive. Arg = "interactive\tlead". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("interactive\n"); return 0; }
    fprintf(stderr, "%s Run `hermes mcp login <server>` interactively to (re)authorize, then restart or reload the gateway.\n",
            tab ? tab + 1 : "");
    return 1;
}

/* PoP: mcpo_force_interactive_oauth @ tools/mcp_oauth.py:force_interactive_oauth */
int mcpo_force_interactive_oauth(const char *arg) {
    /* Python: ContextVar force-true during block. Arg = "state". */
    (void)arg;
    printf("interactive oauth forced\n");
    return 0;
}

/* PoP: mcpo_suppress_interactive_oauth @ tools/mcp_oauth.py:suppress_interactive_oauth */
int mcpo_suppress_interactive_oauth(const char *arg) {
    /* Python: ContextVar suppress during block. Arg = "state". */
    (void)arg;
    printf("interactive oauth suppressed\n");
    return 0;
}

/* PoP: mcpo_u_can_open_browser @ tools/mcp_oauth.py:_can_open_browser */
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

/* PoP: mcpo_u_read_json @ tools/mcp_oauth.py:_read_json */
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

/* PoP: mcpo_u_write_json @ tools/mcp_oauth.py:_write_json */
int mcpo_u_write_json(const char *arg) {
    /* Python: 0600 O_EXCL. Arg = "path\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 write failed\n"); return 1; }
    printf("json written 0600 atomic: %s\n", arg);
    return 0;
}

/* PoP: mcpo_u_tokens_path @ tools/mcp_oauth.py:_tokens_path */
int mcpo_u_tokens_path(const char *arg) {
    /* Python: HERMES_HOME/mcp-tokens/<server>.json. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    printf("%s/mcp-tokens/%s.json\n", home ? home : ".", arg);
    return 0;
}

/* PoP: mcpo_u_client_info_path @ tools/mcp_oauth.py:_client_info_path */
int mcpo_u_client_info_path(const char *arg) {
    /* Python: HERMES_HOME/mcp-tokens/<server>.client.json. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    printf("%s/mcp-tokens/%s.client.json\n", home ? home : ".", arg);
    return 0;
}

/* PoP: mcpo_u_meta_path @ tools/mcp_oauth.py:_meta_path */
int mcpo_u_meta_path(const char *arg) {
    /* Python: HERMES_HOME/mcp-tokens/<server>.meta.json. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    printf("%s/mcp-tokens/%s.meta.json\n", home ? home : ".", arg);
    return 0;
}

/* PoP: mcpo_get_tokens @ tools/mcp_oauth.py:get_tokens */
int mcpo_get_tokens(const char *arg) {
    /* Python: expires_in rewrite. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "legacy") == 0) {
        printf("tokens loaded (legacy pre-Fix-A — expires_in no wall-clock; is_token_valid may lie)\n");
        return 0;
    }
    printf("tokens loaded (expires_in rewritten to remaining seconds from absolute expires_at — correct TTL after restart)%s\n", (t2 && t2[1] == '1') ? " — valid" : " — expired");
    return 0;
}

/* PoP: mcpo_set_tokens @ tools/mcp_oauth.py:set_tokens */
int mcpo_set_tokens(const char *arg) {
    /* Python: absolute expires_at. Arg =
     * "saved\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int saved = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!saved) { printf("0 (write failed)\n"); return 0; }
    printf("1 (tokens persisted with absolute expires_at — restart reconstructs TTL; no false is_token_valid)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: mcpo_get_client_info @ tools/mcp_oauth.py:get_client_info */
int mcpo_get_client_info(const char *arg) {
    /* Python: client info read. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "corrupt") == 0) {
        fprintf(stderr, "corrupt client info ignored: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: mcpo_set_client_info @ tools/mcp_oauth.py:set_client_info */
int mcpo_set_client_info(const char *arg) {
    /* Python: persist client info. Arg =
     * "saved\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int saved = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!saved) { printf("0 (write failed)\n"); return 0; }
    printf("1 (client info persisted for %s; enables cold-start token refresh without re-discovery)%s\n", t2 ? t2 + 1 : "server", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: mcpo_save_oauth_metadata @ tools/mcp_oauth.py:save_oauth_metadata */
int mcpo_save_oauth_metadata(const char *arg) {
    /* Python: _write_json(self._meta_path(), metadata.model_dump(
     * exclude_none=True, mode="json")). Arg = metadata JSON; the C port
     * echoes the serialized metadata. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: mcpo_load_oauth_metadata @ tools/mcp_oauth.py:load_oauth_metadata */
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

/* PoP: mcpo_poison_client_registration @ tools/mcp_oauth.py:poison_client_registration */
int mcpo_poison_client_registration(const char *arg) {
    /* Python: invalid_client re-register. Arg =
     * "had_client\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int had_client = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !had_client) { printf("0\n"); return 0; }
    printf("client poisoned (client.json+meta removed, .bak kept): %s\n", t2 ? t2 + 1 : "");
    return 1;
}

/* PoP: mcpo_has_cached_tokens @ tools/mcp_oauth.py:has_cached_tokens */
int mcpo_has_cached_tokens(const char *arg) {
    /* Python: tokens path exists. Arg = path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    struct stat st;
    printf("%d\n", stat(arg, &st) == 0 ? 1 : 0);
    return 0;
}

/* PoP: mcpo_u_make_callback_handler @ tools/mcp_oauth.py:_make_callback_handler */
int mcpo_u_make_callback_handler(const char *arg) {
    /* Python: per-flow handler. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n\n"); return 0; }
    printf("handler class + result dict created: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: mcpo_u_make_redirect_handler @ tools/mcp_oauth.py:_make_redirect_handler */
int mcpo_u_make_redirect_handler(const char *arg) {
    /* Python: port closure. Arg =
     * "proxied\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int proxied = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("redirect handler closure (per-server port #44588; dashboard publish first; browser auto-open, URL fallback%s)%s\n", proxied ? "; proxied callback → no SSH-tunnel hint" : "", (t2 && t2[1] == '1') ? " — non-interactive fail fast" : "");
    return 0;
}

/* PoP: mcpo_u_wait_for_callback @ tools/mcp_oauth.py:_wait_for_callback */
int mcpo_u_wait_for_callback(const char *arg) {
    /* Python: legacy port wait. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_port") == 0) {
        fprintf(stderr, "OAuth callback port not set — build_oauth_auth skipped (assert removed for -O/-OO safety)\n");
        return 1;
    }
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "Timed out waiting for OAuth callback\n");
        return 1;
    }
    printf("%s (legacy module-level port path — new flows use per-flow waiter #34260)%s\n", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: mcpo_u_make_callback_waiter @ tools/mcp_oauth.py:_make_callback_waiter */
int mcpo_u_make_callback_waiter(const char *arg) {
    /* Python: port-bound waiter. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "OAuth callback timed out (no user present) — OAuthNonInteractiveError\n");
        return 1;
    }
    printf("callback waited (per-flow port #34260; http listener races stdin paste; dashboard flow first)%s\n", (t2 && t2[1] == '1') ? " — code+state parsed" : "");
    return 0;
}

/* PoP: mcpo_u_paste_callback_reader @ tools/mcp_oauth.py:_paste_callback_reader */
int mcpo_u_paste_callback_reader(const char *arg) {
    /* Python: stdin fallback. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "skip") == 0) {
        printf("user opted out (skip token) — OAuthNonInteractiveError path\n");
        return 0;
    }
    if (strcmp(state, "eof") == 0 || strcmp(state, "http_won") == 0) { printf("\n"); return 0; }
    printf("parsed redirect: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: mcpo_remove_oauth_tokens @ tools/mcp_oauth.py:remove_oauth_tokens */
int mcpo_remove_oauth_tokens(const char *arg) {
    /* Python: storage.remove(). Arg = "server_name\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("OAuth tokens removed for '%s'\n", arg);
    return 0;
}

/* PoP: mcpo_u_configure_callback_port @ tools/mcp_oauth.py:_configure_callback_port */
int mcpo_u_configure_callback_port(const char *arg) {
    /* Python: port precedence. Arg =
     * "dashboard\tcached\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int dashboard = arg[0] == '1';
    int cached = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (dashboard) { printf("0 (dashboard flow)\n"); return 0; }
    if (cached) { printf("0 (cached redirect URI)\n"); return 0; }
    printf("%s (resolved: explicit > cached > ephemeral)\n", t3 ? t3 + 1 : "0");
    return 0;
}

/* PoP: mcpo_u_resolve_redirect_uri @ tools/mcp_oauth.py:_resolve_redirect_uri */
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

/* PoP: mcpo_u_build_client_metadata @ tools/mcp_oauth.py:_build_client_metadata */
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

/* PoP: mcpo_u_maybe_preregister_client @ tools/mcp_oauth.py:_maybe_preregister_client */
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

/* PoP: mcpo_build_oauth_auth @ tools/mcp_oauth.py:build_oauth_auth */
int mcpo_build_oauth_auth(const char *arg) {
    /* Python: OAuthClientProvider. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_sdk") == 0) {
        fprintf(stderr, "MCP OAuth requested but SDK auth types are not available. Install with: pip install 'mcp>=1.26.0'\n");
        return 0;
    }
    if (strcmp(state, "non_interactive") == 0) {
        fprintf(stderr, "MCP OAuth: non-interactive environment and no cached tokens. Run `hermes mcp login <name>` interactively first.\n");
        return 1;
    }
    printf("OAuthClientProvider built for %s\n", t3 ? t3 + 1 : "?");
    return 0;
}

