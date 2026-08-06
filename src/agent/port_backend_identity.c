/*
 * port_backend_identity.c — Faithful C11 port of Python agent/backend_identity.py.
 *
 * Ports:
 *   _norm_provider, _norm_model, _norm_base_url, classify_failure_scope,
 *   _both_first_class, same_credential_surface, same_endpoint,
 *   same_deployment, should_skip_candidate.
 *
 * BackendIdentity is an opaque struct (see port_backend_identity.h):
 *   { provider, model, base_url } — all malloc'd, lowercased, trimmed.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "port_backend_identity.h"
#include "port_provider_registry.h"

/* ── string normalization helpers ────────────────────────────────────────── */

/* PoP: agent_backend_identity__norm_provider @ agent/backend_identity.py:_norm_provider */
char *agent_backend_identity_norm_provider(const char *value) {
    /* Python: (value or "").strip().lower() */
    if (!value) return strdup("");
    /* strip leading/trailing whitespace, then lowercase */
    const char *start = value;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
    const char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
    size_t len = (size_t)(end - start);
    char *out = malloc(len + 1);
    if (!out) return strdup("");
    for (size_t i = 0; i < len; i++) out[i] = tolower((unsigned char)start[i]);
    out[len] = '\0';
    return out;
}

/* PoP: agent_backend_identity__norm_model @ agent/backend_identity.py:_norm_model */
char *agent_backend_identity_norm_model(const char *value) {
    /* Python: (value or "").strip().lower() — identical to _norm_provider */
    return agent_backend_identity_norm_provider(value);
}

/* PoP: agent_backend_identity__norm_base_url @ agent/backend_identity.py:_norm_base_url */
char *agent_backend_identity_norm_base_url(const char *value) {
    /* Python: (value or "").strip().rstrip("/").lower() */
    char *stripped = agent_backend_identity_norm_provider(value);
    /* rstrip("/") — already lowercased+stripped, just remove trailing slashes */
    size_t len = strlen(stripped);
    while (len > 0 && stripped[len - 1] == '/') len--;
    stripped[len] = '\0';
    return stripped;
}

/* ── classify_failure_scope ─────────────────────────────────────────────── */

/* FailureScope enum values — matches Python FailureScope.MODEL/credential/endpoint */
/* Returns: 0=MODEL, 1=CREDENTIAL, 2=ENDPOINT */

/* PoP: agent_backend_identity_classify_failure_scope @ agent/backend_identity.py:classify_failure_scope */
int agent_backend_identity_classify_failure_scope(const char *reason) {
    /* Python: _REASON_SCOPES.get((reason or "").strip().lower(), FailureScope.MODEL)
     * Returns the scope enum: 0=MODEL, 1=CREDENTIAL, 2=ENDPOINT */
    char *norm = agent_backend_identity_norm_provider(reason);
    int scope = 0; /* FailureScope.MODEL default */
    if (strcmp(norm, "auth error") == 0 || strcmp(norm, "payment error") == 0)
        scope = 1; /* CREDENTIAL */
    /* "rate limit", "model incompatible with route", "invalid provider response",
     * "connection error", "timeout" → MODEL (0) */
    free(norm);
    return scope;
}

/* ── BackendIdentity struct ──────────────────────────────────────────────── */

struct backend_identity_s {
    char *provider;
    char *model;
    char *base_url;
};

/* PoP: agent_backend_identity__norm_provider @ agent/backend_identity.py:BackendIdentity.build */
BackendIdentity *agent_backend_identity_build(const char *provider,
                                               const char *model,
                                               const char *base_url) {
    /* Python: BackendIdentity(provider=_norm_provider(provider), ...) */
    BackendIdentity *bi = malloc(sizeof(BackendIdentity));
    if (!bi) return NULL;
    bi->provider = agent_backend_identity_norm_provider(provider);
    bi->model    = agent_backend_identity_norm_model(model);
    bi->base_url = agent_backend_identity_norm_base_url(base_url);
    return bi;
}

void agent_backend_identity_free(BackendIdentity *bi) {
    if (!bi) return;
    free(bi->provider);
    free(bi->model);
    free(bi->base_url);
    free(bi);
}

const char *agent_backend_identity_provider(const BackendIdentity *bi) { return bi ? bi->provider : ""; }
const char *agent_backend_identity_model(const BackendIdentity *bi)     { return bi ? bi->model : ""; }
const char *agent_backend_identity_base_url(const BackendIdentity *bi)  { return bi ? bi->base_url : ""; }

