/*
 * surfaces.c — C port of upstream Hermes session_rules/surfaces.py.
 * PoP: ssurf_created_surface @ session_rules/surfaces.py:created_surface
 */
#include "noc_ssurf.h"
#include <string.h>

/*
 * SURFACES = {cli:cli, tui:tui, gui:desktop, acp:acp, tool:cli, oneshot:cli}
 * STORED_AS_LOCAL = SURFACES - {tool, oneshot}
 * _NOT_A_SURFACE  = {None, '', 'local', 'unknown'}
 *
 *   if label in _NOT_A_SURFACE: return None
 *   return SURFACES[label] if label in STORED_AS_LOCAL else label
 */
const char *ssurf_created_surface(const char *label) {
    if (label == NULL) return NULL; /* None */
    if (!*label || !strcmp(label, "local") || !strcmp(label, "unknown")) return NULL;

    /* label in STORED_AS_LOCAL -> SURFACES[label] */
    if (!strcmp(label, "cli"))     return "cli";
    if (!strcmp(label, "tui"))     return "tui";
    if (!strcmp(label, "gui"))     return "desktop";
    if (!strcmp(label, "acp"))     return "acp";

    /* 'tool' and 'oneshot' are keep-stored labels: provenance, not remapped.
     * Every other unknown label is also kept as itself. */
    return label;
}
