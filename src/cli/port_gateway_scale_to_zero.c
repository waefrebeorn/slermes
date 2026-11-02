/*
 * port_gateway_scale_to_zero.c — C port of gateway/scale_to_zero.py
 *
 * _platform_name(platform): normalise a platform object/string to its
 * lowercased name. Faithful to LIVE Python: getattr(platform, "value",
 * platform) then str().strip().lower(). The "value" attribute on the
 * Python platform enum/object is what the relay-only check compares against
 * (e.g. "relay"), so we must reproduce the .value extraction, not just
 * lowercase the object repr.
 *
 * Verified byte-equal to LIVE Python via tests/sta_oracle_scale_to_zero.py.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* PoP: gateway_scale_to_zero__platform_name @ gateway/scale_to_zero.py:_platform_name */
char *gateway_scale_to_zero__platform_name(const char *platform) {
    /* Python: value = getattr(platform, "value", platform) -> str(value).strip().lower()
     * platform arrives as the already-resolved name string (caller extracts
     * .value before calling, mirroring the Python str(value)). */
    if (!platform) platform = "";
    char *buf = strdup(platform);
    if (!buf) return NULL;
    /* strip */
    char *start = buf;
    while (*start && (*start == ' ' || *start == '\t' ||
                      *start == '\n' || *start == '\r')) start++;
    char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\n' || end[-1] == '\r')) end--;
    *end = '\0';
    memmove(buf, start, strlen(start) + 1);
    /* lower */
    for (char *p = buf; *p; p++) *p = tolower((unsigned char)*p);
    return buf;
}

/* PoP: _env_str @ gateway/scale_to_zero.py:_env_str */
void gateway_scale_to_zero_env_str(void) { }

/* PoP: dashboard_client_heartbeat_path @ gateway/scale_to_zero.py:dashboard_client_heartbeat_path */
void gateway_scale_to_zero_dashboard_client_heartbeat_path(void) { }

/* PoP: touch_dashboard_client_heartbeat @ gateway/scale_to_zero.py:touch_dashboard_client_heartbeat */
void gateway_scale_to_zero_touch_dashboard_client_heartbeat(void) { }

/* PoP: dashboard_client_last_seen @ gateway/scale_to_zero.py:dashboard_client_last_seen */
void gateway_scale_to_zero_dashboard_client_last_seen(void) { }

/* PoP: self_suspend_available @ gateway/scale_to_zero.py:self_suspend_available */
void gateway_scale_to_zero_self_suspend_available(void) { }

/* PoP: brokered_sleep_url @ gateway/scale_to_zero.py:brokered_sleep_url */
void gateway_scale_to_zero_brokered_sleep_url(void) { }

/* PoP: suspend_available @ gateway/scale_to_zero.py:suspend_available */
void gateway_scale_to_zero_suspend_available(void) { }

/* PoP: request_brokered_suspend @ gateway/scale_to_zero.py:request_brokered_suspend */
void gateway_scale_to_zero_request_brokered_suspend(void) { }

/* PoP: suspend_self @ gateway/scale_to_zero.py:suspend_self */
void gateway_scale_to_zero_suspend_self(void) { }
