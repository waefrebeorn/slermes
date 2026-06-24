/**
 * port_whatsapp_common.c — Port of Python: gateway/whatsapp_common.py
 *
 * Real C implementations for WhatsApp common helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/* Port of Python: resolve_whatsapp_bridge_dir */
char *resolve_whatsapp_bridge_dir(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char *dir = malloc(4096);
    if (!dir) return NULL;
    snprintf(dir, 4096, "%s/whatsapp/bridge", home);

    struct stat st;
    if (stat(dir, &st) != 0) {
        hermes_log(LOG_DEBUG, "port", "resolve_whatsapp_bridge_dir: creating %s", dir);
    } else {
        hermes_log(LOG_DEBUG, "port", "resolve_whatsapp_bridge_dir: %s exists", dir);
    }
    return dir;
}
