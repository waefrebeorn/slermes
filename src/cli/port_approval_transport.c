/*
 * port_approval_transport.c — Port of hermes_cli/approval_transport.py::
 * _validate_decision: the pure decision gate that maps a worker result to a
 * failure code ("timeout"/"error"/"invalid"/"stale") or NULL when valid.
 *
 *   kind:            "error" if the result is an error, else e.g. "ok"
 *   value_null:      1 when value is None / not an ApprovalDecision-typed object
 *                    (Python raises "invalid" for non-ApprovalDecision)
 *   value_reqid/digest/choice: the decision's fields
 *   completed_at/deadline: monotonic-ish timestamps
 *   req_reqid/req_digest/req_allowed: the request's fields
 *
 * Returns a malloc'd code string, caller frees; NULL = valid decision.
 */

#include "port_approval_transport.h"

#include <stdlib.h>
#include <string.h>

static const char *dupcode(const char *c)
{
    return strdup(c);
}

/* PoP: atr_validate_decision @ hermes_cli/approval_transport.py:_validate_decision */
const char *atr_validate_decision(
    const char *kind,
    int value_null,
    const char *value_reqid,
    const char *value_digest,
    const char *value_choice,
    double completed_at,
    double deadline,
    const char *req_reqid,
    const char *req_digest,
    const char *req_allowed)
{
    if (completed_at > deadline)
        return dupcode("timeout");
    if (kind && strcmp(kind, "error") == 0)
        return dupcode("error");
    if (value_null)
        return dupcode("invalid");
    if (!value_reqid || !value_digest || !req_reqid || !req_digest)
        return dupcode("invalid");
    if (strcmp(value_reqid, req_reqid) != 0 || strcmp(value_digest, req_digest) != 0)
        return dupcode("stale");
    /* req_allowed is "choice1,choice2,..." — Python's `value.choice not in
     * request.allowed_choices`; an empty allowed string also yields invalid. */
    {
        const char *q = req_allowed ? req_allowed : "";
        size_t vlen = value_choice ? strlen(value_choice) : 0;
        int found = 0;
        while (*q) {
            const char *comma = strchr(q, ',');
            size_t len = comma ? (size_t)(comma - q) : strlen(q);
            if (len == vlen && value_choice && strncmp(q, value_choice, len) == 0) {
                found = 1;
                break;
            }
            if (!comma) break;
            q = comma + 1;
        }
        if (!found)
            return dupcode("invalid");
    }
    return NULL;
}
