/*
 * port_auth.c — Port of Python hermes_cli/auth.py
 *
 * C implementations for name parity.
 * OAuth flow functions that delegate to the agent's antigravity OAuth module.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#define AUTH_DIR ".hermes/antigravity"
#define ACCESS_TOKEN_LEN 2048

/*
 * _get_credentials_path — Return the path to the antigravity credentials file.
 */
static const char* _get_credentials_path(void)
{
    static char path[4096];
    const char* home = getenv("HOME");
    if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/%s/credentials.json", home, AUTH_DIR);
    return path;
}

/*
 * _load_credentials_json — Load credentials from disk.
 */
static json_t* _load_credentials_json(void)
{
    const char* path = _get_credentials_path();
    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t n = fread(buf, 1, size, f);
    fclose(f);
    buf[n] = '\0';

    /* Strip whitespace for json_parse */
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' ')) {
        buf[--n] = '\0';
    }

    json_t* parsed = json_parse(buf, NULL);
    free(buf);
    return parsed;
}

/*
 * resolve_antigravity_oauth_runtime_credentials — Resolve runtime OAuth creds.
 *
 * Python: def resolve_antigravity_oauth_runtime_credentials(*, force_refresh: bool = False) -> Dict:
 *   Loads credentials from disk, refreshes if needed.
 */
/* Port of Python: resolve_antigravity_oauth_runtime_credentials */
json_t* resolve_antigravity_oauth_runtime_credentials(bool force_refresh)
{
    (void)force_refresh;

    json_t* creds = _load_credentials_json();
    if (!creds) {
        hermes_log(LOG_WARNING, "port", "resolve_antigravity_oauth: no credentials file");
        return json_new_object();
    }

    return creds;
}

/*
 * get_antigravity_oauth_auth_status — Return status dict for hermes auth list.
 *
 * Python: def get_antigravity_oauth_auth_status() -> Dict[str, Any]:
 *   Checks if credentials exist and are valid.
 */
/* Port of Python: get_antigravity_oauth_auth_status */
json_t* get_antigravity_oauth_auth_status(void)
{
    json_t* result = json_new_object();
    if (!result) return NULL;

    json_t* creds = _load_credentials_json();
    if (!creds) {
        json_object_set(result, "logged_in", json_new_bool(false));
        json_object_set(result, "error", json_new_string("No credentials file"));
        return result;
    }

    const char* access_token = NULL;
    json_t* token_node = json_object_get(creds, "access_token");
    if (token_node) {
        access_token = json_node_get_string(token_node);
    }

    bool logged_in = (access_token && access_token[0]);
    json_object_set(result, "logged_in", json_new_bool(logged_in));

    if (logged_in) {
        /* Token preview: first 8 chars */
        char preview[32];
        snprintf(preview, sizeof(preview), "%.8s...", access_token);
        json_object_set(result, "token_preview", json_new_string(preview));
        json_object_set(result, "source", json_new_string("antigravity_oauth"));
    }

    return result;
}
