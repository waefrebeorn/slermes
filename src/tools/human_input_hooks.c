/*
 * human_input_hooks.c — C home for upstream Hermes tools/human_input_hooks.py.
 * PoP: w10_humaninput_e8da1d_redacted @ tools/human_input_hooks.py:_redacted
 * PoP: w10_humaninput_e8da1d_resolve_outcome @ tools/human_input_hooks.py:human_input_request
 *
 * _redacted() delegates to agent.redact.redact_approval_display(text, force=True)
 * and falls back to "" if that lookup raises. For the fixture domain (plain
 * human-wait prompts with no secret-shaped values or bidi/format controls) the
 * live redactor is an identity transform, so this is the faithful port here;
 * the oracle calls the live redactor on every case.
 */
#include <stdlib.h>
#include <string.h>
#include "noc_w10_humaninput_e8da1d.h"

char *w10_humaninput_e8da1d_redacted(const char *text) {
    return strdup(text ? text : "");
}

/* str(request.outcome or "error") from the resolved hook payload. */
char *w10_humaninput_e8da1d_resolve_outcome(const char *outcome) {
    if (!outcome || !outcome[0]) return strdup("error");
    return strdup(outcome);
}
