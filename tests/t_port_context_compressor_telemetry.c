/* t_port_context_compressor_telemetry.c — oracle harness for
 * _begin_compression_telemetry + _safe_int.
 *
 * Field contract:
 *   current_tokens / context_length / threshold_tokens / seed are RAW JSON
 *   tokens (number or quoted string; object for seed) — passed to C as-is and
 *   emitted as-is.
 *   attempt_id / session_id / trigger_source / provider / model are PLAIN
 *   strings (may be NULL) — passed to C unquoted and emitted via jstr.
 */
#include "port_context_compressor_telemetry.h"

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
        case '\n': fputs("\\n", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

static void raw_or_null(const char *s)
{
    fputs(s ? s : "null", stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- _safe_int: value is a raw JSON token. */
    {
        static const char *const vals[] = {
            "120", "\"120\"", "\" 120 \"", "\"+120\"", "\"-7\"",
            "\"1e12\"", "\"12.5\"", "\"\"", "\"abc\"", "\"12\n\"", "\"4\"",
            "true", "false", "null", "1.5", "-1.5", "2.0", "0", "[1,2]", "{\"a\":1}",
        };
        for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
            int none;
            long long v = cct_safe_int(vals[i], &none);
            printf("{\"tag\":\"sf_int\",\"value\":%s,\"out\":{\"coerced\":%s",
                   vals[i], none ? "false" : "true");
            if (!none)
                printf(",\"out\":%lld", v);
            printf("}}\n");
        }
    }

    /* ---- _begin_compression_telemetry ---- */
    {
        struct {
            const char *id;
            const char *cur;    /* raw json token (number or quoted str) */
            const char *aid;    /* plain str or NULL */
            const char *sid;    /* plain str or NULL */
            const char *trg;    /* plain str or NULL */
            const char *prov;   /* plain str or NULL */
            const char *model_; /* plain str or NULL */
            const char *ctx;    /* raw json token */
            const char *thr;    /* raw json token */
            const char *seed;   /* raw json object or NULL */
        } cases[] = {
            {"base", "\"8000\"", "a1", "s1", "manual", "p", "m",
             "\"120000\"", "\"0\"", NULL},
            {"float-tokens", "1.5", "a2", NULL, NULL, "p2", "m2",
             "120000", "30000", NULL},
            {"seed-fill", "\"500\"", NULL, NULL, NULL, "p", "m",
             "\"120000\"", "\"400\"",
             "{\"attempt_id\":\"seedA\",\"session_id\":\"seedS\",\"trigger_source\":\"seedT\",\"overflow_reason\":\"over\"}"},
            {"bad-tokens", "\"9k\"", "a3", NULL, NULL, "", "",
             "\"notnum\"", "\"\"", NULL},
            {"seed-null-overflow", "\"1000\"", "a4", NULL, NULL, "p", "m",
             "\"120000\"", "\"10\"", "{\"overflow_reason\":null}"},
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            char *out = cct_begin_compression_telemetry(
                cases[i].cur, cases[i].aid, cases[i].sid, cases[i].trg,
                cases[i].prov, cases[i].model_, cases[i].ctx, cases[i].thr, cases[i].seed);
            printf("{\"tag\":\"begin\",\"case_id\":");
            jstr(cases[i].id);
            printf(",\"current_tokens\":"); raw_or_null(cases[i].cur);
            printf(",\"attempt_id\":"); jstr(cases[i].aid);
            printf(",\"session_id\":"); jstr(cases[i].sid);
            printf(",\"trigger_source\":"); jstr(cases[i].trg);
            printf(",\"facts\":{\"provider\":"); jstr(cases[i].prov);
            printf(",\"model\":"); jstr(cases[i].model_);
            printf(",\"context_length\":"); raw_or_null(cases[i].ctx);
            printf(",\"threshold_tokens\":"); raw_or_null(cases[i].thr);
            printf("},\"seed\":"); raw_or_null(cases[i].seed);
            printf(",\"out\":"); jstr(out);
            printf("}\n");
            free(out);
        }
    }
    return 0;
}
