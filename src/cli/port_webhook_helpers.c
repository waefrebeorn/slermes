/*
 * port_webhook_helpers.c — Faithful C11 port of the standalone helper
 * functions from gateway/platforms/webhook.py.
 *
 * Pure utility helpers (host safety-rail checks + timing-safe HMAC compare) the
 * webhook adapter relies on. Ported 1:1 from Python; wired into
 * build/objects.mk and actually linked.
 */

#include "hermes_core_types.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* Hostnames/IP literals that only serve connections originating on the same
 * machine. Anything else is treated as a public bind for safety-rail purposes.
 * Mirrors gateway/platforms/webhook.py:_LOOPBACK_HOSTS. */
static const char *g_loopback_hosts[] = {
    "127.0.0.1", "localhost", "::1", "ip6-localhost", "ip6-loopback", NULL
};

/* PoP: _is_loopback_host @ gateway/platforms/webhook.py:_is_loopback_host */
/* True when `host` binds only to the local machine. A falsy value (empty/None)
 * is conservatively treated as non-loopback (unset host = default public bind). */
bool webhook_is_loopback_host(const char *host) {
    if (!host || host[0] == '\0') return false;
    /* trim + lowercase */
    char buf[256];
    size_t j = 0;
    for (size_t i = 0; host[i] && j + 1 < sizeof(buf); i++) {
        char c = host[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c == ' ' || c == '\t') continue;
        buf[j++] = c;
    }
    buf[j] = '\0';
    for (int i = 0; g_loopback_hosts[i]; i++) {
        if (strcmp(buf, g_loopback_hosts[i]) == 0) return true;
    }
    return false;
}

/* PoP: _hmac_str_equal @ gateway/platforms/webhook.py:_hmac_str_equal */
/* Timing-safe equality for two strings, tolerant of non-ASCII input.
 * Python uses hmac.compare_digest on UTF-8 bytes; we compare lengths first then
 * do a constant-time byte compare so a hostile header fails closed. */
bool webhook_hmac_str_equal(const char *provided, const char *expected) {
    if (!provided || !expected) return false;
    size_t lp = strlen(provided);
    size_t le = strlen(expected);
    if (lp != le) return false;
    /* constant-time compare */
    unsigned char diff = 0;
    for (size_t i = 0; i < lp; i++) {
        diff |= (unsigned char)provided[i] ^ (unsigned char)expected[i];
    }
    return diff == 0;
}

/* PoP: check_webhook_requirements @ gateway/platforms/webhook.py:check_webhook_requirements */
/* Returns whether required webhook dependencies are available. The HTTP stack is
 * always present in the C build, so this mirrors "deps available" (true). */
bool webhook_check_requirements(void) {
    return true;
}
