/*
 * port_portal_cli.c
 *
 * Pure helpers ported from hermes_cli/portal_cli.py.
 *
 *   pc_feature_state <- portal_cli.py:_feature_state
 *
 *      if feat.managed_by_nous:  return color(via_nous, Colors.GREEN)
 *      if feat.active:           return feat.current_provider or "active"
 *      return color("not configured", Colors.DIM)
 *
 * The ANSI colouring is a rendering choice; the DECISION - which string, and
 * that via_nous wins over everything - is what this port returns.
 *
 * The rest of the module is argparse dispatch and webbrowser calls.
 */

#include "port_portal_cli.h"

#include <stdlib.h>
#include <string.h>

/* Feature-state selectors, in the Python's evaluation order. */
#define PC_VIA_NOUS         0   /* feat.managed_by_nous - wins over active */
#define PC_CURRENT_PROVIDER 1   /* feat.current_provider or "active" */
#define PC_NOT_CONFIGURED   2   /* the fallback */

/* ===========================================================================
 *  _feature_state  (portal_cli.py:_feature_state)
 *
 *  `current_provider` NULL means the attribute was absent; Python's `or`
 *  then falls through to "active". Returns one of the PC_* selectors; the
 *  caller renders the string and the colour.
 * =========================================================================== */

/* PoP: _feature_state @ hermes_cli/portal_cli.py:_feature_state */
int pc_feature_state(int managed_by_nous, int active, const char *current_provider)
{
    if (managed_by_nous) return PC_VIA_NOUS;
    if (active) return PC_CURRENT_PROVIDER;
    return PC_NOT_CONFIGURED;
}

/* ===========================================================================
 *  The string each selector renders, so the caller never re-derives it.
 *  `via_nous` is the caller's label for the managed-by-Nous column.
 *  Caller frees.
 * =========================================================================== */

/* PoP: _feature_state (rendered text) @ hermes_cli/portal_cli.py:_feature_state */
char *pc_feature_state_text(int selector, const char *via_nous,
                            const char *current_provider)
{
    switch (selector) {
    case PC_VIA_NOUS:
        return strdup(via_nous ? via_nous : "nous");
    case PC_CURRENT_PROVIDER:
        return strdup((current_provider && current_provider[0]) ? current_provider : "active");
    case PC_NOT_CONFIGURED:
    default:
        return strdup("not configured");
    }
}
