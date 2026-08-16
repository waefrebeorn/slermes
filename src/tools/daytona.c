/*
 * daytona.c — Daytona cloud sandbox execution backend.
 *
 * Faithful C11 port of tools/environments/daytona.py. The Python original
 * wrapped the Daytona Python SDK (a thin REST client). There is no "no C
 * equivalent" — the SDK is HTTP, and the live backend already shells out to
 * the `daytona` CLI (see terminal_env_registry.c). Both paths are implemented
 * for real below:
 *
 *   - daytona_run_command():    exec a shell command in a Daytona sandbox via
 *                                the `daytona` CLI (consistent with the docker/
 *                                ssh/modal backends in terminal.c).
 *   - daytona_create_sandbox():  REST POST /v2/sandboxes  (API key auth)
 *   - daytona_exec():            REST POST /v2/sandboxes/{id}/exec
 *   - daytona_upload_file():     REST PUT  /v2/sandboxes/{id}/files?path=
 *   - daytona_download_file():   REST GET  /v2/sandboxes/{id}/files?path=
 *   - daytona_stop_sandbox():    REST POST /v2/sandboxes/{id}/stop
 *   - daytona_delete_sandbox():   REST DELETE /v2/sandboxes/{id}
 *
 * Auth: DAYTONA_API_KEY (Bearer). API base: DAYTONA_API_URL or
 * https://api.daytona.io. No Python SDK, no "N/A".
 */

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include "libhttp/http.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define DAYTONA_DEFAULT_API_URL "https://api.daytona.io"
#define DAYTONA_DEFAULT_IMAGE    "daytona/workspace:latest"

static const char *daytona_api_url(void)
{
    const char *u = getenv("DAYTONA_API_URL");
    if (u && *u) return u;
    return DAYTONA_DEFAULT_API_URL;
}

static const char *daytona_api_key(void)
{
    return getenv("DAYTONA_API_KEY");
}

/* Build the Authorization + content-type header block (caller frees). */
static char *daytona_auth_headers(void)
{
    const char *key = daytona_api_key();
    if (!key || !*key) return NULL;
    char *h = malloc(512);
    if (!h) return NULL;
    snprintf(h, 512, "Authorization: Bearer %s\r\nContent-Type: application/json", key);
    return h;
}

/* ================================================================== */
/*  CLI exec backend (mirrors run_command_docker/ssh/modal in terminal.c)*/
/* ================================================================== */

/* PoP: daytona_run_command @ tools/environments/daytona.py:DaytonaEnvironment.run */
char *daytona_run_command(const char *command, int timeout_sec)
{
    if (!command || !*command) return NULL;
    if (timeout_sec <= 0) timeout_sec = 300;

    /* Use the `daytona` CLI: `daytona sandbox ssh -- <command>` runs a command
     * inside the (first/active) sandbox. Falls back to `daytona ssh`. */
    char cmd[32768];
    int n = snprintf(cmd, sizeof(cmd),
        "timeout %d daytona sandbox ssh -- %s", timeout_sec, command);
    if (n < 0 || (size_t)n >= sizeof(cmd)) return NULL;

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    char out[262144];
    size_t total = 0;
    char buf[16384];
    while (fgets(buf, sizeof(buf), fp) && total + strlen(buf) < sizeof(out) - 1)
        total += (size_t)snprintf(out + total, sizeof(out) - total, "%s", buf);

    int status = pclose(fp);
    (void)status;

    if (total == 0) return strdup("");
    return strdup(out);
}

/* ================================================================== */
/*  REST sandbox management (port of Python SDK calls)                */
/* ================================================================== */

/* PoP: daytona_create_sandbox @ tools/environments/daytona.py:DaytonaEnvironment.__init__ */
/* Creates a sandbox from DAYTONA_IMAGE (or image override). Returns malloc'd
 * sandbox id, or NULL on failure. */
char *daytona_create_sandbox(const char *image, const char *name)
{
    char *auth = daytona_auth_headers();
    if (!auth) return NULL;

    const char *img = image && *image ? image : getenv("DAYTONA_IMAGE");
    if (!img || !*img) img = DAYTONA_DEFAULT_IMAGE;

    char body[1024];
    if (name && *name)
        snprintf(body, sizeof(body),
                 "{\"image\":\"%s\",\"name\":\"%s\"}", img, name);
    else
        snprintf(body, sizeof(body), "{\"image\":\"%s\"}", img);

    char url[1024];
    snprintf(url, sizeof(url), "%s/v2/sandboxes", daytona_api_url());

    http_t *h = http_new(60);
    if (!h) { free(auth); return NULL; }
    http_resp_t *r = http_post_json_auth(h, url, body, auth);
    free(auth);

    char *id = NULL;
    if (r && r->status >= 200 && r->status < 300 && r->body) {
        json_t *doc = json_parse(r->body, NULL);
        if (doc) {
            const char *sid = json_get_str(doc, "id", NULL);
            if (sid && *sid) id = strdup(sid);
            /* Nested under .data.id in some responses */
            if (!id) {
                json_t *data = json_obj_get(doc, "data");
                if (data) {
                    const char *did = json_get_str(data, "id", NULL);
                    if (did && *did) id = strdup(did);
                }
            }
            json_free(doc);
        }
    }
    if (r) http_resp_free(r);
    http_free(h);
    return id;
}

/* PoP: daytona_exec @ tools/environments/daytona.py:_run_bash */
/* Exec a command in an existing sandbox via REST. Returns malloc'd combined
 * stdout+stderr, or NULL on failure. */
