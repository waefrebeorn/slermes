/*
 * t_port_portal_cli.c -- oracle harness for port_portal_cli.c
 */
#include "port_portal_cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit(const char *label, int managed, int active, const char *provider)
{
    int sel = pc_feature_state(managed, active, provider);
    char *text = pc_feature_state_text(sel, "nous", provider);
    printf("{\"fn\":\"_feature_state\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"selector\":%d,\"out\":", sel);
    if (!text) { fputs("null", stdout); }
    else {
        putchar('"');
        for (char *q = text; *q; q++) {
            if (*q == '"' || *q == '\\') { putchar('\\'); putchar(*q); }
            else putchar(*q);
        }
        putchar('"');
    }
    printf("}\n");
    free(text);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    emit("managed",        1, 1, "anthropic");
    emit("managed-notact", 1, 0, NULL);
    emit("active-provider",0, 1, "anthropic");
    emit("active-empty",   0, 1, "");
    emit("active-null",    0, 1, NULL);
    emit("inactive",       0, 0, NULL);
    emit("inactive-with",  0, 0, "openai");

    return 0;
}
