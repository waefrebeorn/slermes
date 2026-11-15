/* t_port_port_display.c -- harness for the agent/display.py port. Emits JSONL. */
#include "port_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char PLUS_ONE[] = "{first} + {count} command";
static const char PLUS_MANY[] = "{first} + {count} commands";

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

static void jwords(char **w, int n)
{
    putchar('[');
    for (int i = 0; i < n; i++) {
        if (i) putchar(',');
        jstr(w[i]);
    }
    putchar(']');
}

static const char *const TEXTS[] = {
    "", " ", "  a  b  ", "a\nb", "a\tb\n c", "one two  three", " leading",
    "trailing ", "a   \n\n   b", "x", "aa", "aaaa", "aaaaa", "abcdefghij",
};

static const char *const SEGS[] = {
    "", "ls", "ls -la", "ls | head -5", "ls | head", "ls | wc -l", "ls | sort",
    "ls | uniq", "ls | grep x", "FOO=1 BAR=2 ls -la", "FOO=1", "   ",
    "echo ---", "echo \"a  b\"", "echo 'it''s'", "a > out.txt", "a >> out.txt",
    "a 2>&1", "a < in.txt", "a 2> err", "a &> all", "cd /x && ls", "ls; pwd",
    "echo start ---; ls; echo done", "echo $_exit=0", "echo $?",
    "echo ${X}", "echo $PIPESTATUS", "echo hi | grep x", "a|b", "a && b || c",
    "git status", "true", "export X=1", "ls -la | tail -3 && echo end",
    "python -c 'import os; print(1)'", "echo \"unterminated", "echo 'a; b'; ls",
};

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    for (size_t i = 0; i < sizeof(TEXTS) / sizeof(TEXTS[0]); i++) {
        char o[4096];
        st_oneline(TEXTS[i], o, sizeof(o));
        printf("{\"fn\":\"oneline\",\"arg\":");
        jstr(TEXTS[i]);
        printf(",\"out\":");
        jstr(o);
        fputs("}\n", stdout);
    }
    for (size_t i = 0; i < sizeof(TEXTS) / sizeof(TEXTS[0]); i++) {
        long lim[] = {0, 1, 2, 3, 4, 5, 10, -1};
        for (size_t k = 0; k < sizeof(lim) / sizeof(lim[0]); k++) {
            char o[4096];
            st_tail_trunc(TEXTS[i], lim[k], o, sizeof(o));
            printf("{\"fn\":\"tail\",\"arg\":");
            jstr(TEXTS[i]);
            printf(",\"lim\":%ld,\"out\":", lim[k]);
            jstr(o);
            fputs("}\n", stdout);
        }
    }
    for (size_t i = 0; i < sizeof(TEXTS) / sizeof(TEXTS[0]); i++) {
        size_t n[] = {0, 1, 3, 5, 25};
        for (size_t k = 0; k < sizeof(n) / sizeof(n[0]); k++) {
            char o[4096];
            st_clip(TEXTS[i], n[k], o, sizeof(o));
            printf("{\"fn\":\"clip\",\"arg\":");
            jstr(TEXTS[i]);
            printf(",\"n\":%zu,\"out\":", n[k]);
            jstr(o);
            fputs("}\n", stdout);
        }
    }
    for (size_t i = 0; i < sizeof(SEGS) / sizeof(SEGS[0]); i++) {
        char *w[64];
        int n = st_split_shell_words(SEGS[i], w, 0, 64);
        printf("{\"fn\":\"words\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":");
        jwords(w, n);
        fputs("}\n", stdout);
        for (int k = 0; k < n; k++)
            free(w[k]);

        char o[4096];
        st_strip_shell_pipe_tail(SEGS[i], o, sizeof(o));
        printf("{\"fn\":\"pipetail\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":");
        jstr(o);
        fputs("}\n", stdout);

        char *s[64];
        int ns = st_split_shell_compound(SEGS[i], s, 0, 64);
        printf("{\"fn\":\"compound\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":");
        jwords(s, ns);
        fputs("}\n", stdout);
        for (int k = 0; k < ns; k++)
            free(s[k]);

        char h[256];
        st_shell_head_word(SEGS[i], h, sizeof(h));
        printf("{\"fn\":\"head\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":");
        jstr(h);
        fputs("}\n", stdout);

        char c[4096];
        st_clean_shell_segment(SEGS[i], c, sizeof(c));
        printf("{\"fn\":\"clean\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":");
        jstr(c);
        fputs("}\n", stdout);

        printf("{\"fn\":\"echo\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":%d}\n", st_is_shell_boundary_echo(SEGS[i]));

        char sm[8192];
        st_summarize_shell_command(SEGS[i], PLUS_ONE, PLUS_MANY, sm, sizeof(sm));
        printf("{\"fn\":\"summary\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":");
        jstr(sm);
        fputs("}\n", stdout);
    }
    return 0;
}