/* ── _both_first_class ──────────────────────────────────────────────────── */

/* PoP: agent_backend_identity__both_first_class @ agent/backend_identity.py:_both_first_class */
bool agent_backend_identity_both_first_class(const BackendIdentity *a, const BackendIdentity *b) {
    /* Python:
     *   if not a.provider or not b.provider or a.provider == b.provider: return False
     *   try: from hermes_cli.auth import PROVIDER_REGISTRY
     *        return a.provider in PROVIDER_REGISTRY and b.provider in PROVIDER_REGISTRY
     *   except Exception: return False
     */
    if (!a || !b) return false;
    if (!a->provider || !b->provider || !*a->provider || !*b->provider) return false;
    if (strcmp(a->provider, b->provider) == 0) return false;
    /* Check both providers are in the registry */
    return provider_registry_get(a->provider) != NULL &&
           provider_registry_get(b->provider) != NULL;
}

/* ── comparison functions ───────────────────────────────────────────────── */

/* PoP: agent_backend_identity_same_credential_surface @ agent/backend_identity.py:same_credential_surface */
bool agent_backend_identity_same_credential_surface(const BackendIdentity *a, const BackendIdentity *b) {
    /* Python:
     *   if a.provider and b.provider: return a.provider == b.provider
     *   return bool(a.base_url and a.base_url == b.base_url)
     */
    if (!a || !b) return false;
    if (a->provider && *a->provider && b->provider && *b->provider)
        return strcmp(a->provider, b->provider) == 0;
    return (a->base_url && *a->base_url &&
            b->base_url && *b->base_url &&
            strcmp(a->base_url, b->base_url) == 0);
}

/* PoP: agent_backend_identity_same_endpoint @ agent/backend_identity.py:same_endpoint */
bool agent_backend_identity_same_endpoint(const BackendIdentity *a, const BackendIdentity *b) {
    /* Python:
     *   if a.base_url and b.base_url: return a.base_url == b.base_url
     *   return bool(a.provider and a.provider == b.provider)
     */
    if (!a || !b) return false;
    if (a->base_url && *a->base_url && b->base_url && *b->base_url)
        return strcmp(a->base_url, b->base_url) == 0;
    return (a->provider && *a->provider &&
            b->provider && *b->provider &&
            strcmp(a->provider, b->provider) == 0);
}

/* PoP: agent_backend_identity_same_deployment @ agent/backend_identity.py:same_deployment */
bool agent_backend_identity_same_deployment(const BackendIdentity *a, const BackendIdentity *b) {
    /* Full faithful port of the Python logic. */
    if (!a || !b) return false;
    if (!(a->provider && *a->provider && b->provider && *b->provider &&
          strcmp(a->provider, b->provider) == 0)) {
        /* Same-host different-label shims */
        if (a->base_url && *a->base_url &&
            b->base_url && *b->base_url &&
            strcmp(a->base_url, b->base_url) == 0 &&
            a->model && *a->model &&
            b->model && *b->model &&
            strcmp(a->model, b->model) == 0 &&
            !agent_backend_identity_both_first_class(a, b)) {
            return true;
        }
        return false;
    }
    if (!(a->model && *a->model && b->model && *b->model &&
          strcmp(a->model, b->model) == 0))
        return false;
    if (a->base_url && *a->base_url &&
        b->base_url && *b->base_url &&
        strcmp(a->base_url, b->base_url) != 0)
        return false;
    return true;
}

/* PoP: agent_backend_identity_should_skip_candidate @ agent/backend_identity.py:should_skip_candidate */
bool agent_backend_identity_should_skip_candidate(const BackendIdentity *candidate,
                                                   const BackendIdentity *failed,
                                                   int scope) {
    /* Python:
     *   if scope is FailureScope.CREDENTIAL: return same_credential_surface(...)
     *   if scope is FailureScope.ENDPOINT:  return same_endpoint(...)
     *   return same_deployment(...)
     * scope: 0=MODEL, 1=CREDENTIAL, 2=ENDPOINT
     */
    if (!candidate || !failed) return false;
    if (scope == 1) /* CREDENTIAL */
        return agent_backend_identity_same_credential_surface(candidate, failed);
    if (scope == 2) /* ENDPOINT */
        return agent_backend_identity_same_endpoint(candidate, failed);
    return agent_backend_identity_same_deployment(candidate, failed);
}
