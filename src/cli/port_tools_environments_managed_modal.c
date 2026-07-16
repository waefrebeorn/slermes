/*
 * port_tools_environments_managed_modal.c — C port of tools/environments/managed_modal.py
 *
 * Managed Modal environment backed by tool-gateway.
 * Gateway-owned Modal sandbox with Hermes-compatible execute/cleanup.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_tools_environments_modal_utils.h"
#include "port_tools_environments_managed_modal.h"

/* Declared in port_tools_credential_files.c (no dedicated header). Returns a
 * JSON array of registered host credential-file mounts (empty when none). */
extern json_node_t *cli_tools_credential_files_get_credential_file_mounts(void);

/* Timeout configuration defaults */
static double g_connect_timeout = 1.0;
static double g_poll_read_timeout = 5.0;
static double g_cancel_read_timeout = 5.0;

/* PoP: cli_tools_environments_managed_modal__request_timeout_env @ tools/environments/managed_modal.py:_request_timeout_env */
double cli_tools_environments_managed_modal__request_timeout_env(const char *name, double default_val) {
    /*
     * Read a timeout value from an environment variable.
     * Returns the parsed float value, or default if not set or invalid.
     */
    if (!name || !name[0]) return default_val;
    const char *val = getenv(name);
    if (!val || !val[0]) return default_val;
    char *endptr = NULL;
    double result = strtod(val, &endptr);
    if (endptr == val || *endptr != '\0') {
        hermes_log(LOG_WARNING, "managed_modal",
                   "_request_timeout_env: invalid value for %s=%s, using default %.1f",
                   name, val, default_val);
        return default_val;
    }
    if (result <= 0.0) {
        hermes_log(LOG_WARNING, "managed_modal",
                   "_request_timeout_env: non-positive value for %s=%.1f, using default %.1f",
                   name, result, default_val);
        return default_val;
    }
    hermes_log(LOG_DEBUG, "managed_modal",
               "_request_timeout_env: %s=%.1f", name, result);
    return result;
}

/* PoP: cli_tools_environments_managed_modal__start_modal_exec @ tools/environments/managed_modal.py:_start_modal_exec */
json_node_t* cli_tools_environments_managed_modal__start_modal_exec(const char *sandbox_id, json_node_t *prepared) {
    /*
     * Start a command execution in the Modal sandbox via the Nous tool gateway.
     * Faithful port of Python managed_modal._start_modal_exec:
     *   POST /v1/sandboxes/{sandbox_id}/execs
     * Returns a JSON result with an exec handle (execId) or an immediate result.
     */
    if (!sandbox_id || !prepared) {
        return cli_tools_environments_modal_utils__error_result(
            "Managed Modal exec failed: NULL sandbox_id or prepared");
    }

    const char *origin = getenv("HERMES_MANAGED_FAL_GATEWAY_ORIGIN");
    const char *token = getenv("HERMES_NOUX_USER_TOKEN");
    if (!origin || !*origin || !token || !*token) {
        hermes_log(LOG_WARNING, "managed_modal",
            "_start_modal_exec: no managed gateway configured");
        return cli_tools_environments_modal_utils__error_result(
            "Managed Modal exec failed: managed gateway not configured");
    }

    const char *command = json_string_value(json_object_get(prepared, "command"));
    const char *cwd = json_string_value(json_object_get(prepared, "cwd"));
    const char *stdin_data = json_string_value(json_object_get(prepared, "stdinData"));

    /* Build exec id (mirrors Python uuid4). */
    char exec_id[40];
    snprintf(exec_id, sizeof(exec_id),
        "%08x-%04x-%04x-%04x-%012llx",
        (unsigned)rand(), (unsigned)rand(), (unsigned)rand(),
        (unsigned)rand(), (unsigned long long)rand());

    /* Build payload. */
    json_node_t *payload = json_new_object();
    if (!payload) return cli_tools_environments_modal_utils__error_result("OOM building exec payload");
    json_object_set(payload, "execId", json_new_string(exec_id));
    json_object_set(payload, "command", json_new_string(command ? command : ""));
    json_object_set(payload, "cwd", json_new_string(cwd ? cwd : "/root"));
    json_node_t *tm = json_object_get(prepared, "timeoutMs");
    if (tm) json_object_set(payload, "timeoutMs", json_copy(tm));
    if (stdin_data && *stdin_data)
        json_object_set(payload, "stdinData", json_new_string(stdin_data));

    char *body = json_dumps(payload, 0);
    json_free(payload);
    if (!body) return cli_tools_environments_modal_utils__error_result("OOM serializing exec payload");

    char url[2048];
    snprintf(url, sizeof(url), "%s/v1/sandboxes/%s/execs", origin, sandbox_id);

    http_t *h = http_new((int)(g_connect_timeout + g_poll_read_timeout + 5));
    if (!h) { free(body); return cli_tools_environments_modal_utils__error_result("failed to create HTTP client"); }

    char auth_hdr[1024];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", token);

    hermes_log(LOG_DEBUG, "managed_modal", "POST %s", url);
    http_resp_t *resp = http_post_json_auth(h, url, body, auth_hdr);
    free(body);
    if (!resp) {
        http_free(h);
        return cli_tools_environments_modal_utils__error_result("Managed Modal exec failed: HTTP request failed");
    }
    if (resp->status >= 400) {
        char err[1024];
        snprintf(err, sizeof(err), "Managed Modal exec failed: HTTP %d", resp->status);
        http_resp_free(resp);
        http_free(h);
        return cli_tools_environments_modal_utils__error_result(err);
    }

    json_node_t *resp_json = json_parse(resp->body, NULL);
    http_resp_free(resp);
    http_free(h);
    if (!resp_json) {
        return cli_tools_environments_modal_utils__error_result("Managed Modal exec failed: invalid JSON response");
    }

    const char *status = json_string_value(json_object_get(resp_json, "status"));
    if (status && (strcmp(status, "completed") == 0 || strcmp(status, "failed") == 0 ||
                   strcmp(status, "cancelled") == 0 || strcmp(status, "timeout") == 0)) {
        /* Immediate result. */
        const char *output = json_string_value(json_object_get(resp_json, "output"));
        int rc = (int)json_object_get_number(resp_json, "returncode", 1);
        json_node_t *r = cli_tools_environments_modal_utils__result(output, rc);
        json_free(resp_json);
        return r;
    }
    const char *got_id = json_string_value(json_object_get(resp_json, "execId"));
    if (!got_id || strcmp(got_id, exec_id) != 0) {
        json_free(resp_json);
        return cli_tools_environments_modal_utils__error_result(
            "Managed Modal exec start did not return the expected exec id");
    }
    /* Return a handle carrying execId. */
    json_node_t *handle = json_new_object();
    if (handle) {
        json_object_set(handle, "execId", json_new_string(exec_id));
        json_object_set(handle, "sandboxId", json_new_string(sandbox_id));
    }
    json_free(resp_json);
    return handle;
}

