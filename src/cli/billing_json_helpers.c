/*
 * billing_json_helpers.c — Shared raw-JSON text scrapers for billing modules.
 *
 * The Nous billing portal returns unstructured JSON from which the billing
 * views only need a handful of flat scalar values. These helpers extract a
 * numeric or boolean value for a named key directly from the raw JSON *text*
 * (no full parse required). They are intentionally shared between
 * port_agent_billing_view.c and port_nous_billing.c so the (identical) scrape
 * logic lives in exactly one place.
 *
 * NOTE: kept as text scraping (not libjson parsing) deliberately — the inputs
 * are small, flat portal responses and the callers want a tolerant,
 * best-effort scalar pull. For anything structural, prefer hermes_json.h.
 */

#include "hermes_billing.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

double billing_json_get_number(const char *json, const char *key)
{
    if (!json || !key) return 0.0;
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return 0.0;
    p += strlen(search);
    while (*p && *p != ':') p++;
    if (*p) p++;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    return atof(p);
}

bool billing_json_get_bool(const char *json, const char *key)
{
    if (!json || !key) return false;
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    while (*p && *p != ':') p++;
    if (*p) p++;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    return (strncmp(p, "true", 4) == 0);
}
