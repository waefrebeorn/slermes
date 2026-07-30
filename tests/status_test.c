/*
 * status_test.c — Real-behavior tests for src/gateway/status.c
 * (C11 port of gateway/status.py).
 *
 * Exercises the port against a temp SLERMES_HOME with a live/dead PID and
 * asserts the faithful Python semantics: PID-file liveness detection, the
 * cmdline gateway-identity matcher, runtime-status JSON round-trips, scope
 * locks, and the takeover / planned-stop marker protocol.
 *
 * Build (from slermes/):
 *   gcc -O2 -Wall -Wextra -D_DEFAULT_SOURCE -D_GNU_SOURCE \
 *     -I include -I lib -I lib/libjson -I lib/libhash \
 *     tests/status_test.c src/gateway/status.c src/slermes_home.o \
 *     lib/libjson/json.o lib/libhash/hash.o -lssl -lcrypto -o /tmp/status_test
 */

#include "gateway_status.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ok: %s\n", msg); } \
    else { printf("  FAIL: %s\n", msg); g_fail++; } \
} while (0)

int main(void) {
    /* Isolated home. */
    char tmpl[] = "/tmp/slermes_status_test_XXXXXX";
    char *home = mkdtemp(tmpl);
    if (!home) { perror("mkdtemp"); return 2; }
    setenv("SLERMES_HOME", home, 1);
    setenv("HERMES_GATEWAY_LOCK_DIR", home, 1);  /* keep scope locks in temp */
    printf("SLERMES_HOME=%s\n", home);

    /* ── cmdline gateway-identity matcher ─────────────────────────── */
    printf("[cmdline identity]\n");
    CHECK(gwstatus_looks_like_gateway_command_line(
              "python -m hermes_cli.main gateway run"),
          "`hermes_cli.main gateway run` looks like gateway");
    CHECK(gwstatus_looks_like_gateway_command_line(
              "/usr/bin/hermes gateway run --replace"),
          "`hermes gateway run --replace` looks like gateway");
    CHECK(!gwstatus_looks_like_gateway_command_line(
              "python -m hermes_cli.main gateway status"),
          "`gateway status` is NOT a gateway run");
    CHECK(!gwstatus_looks_like_gateway_command_line("vim notes.txt"),
          "unrelated process is not a gateway");
    CHECK(gwstatus_looks_like_gateway_command_line(
              "hermes --profile work gateway run"),
          "profile selector is stripped before subcommand");
    CHECK(gwstatus_looks_like_gateway_runtime_command_line(
              "hermes gateway restart"),
          "`gateway restart` counts as runtime cmdline");
    CHECK(!gwstatus_looks_like_gateway_command_line(
              "hermes gateway restart"),
          "`gateway restart` is NOT a plain run");

    /* ── parse_active_agents ──────────────────────────────────────── */
    printf("[parse_active_agents]\n");
    CHECK(gwstatus_parse_active_agents_str("5") == 5, "\"5\" -> 5");
    CHECK(gwstatus_parse_active_agents_str("-3") == 0, "\"-3\" clamps to 0");
    CHECK(gwstatus_parse_active_agents_str("abc") == 0, "\"abc\" -> 0");
    CHECK(gwstatus_parse_active_agents_str("  7  ") == 7, "whitespace \" 7 \" -> 7");
    CHECK(gwstatus_parse_active_agents_str(NULL) == 0, "NULL -> 0");

    /* ── derive_gateway_busy / drainable ──────────────────────────── */
    printf("[derive busy/drainable]\n");
    CHECK(gwstatus_derive_gateway_busy(true, "running", 2), "running+2 agents = busy");
    CHECK(!gwstatus_derive_gateway_busy(true, "running", 0), "running+0 agents = not busy");
    CHECK(!gwstatus_derive_gateway_busy(false, "running", 5), "not running = not busy");
    CHECK(!gwstatus_derive_gateway_busy(true, "draining", 5), "draining = not busy");
    CHECK(gwstatus_derive_gateway_drainable(true, "running"), "running = drainable");
    CHECK(!gwstatus_derive_gateway_drainable(true, "draining"), "draining = not drainable");
    CHECK(!gwstatus_derive_gateway_drainable(false, "running"), "down = not drainable");

    /* ── process start-time + liveness ────────────────────────────── */
    printf("[process fingerprint]\n");
    long self_start = gwstatus_get_process_start_time(getpid());
    CHECK(self_start > 0, "our own start_time is readable");
    CHECK(gwstatus_pid_exists(getpid()), "our own pid exists");
    CHECK(!gwstatus_pid_exists(999999), "pid 999999 does not exist");

    /* ── runtime status JSON round-trip ───────────────────────────── */
    printf("[runtime status json]\n");
    int rc = gwstatus_write_runtime_status("running", NULL, -1, 3,
                                           "telegram", "connected", NULL, NULL);
    CHECK(rc == 0, "write_runtime_status succeeded");
    char *ser = gwstatus_read_runtime_status(NULL);
    CHECK(ser != NULL, "read_runtime_status returned data");
    if (ser) {
        json_t *o = json_parse(ser, NULL);
        CHECK(o != NULL, "runtime status parses as JSON");
        if (o) {
            CHECK(strcmp(json_get_str(o, "gateway_state", ""), "running") == 0,
                  "gateway_state == running");
            CHECK((int)json_get_num(o, "active_agents", -1) == 3,
                  "active_agents == 3");
            json_t *plats = json_obj_get(o, "platforms");
            CHECK(plats && json_obj_get(plats, "telegram"),
                  "platforms.telegram sub-record written");
            json_t *tg = plats ? json_obj_get(plats, "telegram") : NULL;
            CHECK(tg && strcmp(json_get_str(tg, "state", ""), "connected") == 0,
                  "telegram.state == connected");
            json_free(o);
        }
        free(ser);
    }
    /* Partial update leaves prior fields intact. */
    gwstatus_write_runtime_status(NULL, NULL, -1, 9, NULL, NULL, NULL, NULL);
    ser = gwstatus_read_runtime_status(NULL);
    if (ser) {
        json_t *o = json_parse(ser, NULL);
        CHECK(o && strcmp(json_get_str(o, "gateway_state", ""), "running") == 0,
              "gateway_state preserved across partial update");
        CHECK(o && (int)json_get_num(o, "active_agents", -1) == 9,
              "active_agents updated to 9");
        if (o) json_free(o);
        free(ser);
    }

    /* ── PID file liveness ────────────────────────────────────────── */
    printf("[pid file]\n");
    /* No pid file yet. */
    CHECK(gwstatus_get_running_pid(NULL, false) == -1, "no pid file => -1");
    /* Write our own pid record; but our cmdline is the test binary, not a
     * gateway, so identity guard should reject it. */
    gwstatus_write_pid_file();
    CHECK(gwstatus_get_running_pid(NULL, false) == -1,
          "test binary pid rejected by gateway-identity guard");
    gwstatus_remove_pid_file();

    /* ── scope locks ──────────────────────────────────────────────── */
    printf("[scope locks]\n");
    char *existing = NULL;
    bool got = gwstatus_acquire_scoped_lock("telegram", "bot-token-abc",
                                            "{\"note\":\"t\"}", &existing);
    CHECK(got, "first scope lock acquire succeeds");
    /* Re-acquire same identity from same process => still true (refresh). */
    char *existing2 = NULL;
    bool got2 = gwstatus_acquire_scoped_lock("telegram", "bot-token-abc",
                                             NULL, &existing2);
    CHECK(got2, "re-acquire own scope lock succeeds");
    free(existing); free(existing2);
    /* Release and re-acquire. */
    gwstatus_release_scoped_lock("telegram", "bot-token-abc");
    char *e3 = NULL;
    bool got3 = gwstatus_acquire_scoped_lock("telegram", "bot-token-abc", NULL, &e3);
    CHECK(got3, "acquire after release succeeds");
    free(e3);
    int removed = gwstatus_release_all_scoped_locks(getpid(), self_start);
    CHECK(removed >= 1, "release_all_scoped_locks removed our lock");

    /* ── markers ──────────────────────────────────────────────────── */
    printf("[markers]\n");
    /* Planned-stop marker naming ourselves => consumed as targeting self. */
    CHECK(gwstatus_write_planned_stop_marker(getpid()),
          "write planned-stop marker for self");
    CHECK(gwstatus_planned_stop_marker_targets_self(),
          "planned-stop marker targets self (non-destructive)");
    CHECK(gwstatus_consume_planned_stop_marker_for_self(),
          "consume planned-stop marker for self");
    CHECK(!gwstatus_planned_stop_marker_targets_self(),
          "marker gone after consume");
    /* Marker for another pid => not self. */
    CHECK(gwstatus_write_planned_stop_marker(999998),
          "write planned-stop marker for other pid");
    CHECK(!gwstatus_consume_planned_stop_marker_for_self(),
          "marker for other pid not consumed as self");
    gwstatus_clear_planned_stop_marker();

    /* Takeover marker naming ourselves. */
    CHECK(gwstatus_write_takeover_marker(getpid()),
          "write takeover marker for self");
    CHECK(gwstatus_consume_takeover_marker_for_self(),
          "consume takeover marker for self");
    CHECK(!gwstatus_consume_takeover_marker_for_self(),
          "takeover marker gone after consume");

    /* ── runtime lock ─────────────────────────────────────────────── */
    printf("[runtime lock]\n");
    CHECK(!gwstatus_is_gateway_runtime_lock_active(NULL),
          "runtime lock not active initially");
    CHECK(gwstatus_acquire_gateway_runtime_lock(),
          "acquire runtime lock");
    CHECK(gwstatus_is_gateway_runtime_lock_active(NULL),
          "runtime lock active after acquire");
    gwstatus_release_gateway_runtime_lock();

    /* ── cleanup ──────────────────────────────────────────────────── */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", home);
    if (system(cmd) != 0) fprintf(stderr, "cleanup warning\n");

    printf("\n%s (%d failures)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED", g_fail);
    return g_fail ? 1 : 0;
}
