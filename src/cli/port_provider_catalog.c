#ifndef SRC_CLI_PORT_PROVIDER_CATALOG_C
#define SRC_CLI_PORT_PROVIDER_CATALOG_C

#include "hermes.h"
#include "hermes_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Port of Python: provider_catalog */
void provider_catalog(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "provider_catalog: null context");
        return;
    }
    hermes_log(LOG_INFO, "port", "provider_catalog: listing providers");

    const char *providers[] = {
        "anthropic", "openai", "google", "mistral",
        "xai", "nous", "deepseek", "groq", NULL
    };
    for (int i = 0; providers[i]; i++) {
        hermes_log(LOG_DEBUG, "port", "provider_catalog: %s", providers[i]);
    }
}

#endif /* SRC_CLI_PORT_PROVIDER_CATALOG_C */
