/*
 * t_port_hosted_room_driver.c -- oracle harness for port_hosted_room_driver.c
 */
#include "port_hosted_room_driver.h"

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

static void emit_num(const char *fn, const char *label, int ok, double v, const char *err)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":{\"%s\":", ok ? "ok" : "err");
    if (ok) printf("%.17g", v); else jstr(err);
    printf("}}\n");
}

static void emit_window(const char *label, int ok, double now, double exp, const char *err)
{
    printf("{\"fn\":\"_lease_window\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":{\"%s\":", ok ? "ok" : "err");
    if (ok) printf("[%.17g,%.17g]", now, exp); else jstr(err);
    printf("}}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char *s, *err = NULL;

    /* ---- _task_update ---- */
    s = hd_task_update("status='queued'", "status='running'");
    emit_str("_task_update", "plain", s); free(s);
    s = hd_task_update("", "");
    emit_str("_task_update", "empty", s); free(s);

    /* ---- _generation_update ---- */
    s = hd_generation_update("status=?, updated_at=?", "running");
    emit_str("_generation_update", "running", s); free(s);
    s = hd_generation_update("status=?", "stopping");
    emit_str("_generation_update", "stopping", s); free(s);

    /* ---- _finite ---- */
    int ok = 0;
    double v;
    v = hd_finite("1.5", &ok, 0, "msg", &err);
    emit_num("_finite", "plain", ok, v, err); free(err); err = NULL;
    v = hd_finite("-2", &ok, 0, "msg", &err);
    emit_num("_finite", "negative", ok, v, err); free(err); err = NULL;
    v = hd_finite("-2", &ok, 1, "ttl_seconds must be a finite positive number", &err);
    emit_num("_finite", "positive", ok, v, err); free(err); err = NULL;
    v = hd_finite("nan", &ok, 0, "msg", &err);
    emit_num("_finite", "nan", ok, v, err); free(err); err = NULL;
    v = hd_finite("inf", &ok, 0, "msg", &err);
    emit_num("_finite", "inf", ok, v, err); free(err); err = NULL;
    v = hd_finite("abc", &ok, 0, "msg", &err);
    emit_num("_finite", "junk", ok, v, err); free(err); err = NULL;

    /* ---- _timestamp ---- */
    v = hd_timestamp("1234.5", 1, &ok, &err);
    emit_num("_timestamp", "ok", ok, v, err); free(err); err = NULL;
    v = hd_timestamp("1234.5", 0, &ok, &err);
    emit_num("_timestamp", "not-callable", ok, v, err); free(err); err = NULL;
    v = hd_timestamp("nan", 1, &ok, &err);
    emit_num("_timestamp", "not-finite", ok, v, err); free(err); err = NULL;

    /* ---- _lease_window ---- */
    double now = 0, exp = 0;
    int w;
    w = hd_lease_window("30", "1000", 1, &now, &exp, &err);
    emit_window("ok", w, now, exp, err); free(err); err = NULL;
    w = hd_lease_window("0", "1000", 1, &now, &exp, &err);
    emit_window("zero-ttl", w, now, exp, err); free(err); err = NULL;
    w = hd_lease_window("-1", "1000", 1, &now, &exp, &err);
    emit_window("neg-ttl", w, now, exp, err); free(err); err = NULL;
    w = hd_lease_window("30", "1000", 0, &now, &exp, &err);
    emit_window("no-clock", w, now, exp, err); free(err); err = NULL;
    w = hd_lease_window("30", "nan", 1, &now, &exp, &err);
    emit_window("nan-clock", w, now, exp, err); free(err); err = NULL;

    return 0;
}
