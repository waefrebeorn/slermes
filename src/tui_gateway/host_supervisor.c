/*
 * host_supervisor.c — faithful C11 port of the deterministic decisions in
 * tui_gateway/host_supervisor.py.
 * PoP: w15_hostsuperv_6cf43d_is_compute_host_identity @ tui_gateway/host_supervisor.py:is_compute_host_identity
 * PoP: w15_hostsuperv_6cf43d_log_record_text         @ tui_gateway/host_supervisor.py:append_log_record
 * PoP: w15_hostsuperv_6cf43d_validate_hello          @ tui_gateway/host_supervisor.py:HostSupervisor._validate_hello
 *
 * Spawning, signalling and filesystem I/O stay in Python; the caller injects
 * the already-read command string and hello facts (see header).
 */
#include "noc_w15_hostsuperv_6cf43d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

int w15_hostsuperv_6cf43d_is_compute_host_identity(const char *command) {
    return command && strstr(command, "tui_gateway.compute_host") ? 1 : 0;
}

char *w15_hostsuperv_6cf43d_log_record_text(const char *record) {
    const char *s = record ? record : "";
    size_t n = strlen(s);
    if (n && s[n - 1] == '\n') return dupstr(s);
    char *p = (char *)malloc(n + 2);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\n';
    p[n + 1] = '\0';
    return p;
}

json_t *w15_hostsuperv_6cf43d_validate_hello(int hello_present,
        const char *hello_home, const char *hello_sha,
        const char *expected_home, const char *expected_sha) {
    const char *eh = expected_home ? expected_home : "";
    const char *es = expected_sha ? expected_sha : "";
    if (!hello_present) return json_string("compute host missing hello");
    const char *gh = hello_home ? hello_home : "";
    if (*gh && strcmp(gh, eh)) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "compute host HERMES_HOME mismatch: %s != %s", gh, eh);
        return json_string(buf);
    }
    const char *gs = hello_sha ? hello_sha : "";
    if (strcmp(es, "unknown") && *gs && strcmp(gs, "unknown") && strcmp(gs, es)) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "compute host build mismatch: %s != %s", gs, es);
        return json_string(buf);
    }
    return json_null();
}
