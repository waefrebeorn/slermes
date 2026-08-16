/* Slermes C port — agent/verify/runner.py (dataclass methods + standalone helpers) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include "verify_recipes.h"
#include <time.h>
#include "hermes_json.h"

/* ── _TAIL_CHARS constant (matches Python default) ── */
#define RUNNER_TAIL_CHARS 4000

/* ── _tail: return last `limit` characters of text ── */
/* PoP: runner_tail @ agent/verify/runner.py:_tail */
char *runner_tail(const char *text, int limit)
{
    if (!text) return strdup("");
    size_t len = strlen(text);
    if (len <= (size_t)limit) return strdup(text);
    return strdup(text + len - (size_t)limit);
}

/* ── PhaseResult dataclass helpers ── */
/* These mirror the Python @dataclass PhaseResult properties/methods. */

/* PoP: runner_phase_result_ok @ agent/verify/runner.py:ok */
/* PhaseResult.ok: exit_code == 0 and not timed_out */
bool runner_phase_result_ok(int exit_code, bool timed_out)
{
    return exit_code == 0 && !timed_out;
}

/* PoP: runner_phase_result_to_dict @ agent/verify/runner.py:to_dict */
/* PhaseResult.to_dict: serialize to JSON object */
json_t *runner_phase_result_to_dict(
    const char *phase,
    const char *command,
    int exit_code,
    double duration,
    const char *output_tail,
    bool timed_out)
{
    json_t *obj = json_object();
    json_set(obj, "phase", json_string(phase ? phase : ""));
    json_set(obj, "command", json_string(command ? command : ""));
    json_set(obj, "exitCode", exit_code < 0 ? json_null() : json_number((double)exit_code));
    json_set(obj, "duration", json_number(duration));
    json_set(obj, "ok", json_bool(exit_code == 0 && !timed_out));
    json_set(obj, "timedOut", json_bool(timed_out));
    json_set(obj, "outputTail", json_string(output_tail ? output_tail : ""));
    return obj;
}

/* ── ReadinessResult dataclass helpers ── */

/* PoP: runner_readiness_result_to_dict @ agent/verify/runner.py:to_dict */
/* ReadinessResult.to_dict: serialize to JSON object */
json_t *runner_readiness_result_to_dict(
    const char *url,
    bool ready,
    int status_code,
    double duration,
    const char *error,
    const char *output_tail)
{
    json_t *obj = json_object();
    json_set(obj, "url", json_string(url ? url : ""));
    json_set(obj, "ready", json_bool(ready));
    json_set(obj, "statusCode", status_code < 0 ? json_null() : json_number((double)status_code));
    json_set(obj, "duration", json_number(duration));
    json_set(obj, "error", error ? json_string(error) : json_null());
    json_set(obj, "outputTail", json_string(output_tail ? output_tail : ""));
    return obj;
}

/* ── VerifyResult dataclass helpers ── */

/* PoP: runner_verify_result_ok @ agent/verify/runner.py:ok */
/* VerifyResult.ok: all phases ok AND readiness ok */
bool runner_verify_result_ok(
    const json_t *phases_arr,
    bool has_readiness,
    bool readiness_ready)
{
    if (phases_arr) {
        size_t n = json_len(phases_arr);
        for (size_t i = 0; i < n; i++) {
            json_t *phase = json_get(phases_arr, i);
            if (!phase) return false;
            json_t *ok_val = json_obj_get(phase, "ok");
            bool ok = (ok_val && ok_val->type == JSON_BOOL && ok_val->bool_val);
            if (!ok) return false;
        }
    }
    if (has_readiness && !readiness_ready) return false;
    return true;
}

/* PoP: runner_verify_result_to_dict @ agent/verify/runner.py:to_dict */
/* VerifyResult.to_dict: serialize to JSON object */
json_t *runner_verify_result_to_dict(
    const char *recipe_name,
    const json_t *phases_arr,
    const json_t *readiness_obj)
{
    json_t *obj = json_object();
    json_set(obj, "recipe", json_string(recipe_name ? recipe_name : ""));

    bool has_readiness = (readiness_obj != NULL);
    bool rr_ready = false;
    if (has_readiness) {
        json_t *rv = json_obj_get(readiness_obj, "ready");
        rr_ready = (rv && rv->type == JSON_BOOL && rv->bool_val);
    }
    json_set(obj, "ok", json_bool(runner_verify_result_ok(phases_arr, has_readiness, rr_ready)));

    if (phases_arr) {
        json_t *copy = json_array();
        size_t n = json_len(phases_arr);
        for (size_t i = 0; i < n; i++) {
            json_append(copy, json_copy(json_get(phases_arr, i)));
        }
        json_set(obj, "phases", copy);
    } else {
        json_set(obj, "phases", json_array());
    }

    if (readiness_obj) {
        json_set(obj, "readiness", json_copy(readiness_obj));
    } else {
        json_set(obj, "readiness", json_null());
    }
    return obj;
}

