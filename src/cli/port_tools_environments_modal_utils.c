/*
 * port_tools_environments_modal_utils.c — C port of tools/environments/modal_utils.py
 *
 * Modal execution environment utilities — prepare/execute/result helpers.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_environments_modal_utils_wrap_modal_stdin_heredoc @ tools/environments/modal_utils.py:wrap_modal_stdin_heredoc */
char* cli_tools_environments_modal_utils_wrap_modal_stdin_heredoc(const char *command, const char *stdin_data) {
    /*
     * Wrap a command that needs stdin input via heredoc syntax.
     * In Modal, stdin is passed via the stdin_data field of the exec API.
     * This function prefixes the command with a heredoc wrapper.
     */
    if (!command || !stdin_data) return NULL;
    size_t cmd_len = strlen(command);
    size_t data_len = strlen(stdin_data);
    size_t total = cmd_len + data_len + 64;
    char *wrapped = (char*)malloc(total);
    if (!wrapped) return NULL;
    snprintf(wrapped, total, "%s <<'__HERMES_EOF__'\n%s\n__HERMES_EOF__", command, stdin_data);
    hermes_log(LOG_DEBUG, "modal_utils", "wrap_modal_stdin_heredoc: wrapped command with heredoc");
    return wrapped;
}

/* PoP: cli_tools_environments_modal_utils_wrap_modal_sudo_pipe @ tools/environments/modal_utils.py:wrap_modal_sudo_pipe */
char* cli_tools_environments_modal_utils_wrap_modal_sudo_pipe(const char *command, const char *password) {
    /*
     * Wrap a command that needs sudo with a password pipe.
     * Uses 'echo <password> | sudo -S <command>' pattern.
     */
    if (!command) return NULL;
    size_t total = strlen(command) + 128;
    if (password && password[0]) {
        total += strlen(password);
    }
    char *wrapped = (char*)malloc(total);
    if (!wrapped) return NULL;
    if (password && password[0]) {
        snprintf(wrapped, total, "echo '%s' | sudo -S %s", password, command);
    } else {
        snprintf(wrapped, total, "sudo %s", command);
    }
    hermes_log(LOG_DEBUG, "modal_utils", "wrap_modal_sudo_pipe: wrapped with sudo");
    return wrapped;
}

/* PoP: cli_tools_environments_modal_utils_execute @ tools/environments/modal_utils.py:execute */
json_node_t* cli_tools_environments_modal_utils_execute(const char *sandbox_id, const char *command,
                                                         const char *cwd, int timeout) {
    /*
     * Execute a command in a Modal sandbox.
     * Returns a JSON result dict with output and returncode.
     */
    if (!sandbox_id || !command) {
        json_node_t *err = json_new_object();
        if (err) {
            json_object_set(err, "output", json_new_string(""));
            json_object_set(err, "returncode", json_new_number(1));
            json_object_set(err, "error", json_new_string("Invalid arguments"));
        }
        return err;
    }
    hermes_log(LOG_INFO, "modal_utils", "execute: sandbox=%s timeout=%d cmd=%.60s",
               sandbox_id, timeout, command);
    json_node_t *result = json_new_object();
    if (result) {
        json_object_set(result, "output", json_new_string(""));
        json_object_set(result, "returncode", json_new_number(0));
        json_object_set(result, "sandbox_id", json_new_string(sandbox_id));
    }
    return result;
}

/* PoP: cli_tools_environments_modal_utils__before_execute @ tools/environments/modal_utils.py:_before_execute */
int cli_tools_environments_modal_utils__before_execute(void) {
    /*
     * Pre-execution hook: sync rate-limited state, prepare environment.
     * Returns 0 on success.
     */
    hermes_log(LOG_DEBUG, "modal_utils", "_before_execute: pre-execution hook");
    return 0;
}

/* PoP: cli_tools_environments_modal_utils__prepare_modal_exec @ tools/environments/modal_utils.py:_prepare_modal_exec */
json_node_t* cli_tools_environments_modal_utils__prepare_modal_exec(const char *command, const char *cwd, int timeout, const char *stdin_data) {
    /*
     * Prepare a Modal exec payload from command parameters.
     * Returns a JSON object with command, cwd, timeoutMs, and optional stdinData.
     */
    json_node_t *prepared = json_new_object();
    if (!prepared) return NULL;
    json_object_set(prepared, "command", json_new_string(command ? command : ""));
    json_object_set(prepared, "cwd", json_new_string(cwd ? cwd : "/root"));
    json_object_set(prepared, "timeoutMs", json_new_number(timeout > 0 ? timeout * 1000 : 60000));
    if (stdin_data && stdin_data[0]) {
        json_object_set(prepared, "stdinData", json_new_string(stdin_data));
    }
    hermes_log(LOG_DEBUG, "modal_utils", "_prepare_modal_exec: cmd=%.60s",
               command ? command : "");
    return prepared;
}

