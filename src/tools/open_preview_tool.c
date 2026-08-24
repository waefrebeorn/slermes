/*
 * open_preview_tool.c — canonical C11 port of tools/open_preview_tool.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_open_preview_tool_u_normalize_target @ tools/open_preview_tool.py:_normalize_target */
int tools_open_preview_tool_u_normalize_target(const char *arg) {
    /* Python: bare host -> https; localhost -> http; pass through. Arg =
     * raw. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    size_t len = strlen(p);
    while (len > 0 && p[len-1] == ' ') len--;
    if (!len) { printf("\n"); return 0; }
    if (strstr(p, "://") || p[0] == '/' || strncmp(p, "./", 2) == 0 || strncmp(p, "../", 3) == 0 || p[0] == '~' || strncmp(p, "file:", 5) == 0) {
        printf("%.*s\n", (int)len, p);
        return 0;
    }
    /* localhost:port */
    int is_local = 0;
    if (strncasecmp(p, "localhost", 9) == 0 || strncmp(p, "127.0.0.1", 9) == 0 || strncmp(p, "0.0.0.0", 7) == 0 || strncmp(p, "[::1]", 5) == 0) is_local = 1;
    if (is_local) { printf("http://%.*s\n", (int)len, p); return 0; }
    /* domain with dot */
    if (strchr(p, '.')) { printf("https://%.*s\n", (int)len, p); return 0; }
    printf("%.*s\n", (int)len, p);
    return 0;
}

/* PoP: tools_open_preview_tool_open_preview_tool @ tools/open_preview_tool.py:open_preview_tool */
int tools_open_preview_tool_open_preview_tool(const char *arg) {
    /* Python: normalize + emit preview.open. Arg =
     * "target\tstate\terr". */
    if (!arg || !*arg) {
        printf("error: url is required — a web URL (https://…), a localhost dev server, or a file path to show in the preview pane.\n");
        return 0;
    }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_desktop") == 0) {
        printf("error: The preview pane is only available in the Hermes desktop app.\n");
        return 0;
    }
    if (strcmp(state, "emit_error") == 0) {
        printf("error: Failed to open the preview pane: %s\n", t2 ? t2 + 1 : "unknown");
        return 0;
    }
    printf("{\"success\": true, \"url\": \"%s\"}\n", arg);
    return 0;
}

/* PoP: tools_open_preview_tool_check_open_preview_requirements @ tools/open_preview_tool.py:check_open_preview_requirements */
int tools_open_preview_tool_check_open_preview_requirements(const char *arg) {
    /* Python: env_var_enabled("HERMES_DESKTOP") — desktop GUI only. */
    (void)arg;
    const char *v = getenv("HERMES_DESKTOP");
    int enabled = v && *v && strcmp(v, "0") != 0 && strcasecmp(v, "false") != 0;
    printf("%d\n", enabled);
    return 0;
}
