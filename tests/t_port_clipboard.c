/*
 * t_port_clipboard.c -- oracle harness for port_clipboard.c
 */
#include "port_clipboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *g_names[3];
static const char *g_vals[3];
static int g_env_n;

static int env_has(const char *name)
{
    for (int i = 0; i < g_env_n; i++)
        if (g_vals[i] && *g_vals[i] && name && strcmp(g_names[i], name) == 0) return 1;
    return 0;
}

static void emit_ssh(const char *label, const char *a, const char *b, const char *c)
{
    g_names[0] = "SSH_CONNECTION"; g_vals[0] = a;
    g_names[1] = "SSH_TTY";        g_vals[1] = b;
    g_names[2] = "SSH_CLIENT";     g_vals[2] = c;
    g_env_n = 3;
    int v = cb_is_remote_shell_session(env_has);
    printf("{\"fn\":\"is_remote_shell_session\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

static void emit_bitmap(const char *label, const char *info)
{
    int v = cb_macos_has_bitmap(info);
    printf("{\"fn\":\"_macos_has_bitmap\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"in\":");
    if (!info) printf("null");
    else {
        putchar('"');
        for (const unsigned char *p = (const unsigned char *)info; *p; p++) {
            if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
            else putchar(*p);
        }
        putchar('"');
    }
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

int main(void)
{
    /* ---- is_remote_shell_session ---- */
    emit_ssh("none", NULL, NULL, NULL);
    emit_ssh("connection", "10.0.0.1 22", NULL, NULL);
    emit_ssh("tty", NULL, "/dev/pts/3", NULL);
    emit_ssh("client", NULL, NULL, "10.0.0.1 50000 22");
    emit_ssh("empty-values", "", "", "");

    /* ---- _macos_has_bitmap ---- */
    emit_bitmap("null", NULL);
    emit_bitmap("", "");
    emit_bitmap("«class PNGf»", "«class PNGf»");
    emit_bitmap("«class TIFF»", "«class TIFF»");
    emit_bitmap("«class furl»", "«class furl»");
    emit_bitmap("both", "«class PNGf» «class ICNS»");

    return 0;
}
