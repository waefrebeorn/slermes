#ifndef SRC_CLI_PORT_MEMORY_PROVIDERS_C
#define SRC_CLI_PORT_MEMORY_PROVIDERS_C

#include "hermes_memory_providers.h"
#include "hermes_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Port of Python: is_secret                                            */
/* ------------------------------------------------------------------ */
bool is_secret(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "is_secret: null context");
        return false;
    }
    const char *name = (const char *)ctx;
    const char *val = getenv(name);
    bool secret = (val != NULL && val[0] != '\0');
    hermes_log(LOG_DEBUG, "port", "is_secret: %s=%s", name, secret ? "true" : "false");
    return secret;
}

/* ================================================================== */
/*  Declarative provider registry (faithful to memory_providers.py)      */
/* ================================================================== */

/* Hindsight provider options */
static const hermes_mp_option_t HINDSIGHT_MODE_OPTS[] = {
    { "cloud",         "Cloud",         "Hindsight Cloud API (lightweight, just needs an API key)" },
    { "local_external", "Local External", "Connect to an existing Hindsight instance" },
};
static const hermes_mp_option_t HINDSIGHT_RECALL_OPTS[] = {
    { "low",  "low"  },
    { "mid",  "mid"  },
    { "high", "high" },
};

/* Hindsight provider fields */
static const hermes_mp_field_t HINDSIGHT_FIELDS[] = {
    {
        .key = "mode", .label = "Mode",
        .kind = HERMES_MP_KIND_SELECT, .default_val = "cloud",
        .description = "How Hermes connects to Hindsight.",
        .options = HINDSIGHT_MODE_OPTS,
        .noptions = (int)(sizeof(HINDSIGHT_MODE_OPTS) / sizeof(HINDSIGHT_MODE_OPTS[0])),
    },
    {
        .key = "api_key", .label = "API key",
        .kind = HERMES_MP_KIND_SECRET, .env_key = "HINDSIGHT_API_KEY",
        .description = "Used to authenticate with the Hindsight API.",
        .placeholder = "Enter Hindsight API key",
    },
    {
        .key = "api_url", .label = "API URL",
        .kind = HERMES_MP_KIND_TEXT, .default_val = "https://api.hindsight.vectorize.io",
    },
    {
        .key = "bank_id", .label = "Bank ID",
        .kind = HERMES_MP_KIND_TEXT, .default_val = "hermes",
    },
    {
        .key = "recall_budget", .label = "Recall budget",
        .kind = HERMES_MP_KIND_SELECT, .default_val = "mid",
        .options = HINDSIGHT_RECALL_OPTS,
        .noptions = (int)(sizeof(HINDSIGHT_RECALL_OPTS) / sizeof(HINDSIGHT_RECALL_OPTS[0])),
    },
};

static const hermes_mp_provider_t HINDSIGHT = {
    .name = "hindsight", .label = "Hindsight",
    .fields = HINDSIGHT_FIELDS,
    .nfields = (int)(sizeof(HINDSIGHT_FIELDS) / sizeof(HINDSIGHT_FIELDS[0])),
};

/* Registry of providers that expose a desktop config surface. */
static const hermes_mp_provider_t *MEMORY_PROVIDERS[] = {
    &HINDSIGHT,
    NULL,
};

/* ------------------------------------------------------------------ */
/*  get_memory_provider                                                */
/* ------------------------------------------------------------------ */
/* PoP: hermes_mp_get_provider @ hermes_cli/memory_providers.py:get_memory_provider */
const hermes_mp_provider_t *hermes_mp_get_provider(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; MEMORY_PROVIDERS[i]; i++) {
        if (strcmp(MEMORY_PROVIDERS[i]->name, name) == 0)
            return MEMORY_PROVIDERS[i];
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  ProviderField.allowed_values                                      */
/* ------------------------------------------------------------------ */
/* PoP: hermes_mp_allowed_values @ hermes_cli/memory_providers.py:ProviderField.allowed_values */
int hermes_mp_allowed_values(const hermes_mp_provider_t *provider,
                              const char *key,
                              const char *out[], int out_cap)
{
    if (!provider || !key || !out) return 0;
    for (int i = 0; i < provider->nfields; i++) {
        const hermes_mp_field_t *f = &provider->fields[i];
        if (strcmp(f->key, key) != 0) continue;
        int n = 0;
        for (int j = 0; j < f->noptions && n < out_cap; j++)
            out[n++] = f->options[j].value;
        return n;
    }
    return 0;
}

#endif /* SRC_CLI_PORT_MEMORY_PROVIDERS_C */
