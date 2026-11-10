/*
 * port_browser_connect.h -- pure helpers ported from hermes_cli/browser_connect.py.
 *
 * See port_browser_connect.c: only the dependency-free decision logic is
 * ported; the module's subprocess/sqlite/socket/registry half stays in Python.
 */
#ifndef PORT_BROWSER_CONNECT_H
#define PORT_BROWSER_CONNECT_H

#include <stddef.h>

/* Sentinel _classify_default returns when the value matches a channel that
 * must fail closed (Beta/Dev/Canary) rather than the stable profile. */
#define BC_UNSUPPORTED_CHANNEL "__unsupported_channel__"

typedef struct {
    const char *fragment;    /* matched substring, e.g. "chrome-beta" */
    const char *canonical;   /* canonical key, e.g. "chrome" */
} bc_fragment_t;

/* browser_connect.py:_classify_default -- channel check first (fail closed),
 * then the first table fragment that matches. NULL when nothing matches.
 * `match` may be NULL, in which case nothing matches. */
const char *bc_classify_default(const char *value,
                                const char *const *channels,
                                const bc_fragment_t *table,
                                int (*match)(const char *value, const char *needle));

/* browser_connect.py:_first_present -- first path naming an existing regular
 * file, else NULL. Aliases the input array. */
const char *bc_first_present(const char *const *paths);

#endif /* PORT_BROWSER_CONNECT_H */