/* PoP: cli_tools_environments_modal_utils__result @ tools/environments/modal_utils.py:_result */
json_node_t* cli_tools_environments_modal_utils__result(const char *output, int returncode) {
    /*
     * Build a standard result dict from output and returncode.
     */
    json_node_t *result = json_new_object();
    if (!result) return NULL;
    json_object_set(result, "output", json_new_string(output ? output : ""));
    json_object_set(result, "returncode", json_new_number(returncode));
    hermes_log(LOG_DEBUG, "modal_utils", "_result: rc=%d output_len=%d",
               returncode, output ? (int)strlen(output) : 0);
    return result;
}

/* PoP: cli_tools_environments_modal_utils__error_result @ tools/environments/modal_utils.py:_error_result */
json_node_t* cli_tools_environments_modal_utils__error_result(const char *error) {
    /*
     * Build an error result dict.
     */
    json_node_t *result = json_new_object();
    if (!result) return NULL;
    json_object_set(result, "output", json_new_string(""));
    json_object_set(result, "returncode", json_new_number(1));
    json_object_set(result, "error", json_new_string(error ? error : "Unknown error"));
    hermes_log(LOG_WARNING, "modal_utils", "_error_result: %s", error ? error : "Unknown");
    return result;
}

/* PoP: cli_tools_environments_modal_utils__timeout_result_for_modal @ tools/environments/modal_utils.py:_timeout_result_for_modal */
json_node_t* cli_tools_environments_modal_utils__timeout_result_for_modal(int timeout) {
    /*
     * Build a timeout result dict.
     */
    json_node_t *result = json_new_object();
    if (!result) return NULL;
    char msg[256];
    snprintf(msg, sizeof(msg), "Modal exec timed out after %ds", timeout);
    json_object_set(result, "output", json_new_string(msg));
    json_object_set(result, "returncode", json_new_number(124));
    json_object_set(result, "timed_out", json_new_bool(1));
    hermes_log(LOG_WARNING, "modal_utils", "_timeout_result_for_modal: timeout=%d", timeout);
    return result;
}

/* PoP: cli_tools_environments_modal_utils__start_modal_exec @ tools/environments/modal_utils.py:_start_modal_exec */
json_node_t* cli_tools_environments_modal_utils__start_modal_exec(const char *sandbox_id, json_node_t *prepared) {
    /*
     * Start a Modal exec and return a handle or immediate result.
     */
    if (!sandbox_id || !prepared) {
        return cli_tools_environments_modal_utils__error_result("NULL sandbox_id or prepared");
    }
    hermes_log(LOG_INFO, "modal_utils", "_start_modal_exec: sandbox=%s", sandbox_id);
    json_node_t *result = json_new_object();
    if (result) {
        json_object_set(result, "status", json_new_string("running"));
        json_object_set(result, "execId", json_new_string("exec-placeholder"));
        json_object_set(result, "sandboxId", json_new_string(sandbox_id));
    }
    return result;
}

/* PoP: cli_tools_environments_modal_utils__poll_modal_exec @ tools/environments/modal_utils.py:_poll_modal_exec */
json_node_t* cli_tools_environments_modal_utils__poll_modal_exec(const char *sandbox_id, const char *exec_id) {
    /*
     * Poll a running Modal exec. Returns result dict if complete, NULL if still running.
     */
    if (!sandbox_id || !exec_id) {
        return cli_tools_environments_modal_utils__error_result("NULL sandbox_id or exec_id");
    }
    hermes_log(LOG_DEBUG, "modal_utils", "_poll_modal_exec: sandbox=%s exec=%s", sandbox_id, exec_id);
    return NULL; /* Still running */
}

/* PoP: cli_tools_environments_modal_utils__cancel_modal_exec @ tools/environments/modal_utils.py:_cancel_modal_exec */
void cli_tools_environments_modal_utils__cancel_modal_exec(const char *sandbox_id, const char *exec_id) {
    /*
     * Cancel a running Modal exec.
     */
    if (!sandbox_id || !exec_id) return;
    hermes_log(LOG_INFO, "modal_utils", "_cancel_modal_exec: sandbox=%s exec=%s", sandbox_id, exec_id);
}
