/*
 * run_turn_setup_guard.c - C home for upstream Hermes gateway/run_turn_setup_guard.py.
 * PoP: w16_runturnse_a91896_pending_setup_busy_notice @ gateway/run_turn_setup_guard.py:pending_setup_busy_notice
 *
 * Faithful port of the synchronous busy-ack decision. Environment values, the
 * clock, the notice renderer and the authority check are injected as explicit
 * seams from the fixture (the live oracle supplies the matching values), so no
 * process-global state is touched here.
 */
#include "hermes_json.h"
#include <math.h>
#include <stdio.h>
#include "noc_w16_runturnse_a91896.h"

struct json_t *w16_runturnse_a91896_pending_setup_busy_notice(int enabled, int allowed,
        double timeout, double started, double now, double busy_ack_ts,
        int agent_is_pending, int state_is_none) {
    char buf[64];
    long long minutes;

    /* env gate (HERMES_GATEWAY_BUSY_ACK_ENABLED) + bot-peer gate. */
    if (!enabled || !allowed) return json_null();
    /* timeout <= 0 disables; no state / no start / agent not pending -> silent. */
    if (timeout <= 0.0 || state_is_none || started == 0.0 || !agent_is_pending)
        return json_null();
    double age = now - started;
    /* Stall timeout not reached, or debounce window not elapsed -> silent. */
    if (age < timeout || now - busy_ack_ts < 30.0) return json_null();
    /* t("gateway.busy.setup_waiting", minutes=max(1, int(age // 60))). */
    minutes = (long long)floor(age / 60.0);
    if (minutes < 1) minutes = 1;
    snprintf(buf, sizeof(buf), "wait:%lld", minutes);
    return json_string(buf);
}
