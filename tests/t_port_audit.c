/* t_port_audit.c — oracle harness for _bounded_value.
 * Emits {"tag":"bnd","case":N,"in":<json>,"out":<json string|null>}. */
#include "port_audit.h"

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

static void one_case(int idx, const char *lit)
{
    json_t *doc = json_parse(lit, NULL);
    printf("{\"tag\":\"bnd\",\"case\":%d,\"in\":%s,\"out\":", idx, lit);
    if (!doc) { fputs("null", stdout); }
    else {
        json_t *b = port_audit_bounded_value(doc);
        char *s = json_serialize(b);
        jstr(s);
        free(s); json_free(b); json_free(doc);
    }
    printf("}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    one_case(0, "\"short\"");
    one_case(1, "123");
    one_case(2, "true");
    one_case(3, "null");
    one_case(4, "3.14");
    one_case(5, "[1, \"two\", true, null, 4.5]");
    one_case(6, "{\"a\":\"ok\",\"b\":123,\"c\":[1,2,3]}");
    one_case(7, "[\"x\",\"y\"]");

    /* 299-char string -> truncate to 241 + "...[truncated]" */
    {
        char lit[700]; int p = 0;
        lit[p++] = '"';
        for (int i = 0; i < 299; i++) lit[p++] = (char)('A' + (i % 26));
        lit[p++] = '"'; lit[p] = '\0';
        one_case(8, lit);
    }
    /* nested big string */
    {
        char nl[740]; int p = 0;
        nl[p++] = '{'; nl[p++] = '"'; nl[p++] = 'b'; nl[p++] = 'i'; nl[p++] = 'g';
        nl[p++] = '"'; nl[p++] = ':'; nl[p++] = '"';
        for (int i = 0; i < 299; i++) nl[p++] = (char)('A' + (i % 26));
        nl[p++] = '"'; nl[p++] = '}'; nl[p] = '\0';
        one_case(9, nl);
    }
    /* 257 'z' -> truncate; 200 'z' -> keep */
    {
        char l257[520]; int q = 0;
        l257[q++] = '"';
        for (int i = 0; i < 257; i++) l257[q++] = 'z';
        l257[q++] = '"'; l257[q] = '\0';
        one_case(10, l257);
    }
    return 0;
}
