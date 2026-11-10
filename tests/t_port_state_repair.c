/*
 * t_port_state_repair.c -- oracle harness for port_state_repair.c
 */
#include "port_state_repair.h"

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

static void emit_hex(const char *label, const unsigned char *buf, size_t len)
{
    printf("{\"fn\":\"_mask_volatile_header\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":\"");
    for (size_t i = 0; i < len; i++) printf("%02x", buf[i]);
    printf("\"}\n");
}

static void emit_ll(const char *label, long long v)
{
    printf("{\"fn\":\"_repair_backup_headroom_bytes\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":%lld}\n", v);
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

    /* ---- _mask_volatile_header ---- */
    {
        unsigned char h[100];
        memset(h, 0xAB, sizeof(h));
        sr_mask_volatile_header(h, sizeof(h));
        emit_hex("full", h, sizeof(h));
    }
    {
        unsigned char h[40];
        memset(h, 0xAB, sizeof(h));
        sr_mask_volatile_header(h, sizeof(h));
        emit_hex("short", h, sizeof(h));
    }

    /* ---- _repair_backup_headroom_bytes ---- */
    emit_ll("small", sr_backup_headroom(1024LL));
    emit_ll("exact", sr_backup_headroom(128LL * 1024 * 1024));
    emit_ll("large", sr_backup_headroom(100LL * 1024 * 1024 * 1024));
    emit_ll("zero",  sr_backup_headroom(0));

    /* ---- _repair_failure_consumes_attempt ---- */
    emit_bool("_repair_failure_consumes_attempt", "corrupt",
              sr_failure_consumes(1, 11, ""));
    emit_bool("_repair_failure_consumes_attempt", "notadb",
              sr_failure_consumes(1, 16, ""));
    emit_bool("_repair_failure_consumes_attempt", "extended",
              sr_failure_consumes(1, 267 /* 0x10B = 256|11 */, ""));
    emit_bool("_repair_failure_consumes_attempt", "busy",
              sr_failure_consumes(1, 5, ""));
    emit_bool("_repair_failure_consumes_attempt", "not-db-error",
              sr_failure_consumes(0, 11, ""));
    emit_bool("_repair_failure_consumes_attempt", "msg-corrupt",
              sr_failure_consumes(1, 0, "file is not a database"));
    emit_bool("_repair_failure_consumes_attempt", "msg-malformed",
              sr_failure_consumes(1, 0, "database disk image is malformed"));
    emit_bool("_repair_failure_consumes_attempt", "msg-full",
              sr_failure_consumes(1, 0, "disk is full"));
    emit_bool("_repair_failure_consumes_attempt", "msg-readonly",
              sr_failure_consumes(1, 0, "attempt to write a readonly database"));

    /* ---- _sidecars ---- */
    {
        int n = 0;
        char **sc = sr_sidecar_names("state.db", &n);
        printf("{\"fn\":\"_sidecars\",\"case\":\"names\",\"out\":[");
        for (int i = 0; i < n; i++) {
            if (i) putchar(',');
            jstr(sc[i]);
        }
        printf("]}\n");
        for (int i = 0; i < n; i++) free(sc[i]);
        free(sc);
    }

    return 0;
}
