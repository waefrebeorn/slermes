#ifndef SLERMES_PORT_CONNECTORS_CONTRACT_H
#define SLERMES_PORT_CONNECTORS_CONTRACT_H

/* resolves(kind, state): 1 when the target state is resolved (connected or
 * skipped; plus failed for plugin/skill kinds, matching the live contract). */
int ctc_resolves(const char *kind, const char *state);

#endif /* SLERMES_PORT_CONNECTORS_CONTRACT_H */
