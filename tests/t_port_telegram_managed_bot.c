/*
 * t_port_telegram_managed_bot.c -- oracle harness for port_telegram_managed_bot.c
 */
#include "port_telegram_managed_bot.h"

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

static void emit_url(const char *label, const char *api_url, const char *env_value)
{
    char *s = tg_api_url(api_url, env_value);
    printf("{\"fn\":\"_api_url\",\"case\":");
    jstr(label);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

static void emit_token(const char *tok)
{
    int v = tg_is_valid_bot_token(tok);
    printf("{\"fn\":\"is_valid_telegram_bot_token\",\"in\":");
    jstr(tok);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

static void emit_owner(const char *label, int kind, long long iv, const char *sv)
{
    long long v = tg_parse_owner_user_id(kind, iv, sv);
    printf("{\"fn\":\"_parse_owner_user_id\",\"case\":");
    jstr(label);
    printf(",\"out\":%lld}\n", v);
}

int main(void)
{
    /* ---- _api_url ---- */
    emit_url("explicit", "https://custom.example/", NULL);
    emit_url("explicit-trailing", "https://custom.example///", NULL);
    emit_url("env", NULL, "https://env.example");
    emit_url("default", NULL, NULL);
    emit_url("explicit-blank", "", "https://env.example");

    /* ---- is_valid_telegram_bot_token ---- */
    emit_token("123456789:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    emit_token("123456789:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    emit_token("123456789:AAAAAAAAAAAAAAAAAAAAAAAAAAAAA");   /* 29 -> invalid */
    emit_token("123456789:short");
    emit_token("no-digits:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    emit_token("123456789:bad!chars__________________________");
    emit_token("");
    emit_token(NULL);

    /* ---- _parse_owner_user_id ---- */
    emit_owner("int-ok", 0, 12345, NULL);
    emit_owner("int-zero", 0, 0, NULL);
    emit_owner("int-negative", 0, -5, NULL);
    emit_owner("str-ok", 1, 0, "12345");
    emit_owner("str-zero", 1, 0, "0");
    emit_owner("str-negative", 1, 0, "-5");
    emit_owner("str-padded", 1, 0, " 12345");
    emit_owner("str-empty", 1, 0, "");
    emit_owner("bool", 2, 1, NULL);
    emit_owner("other", 3, 0, "x");

    return 0;
}