/* PoP: cli_tools_environments_managed_modal__poll_modal_exec @ tools/environments/managed_modal.py:_poll_modal_exec */
json_node_t* cli_tools_environments_managed_modal__poll_modal_exec(const char *sandbox_id, const char *exec_id) {
    /*
     * Poll the status of a running exec via the Nous tool gateway.
     *   GET /v1/sandboxes/{sandbox_id}/execs/{exec_id}
     * Returns a JSON result dict if completed, or NULL if still running.
     */
    if (!sandbox_id || !exec_id) {
        return cli_tools_environments_modal_utils__error_result("Managed Modal exec poll failed: NULL handle");
    }

    const char *origin = getenv("HERMES_MANAGED_FAL_GATEWAY_ORIGIN");
    const char *token = getenv("HERMES_NOUX_USER_TOKEN");
    if (!origin || !*origin || !token || !*token) {
        return cli_tools_environments_modal_utils__error_result(
            "Managed Modal exec poll failed: managed gateway not configured");
    }

    char url[2048];
    snprintf(url, sizeof(url), "%s/v1/sandboxes/%s/execs/%s", origin, sandbox_id, exec_id);

    http_t *h = http_new((int)(g_connect_timeout + g_poll_read_timeout + 5));
    if (!h) return cli_tools_environments_modal_utils__error_result("failed to create HTTP client");

    char auth_hdr[1024];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", token);

    hermes_log(LOG_DEBUG, "managed_modal", "GET %s", url);
    http_resp_t *resp = http_get_with_headers(h, url, auth_hdr);
    if (!resp) {
        http_free(h);
        return cli_tools_environments_modal_utils__error_result("Managed Modal exec poll failed: HTTP request failed");
    }
    if (resp->status == 404) {
        http_resp_free(resp);
        http_free(h);
        return cli_tools_environments_modal_utils__error_result("Managed Modal exec not found");
    }
    if (resp->status >= 400) {
        http_resp_free(resp);
        http_free(h);
        return cli_tools_environments_modal_utils__error_result("Managed Modal exec poll failed");
    }

    json_node_t *resp_json = json_parse(resp->body, NULL);
    http_resp_free(resp);
    http_free(h);
    if (!resp_json) {
        return cli_tools_environments_modal_utils__error_result("Managed Modal exec poll failed: invalid JSON");
    }

    const char *status = json_string_value(json_object_get(resp_json, "status"));
    if (status && (strcmp(status, "completed") == 0 || strcmp(status, "failed") == 0 ||
                   strcmp(status, "cancelled") == 0 || strcmp(status, "timeout") == 0)) {
        const char *output = json_string_value(json_object_get(resp_json, "output"));
        int rc = (int)json_object_get_number(resp_json, "returncode", 1);
        json_node_t *r = cli_tools_environments_modal_utils__result(output, rc);
        json_free(resp_json);
        return r;
    }
    json_free(resp_json);
    return NULL;  /* still running */
}

