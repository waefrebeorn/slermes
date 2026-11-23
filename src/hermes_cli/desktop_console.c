/*
 * desktop_console.c — C port of upstream Hermes hermes_cli/desktop_console.py.
 *
 * PoP: w13_desktopcon_98238a_launch_notice @ hermes_cli/desktop_console.py:desktop_launch_notice
 * PoP: w13_desktopcon_98238a_output_stream_keys @ hermes_cli/desktop_console.py:desktop_console_output
 *
 * sys.platform / source_mode are injected by the fixture (the Python behaviour is
 * driven by those two values only); no process environment is mutated.
 */
#include "noc_w13_desktopcon_98238a.h"
#include "hermes_json.h"
#include <string.h>

static int packaged_win(const char *platform, int source_mode) {
    return platform && !strcmp(platform, "win32") && !source_mode;
}

struct json_t *w13_desktopcon_98238a_launch_notice(const char *platform, int source_mode,
                                                   const char *message) {
    json_t *o = json_object();
    json_set(o, "sink", json_string(packaged_win(platform, source_mode) ? "log" : "print"));
    json_set(o, "message", json_string(message ? message : ""));
    return o;
}

struct json_t *w13_desktopcon_98238a_output_stream_keys(const char *platform, int source_mode) {
    json_t *a = json_array();
    if (packaged_win(platform, source_mode)) {
        json_append(a, json_string("stderr"));
        json_append(a, json_string("stdout"));
    }
    return a;
}