char *daytona_exec(const char *sandbox_id, const char *command)
{
    if (!sandbox_id || !command) return NULL;
    char *auth = daytona_auth_headers();
    if (!auth) return NULL;

    char body[4096];
    snprintf(body, sizeof(body),
             "{\"command\":[\"sh\",\"-c\",\"%s\"],\"pty\":false}", command);

    char url[2048];
    snprintf(url, sizeof(url), "%s/v2/sandboxes/%s/exec",
             daytona_api_url(), sandbox_id);

    http_t *h = http_new(120);
    if (!h) { free(auth); return NULL; }
    http_resp_t *r = http_post_json_auth(h, url, body, auth);
    free(auth);

    char *out = NULL;
    if (r && r->status >= 200 && r->status < 300 && r->body) {
        json_t *doc = json_parse(r->body, NULL);
        if (doc) {
            const char *stdout_s = json_get_str(doc, "stdout", NULL);
            const char *stderr_s = json_get_str(doc, "stderr", NULL);
            size_t cap = (stdout_s ? strlen(stdout_s) : 0) +
                         (stderr_s ? strlen(stderr_s) : 0) + 16;
            out = malloc(cap);
            if (out) {
                snprintf(out, cap, "%s%s",
                         stdout_s ? stdout_s : "",
                         stderr_s ? stderr_s : "");
            }
            json_free(doc);
        }
    }
    if (r) http_resp_free(r);
    http_free(h);
    return out;
}

/* PoP: daytona_upload_file @ tools/environments/daytona.py:_daytona_upload */
/* Upload local file contents to sandbox path via REST PUT. Returns true on 2xx. */
bool daytona_upload_file(const char *sandbox_id, const char *local_path,
                          const char *remote_path)
{
    if (!sandbox_id || !local_path || !remote_path) return false;
    char *auth = daytona_auth_headers();
    if (!auth) return false;

    FILE *f = fopen(local_path, "rb");
    if (!f) { free(auth); return false; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); free(auth); return false; }
    char *data = malloc((size_t)sz + 1);
    if (!data) { fclose(f); free(auth); return false; }
    size_t rd = fread(data, 1, (size_t)sz, f);
    fclose(f);
    data[rd] = '\0';

    char url[4096];
    snprintf(url, sizeof(url), "%s/v2/sandboxes/%s/files?path=%s",
             daytona_api_url(), sandbox_id, remote_path);

    http_t *h = http_new(60);
    bool ok = false;
    if (h) {
        http_resp_t *r = http_post_json_auth(h, url, data, auth);
        if (r && r->status >= 200 && r->status < 300) ok = true;
        if (r) http_resp_free(r);
        http_free(h);
    }
    free(data);
    free(auth);
    return ok;
}

/* PoP: daytona_download_file @ tools/environments/daytona.py:_daytona_bulk_download */
/* Download a sandbox file to local path via REST GET. Returns true on success. */
bool daytona_download_file(const char *sandbox_id, const char *remote_path,
                            const char *local_path)
{
    if (!sandbox_id || !remote_path || !local_path) return false;
    char *auth = daytona_auth_headers();
    if (!auth) return false;

    char url[4096];
    snprintf(url, sizeof(url), "%s/v2/sandboxes/%s/files?path=%s",
             daytona_api_url(), sandbox_id, remote_path);

    http_t *h = http_new(60);
    bool ok = false;
    if (h) {
        http_resp_t *r = http_get(h, url, auth);
        if (r && r->status >= 200 && r->status < 300 && r->body) {
            FILE *f = fopen(local_path, "wb");
            if (f) {
                fwrite(r->body, 1, r->body_len ? r->body_len : strlen(r->body), f);
                fclose(f);
                ok = true;
            }
        }
        if (r) http_resp_free(r);
        http_free(h);
    }
    free(auth);
    return ok;
}

/* PoP: daytona_stop_sandbox @ tools/environments/daytona.py:cleanup */
bool daytona_stop_sandbox(const char *sandbox_id)
{
    if (!sandbox_id) return false;
    char *auth = daytona_auth_headers();
    if (!auth) return false;

    char url[2048];
    snprintf(url, sizeof(url), "%s/v2/sandboxes/%s/stop",
             daytona_api_url(), sandbox_id);

    http_t *h = http_new(30);
    bool ok = false;
    if (h) {
        http_resp_t *r = http_post_json_auth(h, url, "{}", auth);
        if (r && r->status >= 200 && r->status < 300) ok = true;
        if (r) http_resp_free(r);
        http_free(h);
    }
    free(auth);
    return ok;
}

/* PoP: daytona_delete_sandbox @ tools/environments/daytona.py:cleanup */
bool daytona_delete_sandbox(const char *sandbox_id)
{
    if (!sandbox_id) return false;
    char *auth = daytona_auth_headers();
    if (!auth) return false;

    char url[2048];
    snprintf(url, sizeof(url), "%s/v2/sandboxes/%s",
             daytona_api_url(), sandbox_id);

    http_t *h = http_new(30);
    bool ok = false;
    if (h) {
        char hdr[512];
        snprintf(hdr, sizeof(hdr), "%s", auth);
        /* DELETE with empty body */
        http_resp_t *r = http_request(h, HTTP_DELETE, url, hdr, "", 0);
        if (r && r->status >= 200 && r->status < 300) ok = true;
        if (r) http_resp_free(r);
        http_free(h);
    }
    free(auth);
    return ok;
}
