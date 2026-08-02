#define _POSIX_C_SOURCE 200809L
/*
 * lsp_manager.c — port of agent/lsp/manager.py (LSPService orchestration).
 *
 * The bridge between the synchronous file_operations layer and the
 * per-(server,workspace) LSPClient subprocesses. Faithful to the Python
 * design:
 *   - one client per (server_id, workspace_root); lazy spawn + reuse
 *   - broken-set: failed (server,root) pairs are never retried
 *   - delta-baseline: pre-edit diagnostics snapshot, post-edit diff
 *   - get_diagnostics_sync: open + wait + drain in one blocking call
 *
 * Faithful-behavior port: server discovery is a minimal embedded table
 * (extension -> command) rather than the full servers.py forest; the
 * workspace root is the nearest .git ancestor (matches resolve_workspace).
 *
 */
#include "lsp_common.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <dirent.h>

#define LSP_DEFAULT_IDLE_TIMEOUT 600

struct lsp_service {
    bool enabled;
    lsp_server_desc_t **servers;  /* NULL-terminated, owned */
    size_t server_n;

    pthread_mutex_t lock;
    /* active clients: array of (server_id, ws_root, client) */
    struct {
        char *key;          /* "server_id@ws_root" */
        lsp_client_t *client;
    } *clients;
    size_t clients_n, clients_cap;

    /* broken-set: "server_id@ws_root" strings */
    char **broken;
    size_t broken_n, broken_cap;

    /* delta baseline: abs_path -> malloc'd JSON array string */
    struct {
        char *path;
        char *diags;   /* owned JSON array or NULL */
    } *baseline;
    size_t baseline_n, baseline_cap;
};

