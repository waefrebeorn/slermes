/* t_port_port_inline_source.c -- harness for the status_inline_source.py port.
 * Emits JSONL: {"fn":"flag","tok":[...],"out":N}. */
#include "port_inline_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else putchar(*p);
        }
    }
    putchar('"');
}

static void jarr(const char **t, size_t n)
{
    putchar('[');
    for (size_t i = 0; i < n; i++) {
        if (i) putchar(',');
        jstr(t[i]);
    }
    putchar(']');
}

/* posix=False shlex: whitespace splits, quotes are PRESERVED as characters. */
static size_t split(const char *s, const char **out, size_t cap)
{
    size_t n = 0;
    const char *p = s;
    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        const char *start = p;
        while (*p && *p != ' ' && *p != '\t') {
            if (*p == '\\' && p[1]) {
                p += 2;
                continue;
            }
            p++;
        }
        if (n < cap)
            out[n] = strndup(start, (size_t)(p - start));
        n++;
    }
    return n;
}

static const char *const CASES[] = {
    "python -c src.py",
    "python -c src.py a b",
    "python -X utf8 -c src.py",
    "python -Xutf8 -c src.py",
    "python -Xc src.py",
    "python -uc",
    "python -IsB -c src.py",
    "python -q -c src.py",
    "python -Q -c src.py",
    "python -m hermes_cli.main gateway run",
    "python -m gateway.run",
    "python --jit -c src.py",
    "python --check-hash-based-pycs always -c src.py",
    "python --check-hash-based-pycs -c src.py",
    "python --jit=always -c src.py",
    "python -- -c src.py",
    "python -c",
    "python",
    "-c src.py",
    "python -b -B -c src.py",
    "python -W ignore -c src.py",
    "python -Wignore -c src.py",
    "python -X utf8 -X dev -c src.py",
    "python -c src.py -m hermes_cli.main gateway run",
    "",
    "python -qq -c x",
    "python -c  src.py",
    "python -m -c src.py",
    "python -W -c src.py",
    "python -Xdev -c x",
};

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
        const char *tok[64];
        size_t n = split(CASES[i], tok, 64);
        int idx = st_inline_source_flag_index(tok, n);
        printf("{\"fn\":\"flag\",\"cmd\":");
        jstr(CASES[i]);
        printf(",\"tok\":");
        jarr(tok, n);
        printf(",\"out\":%d}\n", idx);
        for (size_t k = 0; k < n && k < 64; k++)
            free((void *)tok[k]);
    }
    return 0;
}
