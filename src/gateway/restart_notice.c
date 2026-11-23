/*
 * restart_notice.c — C port of upstream Hermes gateway/restart_notice.py
 * (gate w8_restartnoti_29f67a).
 *
 * PoP: restartnoti_planned_restart_marker_path @ gateway/restart_notice.py:planned_restart_marker_path
 * PoP: restartnoti_marker_payload @ gateway/restart_notice.py:write_planned_restart_marker
 */
#include "noc_w8_restartnoti_29f67a.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static const char PLANNED_RESTART_MARKER_FILENAME[] = ".restart_pending.json";

json_t *restartnoti_planned_restart_marker_path(const char *home, const char *default_home) {
    const char *h = home ? home : (default_home ? default_home : "");
    size_t n = strlen(h);
    size_t m = strlen(PLANNED_RESTART_MARKER_FILENAME);
    char *buf = (char *)malloc(n + m + 4);
    if (!buf) return json_null();
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        buf[k++] = (h[i] == '/') ? '\\' : h[i];
    while (k > 0 && buf[k - 1] == '\\') k--;
    /* WindowsPath('.') / name == name (the '.' component is dropped). */
    if (k == 1 && buf[0] == '.') k = 0;
    if (k > 0) buf[k++] = '\\';
    memcpy(buf + k, PLANNED_RESTART_MARKER_FILENAME, m + 1);
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

json_t *restartnoti_marker_payload(double now, int via_service, int detached) {
    json_t *o = json_object();
    json_set(o, "requested_at", json_number(now));
    json_set(o, "via_service", json_bool(via_service ? true : false));
    json_set(o, "detached", json_bool(detached ? true : false));
    return o;
}
