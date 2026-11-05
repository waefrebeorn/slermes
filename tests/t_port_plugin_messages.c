/* t_port_plugin_messages.c — oracle harness for _err, _fit, _supports_edit. */
#include "port_plugin_messages.h"

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

    /* _err: (code, detail). */
    static const char *const er[][2] = {
        {"E_CAP", "no cap"}, {"BAD", ""}, {"X", "has \"quotes\"\\"}, {"", ""},
    };
    for (int i = 0; i < 4; i++) {
        char *o = plm_err(er[i][0], er[i][1]);
        printf("{\"family\":\"err\",\"case\":%d,\"code\":", i);
        jstr(er[i][0]);
        printf(",\"detail\":");
        jstr(er[i][1]);
        printf(",\"out\":%s}\n", o);
        free(o);
    }

    /* _fit: (text, limit). */
    static const struct { const char *t; int l; } ft[] = {
        {"short", 100}, {"hello world", 8}, {"hello world", 6}, {"a", 0},
        {"abcdefghij", 5}, {"", 3}, {"exactlen", 8},
    };
    for (int i = 0; i < 7; i++) {
        char *o = plm_fit(ft[i].t, ft[i].l);
        printf("{\"family\":\"fit\",\"case\":%d,\"text\":", i);
        jstr(ft[i].t);
        printf(",\"limit\":%d,\"out\":", ft[i].l);
        jstr(o);
        printf("}\n");
        free(o);
    }

    /* _supports_edit: (flag, has_custom). */
    static const int se[][2] = {{1,1},{1,0},{0,1},{0,0}};
    for (int i = 0; i < 4; i++) {
        int o = plm_supports_edit(se[i][0], se[i][1]);
        printf("{\"family\":\"supp\",\"case\":%d,\"flag\":%d,\"custom\":%d,\"out\":%d}\n",
               i, se[i][0], se[i][1], o);
    }
    return 0;
}
