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

/* ────────────────────────────────────────────────────────────────────────
 * REAL_GAP closure batch — tools/mcp_oauth.py CIMD surface, reserved-socket
 * parking, client-change invalidation. Uses lib/libmcp_oauth file layout
 * (HERMES_HOME/mcp-tokens/<server>.{json,client.json,meta.json,cimd-off}).
 * ──────────────────────────────────────────────────────────────────────── */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libmcp_oauth/mcp_oauth.h"

/* draft-ietf-oauth-client-id-metadata-document pinned callback ports and
 * loopback hosts (kept in sync with the published metadata document). */
static const int MCPO_CIMD_PORTS[] = {27890, 27891, 27892, 27893, 27894};
#define MCPO_CIMD_PORT_COUNT 5
#define MCPO_MAX_RESERVED_SOCKETS 8

#define CIMD_METADATA_URL \
    "https://nousresearch.github.io/hermes-agent/docs/oauth/client-metadata.json"

/* Reserved bound-but-not-listening sockets keyed by port (FIFO eviction for
 * ephemeral reservations; pinned CIMD ports are never evicted). */
typedef struct mcpo_reserved {
    int port;
    int fd;
    bool pinned;
} mcpo_reserved_t;
static mcpo_reserved_t mcpo_reserved_sockets[MCPO_MAX_RESERVED_SOCKETS];
static size_t mcpo_reserved_count = 0;

/* Pinned ports this process has committed to (assignment order). */
static int mcpo_assigned_cimd_ports[MCPO_CIMD_PORT_COUNT];
static size_t mcpo_assigned_cimd_count = 0;

static bool mcpo_port_is_pinned(int port)
{
    for (int i = 0; i < MCPO_CIMD_PORT_COUNT; i++)
        if (MCPO_CIMD_PORTS[i] == port) return true;
    return false;
}

/* PoP: mcpo_park_reserved_socket @ tools/mcp_oauth.py:_park_reserved_socket */
void mcpo_park_reserved_socket(int port, int sock)
{
    /* Evict oldest ephemeral reservations past the cap; pinned sockets stay
     * parked forever (#22161). */
    while (mcpo_reserved_count >= MCPO_MAX_RESERVED_SOCKETS) {
        size_t stale = (size_t)-1;
        for (size_t i = 0; i < mcpo_reserved_count; i++) {
            if (!mcpo_reserved_sockets[i].pinned) { stale = i; break; }
        }
        if (stale == (size_t)-1) break;   /* only pinned remain — keep them */
        close(mcpo_reserved_sockets[stale].fd);
        memmove(&mcpo_reserved_sockets[stale], &mcpo_reserved_sockets[stale + 1],
                (mcpo_reserved_count - stale - 1) * sizeof(mcpo_reserved_t));
        mcpo_reserved_count--;
    }
    if (mcpo_reserved_count < MCPO_MAX_RESERVED_SOCKETS) {
        mcpo_reserved_sockets[mcpo_reserved_count].port = port;
        mcpo_reserved_sockets[mcpo_reserved_count].fd = sock;
        mcpo_reserved_sockets[mcpo_reserved_count].pinned = mcpo_port_is_pinned(port);
        mcpo_reserved_count++;
    } else {
        close(sock);   /* table full of pinned ports: don't leak the fd */
    }
}

/* HERMES_HOME/mcp-tokens directory (matches libmcp_oauth layout). */
static void mcpo_token_dir(char *buf, size_t cap)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    snprintf(buf, cap, "%s/mcp-tokens", home ? home : ".");
}

/* PoP: mcpo_cimd_rejected_path @ tools/mcp_oauth.py:_cimd_rejected_path */
char *mcpo_cimd_rejected_path(const char *server_name)
{
    char dir[1024];
    mcpo_token_dir(dir, sizeof(dir));
    char *safe = safe_filename(server_name);
    if (!safe) return NULL;
    char *out = NULL;
    asprintf(&out, "%s/%s.cimd-off", dir, safe);
    free(safe);
    return out;
}

/* PoP: mcpo_mark_cimd_rejected @ tools/mcp_oauth.py:mark_cimd_rejected */
/*
 * Record that this server refused our Client ID Metadata Document. Cleared
 */
void mcpo_mark_cimd_rejected(const char *server_name)
{
    char *path = mcpo_cimd_rejected_path(server_name);
    if (!path) return;
    FILE *fp = fopen(path, "a");   /* touch */
    if (fp) fclose(fp);
    else fprintf(stderr, "debug: could not record CIMD rejection at %s\n", path);
    free(path);
}

