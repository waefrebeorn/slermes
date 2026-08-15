/* Slermes C11 port — hermes_cli/web_server.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: _lifespan @ hermes_cli/web_server.py:_lifespan */
json_t *_lifespan(json_t * app) {
    return json_object();
}

/* PoP: _fs_mime_type @ hermes_cli/web_server.py:_fs_mime_type */
const char *_fs_mime_type(const char * path) {
    return NULL;
}

/* PoP: _start_parent_death_watchdog @ hermes_cli/web_server.py:_start_parent_death_watchdog */
void _start_parent_death_watchdog(void) {
}

