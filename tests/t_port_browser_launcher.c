/*
 * t_port_browser_launcher.c -- oracle harness for port_browser_launcher.c
 */
#include "port_browser_launcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_bool(const char *fn, const char *label, int v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

static void emit_argv(const char *label,
                      const char *exe, const char *udd,
                      int packaged, int is_macos, int headed,
                      int sandbox_bypass, int needs_bypass,
                      const char *const *configured,
                      const char *const *extra)
{
    int n = 0;
    char **argv = bl_build_argv(exe, udd, packaged, is_macos, headed,
                                sandbox_bypass, needs_bypass,
                                configured, extra, &n);
    printf("{\"fn\":\"build_chromium_argv\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) {
        if (i) putchar(',');
        putchar('"');
        for (char *q = argv[i]; *q; q++) {
            if (*q == '"' || *q == '\\') { putchar('\\'); putchar(*q); }
            else putchar(*q);
        }
        putchar('"');
    }
    printf("]}\n");
    for (int i = 0; i < n; i++) free(argv[i]);
    free(argv);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- _has_display ---- */
    static const char *e1[] = { "DISPLAY", ":0", NULL };
    static const char *e2[] = { "WAYLAND_DISPLAY", "wayland-0", NULL };
    static const char *e3[] = { "DISPLAY", "", NULL };
    static const char *e4[] = { "PATH", "/usr/bin", NULL };
    static const char *e5[] = { NULL };
    emit_bool("_has_display", "x11",     bl_has_display(e1));
    emit_bool("_has_display", "wayland", bl_has_display(e2));
    emit_bool("_has_display", "empty",   bl_has_display(e3));
    emit_bool("_has_display", "none",    bl_has_display(e4));
    emit_bool("_has_display", "no-env",  bl_has_display(e5));

    /* ---- build_chromium_argv ---- */
    static const char *cfg1[] = { "--custom=1", NULL };
    static const char *cfg2[] = { "--no-sandbox", NULL };
    static const char *x1[]   = { "--proxy=x", NULL };
    static const char *x0[]   = { NULL };
    static const char *cfg0[] = { NULL };

    emit_argv("plain",      "/chrome", "/udd", 0, 0, 1, 0, 0, cfg0, x0);
    emit_argv("packaged",   "/chrome", "/udd", 1, 0, 1, 0, 0, cfg0, x0);
    emit_argv("macos",      "/chrome", "/udd", 1, 1, 1, 0, 0, cfg0, x0);
    emit_argv("headed",     "/chrome", "/udd", 0, 0, 0, 0, 0, cfg0, x0);
    emit_argv("bypass",     "/chrome", "/udd", 0, 0, 1, 1, 1, cfg0, x0);
    emit_argv("bypass-off", "/chrome", "/udd", 0, 0, 1, 0, 1, cfg0, x0);
    emit_argv("no-need",    "/chrome", "/udd", 0, 0, 1, 1, 0, cfg0, x0);
    emit_argv("configured", "/chrome", "/udd", 0, 0, 1, 0, 0, cfg1, x1);
    emit_argv("dedupe",     "/chrome", "/udd", 0, 0, 1, 1, 1, cfg2, x0);

    return 0;
}
