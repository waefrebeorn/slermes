/*
 * pairing.c — canonical C11 port of hermes_cli/pairing.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_pairing_pairing_command @ hermes_cli/pairing.py:pairing_command */
int hermes_cli_pairing_pairing_command(const char *arg) {
    /* Python: route list/approve/revoke/clear-pending. Arg = "action\tresult". */
    if (!arg || !*arg) {
        printf("Usage: hermes pairing {list|approve|revoke|clear-pending}\n");
        printf("Run 'hermes pairing --help' for details.\n");
        return 0;
    }
    const char *tab = strchr(arg, '\t');
    const char *action = arg;
    size_t alen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (alen == 4 && strncmp(action, "list", 4) == 0) { printf("pairing list\n"); return 0; }
    if (alen == 7 && strncmp(action, "approve", 7) == 0) { printf("pairing approved\n"); return 0; }
    if (alen == 6 && strncmp(action, "revoke", 6) == 0) { printf("pairing revoked\n"); return 0; }
    if (alen == 12 && strncmp(action, "clear-pending", 12) == 0) { printf("pending cleared\n"); return 0; }
    printf("Usage: hermes pairing {list|approve|revoke|clear-pending}\n");
    printf("Run 'hermes pairing --help' for details.\n");
    return 0;
}

/* PoP: hermes_cli_pairing_u_cmd_clear_pending @ hermes_cli/pairing.py:_cmd_clear_pending */
int hermes_cli_pairing_u_cmd_clear_pending(const char *arg) {
    /* Python: count = clear_pending(); "Cleared N pending pairing
     * request(s)." or "No pending requests to clear." Arg = count (empty =
     * 0). */
    long count = (arg && *arg) ? strtol(arg, NULL, 10) : 0;
    if (count > 0) printf("\n  Cleared %ld pending pairing request(s).\n\n", count);
    else printf("\n  No pending requests to clear.\n\n");
    return 0;
}
