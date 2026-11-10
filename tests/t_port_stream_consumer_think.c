/* t_port_stream_consumer_think.c — oracle harness for StreamThinkFilterMixin
 * _at_block_boundary + _earliest_open_tag. Emits {"tag",accumulated,buf,lower?"","out"}.
 * For boundary: {"tag":"bd","accumulated",buf,idx,out}. For open tag:
 * {"tag":"ot","accumulated",buf,lower_buf,idx_out,len_out}. */
#include "port_stream_consumer_think.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

static char *lowercopy(const char *s)
{
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        o[i] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
    }
    o[n] = '\0';
    return o;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ------- _at_block_boundary ------- */
    {
        struct { const char *acc; const char *buf; int idx; } cases[] = {
            {"", "x <thinking> y", 2},
            {"", "  <thinking>", 2},
            {"", "foo<thinking>", 5},
            {"start\n", "  <reasoning>", 3},
            {"prose\n", "x <\xe6\x80\x9d\xe8\x80\x83>", 4},
            {"a", "x\n  <thinking>", 8},
            {"\n", " <thought>", 1},
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            int out = sct_at_block_boundary(cases[i].acc, cases[i].buf, cases[i].idx);
            printf("{\"tag\":\"bd\",\"acc\":"); jstr(cases[i].acc);
            printf(",\"buf\":"); jstr(cases[i].buf);
            printf(",\"idx\":%d,\"out\":%s}\n", cases[i].idx, out ? "true" : "false");
        }
    }

    /* ------- _earliest_open_tag ------- */
    {
        struct { const char *acc; const char *buf; } cases[] = {
            {"", "aa <thinking>"},
            {"start\n", "x\n  <reasoning> x <thought>"},
            {"", "just text"},
            {"", " has <thinking>"},
            {"", "<\xe6\x80\x9d\xe8\x80\x83> in \xe6\x8e\xa8\xe7\x90\x86}"},
            {"line\n", "cont <thinking> then <reasoning>"},
            {"", "\xe6\x80\x9d\xe8\x80\x83 before <thinking>"},  /* CJK before tag */
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            char *lb = lowercopy(cases[i].buf);
            int idx, len;
            int found = sct_earliest_open_tag(cases[i].acc, cases[i].buf, lb, &idx, &len);
            printf("{\"tag\":\"ot\",\"acc\":"); jstr(cases[i].acc);
            printf(",\"buf\":"); jstr(cases[i].buf);
            printf(",\"lower\":"); jstr(lb);
            printf(",\"found\":%s,\"idx\":%d,\"len\":%d}\n",
                   found ? "true" : "false", idx, len);
            free(lb);
        }
    }
    return 0;
}
