/*
 * test_timeouts.c — unit tests for the pure hermes_cli/timeouts.py
 * helpers. Invariants derived from a Python oracle.
 */

#include "timeouts_helpers.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
#define CHK(cond, lbl) do { \
    if (!(cond)) { printf("FAIL: %s\n", lbl); g_fail++; } \
    else printf("ok: %s\n", lbl); \
} while (0)

#define NEAR(a, b) ((a) == (b))

int main(void)
{
    /* _coerce_timeout: None/empty/garbage/<=0 -> -1; positive -> value */
    CHK(timeouts_coerce_timeout(NULL) < 0, "coerce None");
    CHK(timeouts_coerce_timeout("") < 0, "coerce empty");
    CHK(timeouts_coerce_timeout("abc") < 0, "coerce garbage");
    CHK(timeouts_coerce_timeout("0") < 0, "coerce zero");
    CHK(timeouts_coerce_timeout("-5") < 0, "coerce negative");
    CHK(NEAR(timeouts_coerce_timeout("30"), 30.0), "coerce 30");
    CHK(NEAR(timeouts_coerce_timeout("12.5"), 12.5), "coerce 12.5");
    CHK(timeouts_coerce_timeout("-3.0") < 0, "coerce -3.0");

    const char *cfg =
        "{\"providers\":{"
        "  \"openai\":{"
        "    \"request_timeout_seconds\":45,"
        "    \"stale_timeout_seconds\":90,"
        "    \"models\":{\"gpt-4o\":{\"timeout_seconds\":20,\"stale_timeout_seconds\":40}}"
        "  },"
        "  \"anthropic\":{\"request_timeout_seconds\":\"60\"}"
        "}}";

    /* request timeout */
    CHK(NEAR(timeouts_get_provider_request_timeout(cfg, "openai", NULL), 45.0),
         "REQ openai (provider default)");
    CHK(NEAR(timeouts_get_provider_request_timeout(cfg, "openai", "gpt-4o"), 20.0),
         "REQ openai/gpt-4o (model override)");
    CHK(NEAR(timeouts_get_provider_request_timeout(cfg, "openai", "nope"), 45.0),
         "REQ openai/unknown-model (falls back to provider)");
    CHK(NEAR(timeouts_get_provider_request_timeout(cfg, "anthropic", NULL), 60.0),
         "REQ anthropic (string coerced)");
    CHK(timeouts_get_provider_request_timeout(cfg, "", NULL) < 0,
         "REQ empty provider -> None");

    /* stale timeout */
    CHK(NEAR(timeouts_get_provider_stale_timeout(cfg, "openai", NULL), 90.0),
         "STALE openai (provider default)");
    CHK(NEAR(timeouts_get_provider_stale_timeout(cfg, "openai", "gpt-4o"), 40.0),
         "STALE openai/gpt-4o (model override)");
    CHK(timeouts_get_provider_stale_timeout(cfg, "anthropic", NULL) < 0,
         "STALE anthropic (no stale key -> None)");

    /* _get_model_config: borrowed lookup */
    char *err = NULL;
    json_t *c = json_parse(cfg, &err);
    if (err) free(err);
    const json_t *oa = json_obj_get(json_obj_get(c, "providers"), "openai");
    const json_t *mc = timeouts_get_model_config(oa, "gpt-4o");
    CHK(mc != NULL && json_obj_get(mc, "timeout_seconds") != NULL, "model_config found");
    CHK(timeouts_get_model_config(oa, "nope") == NULL, "model_config missing -> NULL");
    CHK(timeouts_get_model_config(oa, NULL) == NULL, "model_config no-model -> NULL");
    json_free(c);

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