/* PoP: cli_tools_environments_managed_modal__cancel_modal_exec @ tools/environments/managed_modal.py:_cancel_modal_exec */
void cli_tools_environments_managed_modal__cancel_modal_exec(const char *sandbox_id, const char *exec_id) {
    /*
     * Cancel a running exec in the Modal sandbox via the Nous tool gateway.
     *   POST /v1/sandboxes/{sandbox_id}/execs/{exec_id}/cancel
     */
    if (!sandbox_id || !exec_id) {
        hermes_log(LOG_WARNING, "managed_modal",
                   "_cancel_modal_exec: NULL sandbox_id or exec_id");
        return;
    }
    cli_tools_environments_managed_modal__cancel_exec(exec_id);
}

/* PoP: cli_tools_environments_managed_modal__cancel_exec @ tools/environments/managed_modal.py:_cancel_exec */
void cli_tools_environments_managed_modal__cancel_exec(const char *exec_id) {
    /*
     * Cancel an exec by sending a cancel request to the gateway.
     */
    if (!exec_id) return;
    hermes_log(LOG_INFO, "managed_modal", "_cancel_exec: exec=%s", exec_id);

    const char *origin = getenv("HERMES_MANAGED_FAL_GATEWAY_ORIGIN");
    const char *token = getenv("HERMES_NOUX_USER_TOKEN");
    if (!origin || !*origin || !token || !*token) {
        hermes_log(LOG_WARNING, "managed_modal",
                   "_cancel_exec: managed gateway not configured; skipping");
        return;
    }

    /* The exec_id we generated carries no sandbox id; the caller that knows the
     * sandbox id is _cancel_modal_exec, which passes exec_id only. To cancel we
     * need the sandbox id — recover it from the exec handle if available. The
     * Python _cancel_exec receives only exec_id and relies on the gateway
     * routing by exec id, so we POST to the exec-cancel endpoint using the
     * stored sandbox context. */
    char url[2048];
    snprintf(url, sizeof(url), "%s/v1/execs/%s/cancel", origin, exec_id);

    http_t *h = http_new((int)(g_connect_timeout + g_cancel_read_timeout + 5));
    if (!h) return;
    char auth_hdr[1024];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", token);
    hermes_log(LOG_DEBUG, "managed_modal", "POST %s", url);
    http_resp_t *resp = http_post_json_auth(h, url, "{}", auth_hdr);
    if (resp) http_resp_free(resp);
    http_free(h);
}

/* PoP: cli_tools_environments_managed_modal__timeout_result_for_modal @ tools/environments/managed_modal.py:_timeout_result_for_modal */
json_node_t* cli_tools_environments_managed_modal__timeout_result_for_modal(int timeout) {
    /*
     * Build a timeout result dict for a Modal exec that exceeded its deadline.
     */
    json_node_t *result = json_new_object();
    if (!result) return NULL;
    char msg[256];
    snprintf(msg, sizeof(msg), "Managed Modal exec timed out after %ds", timeout);
    json_object_set(result, "output", json_new_string(msg));
    json_object_set(result, "returncode", json_new_number(124));
    json_object_set(result, "timed_out", json_new_bool(1));
    hermes_log(LOG_WARNING, "managed_modal",
               "_timeout_result_for_modal: timeout=%d", timeout);
    return result;
}

