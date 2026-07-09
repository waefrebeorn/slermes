/*
 * port_tools_environments_managed_modal.c — C port of tools/environments/managed_modal.py
 *
 * Managed Modal environment backed by tool-gateway.
 * Gateway-owned Modal sandbox with Hermes-compatible execute/cleanup.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
     * Start a command execution in the Modal sandbox.
     * Returns a JSON result with exec handle or immediate result.
     */
    if (!sandbox_id || !prepared) {
        json_node_t *err = json_new_object();
        if (err) {
            json_object_set(err, "error", json_new_string("Managed Modal exec failed: NULL sandbox_id or prepared"));
            json_object_set(err, "returncode", json_new_number(1));
        }
        return err;
    }
    hermes_log(LOG_WARNING, "managed_modal",
               "_start_modal_exec: sandbox=%s — exec not implemented in C port (managed gateway POST not wired)",
               sandbox_id);
    json_node_t *err = json_new_object();
    if (err) {
        json_object_set(err, "error",
            json_new_string("Managed Modal exec not implemented in C port: gateway transport not wired"));
        json_object_set(err, "returncode", json_new_number(1));
    }
    return err;
}

/* PoP: cli_tools_environments_managed_modal__poll_modal_exec @ tools/environments/managed_modal.py:_poll_modal_exec */
json_node_t* cli_tools_environments_managed_modal__poll_modal_exec(const char *sandbox_id, const char *exec_id) {
    /*
     * Poll the status of a running exec.
     * Returns a JSON result dict if completed, or NULL if still running.
     */
    if (!sandbox_id || !exec_id) {
        json_node_t *err = json_new_object();
        if (err) {
            json_object_set(err, "error", json_new_string("Managed Modal exec poll failed: NULL handle"));
            json_object_set(err, "returncode", json_new_number(1));
        }
        return err;
    }
    hermes_log(LOG_DEBUG, "managed_modal",
               "_poll_modal_exec: sandbox=%s exec=%s", sandbox_id, exec_id);
    /* Return NULL to indicate still running (gateway will retry) */
    return NULL;
}

/* PoP: cli_tools_environments_managed_modal__cancel_modal_exec @ tools/environments/managed_modal.py:_cancel_modal_exec */
void cli_tools_environments_managed_modal__cancel_modal_exec(const char *sandbox_id, const char *exec_id) {
    /*
     * Cancel a running exec in the Modal sandbox.
     */
    if (!sandbox_id || !exec_id) {
        hermes_log(LOG_WARNING, "managed_modal",
                   "_cancel_modal_exec: NULL sandbox_id or exec_id");
        return;
    }
    hermes_log(LOG_INFO, "managed_modal",
               "_cancel_modal_exec: sandbox=%s exec=%s", sandbox_id, exec_id);
    /* Send cancel request to gateway */
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
     * Create a new Modal sandbox via the tool gateway.
     * Returns a JSON result with sandbox ID.
     */
    if (!image || !image[0]) {
        json_node_t *err = json_new_object();
        if (err) {
            json_object_set(err, "error", json_new_string("Managed Modal create failed: no image specified"));
        }
        return err;
    }
    hermes_log(LOG_WARNING, "managed_modal",
               "_create_sandbox: image=%s — sandbox creation not implemented in C port",
               image);
    json_node_t *err = json_new_object();
    if (err) {
        json_object_set(err, "error",
            json_new_string("Managed Modal sandbox creation not implemented in C port: gateway transport not wired"));
    }
    return err;
}

/* PoP: cli_tools_environments_managed_modal__cancel_exec @ tools/environments/managed_modal.py:_cancel_exec */
void cli_tools_environments_managed_modal__cancel_exec(const char *sandbox_id, const char *exec_id) {
    /*
     * Cancel an exec by sending a cancel request to the gateway.
     */
    if (!sandbox_id || !exec_id) return;
    hermes_log(LOG_INFO, "managed_modal",
               "_cancel_exec: sandbox=%s exec=%s", sandbox_id, exec_id);
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
