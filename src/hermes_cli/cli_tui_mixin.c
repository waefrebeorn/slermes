/*
 * cli_tui_mixin.c — C home for upstream Hermes hermes_cli/cli_tui_mixin.py.
 *
 * PoP: w10_clituimix_9a2c8a_num_prefix @ hermes_cli/cli_tui_mixin.py:_num_prefix
 * PoP: w10_clituimix_9a2c8a_approval_choice_label @ hermes_cli/cli_tui_mixin.py:_approval_choice_label
 * PoP: w10_clituimix_9a2c8a_tn @ hermes_cli/cli_tui_mixin.py:_tn
 * PoP: w10_clituimix_9a2c8a_audio_bar @ hermes_cli/cli_tui_mixin.py:CLITuiMixin._audio_level_bar
 *
 * Faithful C11 port of the pure prompt/key-label helpers. prompt_toolkit
 * layout, key bindings and overlay rendering are intentionally out of scope.
 */
#include "noc_w10_clituimix_9a2c8a.h"

#include "hermes_json.h"

#include <stdio.h>
#include <string.h>

static const char *w10_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static long long w10_int(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? (long long)v->num_val : 0;
}

/* C truncates toward zero; Python // floors. */
static long long w10_floor_div(long long a, long long b) {
    long long q = a / b, r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) q--;
    return q;
}

struct json_t *w10_clituimix_9a2c8a_num_prefix(const struct json_t *i) {
    long long n = w10_int(i);
    char buf[64];
    if (n < 9) snprintf(buf, sizeof(buf), "%lld", n + 1);
    else if (n == 9) snprintf(buf, sizeof(buf), "0");
    else snprintf(buf, sizeof(buf), " ");
    return json_string(buf);
}

struct json_t *w10_clituimix_9a2c8a_choice_label(const struct json_t *choice) {
    const char *c = w10_str(choice);
    static const char *const known[] = {
        "cli.tui.approval_once", "cli.tui.approval_session", "cli.tui.approval_always",
        "cli.tui.approval_deny", "cli.tui.approval_view"
    };
    static const char *const keys[] = { "once", "session", "always", "deny", "view" };
    for (size_t k = 0; k < 5; ++k)
        if (strcmp(c, keys[k]) == 0) return json_string(known[k]);
    return json_string(c);
}

struct json_t *w10_clituimix_9a2c8a_tn(const struct json_t *key, const struct json_t *count) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s%s", w10_str(key), w10_int(count) == 1 ? "_one" : "_other");
    return json_string(buf);
}

struct json_t *w10_clituimix_9a2c8a_audio_bar(const struct json_t *rms) {
    static const char *const bars[8] = {
        " ", "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83",
        "\xe2\x96\x84", "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87"
    };
    long long r = w10_int(rms);
    if (r > 8000) r = 8000;
    long long idx = w10_floor_div(r * 7, 8000);
    idx = ((idx % 8) + 8) % 8; /* Python negative-index wrap */
    return json_string(bars[idx]);
}
