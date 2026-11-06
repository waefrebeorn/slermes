#ifndef SLERMES_PORT_APPROVAL_TRANSPORT_H
#define SLERMES_PORT_APPROVAL_TRANSPORT_H

/* _validate_decision(kind, value, completed_at, deadline, request): maps a
 * worker result to a failure code string ("timeout"/"error"/"invalid"/"stale")
 * or NULL when valid. See the .c for the flattened field encoding. Returns a
 * malloc'd string, caller frees. */
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
    const char *req_allowed);

#endif /* SLERMES_PORT_APPROVAL_TRANSPORT_H */
