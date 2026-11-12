#ifndef SLERMES_PORT_CONNECTORS_CONTRACT_H
#define SLERMES_PORT_CONNECTORS_CONTRACT_H

/* resolves(kind, state): 1 when the target state is resolved (connected or
 * skipped; plus failed for plugin/skill kinds, matching the live contract). */
int ctc_resolves(const char *kind, const char *state);

/* allowed(kind, current, to): the Actor allowed to cause the move, or NULL
 * when the transition is not permitted (matching the live contract). */
const char *ctc_allowed(const char *kind, const char *current, const char *to);

#endif /* SLERMES_PORT_CONNECTORS_CONTRACT_H */
