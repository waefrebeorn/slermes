/*
 * t_port_nous_account.c -- oracle harness for port_nous_account.c
 */
#include "port_nous_account.h"

#include <stdio.h>
#include <stdlib.h>

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

static void emit_tier(const char *v)
{
    char *s = nous_normalize_tier(v);
    printf("{\"fn\":\"_normalize_tier\",\"in\":");
    jstr(v);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

static void emit_anon(int has, int flag)
{
    int r = nous_is_anonymous_tier(has, flag);
    printf("{\"fn\":\"_is_anonymous_tier\",\"has\":%d,\"flag\":%d,\"out\":%s}\n",
           has, flag, r ? "true" : "false");
}

static void emit_notice(int removed, int present)
{
    char *s = nous_policy_notice(removed, present);
    printf("{\"fn\":\"nous_policy_notice\",\"removed\":%d,\"present\":%d,\"out\":",
           removed, present);
    jstr(s);
    printf("}\n");
    free(s);
}

int main(void)
{
    /* ---- _normalize_tier ---- */
    emit_tier("Anonymous");
    emit_tier("  REGISTERED  ");
    emit_tier("");
    emit_tier(NULL);

    /* ---- _is_anonymous_tier ---- */
    emit_anon(1, 1);
    emit_anon(1, 0);
    emit_anon(0, 1);
    emit_anon(0, 0);

    /* ---- nous_policy_notice ---- */
    emit_notice(1, 1);
    emit_notice(1, 0);
    emit_notice(1, -1);
    emit_notice(0, 1);
    emit_notice(0, 0);

    return 0;
}
