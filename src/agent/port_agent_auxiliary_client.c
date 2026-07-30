/*
 * port_agent_auxiliary_client.c — Port of Python agent/auxiliary_client.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>
#include "hermes_logger.h"


/* Port of Python: _aux_interrupt_protected */
static volatile sig_atomic_t aux_interrupt_count = 0;

bool aux_interrupt_protected(void) {
    return (aux_interrupt_count > 0);
}

void aux_interrupt_protected_inc(void) {
    aux_interrupt_count++;
}

void aux_interrupt_protected_dec(void) {
    if (aux_interrupt_count > 0) aux_interrupt_count--;
}


/* Port of Python: _fallback_entry_api_key */
const char *fallback_entry_api_key(const char *provider) {
    if (!provider) return NULL;
    
    /* Look up API key for fallback provider */
    char env_key[256];
    snprintf(env_key, sizeof(env_key), "%s_API_KEY", provider);
    for (char *p = env_key; *p; p++) *p = toupper(*p);
    
    return getenv(env_key);
}


/* Port of Python: _resolve_fallback_entry */
typedef struct {
    char provider[128];
    char model[256];
    char api_key[4096];
    bool valid;
} fallback_entry_t;

fallback_entry_t resolve_fallback_entry(const char *fallback_config, int index) {
    fallback_entry_t entry = {0};
    if (!fallback_config) return entry;
    
    /* Parse fallback entry from config JSON array */
    /* Simplified: extract provider name */
    const char *p = fallback_config;
    for (int i = 0; i < index && *p; i++) {
        p = strchr(p, '{');
        if (!p) break;
        p = strchr(p, '}');
        if (!p) break;
        p++;
    }
    
    if (!p) return entry;
    
    const char *prov = strstr(p, "\"provider\"");
    if (prov) {
        const char *val = strchr(prov + 10, '"');
        if (val) {
            val++;
            size_t i = 0;
            while (*val && *val != '"' && i < 127) {
                entry.provider[i++] = *val++;
            }
            entry.provider[i] = '\0';
        }
    }
    
    entry.valid = (entry.provider[0] != '\0');
    return entry;
}


/* Port of Python: _try_main_fallback_chain */
bool try_main_fallback_chain(const char *primary_provider, const char *fallback_config) {
    if (!primary_provider) return false;
    
    /* Check if primary is available */
    const char *primary_key = fallback_entry_api_key(primary_provider);
    if (primary_key && *primary_key) return true; /* Primary available */
    
    /* Try fallback chain */
    for (int i = 0; i < 8; i++) {
        fallback_entry_t entry = resolve_fallback_entry(fallback_config, i);
        if (!entry.valid) break;
        
        const char *key = fallback_entry_api_key(entry.provider);
        if (key && *key) return true; /* Fallback available */
    }
    
    return false;
}


/* Port of Python: aux_interrupt_protection */
bool aux_interrupt_protection(bool active)
{
    (void)active;
    return aux_interrupt_protected();
}

/* PoP: aux_interrupt_protection @ agent/auxiliary_client.py:aux_interrupt_protection */
bool aux_interrupt_protection_bridge(void *ctx, void *active)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "aux_interrupt_protection: null context");
        return false;
    }
    hermes_log(LOG_DEBUG, "port", "aux_interrupt_protection called");
    return aux_interrupt_protection(active ? true : false);
}


/* Port of Python: _nous_min_key_ttl_seconds */
int nous_min_key_ttl_seconds(void) {
    const char *env = getenv("HERMES_NOUS_MIN_KEY_TTL_SECONDS");
    if (env && *env) {
        int val = atoi(env);
        if (val > 60) return val;
    }
    return 1800;
}


/* Port of Python: _resolve_nous_pool_runtime_api */
bool resolve_nous_pool_runtime_api(bool force_refresh, char *api_key_out, size_t api_key_sz, char *base_url_out, size_t base_url_sz) {
    if (!api_key_out || api_key_sz == 0 || !base_url_out || base_url_sz == 0) {
        return false;
    }

    /* Try to get credentials from environment */
    const char *api_key = getenv("NOUS_API_KEY");
    if (!api_key) api_key = getenv("NOUS_BILLING_KEY");
    if (!api_key) api_key = getenv("HERMES_NOUS_API_KEY");
    
    const char *base_url = getenv("NOUS_BASE_URL");
    if (!base_url) base_url = getenv("NOUS_PORTAL_URL");
    if (!base_url) base_url = "https://api.nousresearch.com";

    if (!api_key || !*api_key) {
        return false;
    }

    strncpy(api_key_out, api_key, api_key_sz - 1);
    api_key_out[api_key_sz - 1] = '\0';
    strncpy(base_url_out, base_url, base_url_sz - 1);
    base_url_out[base_url_sz - 1] = '\0';

    return true;
}