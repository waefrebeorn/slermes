/* gate: shell_quote_context(template, position).
 * Fixture form: "<pos>|<template>" -- position first so the template may contain anything. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
json_t *w14_ttscommand_bd97e2_shell_quote_context(const struct json_t *command_template, int position);
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        char work[4096]; snprintf(work, sizeof work, "%s", argv[i]);
        char *bar = strchr(work, '|');
        int pos = 0;
        const char *tpl = work;
        if (bar) { *bar = 0; pos = atoi(work); tpl = bar + 1; }
        WCUP_BEGIN("shell_quote_context");
        json_t *j = json_string(tpl);
        json_t *r = w14_ttscommand_bd97e2_shell_quote_context(j, pos);
        const char *s = (r && r->type == JSON_STRING) ? r->str_val : "None";
        WCUP_VALSTR(s ? s : "None");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("shell_quote_context");
        if (r) json_free(r);
        if (j) json_free(j);
    }
    return 0;
}
