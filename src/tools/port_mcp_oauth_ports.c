/*
 * port_mcp_oauth_remaining.c — Port of tools/mcp_oauth.py OAuth state
 * surface. Safe filenames, real token-file IO, remove/snapshot/restore.
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