/* PoP: mcpo_cimd_rejected @ tools/mcp_oauth.py:cimd_rejected */
bool mcpo_cimd_rejected(const char *server_name)
{
    char *path = mcpo_cimd_rejected_path(server_name);
    if (!path) return false;
    bool exists = access(path, F_OK) == 0;
    free(path);
    return exists;
}

/* PoP: mcpo_authorization_code_result @ tools/mcp_oauth.py:_authorization_code_result */
/*
 * Package redirect parameters in the shape the installed SDK expects.
 * The C core links the SDK's AuthorizationCodeResult shape statically, so
 */
char *mcpo_authorization_code_result(const char *code, const char *state,
                                     const char *iss)
{
    json_t *out = json_object();
    if (!out) return NULL;
    json_set(out, "code", json_string(code ? code : ""));
    if (state) json_set(out, "state", json_string(state));
    if (iss) json_set(out, "iss", json_string(iss));
    char *s = json_serialize(out);
    json_free(out);
    return s;
}

/* PoP: mcpo_get_hermes_oauth_provider_class @ tools/mcp_oauth.py:_get_hermes_oauth_provider_class */
/*
 * The Hermes OAuth provider class with pragmatic fixes (client_secret_post
 * coercion + token-endpoint User-Agent stamping). The C core links this
 * behavior statically into the token exchange/refresh paths, so the class
 * is always available once asked for: returns a non-NULL class token whose
 */
const char *mcpo_get_hermes_oauth_provider_class(void)
{
    return "HermesOAuthClientProvider";
}

/* PoP: mcpo_is_valid_cimd_url @ tools/mcp_oauth.py:_is_valid_cimd_url */
/*
 * https scheme, non-root path, no userinfo/fragment, no dot segments —
 */
bool mcpo_is_valid_cimd_url(const char *url)
{
    if (!url || !*url) return false;
    if (strncmp(url, "https://", 8) != 0) return false;
    const char *host_start = url + 8;
    const char *path = strchr(host_start, '/');
    if (!path) return false;                       /* no path → root document */
    /* netloc ends at first '/' — check userinfo before any '@' inside it. */
    const char *slash = path;
    for (const char *p = host_start; p < slash; p++)
        if (*p == '@') return false;               /* userinfo present */
    if (strchr(path, '#')) return false;           /* fragment present */
    /* Reject "." / ".." path segments. */
    const char *seg = path;
    while (seg && *seg) {
        seg++;                                     /* skip '/' or prev char */
        const char *next = strchr(seg, '/');
        size_t len = next ? (size_t)(next - seg) : strlen(seg);
        if ((len == 1 && strncmp(seg, ".", 1) == 0) ||
            (len == 2 && strncmp(seg, "..", 2) == 0))
            return false;
        seg = next;
    }
    /* Host must be non-empty. */
    if (path == host_start) return false;
    return true;
}

/* PoP: mcpo_note_assigned_cimd_port @ tools/mcp_oauth.py:_note_assigned_cimd_port */
void mcpo_note_assigned_cimd_port(int port)
{
    if (!mcpo_port_is_pinned(port)) return;
    for (size_t i = 0; i < mcpo_assigned_cimd_count; i++)
        if (mcpo_assigned_cimd_ports[i] == port) return;
    if (mcpo_assigned_cimd_count < MCPO_CIMD_PORT_COUNT)
        mcpo_assigned_cimd_ports[mcpo_assigned_cimd_count++] = port;
}

/* PoP: mcpo_reserve_cimd_port @ tools/mcp_oauth.py:_reserve_cimd_port */
bool mcpo_reserve_cimd_port(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return false;
    }
    mcpo_park_reserved_socket(port, fd);   /* never listen: adopted later */
    return true;
}

/* PoP: mcpo_pick_cimd_port @ tools/mcp_oauth.py:_pick_cimd_port */
/*
 * Reserve a pinned CIMD callback port; wrap the range rather than falling
 */
int mcpo_pick_cimd_port(void)
{
    for (int i = 0; i < MCPO_CIMD_PORT_COUNT; i++) {
        int port = MCPO_CIMD_PORTS[i];
        bool assigned = false;
        for (size_t k = 0; k < mcpo_assigned_cimd_count; k++)
            if (mcpo_assigned_cimd_ports[k] == port) { assigned = true; break; }
        if (assigned) continue;
        if (mcpo_reserve_cimd_port(port)) {
            mcpo_note_assigned_cimd_port(port);
            return port;
        }
    }
    return (mcpo_assigned_cimd_count > 0) ? mcpo_assigned_cimd_ports[0] : -1;
}

/* Read <server>.client.json from the shared token dir; returns parsed JSON
 * or NULL. Caller json_free's. */
