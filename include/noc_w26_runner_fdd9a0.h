/*
 * noc_w26_runner_fdd9a0.h - C surface for upstream agent/verify/runner.py
 * (gate w26_runner_fdd9a0).
 *
 * The decision surface of the verification runner: how a phase's output is
 * tailed, what a PhaseResult/ReadinessResult/VerifyResult serializes to, when a
 * verify run counts as ok, the compose-live-state refusal (docker compose build
 * + up would replace live containers, #103567), and the refusal text itself.
 * The subprocess/HTTP/clock seams are fixed inputs here.
 */
#ifndef NOC_W26_RUNNER_FDD9A0_H
#define NOC_W26_RUNNER_FDD9A0_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W26_RUNNER_FDD9A0_PHASE_TIMEOUT 600.0
#define W26_RUNNER_FDD9A0_READY_TIMEOUT 60.0
#define W26_RUNNER_FDD9A0_TAIL_CHARS 2000

/* {"text": str, "limit": int|null} -> str   (runner._tail) */
json_t *w26_runner_fdd9a0_tail(const json_t *in);

/* {phase, command, exit_code|null, duration, output_tail, timed_out} ->
 * runner.PhaseResult.to_dict() */
json_t *w26_runner_fdd9a0_phase_result(const json_t *in);

/* {url, ready, status_code|null, duration, error|null, output_tail} ->
 * runner.ReadinessResult.to_dict() */
json_t *w26_runner_fdd9a0_readiness_result(const json_t *in);

/* {"phases": [...], "readiness": {...}|null} -> bool  (runner.VerifyResult.ok) */
json_t *w26_runner_fdd9a0_verify_ok(const json_t *in);

/* {"mode": "ok"|"missing"|"timeout", "returncode": int, "stdout": str, "stderr": str}
 * -> str | null   (runner._compose_live_state_reason) */
json_t *w26_runner_fdd9a0_compose_live_state_reason(const json_t *in);

/* {"reason": str, "build": [str]|null} -> runner.run_verify(...).to_dict() for a
 * compose recipe that refuses because of that reason. */
json_t *w26_runner_fdd9a0_verify_refusal(const json_t *in);

/* The module's literal budgets. */
json_t *w26_runner_fdd9a0_constants(void);

#ifdef __cplusplus
}
#endif
#endif
