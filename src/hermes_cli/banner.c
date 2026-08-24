/*
 * banner.c — canonical C11 port of hermes_cli/banner.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_banner_u_skin_color @ hermes_cli/banner.py:_skin_color */
int hermes_cli_banner_u_skin_color(const char *arg) {
    /* Python: active skin color or fallback. Arg = "key\tfallback\tcolor". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *color = t2 ? t2 + 1 : "";
    if (color[0]) { printf("%s\n", color); return 0; }
    printf("%s\n", t1 ? t1 + 1 : "");
    return 0;
}
