/* t_port_repetition_guard.c — Oracle harness: emits each fixture text plus the
 * C result of every repetition check; sta_oracle recomputes all on the same
 * text against the LIVE Python module. */
#include "port_repetition_guard.h"

#include <stdio.h>
#include <string.h>

static void jstr(const char *s)
{
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        default:   fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

static void emit_dom(const char *case_, const char *text)
{
    int n = (int)strlen(text);
    printf("{\"family\":\"dom\",\"case\":\"%s\",\"text\":", case_);
    jstr(text);
    printf(",\"line\":%d,\"window\":%d,\"periodic\":%d,\"dominated\":%d,\"runaway\":%d}\n",
           rg_line_repetition_dominated(text, n),
           rg_window_count_dominated(text, n),
           rg_periodic_run_dominated(text, n),
           rg_is_repetition_dominated(text),
           rg_is_runaway_repetition(text));
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    char d8[2048];
    size_t off = 0;
    for (int i = 0; i < 24 && off + 40 < sizeof(d8); i++)
        off += (size_t)snprintf(d8 + off, sizeof(d8) - off,
                                "INSERT INTO t VALUES (%d, 'row%d-data');\n", i, i);

    static const char *const prose =
        "linux kernel modules are loaded with insmod and removed with rmmod today "
        "the scheduler picks the next runnable task while the memory manager "
        "reclaims pages under pressure and the block layer batches requests "
        "across devices the network stack decodes tcp segments in the input path "
        "then the filesystem journals its transaction before the log is flushed "
        "to the device after that file the dispatcher balances work across the "
        "pool meanwhile the profiler samples a stack and the tracer records the "
        "latency window each subsystem publishes its counters in a metrics array ";
    char d9[2048];
    snprintf(d9, sizeof(d9), "%s%s", prose, prose); /* >400, some repeated phrases */

    emit_dom("d0", "abababababababababababababababababababab"); /* 40 <400 */
    emit_dom("d8", d8);      /* 24 distinct INSERT rows, ~900 chars */
    emit_dom("d9", d9);      /* prose, prose repeated -> periodic-ish */

    static const char *d1 = "QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ"; /* 450 Q */
    static const char *d2 = "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"; /* 520 */
    static const char *d3 = "INSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\nINSERT INTO t VALUES (1,2);\n";
    static const char *d5 = "looploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooploplooplop"; /* 480 = loop(8)*60 */
    static const char *d6 = "HIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHIHI"; /* 510 = HI(3)*170 */
    static const char *d7 = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()<>?;:~`abcd";
    emit_dom("d1", d1);
    emit_dom("d2", d2);
    emit_dom("d3", d3);
    emit_dom("d5", d5);
    emit_dom("d6", d6);
    emit_dom("d7", d7); /* ~72 chars <400 -> dominated 0 */

    /* _expand_run: "abcabcabcabcabc" start=0 period=3 matched=3 */
    {
        int l = -1, r = -1;
        const char *t = "abcabcabcabcabc";
        rg_expand_run(t, 15, 0, 3, 3, &l, &r);
        printf("{\"family\":\"expand\",\"case\":\"e0\",\"n\":15,\"start\":0,\"period\":3,\"matched\":3,\"left\":%d,\"right\":%d}\n", l, r);
    }
    /* _expand_run with noise on the left: "  abcabcabcabc" start=2 period=3 matched=3 */
    {
        int l = -1, r = -1;
        const char *t = "xxabcabcabcabcabc";
        rg_expand_run(t, 17, 2, 3, 3, &l, &r);
        printf("{\"family\":\"expand\",\"case\":\"e1\",\"n\":17,\"start\":2,\"period\":3,\"matched\":3,\"left\":%d,\"right\":%d,\"text\":", l, r);
        jstr(t);
        printf("}\n");
    }
    return 0;
}
