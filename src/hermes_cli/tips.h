/* tips.h — C11 port of hermes_cli/tips.py API surface. */

#ifndef HERMES_CLI_TIPS_H
#define HERMES_CLI_TIPS_H

/**
 * PoP: hcli_get_random_tip @ hermes_cli/tips.py:get_random_tip
 * Return a random tip string (mirrors Python random.choice(TIPS)).
 */
const char *hcli_get_random_tip(void);

#endif /* HERMES_CLI_TIPS_H */