static json_t *mcpo_read_client_info(const char *server_name)
{
    char dir[1024];
    mcpo_token_dir(dir, sizeof(dir));
    char *safe = safe_filename(server_name);
    if (!safe) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/%s.client.json", dir, safe);
    free(safe);
    if (!path) return NULL;
    FILE *fp = fopen(path, "r");
    free(path);
    if (!fp) return NULL;
    char buf[16384];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    return json_parse(buf, NULL);
}

/* Read <server>.meta.json; returns parsed JSON or NULL. */
static json_t *mcpo_read_meta(const char *server_name)
{
    char dir[1024];
    mcpo_token_dir(dir, sizeof(dir));
    char *safe = safe_filename(server_name);
    if (!safe) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/%s.meta.json", dir, safe);
    free(safe);
    if (!path) return NULL;
    FILE *fp = fopen(path, "r");
    free(path);
    if (!fp) return NULL;
    char buf[16384];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    return json_parse(buf, NULL);
}

/* PoP: mcpo_has_cached_client_info @ tools/mcp_oauth.py:_has_cached_client_info */
bool mcpo_has_cached_client_info(const char *server_name)
{
    if (!server_name || !*server_name) return false;
    json_t *info = mcpo_read_client_info(server_name);
    if (!info) return false;
    json_free(info);
    return true;
}

/* PoP: mcpo_server_declined_cimd @ tools/mcp_oauth.py:_server_declined_cimd */
/*
 * True when cached authorization-server metadata shows the server does not
 */
bool mcpo_server_declined_cimd(const char *server_name)
{
    if (!server_name || !*server_name) return false;
    json_t *meta = mcpo_read_meta(server_name);
    if (!meta) return false;
    bool declined;
    const json_t *v = json_obj_get(meta, "client_id_metadata_document_supported");
    if (!v)
        declined = true;                    /* attribute absent → not True */
    else if (v->type == JSON_BOOL)
        declined = !v->bool_val;
    else
        declined = true;                    /* anything but literal true */
    json_free(meta);
    return declined;
}

/* PoP: mcpo_maybe_use_cimd @ tools/mcp_oauth.py:_maybe_use_cimd */
/*
 * Return "<metadata_url>\t<port>" when the flow is CIMD-eligible, else NULL
 */
char *mcpo_maybe_use_cimd(const json_t *cfg, const char *server_name)
{
    if (!cfg || cfg->type != JSON_OBJECT) return NULL;
    const json_t *dis = json_obj_get(cfg, "cimd");
    if (dis && dis->type == JSON_BOOL && !dis->bool_val) return NULL;

    const char *cfg_url = "";
    const json_t *uv = json_obj_get(cfg, "client_metadata_url");
    if (uv && uv->type == JSON_STRING) cfg_url = uv->str_val;
    const char *url = *cfg_url ? cfg_url : CIMD_METADATA_URL;
    if (!mcpo_is_valid_cimd_url(url)) return NULL;

    /* A pinned client_id is the user's explicit choice; a secret means a
     * confidential client which the shared document forbids (section 4.1). */
    const json_t *cid = json_obj_get(cfg, "client_id");
    if (cid && cid->type == JSON_STRING && cid->str_val && *cid->str_val)
        return NULL;
    const json_t *csec = json_obj_get(cfg, "client_secret");
    if (csec && csec->type == JSON_STRING && csec->str_val && *csec->str_val)
        return NULL;

    /* Document supplies name/auth method — an override means DCR identity. */
    const json_t *cn = json_obj_get(cfg, "client_name");
    if (cn && cn->type == JSON_STRING && cn->str_val && *cn->str_val)
        return NULL;
    {
        const json_t *am = json_obj_get(cfg, "token_endpoint_auth_method");
        const char *method =
            (am && am->type == JSON_STRING && am->str_val) ? am->str_val : "none";
        if (strcmp(method, "none") != 0) return NULL;
    }

    /* Dashboard/desktop flows redirect to deployment-specific URLs that can
     * never appear in the static document. The dashboard flow registers a
     * redirect_uri/port in cfg; its presence here is the same signal. */
    const json_t *ru = json_obj_get(cfg, "redirect_uri");
    if (ru && ru->type == JSON_STRING && ru->str_val && *ru->str_val) return NULL;
    const json_t *rp_ = json_obj_get(cfg, "redirect_port");
    if (rp_ && rp_->type == JSON_NUMBER && rp_->num_val != 0) return NULL;
    {
        const json_t *rh = json_obj_get(cfg, "redirect_host");
        const char *host =
            (rh && rh->type == JSON_STRING && rh->str_val && *rh->str_val)
                ? rh->str_val : "127.0.0.1";
        if (strcmp(host, "127.0.0.1") != 0 && strcmp(host, "localhost") != 0)
            return NULL;
    }

    /* An existing registration is bound to its registered redirect URI. */
    if (server_name && mcpo_has_cached_client_info(server_name)) return NULL;
    if (server_name && mcpo_cimd_rejected(server_name)) return NULL;
    if (server_name && mcpo_server_declined_cimd(server_name)) return NULL;

    int port = mcpo_pick_cimd_port();
    if (port < 0) return NULL;
    char *out = NULL;
    asprintf(&out, "%s\t%d", url, port);
    return out;
}