/* ── helpers ──────────────────────────────────────────────────────────── */
static char *xstrdup_m(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* key for a (server_id, ws_root) pair */
static char *make_key(const char *server_id, const char *ws_root)
{
    size_t n = strlen(server_id) + 1 + strlen(ws_root) + 1;
    char *k = malloc(n);
    snprintf(k, n, "%s@%s", server_id, ws_root);
    return k;
}

/* nearest ancestor dir containing .git (or NULL if none) */
static char *resolve_workspace(const char *file_path)
{
    char abs[PATH_MAX];
    if (!realpath(file_path, abs)) {
        /* fall back to dirname of file */
        char *slash = strrchr((char*)file_path, '/');
        if (slash && slash != file_path) { strncpy(abs, file_path, slash - file_path); abs[slash-file_path]='\0'; }
        else strcpy(abs, ".");
    }
    char dir[PATH_MAX];
    strcpy(dir, abs);
    /* walk up looking for .git */
    for (;;) {
        char probe[PATH_MAX];
        snprintf(probe, sizeof(probe), "%s/.git", dir);
        if (access(probe, F_OK) == 0) return xstrdup_m(dir);
        char *slash = strrchr(dir, '/');
        if (!slash) break;
        if (slash == dir) { dir[1] = '\0'; break; }  /* root */
        *slash = '\0';
    }
    /* no git: use the file's own directory as workspace root */
    char *slash = strrchr(abs, '/');
    if (slash && slash != abs) { *slash = '\0'; return xstrdup_m(abs); }
    return xstrdup_m(".");
}

static const char *ext_of(const char *path)
{
    const char *dot = strrchr(path, '.');
    return dot ? dot : "";
}

/* ── service lifecycle ────────────────────────────────────────────────── */
/* PoP: lsp_service_create @ agent/lsp/manager.py:create_from_config */
lsp_service_t *lsp_service_create(bool enabled, lsp_server_desc_t **servers)
{
    lsp_service_t *svc = calloc(1, sizeof(*svc));
    svc->enabled = enabled;
    svc->servers = servers;  /* ownership transferred */
    if (servers) while (servers[svc->server_n]) svc->server_n++;
    pthread_mutex_init(&svc->lock, NULL);
    return svc;
}

/* PoP: lsp_service_destroy @ agent/lsp/manager.py:shutdown */
void lsp_service_destroy(lsp_service_t *svc)
{
    if (!svc) return;
    pthread_mutex_lock(&svc->lock);
    for (size_t i = 0; i < svc->clients_n; i++) {
        if (svc->clients[i].client) lsp_client_destroy(svc->clients[i].client);
        free(svc->clients[i].key);
    }
    free(svc->clients);
    for (size_t i = 0; i < svc->broken_n; i++) free(svc->broken[i]);
    free(svc->broken);
    for (size_t i = 0; i < svc->baseline_n; i++) { free(svc->baseline[i].path); free(svc->baseline[i].diags); }
    free(svc->baseline);
    pthread_mutex_unlock(&svc->lock);
    /* free server descs */
    if (svc->servers) {
        for (size_t i = 0; svc->servers[i]; i++) {
            lsp_server_desc_t *d = svc->servers[i];
            free(d->server_id);
            if (d->command) { for (size_t j = 0; d->command[j]; j++) free(d->command[j]); free(d->command); }
            if (d->extensions) { for (size_t j = 0; d->extensions[j]; j++) free(d->extensions[j]); free(d->extensions); }
            free(d->init_options); free(d->env_json);
            free(d);
        }
        free(svc->servers);
    }
    pthread_mutex_destroy(&svc->lock);
    free(svc);
}

/* PoP: lsp_service_is_active @ agent/lsp/manager.py:is_active */
bool lsp_service_is_active(lsp_service_t *svc) { return svc && svc->enabled; }

/* ── resolution ──────────────────────────────────────────────────────── */
/* PoP: lsp_service_resolve @ agent/lsp/manager.py:enabled_for */
const char *lsp_service_resolve(lsp_service_t *svc, const char *file_path,
                                 char **out_ws_root)
{
    if (!svc || !svc->enabled) return NULL;
    const char *ext = ext_of(file_path);
    lsp_server_desc_t *matched = NULL;
    for (size_t i = 0; i < svc->server_n; i++) {
        lsp_server_desc_t *d = svc->servers[i];
        if (!d->extensions) continue;
        for (size_t j = 0; d->extensions[j]; j++) {
            if (strcasecmp(d->extensions[j], ext) == 0) { matched = d; goto found; }
        }
    }
found:
    if (!matched) return NULL;
    char *ws = resolve_workspace(file_path);
    /* broken-set check */
    char *key = make_key(matched->server_id, ws);
    bool broken = false;
    for (size_t i = 0; i < svc->broken_n; i++)
        if (strcmp(svc->broken[i], key) == 0) { broken = true; break; }
    free(key);
    if (broken) { free(ws); return NULL; }
    if (out_ws_root) *out_ws_root = ws; else free(ws);
    return matched->server_id;
}

/* ── client get-or-spawn ─────────────────────────────────────────────── */
/* PoP: _get_or_spawn @ agent/lsp/manager.py:_get_or_spawn */
static lsp_client_t *get_or_spawn(lsp_service_t *svc, const char *server_id,
                                   const char *ws_root, char **err_out)
{
    char *key = make_key(server_id, ws_root);
    pthread_mutex_lock(&svc->lock);
    for (size_t i = 0; i < svc->clients_n; i++) {
        if (strcmp(svc->clients[i].key, key) == 0) {
            lsp_client_t *c = svc->clients[i].client;
            pthread_mutex_unlock(&svc->lock);
            free(key);
            return c;
        }
    }
    /* not found: spawn */
    /* find desc */
    lsp_server_desc_t *desc = NULL;
    for (size_t i = 0; i < svc->server_n; i++)
        if (strcmp(svc->servers[i]->server_id, server_id) == 0) { desc = svc->servers[i]; break; }
    if (!desc) { pthread_mutex_unlock(&svc->lock); free(key); if (err_out) *err_out = xstrdup_m("no such server"); return NULL; }

    lsp_client_t *c = lsp_client_create(server_id, ws_root, desc->command, NULL, ws_root,
                                        desc->init_options ? desc->init_options : "{}");
    int rc = lsp_client_start(c, err_out);
    if (rc != 0) {
        lsp_client_destroy(c);
        /* mark broken */
        if (svc->broken_n == svc->broken_cap) { svc->broken_cap = svc->broken_cap?svc->broken_cap*2:8; svc->broken = realloc(svc->broken, svc->broken_cap*sizeof(char*)); }
        svc->broken[svc->broken_n++] = xstrdup_m(key);
        pthread_mutex_unlock(&svc->lock);
        free(key);
        return NULL;
    }
    /* store */
    if (svc->clients_n == svc->clients_cap) { svc->clients_cap = svc->clients_cap?svc->clients_cap*2:8; svc->clients = realloc(svc->clients, svc->clients_cap*sizeof(*svc->clients)); }
    svc->clients[svc->clients_n].key = key;
    svc->clients[svc->clients_n].client = c;
    svc->clients_n++;
    pthread_mutex_unlock(&svc->lock);
    return c;  /* key consumed by storage */
}

/* ── diagnostics bridge ─────────────────────────────────────────────── */
/* PoP: lsp_service_get_diagnostics @ agent/lsp/manager.py:get_diagnostics_sync */
char *lsp_service_get_diagnostics(lsp_service_t *svc, const char *file_path,
                                   int timeout_ms)
{
    if (!svc || !svc->enabled) return NULL;
    char *ws_root = NULL;
    const char *sid = lsp_service_resolve(svc, file_path, &ws_root);
    if (!sid || !ws_root) { free(ws_root); return NULL; }

    char *err = NULL;
    lsp_client_t *c = get_or_spawn(svc, sid, ws_root, &err);
    if (!c) { free(ws_root); free(err); return NULL; }

    int ver = lsp_client_open_file(c, file_path, "");
    int fresh = lsp_client_wait_for_diagnostics(c, file_path, ver, timeout_ms);
    char *diags = NULL;
    if (fresh == 0) diags = lsp_client_diagnostics_for(c, file_path);
    free(ws_root);
    free(err);
    return diags;  /* malloc'd JSON array or NULL */
}

/* PoP: lsp_service_snapshot_baseline @ agent/lsp/manager.py:snapshot_baseline */
void lsp_service_snapshot_baseline(lsp_service_t *svc, const char *file_path)
{
    if (!svc || !svc->enabled) return;
    char abs[PATH_MAX];
    if (!realpath(file_path, abs)) strncpy(abs, file_path, sizeof(abs)-1), abs[sizeof(abs)-1]='\0';
    char *diags = lsp_service_get_diagnostics(svc, file_path, 3000);
    pthread_mutex_lock(&svc->lock);
    /* upsert baseline[abs] */
    for (size_t i = 0; i < svc->baseline_n; i++) {
        if (strcmp(svc->baseline[i].path, abs) == 0) {
            free(svc->baseline[i].diags);
            svc->baseline[i].diags = diags;  /* may be NULL */
            pthread_mutex_unlock(&svc->lock);
            return;
        }
    }
    if (svc->baseline_n == svc->baseline_cap) { svc->baseline_cap = svc->baseline_cap?svc->baseline_cap*2:8; svc->baseline = realloc(svc->baseline, svc->baseline_cap*sizeof(*svc->baseline)); }
    svc->baseline[svc->baseline_n].path = xstrdup_m(abs);
    svc->baseline[svc->baseline_n].diags = diags;
    svc->baseline_n++;
    pthread_mutex_unlock(&svc->lock);
}
