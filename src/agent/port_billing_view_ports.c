/*
 * port_billing_view_remaining.c — Port of agent/billing_view.py card-view
 * surface. Provenance labels, one-line display, legacy admin check.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: provenance @ agent/billing_view.py:provenance */
char *biv_provenance(const char *rung) {
    /* Python: human label for card pick reason. */
    if (!rung) return NULL;
    char *l = lowerdup(rung);
    if (!l) return NULL;
    char *out = NULL;
    if (strcmp(l, "subscription") == 0) out = strdup("card on your subscription");
    else if (strcmp(l, "default") == 0) out = strdup("default card");
    else if (strcmp(l, "server") == 0) out = strdup("server-selected card");
    else out = strdup("unknown rung");
    free(l);
    return out;
}

/* PoP: display @ agent/billing_view.py:display */
char *biv_display(const char *brand, const char *last4) {
    /* Python: Visa ····4242. */
    if (!brand || !last4) return NULL;
    char *out = NULL;
    asprintf(&out, "%s ····%s", brand, last4);
    return out;
}

/* PoP: is_admin @ agent/billing_view.py:is_admin */
bool biv_is_admin(const char *role) {
    /* Python: legacy OWNER/ADMIN display-only check. */
    if (!role) return false;
    char *l = lowerdup(role);
    if (!l) return false;
    bool r = strcmp(l, "owner") == 0 || strcmp(l, "admin") == 0;
    free(l);
    return r;
}
