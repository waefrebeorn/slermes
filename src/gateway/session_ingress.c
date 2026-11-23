/*
 * session_ingress.c — C home for upstream Hermes gateway/session_ingress.py.
 *
 * PoP: w9_sessioningr_2d59a9_row_turn_author @ gateway/session_ingress.py:row_turn_author
 * PoP: w9_sessioningr_2d59a9_pause_notice   @ gateway/session_ingress.py:pause_notice
 *
 * Two pure helpers from trusted messaging admission.  The lazily imported
 * gateway.session_a2a.forward_author is injected as `fallback_author`, and the
 * pause_notice session/platform state is injected as explicit booleans (the
 * Python reads it off a fake authority in the oracle).  Notice strings are
 * the exact live wording.
 */
#include "noc_w9_sessioningr_2d59a9.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct json_t *w9_sessioningr_2d59a9_row_turn_author(const struct json_t *row,
                                                     const struct json_t *fallback_author) {
    static const char *const keys[] = {"local_automation_v1", "api_turn_v1", "cli_turn_v1"};
    /* row['payload'] is a direct index in the live code: a missing key raises
     * KeyError('payload'), surfaced here as the oracle's caught-exception form. */
    if (!row || row->type != JSON_OBJECT || !json_has(row, "payload")) {
        json_t *e = json_object();
        json_set(e, "__raise__", json_string("KeyError"));
        json_set(e, "message", json_string("'payload'"));
        return e;
    }
    json_t *payload = json_obj_get(row, "payload");
    if (!payload || payload->type != JSON_OBJECT) payload = NULL;

    for (size_t i = 0; i < 3; ++i) {
        json_t *v = json_obj_get(payload, keys[i]);
        if (!v || v->type != JSON_OBJECT || v->c.count == 0) continue;  /* (get(key) or {}) */
        json_t *author = json_obj_get(v, "turn_author");
        if (author && author->type != JSON_NULL)
            return json_copy(author);
    }
    return json_copy(fallback_author ? fallback_author : json_null());
}

struct json_t *w9_sessioningr_2d59a9_pause_notice(const char *reason,
                                                  int pause_notified, int slack) {
    if (pause_notified) return json_null();
    const char *r = reason ? reason : "";
    char buf[2048];

    if (!strcmp(r, "runtime_draining")) {
        return json_string("⏳ Hermes is restarting — your message is saved and will be answered when it is back.");
    }
    if (!strcmp(r, "session_busy")) {
        return json_string("⏳ Your message is saved and will be answered when the work running in this conversation finishes.");
    }
    if (!strcmp(r, "unknown_execution")) {
        const char *discard = slack ? "/hermes discard" : "/discard";
        snprintf(buf, sizeof buf,
                 "⏸️ This conversation is paused: a previous turn did not finish when Hermes restarted, "
                 "and only you know whether to send it again. Your message is saved. Send %s to drop "
                 "the lost turn and continue (it is not re-run), or /reset to start a fresh conversation.",
                 discard);
        return json_string(buf);
    }
    snprintf(buf, sizeof buf,
             "⏸️ This conversation is paused: a queued message could not be re-authorized (%s). "
             "Your message is saved but will not be answered here. Send /reset to start a fresh "
             "conversation, or ask the operator to resume this one.", r);
    return json_string(buf);
}
