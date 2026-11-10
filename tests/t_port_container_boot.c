/*
 * t_port_container_boot.c -- oracle harness for port_container_boot.c
 */
#include "port_container_boot.h"

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

static void emit_strip(const char *label, const char *const *argv)
{
    int n = 0;
    const char **out = cb_strip_argv_prefix(argv, &n);
    printf("{\"fn\":\"_strip_container_argv_prefix\",\"case\":");
    jstr(label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) { if (i) putchar(','); jstr(out[i]); }
    printf("]}\n");
    free(out);
}

static void emit_pred(const char *label, const char *fn,
                      const char *const *argv, int v)
{
    printf("{\"fn\":");
    jstr(fn);
    printf(",\"case\":");
    jstr(label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

static void emit_notice(const char *label, const char *const *folded)
{
    char *s = cb_boot_notice(folded);
    printf("{\"fn\":\"boot_notice\",\"case\":");
    jstr(label);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

int main(void)
{
    /* ---- _strip_container_argv_prefix ---- */
    static const char *a1[] = { "s6-init", "/etc/s6/main-wrapper.sh", "gateway", "run", NULL };
    static const char *a2[] = { "main-wrapper.sh", "gateway", "run", NULL };
    static const char *a3[] = { "init", "gateway", "run", NULL };
    static const char *a4[] = { "entrypoint-dispatch.sh", "dashboard", NULL };
    static const char *a5[] = { "gateway", "run", NULL };
    static const char *a6[] = { "/opt/hermes/bin/main-wrapper.sh", "--no-supervise", "gateway", "run", NULL };
    static const char *a7[] = { NULL };

    emit_strip("wrapper-mid", a1);
    emit_strip("wrapper-first", a2);
    emit_strip("init-defensive", a3);
    emit_strip("entrypoint", a4);
    emit_strip("plain", a5);
    emit_strip("wrapper-with-flag", a6);
    emit_strip("empty", a7);

    /* ---- _is_legacy_gateway_run_request ---- */
    emit_pred("legacy-run", "_is_legacy_gateway_run_request", a5,
              cb_is_legacy_gateway_run(a5));
    emit_pred("legacy-via-wrapper", "_is_legacy_gateway_run_request", a1,
              cb_is_legacy_gateway_run(a1));
    emit_pred("legacy-no-supervise", "_is_legacy_gateway_run_request", a6,
              cb_is_legacy_gateway_run(a6));
    emit_pred("not-legacy", "_is_legacy_gateway_run_request", a4,
              cb_is_legacy_gateway_run(a4));

    /* ---- _is_dashboard_container ---- */
    emit_pred("dashboard", "_is_dashboard_container", a4,
              cb_is_dashboard_container(a4));
    emit_pred("not-dashboard", "_is_dashboard_container", a5,
              cb_is_dashboard_container(a5));
    emit_pred("dashboard-substr-not-first", "_is_dashboard_container", a1,
              cb_is_dashboard_container(a1));

    /* ---- boot_notice ---- */
    static const char *f1[] = { "beta", "alpha", "gamma", NULL };
    static const char *f2[] = { NULL };
    static const char *f3[] = { "solo", NULL };
    emit_notice("three", f1);
    emit_notice("empty", f2);
    emit_notice("one", f3);

    return 0;
}
