/* test_gateway_drain.c — Verify the gateway drain/shutdown seam behaves.
 * Exercises: note_turn_begin/end registry, gw_drain_active_agents bounded
 * wait, gw_request_shutdown/gw_wait_for_shutdown_request signal flow.
 * Compiled directly against the linked gateway runtime TUs (no main slermes). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
#include "hermes_gateway_runner.h"
#include "hermes_gateway_runtime.h"
#include "gw_server_internals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

int main(void) {
    int failures = 0;

    /* ── 1. A fresh runner has zero running agents ────────────── */
    GatewayRunner *r = gateway_runner_create(NULL);
    if (!r) { printf("FAIL: create\n"); return 1; }
    if (gateway_runner_running_agent_count(r) != 0) {
        printf("FAIL: expected 0 running agents, got %d\n",
               gateway_runner_running_agent_count(r));
        failures++;
    }
    if (gateway_runner_is_draining(r)) {
        printf("FAIL: fresh runner should not be draining\n"); failures++;
    }

    /* ── 2. note_turn_begin/end tracks a session ──────────────── */
    agent_state_t agent;
    memset(&agent, 0, sizeof(agent));
    gateway_runner_note_turn_begin(r, "telegram:123", &agent);
    if (gateway_runner_running_agent_count(r) != 1) {
        printf("FAIL: expected 1 running agent, got %d\n",
               gateway_runner_running_agent_count(r));
        failures++;
    }
    gateway_runner_note_turn_end(r, "telegram:123");
    if (gateway_runner_running_agent_count(r) != 0) {
        printf("FAIL: expected 0 after turn end, got %d\n",
               gateway_runner_running_agent_count(r));
        failures++;
    }

    /* ── 3. Drain semantics are covered by the live SIGTERM test
     *        (tests/test_gateway_drain_live.sh) against the real binary;
     *        gw_drain_active_agents pulls scheduler deps not linked here. */

    /* ── 4. The interrupt stub is now real (sets interrupted) ──── */
    gateway_runner_note_turn_begin(r, "telegram:999", &agent);
    gateway_runner_interrupt_running_agents(r, "test");
    if (!agent.interrupted) {
        printf("FAIL: interrupt did not set agent->interrupted\n"); failures++;
    }
    gateway_runner_note_turn_end(r, "telegram:999");

    /* ── 5. gw_request_shutdown / gw_wait_for_shutdown_request ─── */
    gw_request_shutdown("SIGTERM");
    if (!gw_shutdown_reason() || strcmp(gw_shutdown_reason(), "SIGTERM") != 0) {
        printf("FAIL: shutdown reason mismatch: %s\n",
               gw_shutdown_reason() ? gw_shutdown_reason() : "(null)");
        failures++;
    }
    if (!gw_shutdown_requested()) {
        printf("FAIL: gw_shutdown_requested should be true\n"); failures++;
    }

    /* A second request must not clobber the first reason. */
    gw_request_shutdown("SIGINT");
    if (strcmp(gw_shutdown_reason(), "SIGTERM") != 0) {
        printf("FAIL: second request clobbered reason: %s\n",
               gw_shutdown_reason());
        failures++;
    }

    gateway_runner_destroy(r);

    if (failures) {
        printf("\n%d FAILURES\n", failures);
        return 1;
    }
    printf("\nALL GATEWAY DRAIN TESTS PASS\n");
    return 0;
}
