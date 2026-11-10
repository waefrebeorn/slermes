/*
 * t_port_nous_subscription.c -- oracle harness for port_nous_subscription.c
 */
#include "port_nous_subscription.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p == '\n') fputs("\\n", stdout);
        else putchar(*p);
    }
    putchar('"');
}

static void *parse(const char *s) { return json_parse(s, NULL); }

static void emit_norm(const char *label, const char *json, const char *fallback)
{
    json_t *v = json ? parse(json) : NULL;
    char *s = sub_norm(v, fallback);
    printf("{\"fn\":\"_norm\",\"case\":");
    jstr(label);
    printf(",\"in\":");
    jstr(json);
    printf(",\"fallback\":");
    jstr(fallback);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
    if (v) json_free(v);
}

static void emit_provider_is_nous(const char *label, const char *cfg)
{
    json_t *c = parse(cfg);
    int r = sub_provider_is_nous(c);
    printf("{\"fn\":\"_provider_is_nous\",\"case\":");
    jstr(label);
    printf(",\"out\":%s}\n", r ? "true" : "false");
    json_free(c);
}

static void emit_audio(const char *label, const char *cfg, const char *fallback, int gw)
{
    json_t *c = parse(cfg);
    char *s = sub_audio_provider(c, fallback, gw);
    printf("{\"fn\":\"_audio_provider\",\"case\":");
    jstr(label);
    printf(",\"fallback\":");
    jstr(fallback);
    printf(",\"gw\":%d,\"out\":", gw);
    jstr(s);
    printf("}\n");
    free(s);
    json_free(c);
}

static void emit_gateway(const char *label, const char *section)
{
    json_t *s = section ? parse(section) : NULL;
    int r = sub_uses_gateway(s);
    printf("{\"fn\":\"_uses_gateway\",\"case\":");
    jstr(label);
    printf(",\"out\":%s}\n", r ? "true" : "false");
    if (s) json_free(s);
}

int main(void)
{
    /* ---- _norm ---- */
    emit_norm("string", "\"  OpenRouter \"", "");
    emit_norm("empty-uses-default", "\"\"", "nous");
    emit_norm("number", "2070", "x");
    emit_norm("zero-falsy", "0", "nous");
    emit_norm("bool-true", "true", "x");
    emit_norm("bool-false", "false", "nous");
    emit_norm("null", "null", "nous");
    emit_norm("absent", NULL, "nous");

    /* ---- _provider_is_nous ---- */
    emit_provider_is_nous("nous", "{\"model\":{\"provider\":\"nous\"}}");
    emit_provider_is_nous("cased", "{\"model\":{\"provider\":\"  NOUS  \"}}");
    emit_provider_is_nous("other", "{\"model\":{\"provider\":\"openrouter\"}}");
    emit_provider_is_nous("no-model", "{}");
    emit_provider_is_nous("model-not-object", "{\"model\":\"nous\"}");

    /* ---- _audio_provider ---- */
    emit_audio("nous-managed", "{\"provider\":\"nous\"}", "edge", 0);
    emit_audio("gw-override", "{\"provider\":\"elevenlabs\"}", "edge", 1);
    emit_audio("byok", "{\"provider\":\"  ELEVENLABS \"}", "edge", 0);
    emit_audio("default", "{}", "edge", 0);

    /* ---- _uses_gateway ---- */
    emit_gateway("true", "{\"use_gateway\":true}");
    emit_gateway("yes", "{\"use_gateway\":\"yes\"}");
    emit_gateway("on", "{\"use_gateway\":\"on\"}");
    emit_gateway("one", "{\"use_gateway\":1}");
    emit_gateway("false", "{\"use_gateway\":false}");
    emit_gateway("zero", "{\"use_gateway\":0}");
    emit_gateway("missing", "{}");
    emit_gateway("not-object", "\"x\"");

    return 0;
}
