/* gate: gateway_health_classify_gateway_error — traced copy of
 * src/agent/health.c::gw_classify_gateway_error.
 * LIVE twin: agent/monitoring/gateway_health.py::classify_gateway_error.
 * Branch ladder (same rung order both sides):
 *   B1 auth/token/401/403   B2 rate+limit   B3 timeout
 *   B4 network family       B5 config/missing/invalid
 *   B6 startup              B7 fatal        V result
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

static const char *classify(const char *raw) {
    WCUP_BEGIN("classify_gateway_error");
    char buf[512];
    if (!raw) { buf[0] = '\0'; raw = ""; }
    size_t i = 0;
    for (; raw[i] && i < sizeof(buf) - 1; i++)
        buf[i] = (char)tolower((unsigned char)raw[i]);
    buf[i] = '\0';
    const char *s = buf;
    const char *r = NULL;

    if (strstr(s,"auth")||strstr(s,"token")||strstr(s,"unauthorized")||
        strstr(s,"forbidden")||strstr(s,"401")||strstr(s,"403")) {
        WCUP_BRANCH(1); r = "auth_failed";
    } else { WCUP_BRANCH(0);
    if (strstr(s,"rate") && strstr(s,"limit")) {
        WCUP_BRANCH(1); r = "rate_limited";
    } else { WCUP_BRANCH(0);
    if (strstr(s,"timeout")||strstr(s,"timed out")) {
        WCUP_BRANCH(1); r = "timeout";
    } else { WCUP_BRANCH(0);
    if (strstr(s,"network")||strstr(s,"connection")||strstr(s,"dns")||
        strstr(s,"socket")||strstr(s,"connect call failed")||
        strstr(s,"failed to connect")||strstr(s,"cannot connect")||
        strstr(s,"unreachable")||strstr(s,"name resolution")) {
        WCUP_BRANCH(1); r = "network_error";
    } else { WCUP_BRANCH(0);
    if (strstr(s,"config")||strstr(s,"missing")||strstr(s,"invalid")) {
        WCUP_BRANCH(1); r = "invalid_config";
    } else { WCUP_BRANCH(0);
    if (strstr(s,"startup")) { WCUP_BRANCH(1); r = "startup_failed"; }
    else { WCUP_BRANCH(0);
    if (strstr(s,"fatal")) { WCUP_BRANCH(1); r = "platform_fatal"; }
    else { WCUP_BRANCH(0); r = "unknown"; }
    }}}}}}
    char out[64]; snprintf(out, sizeof out, "%s", r);
    WCUP_VALSTR(out);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("classify_gateway_error");
    return r;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s classify_gateway_error <str>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) classify(argv[i]);
    return 0;
}