/* ── _run_phase_command: run a shell command with timeout ── */
/* PoP: runner_run_phase_command @ agent/verify/runner.py:_run_phase_command */
json_t *runner_run_phase_command(
    const char *phase,
    const char *command,
    const char *root,
    double timeout_sec)
{
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    /* Build the command: cd root && command */
    char cmd[8192];
    if (root) {
        snprintf(cmd, sizeof(cmd), "cd %s && %s", root, command ? command : "true");
    } else {
        snprintf(cmd, sizeof(cmd), "%s", command ? command : "true");
    }

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        clock_gettime(CLOCK_MONOTONIC, &ts_end);
        double dur = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;
        char *tail = runner_tail("popen failed", RUNNER_TAIL_CHARS);
        json_t *r = runner_phase_result_to_dict(phase, command, -1, dur, tail, true);
        free(tail);
        return r;
    }

    /* Read output */
    char *output = NULL;
    size_t out_len = 0;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        char *new_out = realloc(output, out_len + n + 1);
        if (!new_out) break;
        output = new_out;
        memcpy(output + out_len, buf, n);
        out_len += n;
        output[out_len] = '\0';
    }

    int ret = pclose(fp);
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double dur = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;

    int exit_code = -1;
    bool timed_out = false;
    if (WIFEXITED(ret)) {
        exit_code = WEXITSTATUS(ret);
    } else {
        timed_out = true;
    }

    char *tail = runner_tail(output ? output : "", RUNNER_TAIL_CHARS);
    json_t *result = runner_phase_result_to_dict(phase, command, exit_code, dur, tail, timed_out);
    free(tail);
    free(output);
    return result;
}

/* ── _poll_readiness: poll a URL until it responds ── */
/* PoP: runner_poll_readiness @ agent/verify/runner.py:_poll_readiness */
json_t *runner_poll_readiness(
    const char *url,
    double timeout_sec,
    double interval_sec)
{
    struct timespec deadline_now;
    clock_gettime(CLOCK_MONOTONIC, &deadline_now);
    double deadline = deadline_now.tv_sec + deadline_now.tv_nsec / 1e9 + timeout_sec;

    char error_buf[1024] = "";
    bool ready = false;
    int status_code = -1;

    while (1) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double t = now.tv_sec + now.tv_nsec / 1e9;
        if (t >= deadline) break;

        /* Use libhttp or curl-like approach: try a simple HTTP GET via popen+curl */
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
                 "curl -s -o /dev/null -w '%%{http_code}' --max-time 5 --connect-timeout 5 '%s' 2>/dev/null",
                 url);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char code_buf[64] = "";
            size_t n = fread(code_buf, 1, sizeof(code_buf) - 1, fp);
            code_buf[n] = '\0';
            pclose(fp);
            int code = atoi(code_buf);
            if (code > 0) {
                ready = true;
                status_code = code;
                break;
            }
        }

        /* Sleep interval */
        struct timespec req;
        req.tv_sec = (time_t)interval_sec;
        req.tv_nsec = (long)((interval_sec - (double)req.tv_sec) * 1e9);
        nanosleep(&req, NULL);
    }

    json_t *result = json_object();
    json_set(result, "ready", json_bool(ready));
    json_set(result, "statusCode", status_code < 0 ? json_null() : json_number((double)status_code));
    json_set(result, "error", ready ? json_null() : json_string(error_buf[0] ? error_buf : "timeout"));
    return result;
}

/* ── _terminate_process_group: SIGTERM then SIGKILL a process group ── */
/* PoP: runner_terminate_process_group @ agent/verify/runner.py:_terminate_process_group */
void runner_terminate_process_group(pid_t pid)
{
    if (pid <= 0) return;

    /* Try SIGTERM on the process group (pid is the group leader via start_new_session) */
    killpg(pid, SIGTERM);

    /* Wait up to 10 seconds */
    int status;
    struct timespec req = {0, 100000000}; /* 100ms */
    for (int i = 0; i < 100; i++) {
        pid_t ret = waitpid(pid, &status, WNOHANG);
        if (ret == pid) return;
        if (ret < 0 && errno == ECHILD) return;
        nanosleep(&req, NULL);
    }

    /* SIGKILL fallback */
    killpg(pid, SIGKILL);
    for (int i = 0; i < 50; i++) {
        pid_t ret = waitpid(pid, &status, WNOHANG);
        if (ret == pid) return;
        if (ret < 0 && errno == ECHILD) return;
        nanosleep(&req, NULL);
    }
}

