/*
 * test_fallback_config.c — unit tests for the pure hermes_cli/fallback_config.py
 * helpers. Invariants derived from a Python oracle.
 */

#include "fallback_config_helpers.h"
#include <libjson/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static int g_fail = 0;

#define EQFAIL(lbl, got, exp) do { \
    if (strcmp(got, exp) != 0) { \
        printf("FAIL: %s got[%s] want[%s]\n", lbl, got, exp); g_fail++; \
    } else printf("ok: %s\n", lbl); \
} while (0)

static char *chain_json(const char *txt)
{
    char *j = strdup(txt);
    return j;
}

static void check_chain(const char *label, const char *json_cfg, int exp_count, ...)
{
    json_t *cfg = json_parse(json_cfg, NULL);
    int cnt = 0;
    fallback_entry_t *e = fallback_config_get_chain(cfg, &cnt);
    if (cnt != exp_count) {
        printf("FAIL: %s count=%d want %d\n", label, cnt, exp_count); g_fail++;
    } else {
        va_list ap; va_start(ap, exp_count);
        int ok = 1;
        for (int i = 0; i < cnt; i++) {
            const char *ep = va_arg(ap, const char *);
            const char *em = va_arg(ap, const char *);
            const char *eb = va_arg(ap, const char *);
            if (strcmp(e[i].provider, ep) || strcmp(e[i].model, em) || strcmp(e[i].base_url, eb)) {
                printf("FAIL: %s[%d] got[%s,%s,%s] want[%s,%s,%s]\n", label, i,
                       e[i].provider, e[i].model, e[i].base_url, ep, em, eb);
                ok = 0; g_fail++;
            }
        }
        if (ok) printf("ok: %s (%d entries)\n", label, cnt);
        va_end(ap);
    }
    fallback_config_free_entries(e, cnt);
    json_free(cfg);
}

int main(void)
{
    char *u;
    u = fallback_config_normalize_base_url("  https://x.com/  "); EQFAIL("url1", u, "https://x.com"); free(u);
    u = fallback_config_normalize_base_url("https://y.com");        EQFAIL("url3", u, "https://y.com"); free(u);
    u = fallback_config_normalize_base_url("");                     EQFAIL("url_empty", u, ""); free(u);

    check_chain("c1", "{\"fallback_providers\":[{\"provider\":\"OpenAI\",\"model\":\"gpt-4\"}]}",
                1, "OpenAI", "gpt-4", "");
    check_chain("c2", "{\"fallback_providers\":[{\"provider\":\" OpenAI \",\"model\":\" gpt-4 \",\"base_url\":\"https://x.com/\"}]}",
                1, "OpenAI", "gpt-4", "https://x.com");
    check_chain("c3", "{\"fallback_providers\":[{\"provider\":\"a\",\"model\":\"m\"},{\"provider\":\"a\",\"model\":\"m\"}]}",
                1, "a", "m", "");
    check_chain("c4", "{\"fallback_providers\":[{\"provider\":\"\",\"model\":\"\"}]}", 0);
    check_chain("c5", "{\"fallback_model\":[{\"provider\":\"anthropic\",\"model\":\"claude\"}]}",
                1, "anthropic", "claude", "");
    check_chain("c6", "{\"fallback_providers\":[{\"provider\":\"a\",\"model\":\"m\"}],\"fallback_model\":[{\"provider\":\"a\",\"model\":\"m\"}]}",
                1, "a", "m", "");
    check_chain("c7", "{\"fallback_providers\":[{\"provider\":\"a\",\"model\":\"m\",\"base_url\":\"https://Y.com/\"},{\"provider\":\"a\",\"model\":\"m\"}]}",
                2, "a", "m", "https://Y.com", "a", "m", "");
    check_chain("c8", "null", 0);
    check_chain("c9", "{\"fallback_providers\":\"notalist\"}", 0);

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
