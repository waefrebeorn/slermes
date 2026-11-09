/*
 * t_port_service_manager.c -- oracle harness for port_service_manager.c
 */
#include "port_service_manager.h"

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

static void emit_validate(const char *name)
{
    char err[256];
    int rc = sm_validate_profile_name(name, err, sizeof err);
    printf("{\"fn\":\"validate_profile_name\",\"in\":");
    jstr(name);
    printf(",\"rc\":%d,\"err\":", rc);
    jstr(err);
    printf("}\n");
}

static void emit_strip(const char *name)
{
    char *s = sm_profile_from_service(name);
    printf("{\"fn\":\"_profile_from_service\",\"in\":");
    jstr(name);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

int main(void)
{
    /* ---- validate_profile_name ---- */
    emit_validate("main");
    emit_validate("gateway");
    emit_validate("a");
    emit_validate("0abc");
    emit_validate("with_underscore");
    emit_validate("with-dash");
    emit_validate("");
    emit_validate(NULL);
    emit_validate("Uppercase");
    emit_validate("-leading-dash");
    emit_validate("_leading-underscore");
    emit_validate("has space");
    emit_validate("has.dot");
    emit_validate("has/slash");

    /* ---- _profile_from_service ---- */
    emit_strip("gateway-main");
    emit_strip("gateway-");
    emit_strip("main");
    emit_strip("");
    emit_strip(NULL);

    return 0;
}
