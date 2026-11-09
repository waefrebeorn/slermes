/*
 * t_port_secrets_cli.c -- oracle harness for port_secrets_cli.c
 */
#include "port_secrets_cli.h"

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

/* the env lookups the C port takes as a callback */
static const char *g_env_bws = NULL;
static const char *g_env_token = NULL;
static int env_has_bws(const char *n) { return strcmp(n, "BWS_SERVER_URL") == 0 && g_env_bws && *g_env_bws; }
static int env_has_token(const char *n) { return strcmp(n, "BWS_ACCESS_TOKEN") == 0 && g_env_token && *g_env_token; }

static void emit_section(const char *label, const char *cfg)
{
    json_t *c = json_parse(cfg, NULL);
    json_t *created = NULL;
    const json_t *sec = secrets_section_cfg(c, "bitwarden", &created);
    char *d = json_dumps(sec, 0);
    printf("{\"fn\":\"section_cfg\",\"case\":");
    jstr(label);
    printf(",\"out\":%s}\n", d ? d : "null");
    free(d);
    if (created) json_free(created);
    json_free(c);
}

static void emit_status(const char *label, const char *key, const char *token_env)
{
    char *s = secrets_fetch_status(key, token_env, env_has_token);
    printf("{\"fn\":\"_fetch_status\",\"case\":");
    jstr(label);
    printf(",\"key\":");
    jstr(key);
    printf(",\"token_env\":");
    jstr(token_env);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

static void emit_missing(const char *label, const char *tok, const char *url,
                         const char *proj, const char *env_url)
{
    g_env_bws = env_url;
    int n = 0;
    const char **m = secrets_missing_flags(tok, url, proj, env_url, &n);
    printf("{\"fn\":\"_missing_noninteractive_flags\",\"case\":");
    jstr(label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) { if (i) putchar(','); jstr(m[i]); }
    printf("]}\n");
    free(m);
}

int main(void)
{
    /* ---- section_cfg ---- */
    emit_section("present", "{\"secrets\":{\"bitwarden\":{\"token\":\"x\"}}}");
    emit_section("empty-object", "{\"secrets\":{}}");
    emit_section("null-secrets", "{\"secrets\":null}");
    emit_section("no-secrets", "{}");
    emit_section("section-null", "{\"secrets\":{\"bitwarden\":null}}");

    /* ---- _fetch_status ---- */
    g_env_token = "tok";
    emit_status("bootstrap", "BWS_ACCESS_TOKEN", "BWS_ACCESS_TOKEN");
    emit_status("already-set", "BWS_ACCESS_TOKEN", "OTHER");
    g_env_token = NULL;
    emit_status("new", "BWS_ACCESS_TOKEN", "OTHER");

    /* ---- _missing_noninteractive_flags ---- */
    emit_missing("all-set", "tok", "https://x", "proj", NULL);
    emit_missing("none", NULL, NULL, NULL, NULL);
    emit_missing("blank-token", "   ", "https://x", "proj", NULL);
    emit_missing("env-server-url", "tok", "", "proj", "https://env");
    emit_missing("env-fallback-empty", "tok", "  ", "proj", "");
    emit_missing("only-token", "tok", NULL, NULL, NULL);

    return 0;
}
