/*
 * shim.c for gateway_acp_acp_get_id — mirrors Python ACPServer.get_id.
 * The Python code in agent/copilot_acp_client.py defines an ACPServer
 * class with a get_id method.
 *
 * For the watercup, we match the Python behavior: the Python ACPServer.get_id
 * returns a hardcoded ID string.
 */

#include <stdlib.h>
#include <string.h>

static char * ACP_ID = NULL;

const char *acp_get_id(void *req) {
    (void)req;
    if (!ACP_ID) {
        ACP_ID = strdup("acp-server-0");
    }
    return ACP_ID;
}
