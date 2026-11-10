/* t_port_hermes_auth_nous.c — Oracle harness for the pure auth_nous helpers. */
#include "port_hermes_auth_nous.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

static void emit_obj(json_t *o)
{
    if (!o) { fputs("null", stdout); return; }
    char *s = json_serialize(o);
    fputs(s ? s : "null", stdout);
    free(s);
    json_free(o);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    static const char *const toks[] = { NULL, "", "   ", "abcdef123456",
                                        " access-token-xyz ", "UPPER Case" };
    for (size_t i = 0; i < sizeof(toks) / sizeof(toks[0]); i++) {
        printf("{\"family\":\"fp\",\"case\":\"fp_%zu\",\"in\":", i);
        jstr(toks[i]);
        printf(",\"out\":");
        jstr(authn_token_fingerprint(toks[i]));
        printf("}\n");
    }

    static const struct { double now; long long ttl; } iso[] = {
        { 1793880000.0, 3600 }, { 1793880000.0, 0 }, { 1793880000.5, 120 },
        { 1000000000.0, 604800 },
    };
    for (size_t i = 0; i < sizeof(iso) / sizeof(iso[0]); i++) {
        printf("{\"family\":\"iso\",\"case\":\"iso_%zu\",\"now\":%.1f,\"ttl\":%lld,\"out\":",
               i, iso[i].now, iso[i].ttl);
        jstr(authn_iso_after(iso[i].now, iso[i].ttl));
        printf("}\n");
    }

    static const struct { const char *host, *ov; } ha[] = {
        { "inference-api.nousresearch.com", NULL },
        { "welcome-api.nousresearch.com", NULL },
        { "evil.example.com", NULL },
        { NULL, "https://inference-api.nousresearch.com/v1" },
        { "", NULL },
        { "stage.example.com", "https://STAGE.Example.com/v1/" },
        { "stage.example.com", "https://other.example.com/v1" },
        { "stage.example.com:443", NULL },
        { "api.nousresearch.com", "https://api.nousresearch.com" },
        { "Inference-Api.NousResearch.Com", NULL },
    };
    for (size_t i = 0; i < sizeof(ha) / sizeof(ha[0]); i++) {
        printf("{\"family\":\"host\",\"case\":\"h_%zu\",\"host\":", i);
        jstr(ha[i].host);
        printf(",\"ov\":");
        jstr(ha[i].ov);
        printf(",\"ret\":%d}\n", authn_inference_host_allowed(ha[i].host, ha[i].ov));
    }

    static const char *const sc[] = {
        "(\"a b c\" if 0 else \"a b\")==\"b a\" and 1==1",
        "\"read write\"", "\"read,write audit\"", "()", "[]",
        "[\"read\",\"write\",\"read\"]", "\"  spaced   tokens  \"",
        "[\"build\",[\"test,audit\"]]", "\"\"", "null", "7",
    };
    /* re-purpose: feed whitespace/collection scope strings */
    static const char *const scopes[] = {
        "\"read write\"", "\"read,write audit\"", "\"read\"", "[]",
        "[\"read\",\"write\",\"read\"]", "\"  spaced   tokens  \"",
        "\"\"", "null", "[\"build\",[\"test\"]]", "\"read write\"" /* dedupe via set */
    };
    (void)sc;
    for (size_t i = 0; i < sizeof(scopes) / sizeof(scopes[0]); i++) {
        printf("{\"family\":\"scope\",\"case\":\"sc_%zu\",\"in\":", i);
        fputs(scopes[i], stdout);
        printf(",\"out\":");
        emit_obj(authn_scope_values(scopes[i]));
        printf("}\n");
    }

    static const struct { const char *r; int nr; } un[] = {
        { "access_token_not_jwt", 0 }, { "missing_inference_invoke_scope", 0 },
        { "invoke_jwt_expiring", 1 }, { "invoke_jwt_expiry_unknown_or_expiring", 0 },
        { "", 0 },
    };
    for (size_t i = 0; i < sizeof(un) / sizeof(un[0]); i++) {
        const char *code = NULL;
        char *m = authn_unusable_invoke_jwt_error(un[i].r, un[i].nr, &code);
        printf("{\"family\":\"unusable\",\"case\":\"un_%zu\",\"r\":", i, un[i].nr);
        jstr(un[i].r);
        printf(",\"nr\":%d,\"msg\":", un[i].nr);
        jstr(m);
        printf(",\"code\":");
        jstr(code);
        printf("}\n");
        free(m);
    }

    static const char *const prov[] = {
        "{}",
        "{\"openai\":{\"api_key\":\"k\"},\"nous\":{\"portal_base_url\":\"https://api.nousresearch.com\"}}",
        "{\"nous\":{\"portal_base_url\":\"https://eu.portal.example\"}}",
        "{\"nous\":{\"portal_base_url\":null}}",
        "{\"nous\":\"not-a-dict\"}",
        "null",
    };
    for (size_t i = 0; i < sizeof(prov) / sizeof(prov[0]); i++) {
        printf("{\"family\":\"migrate\",\"case\":\"mg_%zu\",\"in\":", i);
        fputs(prov[i], stdout);
        printf(",\"out\":");
        emit_obj(authn_migrate_stale_portal_url(prov[i]));
        printf("}\n");
    }

    static const char *const sts[] = {
        "{}",
        "{\"access_token\":\"t\",\"expires_in\":120,\"agent_key_expires_in\":500,\"expires_at\":\"2026-12-01T00:00:00+00:00\",\"scope\":\"read\"}",
        "{\"expires_in\":120,\"agent_key_expires_in\":500}",
        "{\"a\":1}",
        "null",
    };
    for (size_t i = 0; i < sizeof(sts) / sizeof(sts[0]); i++) {
        printf("{\"family\":\"effective\",\"case\":\"eff_%zu\",\"in\":", i);
        fputs(sts[i], stdout);
        printf(",\"out\":");
        emit_obj(authn_effective_provider_state(sts[i]));
        printf("}\n");
    }

    static const char *const urls[] = { NULL, "", "https://portal.nousresearch.com",
                                        "https://eu.portal.example:8443/x", "not-a-url", "vanity" };
    for (size_t i = 0; i < sizeof(urls) / sizeof(urls[0]); i++) {
        printf("{\"family\":\"portal_host\",\"case\":\"ph_%zu\",\"in\":", i);
        jstr(urls[i]);
        printf(",\"out\":");
        jstr(authn_portal_host(urls[i]));
        printf("}\n");
    }
    return 0;
}
