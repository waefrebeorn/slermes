/* Slermes C11 port — hermes_cli/_subprocess_compat.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: resolve_node_command @ hermes_cli/_subprocess_compat.py:resolve_node_command */
static json_t *resolve_node_command(const char * name, json_t * argv) {
    return json_object();
}

