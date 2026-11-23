/*
 * sessions_adopt.c — C home for upstream Hermes hermes_cli/sessions_adopt.py.
 * PoP: w12_sessionsad_c876ba_refusal @ hermes_cli/sessions_adopt.py:_refusal
 * PoP: w12_sessionsad_c876ba_unreadable_store @ hermes_cli/sessions_adopt.py:_unreadable_store
 *
 * The two pure report-formatting seams of the adopt command.  Every string is
 * copied out of the JSON node by the harness before these functions are called.
 */
#include "hermes_json.h"
#include "noc_w12_sessionsad_c876ba.h"

#include <stdio.h>
#include <string.h>

void w12_sessionsad_c876ba_refusal(const char *reason, const char *field,
                                   const char *detail, char *out, size_t cap)
{
    const char *why;
    char detail_buf[1024];

    if (!out || !cap) return;
    if (!reason) reason = "";

    if (!strcmp(reason, "launch_credentials_unavailable"))
        why = "its saved model route is gone; open it once and pick a model";
    else if (!strcmp(reason, "not_found"))
        why = "not adoptable here (another identity or profile owns it)";
    else if (field && *field && detail && *detail) {
        snprintf(detail_buf, sizeof(detail_buf), "%s: %s", field, detail);
        why = detail_buf;
    } else
        why = "refused by the gateway";

    snprintf(out, cap, "%s (%s)", reason, why);
}

void w12_sessionsad_c876ba_unreadable_store(const char *kind, const char *store,
                                            const char *exc_text,
                                            char *out, size_t cap)
{
    if (!out || !cap) return;
    if (!store) store = "";
    if (!exc_text) exc_text = "";
    if (kind && !strcmp(kind, "lock")) {
        snprintf(out, cap,
                 "Error: %s is locked by another program. Close that program, then run this command again.",
                 store);
    } else if (kind && !strcmp(kind, "schema")) {
        snprintf(out, cap,
                 "Error: %s is from an older Hermes and has not been upgraded (%s). Run "
                 "`hermes sessions repair` to upgrade it (it makes a backup first), then run this "
                 "command again.",
                 store, exc_text);
    } else {
        snprintf(out, cap,
                 "Error: could not read %s (%s). Run `hermes sessions repair` to check it.",
                 store, exc_text);
    }
}
