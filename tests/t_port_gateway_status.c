/*
 * t_port_gateway_status.c -- oracle harness for port_gateway_status.c
 */
#include "port_gateway_status.h"

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

static void emit_bool(const char *fn, const char *label, int v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char *s;

    /* ---- _scope_hash ---- */
    s = gs_scope_hash("profile:work");
    emit_str("_scope_hash", "plain", s); free(s);
    s = gs_scope_hash("");
    emit_str("_scope_hash", "empty", s); free(s);
    s = gs_scope_hash(NULL);
    emit_str("_scope_hash", "null", s); free(s);

    /* ---- _start_times_agree ---- */
    static const char *r1[] = { "1700000000.001", "1700000000.002", NULL };
    static const char *r2[] = { "1700000000.001", "1700000000.500", NULL };
    static const char *r3[] = { "0", NULL };
    static const char *r4[] = { NULL };
    emit_bool("_start_times_agree", "agree",   gs_start_times_agree("1700000000.000", r1));
    emit_bool("_start_times_agree", "drift",   gs_start_times_agree("1700000000.000", r2));
    emit_bool("_start_times_agree", "zero",    gs_start_times_agree("1700000000.000", r3));
    emit_bool("_start_times_agree", "cur-zero",gs_start_times_agree("0", r1));
    emit_bool("_start_times_agree", "none",    gs_start_times_agree("1700000000.000", r4));

    /* ---- start_time_fingerprints_match ---- */
    emit_bool("start_time_fingerprints_match", "same",   gs_fingerprints_match("170000000000", "170000000000", 200));
    emit_bool("start_time_fingerprints_match", "drift",  gs_fingerprints_match("170000000000", "170000000100", 200));
    emit_bool("start_time_fingerprints_match", "far",    gs_fingerprints_match("170000000000", "170000001000", 200));
    emit_bool("start_time_fingerprints_match", "tight",  gs_fingerprints_match("170000000000", "170000000005", 4));

    /* ---- scoped_lock_owner_label ---- */
    static const char *l1[] = { "profile", "  work  ", NULL };
    static const char *l2[] = { "profile", "BAD LABEL", "hermes_home", "C:/hermes/profiles/work", NULL };
    static const char *l3[] = { "hermes_home", "C:/hermes/profiles/work", NULL };
    static const char *l4[] = { "hermes_home", "C:/hermes", NULL };
    static const char *l5[] = { NULL };
    s = gs_lock_owner_label(l1, "C:/hermes"); emit_str("scoped_lock_owner_label", "profile", s); free(s);
    s = gs_lock_owner_label(l2, "C:/hermes"); emit_str("scoped_lock_owner_label", "from-home", s); free(s);
    s = gs_lock_owner_label(l3, "C:/hermes"); emit_str("scoped_lock_owner_label", "home-only", s); free(s);
    s = gs_lock_owner_label(l4, "C:/hermes"); emit_str("scoped_lock_owner_label", "root-home", s); free(s);
    s = gs_lock_owner_label(l5, "C:/hermes"); emit_str("scoped_lock_owner_label", "no-record", s); free(s);

    /* ---- _same_hermes_home ---- */
    emit_bool("_same_hermes_home", "same",   gs_same_hermes_home("C:/Hermes", NULL, "c:\\hermes", NULL));
    emit_bool("_same_hermes_home", "diff",   gs_same_hermes_home("C:/Hermes", NULL, "C:/Other", NULL));

    /* ---- retained_gateway_state ---- */
    static const char *rt1[] = { "desired_state", "running", "gateway_state", "startup_failed", NULL };
    static const char *rt2[] = { "desired_state", "running", "gateway_state", "degraded", "exit_reason", "loop_liveness_watchdog", NULL };
    static const char *rt3[] = { "desired_state", "stopped", "gateway_state", "startup_failed", NULL };
    static const char *rt4[] = { "desired_state", "running", "gateway_state", "degraded", "exit_reason", "other", NULL };
    static const char *rt5[] = { "desired_state", "running", "gateway_state", "running", NULL };
    static const char *rt6[] = { NULL };
    s = gs_retained_state(rt1); emit_str("retained_gateway_state", "failed", s); free(s);
    s = gs_retained_state(rt2); emit_str("retained_gateway_state", "degraded", s); free(s);
    s = gs_retained_state(rt3); emit_str("retained_gateway_state", "stopped", s); free(s);
    s = gs_retained_state(rt4); emit_str("retained_gateway_state", "degraded-other", s); free(s);
    s = gs_retained_state(rt5); emit_str("retained_gateway_state", "running", s); free(s);
    s = gs_retained_state(rt6); emit_str("retained_gateway_state", "no-runtime", s); free(s);

    return 0;
}
