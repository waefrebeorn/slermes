/*
 * t_port_dingtalk_auth.c -- oracle harness for port_dingtalk_auth.c
 */
#include "port_dingtalk_auth.h"

#include <stdio.h>
#include <stdlib.h>

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

static void emit_status(const char *s)
{
    char *out = dt_normalize_poll_status(s);
    printf("{\"fn\":\"poll_status\",\"in\":");
    jstr(s);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
}

static void emit_field(const char *label, const char *json, const char *key)
{
    json_t *d = json_parse(json, NULL);
    char *out = dt_str_field(d, key);
    printf("{\"fn\":\"str_field\",\"case\":");
    jstr(label);
    printf(",\"key\":");
    jstr(key);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
    json_free(d);
}

int main(void)
{
    /* ---- status normalization ---- */
    emit_status("success");
    emit_status("  SUCCESS  ");
    emit_status("WAITING");
    emit_status("fail");
    emit_status("EXPIRED");
    emit_status("something-else");
    emit_status("");
    emit_status(NULL);

    /* ---- field extraction ---- */
    emit_field("present", "{\"client_id\":\"abc\"}", "client_id");
    emit_field("padded", "{\"client_id\":\"  abc  \"}", "client_id");
    emit_field("blank", "{\"client_id\":\"   \"}", "client_id");
    emit_field("missing", "{}", "client_id");
    emit_field("null", "{\"client_id\":null}", "client_id");
    emit_field("secret", "{\"client_secret\":\"s3cr3t\"}", "client_secret");
    emit_field("reason", "{\"fail_reason\":\"nope\"}", "fail_reason");

    return 0;
}
