/*
 * t_port_update_receipt.c -- oracle harness for port_update_receipt.c
 */
#include "port_update_receipt.h"

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

static void emit_str(const char *fn, const char *label, const char *v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(v);
    printf("}\n");
}

static void emit_records(const char *label, char ***recs, int n, const char *const *keys, int pid)
{
    printf("{\"fn\":\"_str_records\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) {
        if (i) putchar(',');
        putchar('{');
        int first = 1;
        int base = pid ? 1 : 0;
        if (pid) {
            printf("\"pid\":");
            jstr(recs[i][0]);
            first = 0;
        }
        /* keys are not known here; emit positionally as k0..kN */
        for (int j = base; recs[i][j]; j++) {
            if (!first) putchar(',');
            first = 0;
            printf("\"%s\":", keys[j - base]);
            jstr(recs[i][j]);
        }
        putchar('}');
    }
    printf("]}\n");
}

static void free_records(char ***recs, int n)
{
    if (!recs) return;
    for (int i = 0; i < n; i++) {
        if (!recs[i]) continue;
        for (int j = 0; recs[i][j]; j++) free(recs[i][j]);
        free(recs[i]);
    }
    free(recs);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char *s;
    int n = 0;
    char ***recs;

    /* ---- _str_records ---- */
    static const char *e1[]  = { "pid", "42", "name", "build", "outcome", "success", NULL };
    static const char *e2[]  = { "pid", "0", "name", "test", "outcome", "failed", NULL };
    static const char *e3[]  = { "pid", "7", "name", "docs", NULL };
    static const char *e4[]  = { "name", "no-pid", NULL };
    static const char *const *es1[] = { e1, e2, e3, e4, NULL };
    static const char *const *es2[] = { NULL };
    static const char *k1[]  = { "name", "outcome", NULL };

    recs = ur_str_records((const char *const *const *)es1, k1, 0, &n);
    emit_records("plain", recs, n, k1, 0); free_records(recs, n);

    recs = ur_str_records((const char *const *const *)es1, k1, 1, &n);
    emit_records("with-pid", recs, n, k1, 1); free_records(recs, n);

    recs = ur_str_records((const char *const *const *)es2, k1, 0, &n);
    emit_records("empty", recs, n, k1, 0); free_records(recs, n);

    /* ---- _run_file ---- */
    static const char *d1[] = { "started_at", "2026-11-09T12:34:56+00:00", "pid", "4242", "update_id", "upd-1", NULL };
    static const char *d2[] = { "started_at", "", "pid", "0", "update_id", "upd-2", NULL };
    static const char *d3[] = { "update_id", "upd-3", NULL };
    s = ur_run_filename("C:/hermes/receipts", d1, NULL, 0);
    emit_str("_run_file", "plain", s); free(s);
    s = ur_run_filename("C:/hermes/receipts", d2, "20261109120000", 0);
    emit_str("_run_file", "fallback", s); free(s);
    s = ur_run_filename("C:/hermes/receipts", d3, "20261109120000", 4232);
    emit_str("_run_file", "no-stamp", s); free(s);
    s = ur_run_filename("C:/hermes/receipts", d1, NULL, 0);
    emit_str("_run_file", "repeatable", s); free(s);

    /* ---- _launcher_correlation_id ---- */
    static const char *v1[] = { "HERMES_UPDATE_CORRELATION_ID", "abc-123", NULL };
    static const char *v2[] = { "HERMES_UPDATE_CORRELATION_ID", "   ", NULL };
    static const char *v3[] = { "OTHER", "x", NULL };
    static const char *v4[] = { NULL };
    s = ur_launcher_corr_id(v1); emit_str("_launcher_correlation_id", "set", s); free(s);
    s = ur_launcher_corr_id(v2); emit_str("_launcher_correlation_id", "blank", s); free(s);
    s = ur_launcher_corr_id(v3); emit_str("_launcher_correlation_id", "absent", s); free(s);
    s = ur_launcher_corr_id(v4); emit_str("_launcher_correlation_id", "no-env", s); free(s);

    return 0;
}
