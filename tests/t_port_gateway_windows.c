/* t_port_gateway_windows.c -- oracle harness for the added slice of
 * hermes_cli/gateway_windows.py. Emits one JSON line per case. */
#include "port_gateway_windows.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cs = 0;

static void jstr(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar(ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

static void quote_case(const char *v)
{
    char *err = NULL;
    char *out = gw_quote_cmd(v, &err);
    printf("{\"case\":%d,\"fn\":\"quote_cmd\",\"in\":{\"value\":");
    jstr(v);
    printf("},\"out\":");
    if (out)
        jstr(out);
    else
        printf("null");
    printf("}\n");
    fflush(stdout);
    free(out);
    free(err);
}

static void pp_case(const char *entries_json, const char *existing)
{
    /* Parse the JSON array into a NULL-terminated argv-like list. */
    const char *arr[32];
    int n = 0;
    const char *p = entries_json;
    while (*p && *p != '[') p++;
    if (*p == '[') p++;
    while (*p && *p != ']' && n < 32) {
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        if (*p == ']' || !*p) break;
        if (*p == '"') {
            p++;
            static char bufs[32][256];
            size_t k = 0;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) {
                    p++;
                    bufs[n][k++] = *p++;
                } else {
                    bufs[n][k++] = *p++;
                }
            }
            if (*p == '"') p++;
            bufs[n][k] = '\0';
            arr[n] = bufs[n];
            n++;
        } else {
            /* null or bare token: treat as an empty (falsy) entry */
            const char *start = p;
            while (*p && *p != ',' && *p != ']') p++;
            if (p - start == 4 && strncmp(start, "null", 4) == 0) {
                arr[n++] = "";
            } else {
                static char tok[32][64];
                size_t l = (size_t)(p - start);
                if (l > 63) l = 63;
                memcpy(tok[n], start, l);
                tok[n][l] = '\0';
                arr[n] = tok[n];
                n++;
            }
        }
    }

    char *out = gw_prepend_pythonpath(arr, n, existing);
    printf("{\"case\":%d,\"fn\":\"prepend_pythonpath\",\"in\":{\"entries\":%s,"
           "\"existing\":", ++cs, entries_json);
    jstr(existing);
    printf("},\"out\":");
    if (out)
        jstr(out);
    else
        printf("null");
    printf("}\n");
    fflush(stdout);
    free(out);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* _quote_cmd_script_arg */
    quote_case("");
    quote_case("abc");
    quote_case("a b");
    quote_case("a\tb");
    quote_case("a\"b");
    quote_case("\"\"");
    quote_case(" a ");
    quote_case("a\\b");
    quote_case("C:/Program Files/hermes/python.exe");
    quote_case("--profile");
    quote_case("-x");
    quote_case("a\rb");
    quote_case("a\nb");
    quote_case("a b\"c");
    quote_case(" ");
    quote_case("\"");

    /* _stdin_is_interactive -- cover the console_mode_ok triple
     * (unknown / false / true) against both isatty values. */
    {
        int tty[] = { 0, 1 };
        int cm[] = { -1, 0, 1 };
        for (int t = 0; t < 2; t++) {
            for (int c = 0; c < 3; c++) {
                int r = gw_stdin_interactive(tty[t], cm[c]);
                printf("{\"case\":%d,\"fn\":\"stdin_interactive\",\"in\":{\"isatty\":%s,"
                       "\"console_mode_ok\":%s},\"out\":%s}\n",
                       ++cs, tty[t] ? "true" : "false",
                       cm[c] == -1 ? "null" : (cm[c] ? "true" : "false"),
                       r ? "true" : "false");
            }
        }
        fflush(stdout);
    }

    /* _prepend_pythonpath */
    pp_case("[]", "");
    pp_case("[\"\"]", "");
    pp_case("[\"\",\"\"]", "");
    pp_case("[\"a\"]", "");
    pp_case("[\"a\"]", "b");
    pp_case("[\"a\",\"b\"]", "");
    pp_case("[\"a\",\"b\"]", "c");
    pp_case("[\"a\",\"\",\"\"]", "c");
    pp_case("[\"\",\"a\"]", "");
    pp_case("[\"a;b\"]", "");
    pp_case("[\"a\",\"b\",\"c\"]", "x;y");

    return 0;
}
