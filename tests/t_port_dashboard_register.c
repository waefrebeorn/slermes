/*
 * t_port_dashboard_register.c -- oracle harness for port_dashboard_register.c
 */
#include "port_dashboard_register.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p == '\n') fputs("\\n", stdout);
        else putchar(*p);
    }
    putchar('"');
}

static void emit_redirect(const char *uri)
{
    char *s = dr_public_url_from_redirect(uri);
    printf("{\"fn\":\"_public_url_from_redirect\",\"in\":");
    jstr(uri);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

static void emit_resolve(const char *label, const char *ovr, const char *stored, const char *def)
{
    char *s = dr_resolve_portal_base_url(ovr, stored, def);
    printf("{\"fn\":\"_resolve_portal_base_url\",\"case\":");
    jstr(label);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

int main(void)
{
    /* ---- _public_url_from_redirect ---- */
    emit_redirect("https://example.com/auth/callback");
    emit_redirect("http://localhost:8080/auth/callback");
    emit_redirect("https://example.com");
    emit_redirect("https://example.com/");
    emit_redirect("https://example.com:8443/cb?x=1");
    emit_redirect("ftp://example.com/cb");
    emit_redirect("not-a-url");
    emit_redirect("");
    emit_redirect(NULL);
    emit_redirect("https://[::1]:9000/cb");

    /* ---- _resolve_portal_base_url ---- */
    emit_resolve("override", "https://custom.example/", NULL, "https://portal.nousresearch.com");
    emit_resolve("override-trailing", "https://custom.example///", NULL, "https://portal.nousresearch.com");
    emit_resolve("stored", NULL, "https://stored.example/", "https://portal.nousresearch.com");
    emit_resolve("default", NULL, NULL, "https://portal.nousresearch.com");
    emit_resolve("override-blank", "   ", "https://stored.example", "https://portal.nousresearch.com");
    emit_resolve("stored-blank", NULL, "  ", "https://portal.nousresearch.com");

    return 0;
}
