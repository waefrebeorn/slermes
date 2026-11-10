/*
 * t_port_slack_cli.c -- oracle harness for port_slack_cli.c
 */
#include "port_slack_cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const SLASHES[] = {
    "hermes", "Control the Hermes agent", "",
    "status", "Show the current status", "[section]",
    "model",  "Switch the active model",  "<name>",
    NULL
};

static void emit(const char *label, const char *bot, const char *desc,
                 int include_assistant, const char *mode, const char *long_desc)
{
    int err = 0;
    char *o = sl_build_full_manifest(bot, desc, include_assistant, mode, long_desc,
                                     SLASHES, &err);
    printf("{\"fn\":\"_build_full_manifest\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"err\":%d,\"out\":", err);
    if (!o) { fputs("null", stdout); }
    else {
        putchar('"');
        for (char *q = o; *q; q++) {
            if (*q == '"' || *q == '\\') { putchar('\\'); putchar(*q); }
            else putchar(*q);
        }
        putchar('"');
    }
    printf("}\n");
    free(o);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    emit("assistant-default", "Hermes", "Your Hermes agent on Slack", 1, NULL, NULL);
    emit("none",             "Hermes", "Your Hermes agent on Slack", 0, NULL, NULL);
    emit("agent",            "Hermes", "Your Hermes agent on Slack", 1, "agent", NULL);
    emit("assistant-upper",  "Hermes", "Your Hermes agent on Slack", 1, "  ASSISTANT  ", NULL);
    emit("long-desc",        "Hermes", "Short", 1, "assistant", "A much longer description.");
    emit("name-capped",      "This bot name is deliberately much longer than thirty five chars",
         "Desc", 1, "assistant", NULL);
    emit("desc-capped",      "Hermes",
         "This description is deliberately much longer than one hundred and forty characters so that the "
         "truncation path is actually exercised by this test case and not merely asserted.",
         1, "assistant", NULL);
    emit("empty-desc",       "Hermes", "", 1, "assistant", NULL);
    emit("null-desc",        "Hermes", NULL, 1, "assistant", NULL);
    emit("bad-mode",         "Hermes", "Desc", 1, "bogus", NULL);

    return 0;
}
