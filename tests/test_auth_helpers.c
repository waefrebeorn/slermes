/*
 * test_auth_helpers.c — Faithful port of hermes_cli/auth.py pure helpers.
 */

#include "auth_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); g_fail++; } } while (0)

static int is_null(char *s) { int n=(s==NULL); free(s); return n; }
static int eq(char *s, const char *e) { int r=(s && strcmp(s,e)==0); free(s); return r; }

int main(void) {
    /* has_usable_secret */
    CHECK(auth_has_usable_secret("sk-1234", 4), "normal secret usable");
    CHECK(!auth_has_usable_secret("", 4), "empty not usable");
    CHECK(!auth_has_usable_secret("ab", 4), "too short not usable");
    CHECK(!auth_has_usable_secret("changeme", 4), "placeholder rejected");
    CHECK(!auth_has_usable_secret("your_api_key_here", 4), "placeholder rejected 2");
    CHECK(!auth_has_usable_secret("  ", 4), "whitespace-only not usable");
    CHECK(auth_has_usable_secret("  realkey  ", 4), "surrounding ws stripped -> usable");

    /* kimi base url */
    char *k1 = auth_resolve_kimi_base_url("sk-kimi-abc", "https://default", NULL);
    CHECK(eq(k1, "https://api.kimi.com/coding"), "sk-kimi- routes to kimi coding");
    char *k2 = auth_resolve_kimi_base_url("sk-other", "https://default", NULL);
    CHECK(eq(k2, "https://default"), "non-kimi key uses default");
    char *k3 = auth_resolve_kimi_base_url("", "https://default", NULL);
    CHECK(eq(k3, "https://default"), "no key uses default");
    char *k4 = auth_resolve_kimi_base_url("sk-kimi-x", "https://default", "https://override");
    CHECK(eq(k4, "https://override"), "env override wins");

    /* lmstudio */
    char *l1 = auth_normalize_lmstudio_runtime_base_url("http://127.0.0.1:1234/api/v1");
    CHECK(eq(l1, "http://127.0.0.1:1234/v1"), "strips /api/v1");
    char *l2 = auth_normalize_lmstudio_runtime_base_url("https://host.example/v1/");
    CHECK(eq(l2, "https://host.example/v1"), "strips /v1 and trailing slash");
    char *l3 = auth_normalize_lmstudio_runtime_base_url("");
    CHECK(eq(l3, "http://127.0.0.1:1234/v1"), "empty -> default /v1");

    /* token fingerprint */
    char *f1 = auth_token_fingerprint("my-secret-token");
    CHECK(f1 && strlen(f1)==12, "fingerprint is 12 hex chars");
    char *f1b = auth_token_fingerprint("my-secret-token");
    CHECK(f1 && f1b && strcmp(f1,f1b)==0, "fingerprint deterministic");
    free(f1); free(f1b);
    CHECK(is_null(auth_token_fingerprint("")), "empty token -> null fingerprint");
    CHECK(is_null(auth_token_fingerprint(NULL)), "null token -> null fingerprint");

    /* retry-after */
    CHECK(auth_parse_retry_after("120")==120, "retry-after 120");
    CHECK(auth_parse_retry_after(" 60 " )==60, "retry-after trimmed");
    CHECK(auth_parse_retry_after(NULL)==-1, "NULL -> -1");
    CHECK(auth_parse_retry_after("Mon, 02 Jan 2000 00:00:00 GMT")==-1, "http-date -> -1");
    CHECK(auth_parse_retry_after("abc")==-1, "garbage -> -1");
    CHECK(auth_parse_retry_after("-5")==-1, "negative -> -1");

    /* iso timestamp */
    double t = auth_parse_iso_timestamp("2024-01-01T00:00:00Z");
    CHECK(t > 0, "valid iso parses");
    /* 2024-01-01 UTC epoch == 1704067200 */
    CHECK(t == 1704067200.0, "epoch matches known value");
    CHECK(auth_parse_iso_timestamp("not-a-date") < 0, "bad date -> -1");
    CHECK(auth_parse_iso_timestamp(NULL) < 0, "null -> -1");
    /* expiring */
    char buf[64]; snprintf(buf,sizeof(buf),"%s", "2030-01-01T00:00:00Z");
    CHECK(!auth_is_expiring(buf, 60), "far-future not expiring");
    snprintf(buf,sizeof(buf),"%s","2000-01-01T00:00:00Z");
    CHECK(auth_is_expiring(buf, 60), "past is expiring");

    /* ttl */
    CHECK(auth_coerce_ttl_seconds("3600")==3600, "ttl 3600");
    CHECK(auth_coerce_ttl_seconds("-5")==0, "negative ttl -> 0");
    CHECK(auth_coerce_ttl_seconds("garbage")==0, "garbage ttl -> 0");

    /* optional base url */
    char *o1 = auth_optional_base_url("https://x.com/path/");
    CHECK(eq(o1, "https://x.com/path"), "trailing slash stripped");
    CHECK(is_null(auth_optional_base_url("")), "empty -> null");
    CHECK(is_null(auth_optional_base_url(NULL)), "null -> null");

    /* scope values */
    int ns; char **sc = auth_scope_values("a b,c  d", &ns);
    CHECK(ns==4, "scope split into 4");
    auth_free_scope(sc, ns);
    int nz; char **zc = auth_scope_values(NULL, &nz);
    CHECK(nz==0, "null scope -> 0");
    auth_free_scope(zc, nz);

    /* JWT decode (base64url payload) */
    /* payload {"scope":"nous.inference.invoke","exp":9999999999,"sub":"u1"} */
    const char *jwt = "h.eyJzY29wZSI6ICJub3VzLmluZmVyZW5jZS5pbnZva2UiLCAiZXhwIjogOTk5OTk5OTk5OSwgInN1YiI6ICJ1MSJ9.sig";
    char *pl = auth_decode_jwt_payload(jwt);
    CHECK(pl != NULL, "jwt payload decoded");
    char *scope = auth_jwt_get_str(pl, "scope");
    CHECK(eq(scope, "nous.inference.invoke"), "jwt scope extracted");
    double exp = auth_jwt_get_num(pl, "exp");
    CHECK(exp == 9999999999.0, "jwt exp extracted");
    char *sub = auth_jwt_get_str(pl, "sub");
    CHECK(eq(sub, "u1"), "jwt sub extracted");
    free(pl);
    CHECK(is_null(auth_decode_jwt_payload("not.a.jwt.with.extra.dots")), "wrong dot count -> null");
    CHECK(is_null(auth_decode_jwt_payload("invalid")), "no dots -> null");

    /* nous invoke jwt status */
    CHECK(is_null(auth_nous_invoke_jwt_status(jwt, NULL, NULL, 300)), "valid invoke jwt -> null (usable)");
    const char *jwt_noscope = "h.eyJzdWIiOiAidTEifQ==.sig";
    char *reason = auth_nous_invoke_jwt_status(jwt_noscope, NULL, NULL, 300);
    CHECK(reason && strcmp(reason,"missing_inference_invoke_scope")==0, "missing scope -> reason");
    free(reason);
    const char *jwt_expired = "h.eyJzY29wZSI6ICJub3VzLmluZmVyZW5jZS5pbnZva2UiLCAiZXhwIjogMX0=.sig";
    char *reason2 = auth_nous_invoke_jwt_status(jwt_expired, NULL, NULL, 300);
    CHECK(reason2 && strcmp(reason2,"invoke_jwt_expiring")==0, "expired -> reason");
    free(reason2);

    /* auth error */
    auth_error_t *e = auth_error_new("bad creds", "openai", "subscription_required", false);
    char *msg = auth_format_error(e);
    CHECK(msg && strstr(msg, "subscription") && strstr(msg, "purchase"), "subscription msg formatted");
    free(msg); auth_error_free(e);

    auth_error_t *rl = auth_error_new("rate limited", "x", "codex_rate_limited", false);
    CHECK(auth_is_rate_limited_error(rl), "rate-limited detected");
    char *msg2 = auth_format_error(rl);
    CHECK(msg2 && strcmp(msg2, "rate limited")==0, "rate-limited msg not appending relogin");
    free(msg2); auth_error_free(rl);

    auth_error_t *rel = auth_error_new("expired", "x", "token_expired", true);
    char *msg3 = auth_format_error(rel);
    CHECK(msg3 && strstr(msg3, "hermes model"), "relogin appends remediation");
    free(msg3); auth_error_free(rel);

    if (g_fail==0) printf("ALL PASSED\n"); else printf("%d FAIL\n", g_fail);
    return g_fail ? 1 : 0;
}
