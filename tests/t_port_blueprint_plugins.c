/* t_port_blueprint_plugins.c — oracle harness for cron/blueprint_plugins.py
 * pure validators: _text, _strings, _placeholders. */
#include "port_blueprint_plugins.h"

#include <stdio.h>
#include <stdlib.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* _text: value -> stripped or error. */
    static const char *const txt[] = {
        "Hello", "  spaced  ", "", "   ", NULL, "a\tb",
    };
    for (size_t i = 0; i < sizeof(txt) / sizeof(txt[0]); i++) {
        char *out = NULL;
        int ok = bp_text("title", txt[i], &out);
        printf("{\"family\":\"text\",\"case\":%zu,\"in\":", i);
        jstr(txt[i]);
        if (ok == 0) {
            printf(",\"out\":");
            jstr(out);
            printf("}\n");
            free(out);
        } else {
            printf(",\"err\":1}\n");
        }
    }

    /* _strings: JSON array -> 0 or error. */
    static const char *const lst[] = {
        "[]", "[\"a\",\"b\"]", "[\" a \"]", "[\"\"]", "[\"x\",5]",
        "\"notarray\"", "[null]", NULL,
    };
    for (size_t i = 0; i < sizeof(lst) / sizeof(lst[0]); i++) {
        int ok = bp_strings("tags", lst[i]);
        printf("{\"family\":\"strings\",\"case\":%zu,\"in\":", i);
        jstr(lst[i]);
        printf(",\"ok\":%d}\n", ok == 0 ? 1 : 0);
    }

    /* _placeholders: template -> JSON name array or error. */
    static const char *const tmpl[] = {
        "no fields here",
        "{name} and {other}",
        "mix {{literal}} {a} {b}!{conv} {c:width}",
        "{Name_1} ok",
        "{0} positional",        /* bad name -> ValueError */
        "{} auto",                /* skipped */
        "{bad name!x}",           /* bad -> ValueError */
        "{nested:{width}}",
        "}}escaped{{",            /* escapes only */
        "{",                      /* unbalanced -> ValueError */
    };
    for (size_t i = 0; i < sizeof(tmpl) / sizeof(tmpl[0]); i++) {
        char *out = NULL;
        int ok = bp_placeholders(tmpl[i], "prompt_template", &out);
        printf("{\"family\":\"ph\",\"case\":%zu,\"in\":", i);
        jstr(tmpl[i]);
        if (ok == 0) {
            printf(",\"out\":%s}\n", out);
            free(out);
        } else {
            printf(",\"err\":1}\n");
        }
    }
    return 0;
}
