/*
 * port_portal_cli.h -- pure helper ported from hermes_cli/portal_cli.py.
 *
 * The argparse dispatch and webbrowser calls stay in Python.
 */
#ifndef PORT_PORTAL_CLI_H
#define PORT_PORTAL_CLI_H

#include <stddef.h>

/* portal_cli.py:_feature_state -- the routing column shared by `portal info`
 * and `portal tools`. Returns one of the PC_* selectors below. */
int pc_feature_state(int managed_by_nous, int active, const char *current_provider);

/* The text each selector renders (caller frees). */
char *pc_feature_state_text(int selector, const char *via_nous,
                            const char *current_provider);

#define PC_VIA_NOUS         0
#define PC_CURRENT_PROVIDER 1
#define PC_NOT_CONFIGURED   2

#endif /* PORT_PORTAL_CLI_H */
