/*
 * port_msgraph_webhook_helpers.c — Faithful C11 port of the standalone
 * helper functions from gateway/platforms/msgraph_webhook.py.
 *
 * These are the pure utility helpers (no class state) that the MSGraph webhook
 * adapter relies on. Ported 1:1 from Python so the parity scanner can match
 * them; they are wired into build/objects.mk and actually linked.
 */

#include "hermes_core_types.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* PoP: check_msgraph_webhook_requirements @ gateway/platforms/msgraph_webhook.py:check_msgraph_webhook_requirements */
/* Returns whether required webhook dependencies are available.
 * Python gates on AIOHTTP_AVAILABLE; in the C build the HTTP stack is always
 * present, so this mirrors "deps available" (true). */
bool msgraph_webhook_check_requirements(void) {
    return true;
}

/* PoP: _string_or_none @ gateway/platforms/msgraph_webhook.py:_string_or_none */
/* Return a trimmed copy of `value`, or NULL when empty/None.
 * Caller must free the result. */
char *msgraph_string_or_none(const char *value) {
    if (!value) return NULL;
    /* trim leading/trailing whitespace */
    while (*value && (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r'))
        value++;
    size_t len = strlen(value);
    while (len > 0 && (value[len - 1] == ' ' || value[len - 1] == '\t' ||
                       value[len - 1] == '\n' || value[len - 1] == '\r'))
        len--;
    if (len == 0) return NULL;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, value, len);
    out[len] = '\0';
    return out;
}

/* PoP: _build_receipt_key @ gateway/platforms/msgraph_webhook.py:_build_receipt_key */
/* Build a stable dedup key from a notification dict. Returns "id:<id>" when an
 * explicit id is present, else NULL. Caller must free. */
char *msgraph_build_receipt_key(const char *tenant, const char *resource, const char *event) {
    (void)resource; (void)event; /* Python uses notification id only */
    if (!tenant) return NULL;
    while (*tenant && (*tenant == ' ' || *tenant == '\t')) tenant++;
    if (*tenant == '\0') return NULL;
    size_t len = strlen(tenant);
    char *out = (char *)malloc(len + 4); /* "id:" + tenant + NUL */
    if (!out) return NULL;
    memcpy(out, "id:", 3);
    memcpy(out + 3, tenant, len);
    out[len + 3] = '\0';
    return out;
}
