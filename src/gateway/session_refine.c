/*
 * session_refine.c — C port of upstream Hermes gateway/session_refine.py.
 *
 * PoP: w8_sessionrefi_552870_is_local @ gateway/session_refine.py:_local_session
 * PoP: w8_sessionrefi_552870_focus_tail @ gateway/session_refine.py:refine_local_session
 * PoP: w8_sessionrefi_552870_started @ gateway/session_refine.py:refine_local_session
 *
 * Faithful port of the authority-local early gate and the two i18n formatting
 * steps that decide the /refine reply. The English catalog strings from
 * locales/en.yaml are mirrored verbatim; the oracle injects the same templates
 * into the live module's `t` (the real catalog loader needs ruamel.yaml, which
 * is not available on this host, so the strings are pinned explicitly).
 */
#include "noc_w8_sessionrefi_552870.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

int w8_sessionrefi_552870_is_local(const char *platform) {
    return (platform && !strcmp(platform, "local")) ? 1 : 0;
}

/* locales/en.yaml: gateway.refine.focus_suffix: " (focus: {focus})" */
struct json_t *w8_sessionrefi_552870_focus_tail(const char *focus) {
    char buf[8192];
    if (!focus || !focus[0]) return json_string("");
    snprintf(buf, sizeof(buf), " (focus: %s)", focus);
    return json_string(buf);
}

/* locales/en.yaml: gateway.refine.started:
 * "⚗ Reviewing this conversation in the background{focus_suffix} — any memory/skill updates will be reported when done." */
struct json_t *w8_sessionrefi_552870_started(const char *focus_suffix) {
    char buf[8192];
    const char *s = focus_suffix ? focus_suffix : "";
    snprintf(buf, sizeof(buf),
             "⚗ Reviewing this conversation in the background%s — any memory/skill updates will be reported when done.",
             s);
    return json_string(buf);
}
