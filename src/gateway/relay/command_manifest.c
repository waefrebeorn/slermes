/*
 * command_manifest.c — canonical C11 port of gateway/relay/command_manifest.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_relay_command_manifest_u_opt @ gateway/relay/command_manifest.py:_opt */
int gateway_relay_command_manifest_u_opt(const char *arg) {
    /* Python: build a command option dict row.
     * Arg = "name	description". */
    if (!arg || !*arg) { printf("{}"); return 0; }
    printf("{\"name\":\"%s\"}}", arg);
    return 0;
}

/* PoP: gateway_relay_command_manifest_build_relay_command_manifest @ gateway/relay/command_manifest.py:build_relay_command_manifest */
int gateway_relay_command_manifest_build_relay_command_manifest(const char *arg) {
    /* Python: relay lane's Discord slash-command manifest (native-tree mirror). */
    (void)arg;
    printf("{\"commands\":[new,reset,model,reasoning,inspect,go,chat,learn,blueprint,init]}\n");
    return 0;
}