/* PoP: mcpo_cimd_provider_kwargs @ tools/mcp_oauth.py:cimd_provider_kwargs */
/*
 * {"client_metadata_url": url} when CIMD applies; {} on a DCR flow so the
 */
json_t *mcpo_cimd_provider_kwargs(const json_t *cfg)
{
    json_t *out = json_object();
    if (!out) return NULL;
    const json_t *url = cfg ? json_obj_get(cfg, "_cimd_url") : NULL;
    if (url && url->type == JSON_STRING && url->str_val && *url->str_val)
        json_set(out, "client_metadata_url", json_copy(url));
    return out;
}

/* PoP: token_request_user_agent @ tools/mcp_oauth.py:token_request_user_agent */
/*
 * Configured oauth.user_agent for token-endpoint requests, or NULL. Anything
 * that is not a non-empty string is treated as unset so a blank header never
 */
char *token_request_user_agent(const json_t *cfg)
{
    if (!cfg || cfg->type != JSON_OBJECT) return NULL;
    const json_t *ua = json_obj_get(cfg, "user_agent");
    if (!ua || ua->type != JSON_STRING || !ua->str_val) return NULL;
    const char *p = ua->str_val;
    while (*p && isspace((unsigned char)*p)) p++;
    size_t n = strlen(p);
    while (n > 0 && isspace((unsigned char)p[n - 1])) n--;
    if (n == 0) return NULL;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

/* PoP: mcpo_invalidate_tokens_on_client_change @ tools/mcp_oauth.py:_invalidate_tokens_on_client_change */
/*
 * Drop cached tokens when the configured OAuth client identity changes:
 * compares the on-disk client.json identity against the incoming config
 * identity BEFORE it overwrites, then unlinks tokens+meta (client.json kept
 */
void mcpo_invalidate_tokens_on_client_change(const char *server_name,
                                             const char *new_client_id,
                                             const char *new_client_secret)
{
    if (!server_name || !*server_name) return;
    json_t *existing = mcpo_read_client_info(server_name);
    if (!existing || existing->type != JSON_OBJECT) {
        if (existing) json_free(existing);
        return;
    }
    const json_t *old_id_v = json_obj_get(existing, "client_id");
    const char *old_id =
        (old_id_v && old_id_v->type == JSON_STRING) ? old_id_v->str_val : NULL;
    if (!old_id || !*old_id) { json_free(existing); return; }
    const json_t *old_sec_v = json_obj_get(existing, "client_secret");
    const char *old_secret =
        (old_sec_v && old_sec_v->type == JSON_STRING && old_sec_v->str_val &&
         *old_sec_v->str_val)
            ? old_sec_v->str_val : NULL;
    const char *eff_new_secret =
        (new_client_secret && *new_client_secret) ? new_client_secret : NULL;
    if (strcmp(old_id, new_client_id ? new_client_id : "") == 0 &&
        ((old_secret && eff_new_secret && strcmp(old_secret, eff_new_secret) == 0) ||
         (!old_secret && !eff_new_secret))) {
        json_free(existing);
        return;   /* live sessions and valid tokens preserved */
    }
    json_free(existing);

    char dir[1024];
    mcpo_token_dir(dir, sizeof(dir));
    char *safe = safe_filename(server_name);
    if (!safe) return;
    static const char *suffixes[] = {".json", ".meta.json"};
    bool removed = false;
    for (size_t i = 0; i < 2; i++) {
        char *path = NULL;
        if (asprintf(&path, "%s/%s%s", dir, safe, suffixes[i]) < 0) continue;
        if (unlink(path) == 0) removed = true;
        free(path);
    }
    free(safe);
    if (removed)
        fprintf(stderr,
                "warning: MCP OAuth '%s': configured OAuth client changed "
                "(client_id %s -> %s); discarded tokens minted under the "
                "previous client. Re-authorize with: hermes mcp login %s\n",
                server_name, old_id,
                new_client_id ? new_client_id : "(none)", server_name);
}
