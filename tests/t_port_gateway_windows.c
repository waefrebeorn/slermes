/*
 * t_port_gateway_windows.c -- oracle harness for port_gateway_windows.c
 */
#include "port_gateway_windows.h"

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

static void emit_argv(const char *label, const char *const *out)
{
    printf("{\"fn\":\"_gateway_run_argv\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":[");
    for (int i = 0; out && out[i]; i++) {
        if (i) putchar(',');
        jstr(out[i]);
    }
    printf("]}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char *s, *err = NULL;

    /* ---- _normalize_windows_path ---- */
    s = gw_normalize_path("  \"C:\\\\Users\\\\Hermes\\\\\"  ");
    emit_str("_normalize_windows_path", "quoted", s); free(s);
    s = gw_normalize_path("C:/Users/Hermes/");
    emit_str("_normalize_windows_path", "trailing", s); free(s);
    s = gw_normalize_path("  ");
    emit_str("_normalize_windows_path", "blank", s); free(s);
    s = gw_normalize_path(NULL);
    emit_str("_normalize_windows_path", "null", s); free(s);

    /* ---- _sanitize_filename ---- */
    s = gw_sanitize_filename("a<b>c:d\"e/f\\g|h?i*j");
    emit_str("_sanitize_filename", "illegal", s); free(s);
    s = gw_sanitize_filename("plain-name.txt");
    emit_str("_sanitize_filename", "plain", s); free(s);
    s = gw_sanitize_filename("tab\there");
    emit_str("_sanitize_filename", "control", s); free(s);

    /* ---- _quote_vbs_string ---- */
    s = gw_quote_vbs("plain", &err);
    emit_str("_quote_vbs_string", "plain", s); free(s); free(err); err = NULL;
    s = gw_quote_vbs("say \"hi\"", &err);
    emit_str("_quote_vbs_string", "quotes", s); free(s); free(err); err = NULL;
    s = gw_quote_vbs("line1\nline2", &err);
    emit_str("_quote_vbs_string", "newline", s ? s : "");
    free(s); free(err); err = NULL;

    /* ---- _is_access_denied ---- */
    emit_bool("_is_access_denied", "en",  gw_is_access_denied("Access is denied."));
    emit_bool("_is_access_denied", "es",  gw_is_access_denied("Acceso denegado"));
    emit_bool("_is_access_denied", "zh",  gw_is_access_denied("拒绝访问"));
    emit_bool("_is_access_denied", "none",gw_is_access_denied("all good"));
    emit_bool("_is_access_denied", "null",gw_is_access_denied(NULL));

    /* ---- _should_fall_back ---- */
    emit_bool("_should_fall_back", "timeout",  gw_should_fall_back(124, ""));
    emit_bool("_should_fall_back", "denied",   gw_should_fall_back(1, "Access is denied"));
    emit_bool("_should_fall_back", "schtasks",gw_should_fall_back(1, "schtasks timed out"));
    emit_bool("_should_fall_back", "nooutput",gw_should_fall_back(1, "schtasks produced no output"));
    emit_bool("_should_fall_back", "clean",    gw_should_fall_back(0, "ok"));
    emit_bool("_should_fall_back", "other",    gw_should_fall_back(1, "something else"));

    /* ---- _gateway_run_argv ---- */
    static const char *pa1[] = { "--profile", "work", NULL };
    static const char *pa0[] = { NULL };
    int n = 0;
    char **av;
    av = gw_run_argv("python.exe", pa1, &n);
    emit_argv("profile", (const char *const *)av);
    for (int i = 0; i < n; i++) free(av[i]);
    free(av);
    av = gw_run_argv("python.exe", pa0, &n);
    emit_argv("no-profile", (const char *const *)av);
    for (int i = 0; i < n; i++) free(av[i]);
    free(av);

    /* ---- hermes_owns_windows_service ---- */
    static const char *roots[] = { "C:\\hermes", NULL };
    emit_bool("hermes_owns_windows_service", "name",   gw_owns_service("Hermes Gateway", "C:\\x\\y.exe", roots));
    emit_bool("hermes_owns_windows_service", "under",  gw_owns_service("Schedule", "C:\\hermes\\bin\\g.exe", roots));
    emit_bool("hermes_owns_windows_service", "outside",gw_owns_service("Schedule", "C:\\windows\\s.exe", roots));
    emit_bool("hermes_owns_windows_service", "empty-root", gw_owns_service("Schedule", "C:\\hermes\\g.exe", pa0));
    emit_bool("hermes_owns_windows_service", "null-name", gw_owns_service(NULL, "C:\\hermes\\g.exe", roots));

    return 0;
}
