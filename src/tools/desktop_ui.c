/*
 * desktop_ui.c — canonical C11 port of tools/desktop_ui.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_desktop_ui_set_emitter @ tools/desktop_ui.py:set_emitter */
int tools_desktop_ui_set_emitter(const char *arg) {
    /* Python: global _emit; _emit = fn — install (or clear) the renderer
     * event sink. The C port stores the emitter token; NULL/empty clears. */
    static char g_desktop_ui_emitter[512] = {0};
    if (arg && *arg) snprintf(g_desktop_ui_emitter, sizeof(g_desktop_ui_emitter), "%s", arg);
    else g_desktop_ui_emitter[0] = '\0';
    printf("emitter %s\n", g_desktop_ui_emitter[0] ? "set" : "cleared");
    return 0;
}
