/*
 * test_port_web_server_auth.c — Faithful behavior test for the
 * web_server auth-helper port (port_web_server_auth.c).
 *
 * Mirrors the Python semantics in hermes_cli/web_server.py for
 * _is_accepted_host / should_require_auth / _has_valid_session_token and
 * asserts the C port matches. Compiled as a standalone binary.
 */

#include "hermes_web_dashboard.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* For the standalone test, provide g_session_token (defined in
 * web_dashboard.c in the real build). */
char g_session_token[256] = "";

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok:   %s\n", msg); } \
} while (0)

int main(void) {
    /* Set a known session token so has_valid_session_token can validate. */
    strncpy(g_session_token, "test-token-123", sizeof(g_session_token) - 1);

    /* ── ws_should_require_auth ── */
    CHECK(ws_should_require_auth("localhost", false) == false,
          "loopback localhost -> no auth");
    CHECK(ws_should_require_auth("127.0.0.1", false) == false,
          "loopback 127.0.0.1 -> no auth");
    CHECK(ws_should_require_auth("::1", false) == false,
          "loopback ::1 -> no auth");
    CHECK(ws_should_require_auth("0.0.0.0", false) == true,
          "0.0.0.0 -> auth required");
    CHECK(ws_should_require_auth("192.168.1.5", false) == true,
          "RFC1918 -> auth required (allow_public ignored)");
    CHECK(ws_should_require_auth("example.com", true) == true,
          "public host with allow_public=true STILL requires auth");

    /* ── ws_is_accepted_host ── */
    CHECK(ws_is_accepted_host("localhost", "localhost") == true,
          "exact loopback match");
    CHECK(ws_is_accepted_host("localhost:9119", "localhost") == true,
          "loopback with port suffix");
    CHECK(ws_is_accepted_host("127.0.0.1", "localhost") == true,
          "loopback alias accepted when bound to loopback");
    CHECK(ws_is_accepted_host("evil.test", "localhost") == false,
          "DNS-rebinding host rejected (GHSA-ppp5-vxwm-4cf7)");
    CHECK(ws_is_accepted_host("0.0.0.0", "0.0.0.0") == true,
          "0.0.0.0 bind accepts any host");
    CHECK(ws_is_accepted_host("evil.test", "0.0.0.0") == true,
          "0.0.0.0 bind accepts any host (operator opt-in)");
    CHECK(ws_is_accepted_host("10.0.0.5", "10.0.0.5") == true,
          "exact non-loopback match");
    CHECK(ws_is_accepted_host("10.0.0.6", "10.0.0.5") == false,
          "non-loopback mismatch rejected");
    CHECK(ws_is_accepted_host("[::1]:9119", "localhost") == true,
          "IPv6 bracketed loopback alias accepted");
    CHECK(ws_is_accepted_host("[::1]", "localhost") == true,
          "IPv6 bracketed loopback (no port) accepted");
    CHECK(ws_is_accepted_host("", "localhost") == false,
          "empty host rejected");

    /* ── ws_has_valid_session_token ── */
    CHECK(ws_has_valid_session_token(
              "X-Hermes-Session-Token: test-token-123\r\n") == true,
          "valid dedicated session header accepted");
    CHECK(ws_has_valid_session_token(
              "x-hermes-session-token: test-token-123\r\n") == true,
          "case-insensitive session header name accepted");
    CHECK(ws_has_valid_session_token(
              "X-Hermes-Session-Token: test-token-123 extra\r\n") == false,
          "token with trailing junk rejected");
    CHECK(ws_has_valid_session_token(
              "Authorization: Bearer test-token-123\r\n") == true,
          "legacy Bearer form accepted");
    CHECK(ws_has_valid_session_token(
              "Authorization: Bearer wrong\r\n") == false,
          "wrong Bearer token rejected");
    CHECK(ws_has_valid_session_token(
              "X-Hermes-Session-Token: other\r\n") == false,
          "wrong session token rejected");

    if (failures == 0) {
        printf("\nALL AUTH HELPER TESTS PASSED\n");
        return 0;
    }
    printf("\n%d AUTH HELPER TESTS FAILED\n", failures);
    return 1;
}
