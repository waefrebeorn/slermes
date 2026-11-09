/*
 * t_port_local_models.c -- oracle harness for port_local_models.c
 */
#include "port_local_models.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else putchar(*p);
    }
    putchar('"');
}

static void emit_gb(const char *label, double n)
{
    char *out = lm_human_gb(n);
    printf("{\"fn\":\"_human_gb\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
}

static void emit_k(const char *label, long long tokens)
{
    char *out = lm_k_label(tokens);
    printf("{\"fn\":\"_k_label\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
}

static void emit_rate(const char *label, const double *samples, int n,
                      long long done, long long total)
{
    int have_rate = 0, have_eta = 0;
    double rate = 0; long long eta = 0;
    lm_rate_and_eta(samples, n, done, total, &have_rate, &rate, &have_eta, &eta);
    printf("{\"fn\":\"_rate_and_eta\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"have_rate\":%s,\"rate\":", have_rate ? "true" : "false");
    if (have_rate) printf("%.6f", rate); else fputs("null", stdout);
    printf(",\"have_eta\":%s,\"eta\":", have_eta ? "true" : "false");
    if (have_eta) printf("%lld", eta); else fputs("null", stdout);
    printf("}\n");
}

static void emit_trim(const char *label, double *samples, int n, double now)
{
    int kept = lm_trim_sample_window(samples, n, now);
    printf("{\"fn\":\"_record_rate_window\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":[");
    for (int i = 0; i < kept; i++) {
        if (i) putchar(',');
        printf("%.3f,%.0f", samples[i * 2], samples[i * 2 + 1]);
    }
    printf("]}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- _human_gb ---- */
    emit_gb("one",     1073741824.0);
    emit_gb("half",    536870912.0);
    emit_gb("zero",    0.0);
    emit_gb("fraction", 1610612736.0);
    emit_gb("small",   1.0);

    /* ---- _k_label ---- */
    emit_k("exact",   2048);
    emit_k("under",   2047);
    emit_k("over",    2049);
    emit_k("zero",    0);
    emit_k("negative", -1024);
    emit_k("large",   1234567);

    /* ---- _rate_and_eta ---- */
    static const double s1[]  = { 0.0, 0.0, 1.0, 100.0 };
    static const double s2[]  = { 0.0, 0.0 };
    static const double s3[]  = { 0.0, 0.0, 0.25, 100.0 };
    static const double s4[]  = { 0.0, 100.0, 1.0, 100.0 };
    static const double s5[]  = { 0.0, 0.0, 2.0, 500.0, 4.0, 900.0 };
    static const double s6[]  = { 0.0, 0.0, 1.0, 100.0 };
    emit_rate("normal",     s1, 2, 100, 200);
    emit_rate("one-sample", s2, 1, 100, 200);
    emit_rate("too-short",  s3, 2, 100, 200);
    emit_rate("not-moved",  s4, 2, 100, 200);
    emit_rate("window",     s5, 3, 900, 1000);
    emit_rate("no-total",   s6, 2, 100, 0);
    emit_rate("finished",   s1, 2, 200, 200);

    /* ---- _record_rate window trim ---- */
    double w1[] = { 0.0, 0.0, 1.0, 50.0, 2.0, 100.0 };
    emit_trim("keep-all", w1, 3, 2.0);

    double w2[] = { 0.0, 0.0, 1.0, 50.0, 10.0, 100.0 };
    emit_trim("trim-old", w2, 3, 10.0);

    double w3[] = { 0.0, 0.0 };
    emit_trim("keep-one", w3, 1, 100.0);

    double w4[] = { 0.0, 0.0, 8.0, 50.0, 16.0, 100.0 };
    emit_trim("trim-to-one", w4, 3, 16.0);

    return 0;
}
