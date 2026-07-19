/*
 * test_url_safety_helpers.c — unit tests for the pure tools/url_safety.py
 * helpers. Expected values derived from a faithful Python oracle of
 * sensitive_query_param_name / has_sensitive_query_params.
 */

#include "url_safety_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

static void check_param(const char *url, const char *exp)
{
    char *g = tools_url_safety_sensitive_query_param_name(url);
    int ok = (exp == NULL) ? (g == NULL) : (g && strcmp(g, exp) == 0);
    if (!ok) {
        printf("FAIL: param(%s) -> [%s] want [%s]\n", url, g ? g : "NULL", exp ? exp : "NULL");
        g_fail++;
    } else {
        printf("ok: param(%s) -> [%s]\n", url, g ? g : "NULL");
    }
    free(g);
}

static void check_has(const char *url, int exp)
{
    int g = tools_url_safety_has_sensitive_query_params(url);
    if (g != exp) {
        printf("FAIL: has(%s)=%d want %d\n", url, g, exp);
        g_fail++;
    } else {
        printf("ok: has(%s)=%d\n", url, g);
    }
}

int main(void)
{
    check_param("https://example.com/x?token=abc", "token");
    check_param("https://example.com/x?TOKEN=abc", "TOKEN");
    check_param("https://example.com/x?api_key=xyz", "api_key");
    check_param("https://example.com/x?code=123", NULL);
    check_param("https://example.com/x?token=", NULL);
    check_param("https://example.com/x?key=val&secret=top", "secret");
    check_param("ftp://example.com/x?token=abc", NULL);
    check_param("https://example.com/x", NULL);
    check_param("https://example.com/x?%74oken=abc", "token");
    check_param("https://example.com/x?page=2&token=abc", "token");

    check_has("https://example.com/x?token=abc", 1);
    check_has("https://example.com/x?code=123", 0);

    /* private-IP gate */
    if (!tools_url_safety_allows_private_ip_resolution("multimedia.nt.qq.com.cn", "https"))
        { printf("FAIL: trusted host https\n"); g_fail++; }
    else printf("ok: trusted host https\n");
    if (tools_url_safety_allows_private_ip_resolution("multimedia.nt.qq.com.cn", "http"))
        { printf("FAIL: trusted host http should be false\n"); g_fail++; }
    else printf("ok: trusted host http false\n");
    if (tools_url_safety_allows_private_ip_resolution("evil.com", "https"))
        { printf("FAIL: untrusted host should be false\n"); g_fail++; }
    else printf("ok: untrusted host false\n");

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