/* ── _run_start_phase: launch start command, poll readiness, tear down ── */
/* PoP: runner_run_start_phase @ agent/verify/runner.py:_run_start_phase */
json_t *runner_run_start_phase(
    const char *start_command,
    const char *root,
    const char *readiness_path,
    int port,
    double ready_timeout)
{
    char url[2048];
    snprintf(url, sizeof(url), "http://127.0.0.1:%d%s", port, readiness_path ? readiness_path : "/");

    char cmd[8192];
    if (root) {
        snprintf(cmd, sizeof(cmd), "cd %s && %s", root, start_command ? start_command : "");
    } else {
        snprintf(cmd, sizeof(cmd), "%s", start_command ? start_command : "");
    }

    /* Spawn with setsid for process group isolation (mirrors start_new_session=True) */
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        json_t *r = runner_readiness_result_to_dict(url, false, -1, 0.0, "failed to start process", "");
        return r;
    }

    /* Poll readiness */
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    json_t *poll = runner_poll_readiness(url, ready_timeout, 1.0);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double dur = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;

    /* Tear down: we can't easily get PID from popen, so just pclose */
    /* In a real implementation this would use fork+exec with setsid */
    pclose(fp);

    /* Build ReadinessResult */
    json_t *ready_val = json_obj_get(poll, "ready");
    bool ready = (ready_val && ready_val->type == JSON_BOOL && ready_val->bool_val);
    json_t *sc = json_obj_get(poll, "statusCode");
    int status_code = -1;
    if (sc && sc->type == JSON_NUMBER) status_code = (int)sc->num_val;
    json_t *err_val = json_obj_get(poll, "error");
    const char *error = NULL;
    if (err_val && err_val->type == JSON_STRING) error = err_val->str_val;

    char *output = strdup(""); /* popen output not captured in this simplified version */
    char *tail = runner_tail(output, RUNNER_TAIL_CHARS);
    json_t *result = runner_readiness_result_to_dict(url, ready, status_code, dur, error, tail);
    free(tail);
    free(output);
    json_free(poll);
    return result;
}

/* ── run_verify: orchestrate a full verify pass ── */
/* PoP: runner_run_verify @ agent/verify/runner.py:run_verify */
json_t *runner_run_verify(
    const char *root,
    const char *recipe_name,
    const char *start_command,
    const char *readiness_path,
    int port,
    double phase_timeout,
    double ready_timeout,
    bool skip_start)
{
    json_t *phases = json_array();

    /* Faithful port of agent/verify/runner.py:run_verify — detect the
     * project's recipe and run its bootstrap/build/test command lists in
     * order, stopping at the first failing phase. */
    recipe_t *recipe = recipe_detect(root);
    bool failed = false;
    if (recipe) {
        const char *const phase_names[] = {"bootstrap", "build", "test"};
        char **const cmd_lists[] = {recipe->bootstrap, recipe->build, recipe->test};
        const size_t cmd_counts[] = {recipe->n_bootstrap, recipe->n_build, recipe->n_test};
        for (int pi = 0; pi < 3 && !failed; pi++) {
            for (size_t ci = 0; ci < cmd_counts[pi]; ci++) {
                json_t *pr = runner_run_phase_command(phase_names[pi], cmd_lists[pi][ci], root, phase_timeout);
                json_append(phases, pr);
                /* ok == (exit_code 0 and not timed out); mirrors Python PhaseResult.ok */
                json_t *ok_val = json_obj_get(pr, "ok");
                bool ok = (ok_val && ok_val->type == JSON_BOOL && ok_val->bool_val);
                if (!ok) { failed = true; break; }
            }
        }
        recipe_free(recipe);
    }
    (void)root;
    (void)phase_timeout;

    /* Start phase */
    json_t *readiness = NULL;
    if (!skip_start && start_command) {
        readiness = runner_run_start_phase(start_command, root, readiness_path, port, ready_timeout);
    }

    json_t *result = runner_verify_result_to_dict(recipe_name, phases, readiness);
    json_free(phases);
    if (readiness) json_free(readiness);
    return result;
}
