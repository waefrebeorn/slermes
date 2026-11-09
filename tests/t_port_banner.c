/*
 * t_port_banner.c -- oracle harness for port_banner.c
 *
 * Emits one JSON object per line; the oracle replays the live banner.py defs.
 */
#include "port_banner.h"

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

static void emit_plural(int n, const char *word)
{
    char *s = banner_plural(n, word);
    printf("{\"fn\":\"_plural\",\"in\":%d,\"word\":", n);
    jstr(word);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

static void emit_ctx(long long tokens)
{
    char *s = banner_format_ctx_length(tokens);
    printf("{\"fn\":\"_format_context_length\",\"in\":%lld,\"out\":", tokens);
    jstr(s);
    printf("}\n");
    free(s);
}

static void emit_toolset(const char *name)
{
    char *s = banner_toolset_name(name);
    printf("{\"fn\":\"_display_toolset_name\",\"in\":");
    jstr(name);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

int main(void)
{
    /* ---- _plural ---- */
    emit_plural(1, "commit");
    emit_plural(0, "commit");
    emit_plural(2, "commit");
    emit_plural(-1, "model");

    /* ---- _format_context_length ---- */
    emit_ctx(0);
    emit_ctx(1);
    emit_ctx(999);
    emit_ctx(1000);
    emit_ctx(128000);
    emit_ctx(123456);
    emit_ctx(1048576);
    emit_ctx(1500000);
    emit_ctx(999999);
    emit_ctx(1000000000LL);

    /* ---- _display_toolset_name ---- */
    emit_toolset("code_tools");
    emit_toolset("code");
    emit_toolset("_tools");
    emit_toolset("");
    emit_toolset(NULL);

    return 0;
}
