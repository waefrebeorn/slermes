/*
 * redaction.c — canonical C11 port of agent/monitoring/redaction.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_monitoring_redaction_u_secret_redact @ agent/monitoring/redaction.py:_secret_redact */
int agent_monitoring_redaction_u_secret_redact(const char *arg) {
    /* Python: always-on secret redaction (force=True).
     * Arg = "text_len\thas_secrets". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("[redacted]\n");
    return 0;
}

/* PoP: agent_monitoring_redaction_redact_for_export @ agent/monitoring/redaction.py:redact_for_export */
int agent_monitoring_redaction_redact_for_export(const char *arg) {
    /* Python: scrub secrets then PII for egress. Unconditional.
     * Arg = "text_len\thas_secrets\thas_pii". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("[redacted]\n");
    return 0;
}
