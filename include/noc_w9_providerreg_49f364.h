#ifndef NOC_W9_PROVIDERREG_49F364_H
#define NOC_W9_PROVIDERREG_49F364_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* PoP: w9_providerreg_49f364_lower_key @ agent/provider_registry.py:lower_key
 * name.strip().lower(); caller frees. */
char *w9_providerreg_49f364_lower_key(const char *name);

/* PoP: w9_providerreg_49f364_exact_key @ agent/provider_registry.py:exact_key
 * name unchanged; caller frees. */
char *w9_providerreg_49f364_exact_key(const char *name);

/* PoP: w9_providerreg_49f364_configured_ex @ agent/provider_registry.py:configured_provider_name
 * Seam: raw provider string (already trimmed by caller's config read). Returns
 * NULL when unconfigured, "fal" for the managed Nous selection, else the
 * stripped name; caller frees non-NULL. */
char *w9_providerreg_49f364_configured_ex(const char *raw);

/* PoP: w9_providerreg_49f364_is_available_safe @ agent/provider_registry.py:is_available_safe
 * bool(provider.is_available()) with a raising provider treated as unavailable. */
bool w9_providerreg_49f364_is_available_safe(bool available, bool raises);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_PROVIDERREG_49F364_H */