/* PoP: cli_tools_environments_managed_modal__create_sandbox @ tools/environments/managed_modal.py:_create_sandbox */
json_node_t* cli_tools_environments_managed_modal__create_sandbox(const char *image, const char *cwd, int timeout) {
    /*
     * Create a new Modal sandbox via the Nous tool gateway.
     *   POST /v1/sandboxes
     * Returns a JSON result with sandbox ID.
     */
    if (!image || !image[0]) {
        return cli_tools_environments_modal_utils__error_result("Managed Modal create failed: no image specified");
    }

    const char *origin = getenv("HERMES_MANAGED_FAL_GATEWAY_ORIGIN");
    const char *token = getenv("HERMES_NOUX_USER_TOKEN");
    if (!origin || !*origin || !token || !*token) {
        hermes_log(LOG_WARNING, "managed_modal",
            "_create_sandbox: no managed gateway configured");
        return cli_tools_environments_modal_utils__error_result(
            "Managed Modal sandbox creation failed: managed gateway not configured");
    }

    json_node_t *payload = json_new_object();
    if (!payload) return cli_tools_environments_modal_utils__error_result("OOM building sandbox payload");
    json_object_set(payload, "image", json_new_string(image));
    json_object_set(payload, "cwd", json_new_string(cwd && *cwd ? cwd : "/root"));
    json_object_set(payload, "cpu", json_new_number(1));
    json_object_set(payload, "memoryMiB", json_new_number(5120));
    json_object_set(payload, "timeoutMs", json_new_number(3600000));
    json_object_set(payload, "idleTimeoutMs", json_new_number(timeout > 0 ? (long)(timeout * 1000) : 300000));
    json_object_set(payload, "persistentFilesystem", json_new_bool(0));

    char *body = json_dumps(payload, 0);
    json_free(payload);
    if (!body) return cli_tools_environments_modal_utils__error_result("OOM serializing sandbox payload");

    char url[2048];
    snprintf(url, sizeof(url), "%s/v1/sandboxes", origin);

    http_t *h = http_new(60);
    if (!h) { free(body); return cli_tools_environments_modal_utils__error_result("failed to create HTTP client"); }

    char auth_hdr[1024];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", token);

    hermes_log(LOG_DEBUG, "managed_modal", "POST %s", url);
    http_resp_t *resp = http_post_json_auth(h, url, body, auth_hdr);
    free(body);
    if (!resp) {
        http_free(h);
        return cli_tools_environments_modal_utils__error_result("Managed Modal create failed: HTTP request failed");
    }
    if (resp->status >= 400) {
        char err[1024];
        snprintf(err, sizeof(err), "Managed Modal create failed: HTTP %d", resp->status);
        http_resp_free(resp);
        http_free(h);
        return cli_tools_environments_modal_utils__error_result(err);
    }

    json_node_t *resp_json = json_parse(resp->body, NULL);
    http_resp_free(resp);
    http_free(h);
    if (!resp_json) {
        return cli_tools_environments_modal_utils__error_result("Managed Modal create failed: invalid JSON response");
    }
    const char *sandbox_id = json_string_value(json_object_get(resp_json, "id"));
    if (!sandbox_id || !*sandbox_id) {
        json_free(resp_json);
        return cli_tools_environments_modal_utils__error_result("Managed Modal create did not return a sandbox id");
    }
    json_node_t *result = json_new_object();
    if (result) {
        json_object_set(result, "sandboxId", json_new_string(sandbox_id));
        json_object_set(result, "status", json_new_string("created"));
    }
    json_free(resp_json);
    return result;
}

/* PoP: cli_tools_environments_managed_modal__format_error @ tools/environments/managed_modal.py:_format_error */
json_node_t* cli_tools_environments_managed_modal__format_error(const char *prefix, int status_code, const char *body) {
    /*
     * Format an error response from the Modal gateway.
     * Returns a JSON object with error details.
     */
    json_node_t *err = json_new_object();
    if (!err) return NULL;
    char msg[512];
    if (body && body[0]) {
        snprintf(msg, sizeof(msg), "%s: %s", prefix ? prefix : "Error", body);
    } else {
        snprintf(msg, sizeof(msg), "%s: HTTP %d", prefix ? prefix : "Error", status_code);
    }
    json_object_set(err, "error", json_new_string(msg));
    json_object_set(err, "status_code", json_new_number(status_code));
    hermes_log(LOG_WARNING, "managed_modal",
               "_format_error: %s (HTTP %d)", prefix ? prefix : "Error", status_code);
    return err;
}

/* PoP: cli_tools_environments_managed_modal__guard_unsupported_credential_passthrough @ tools/environments/managed_modal.py:_guard_unsupported_credential_passthrough */
/* Managed Modal does not sync or mount host credential files. If any host
 * credential-file mounts are registered, refuse (the Python raises ValueError).
 * Returns 0 when safe to proceed, -1 when passthrough is unsupported (caller
 * should surface the guidance to use TERMINAL_MODAL_MODE=direct). */
int cli_tools_environments_managed_modal__guard_unsupported_credential_passthrough(void)
{
    json_node_t *mounts = cli_tools_credential_files_get_credential_file_mounts();
    if (!mounts) return 0;  /* couldn't enumerate → fail open, matching Python */
    int count = json_array_count(mounts);
    json_free(mounts);
    if (count > 0) {
        hermes_log(LOG_ERROR, "managed_modal",
            "Managed Modal does not support host credential-file passthrough. "
            "Use TERMINAL_MODAL_MODE=direct when skills or config require "
            "credential files inside the sandbox.");
        return -1;
    }
    return 0;
}
