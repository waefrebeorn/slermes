/*
 * port_nous_subscription.h -- pure feature-gate helpers ported from
 * hermes_cli/nous_subscription.py.
 *
 * The account lookup, managed-tool-gateway calls and config writes that need a
 * live login stay in Python.
 */
#ifndef PORT_NOUS_SUBSCRIPTION_H
#define PORT_NOUS_SUBSCRIPTION_H

#include <stddef.h>
#include "hermes_json.h"

/* nous_subscription.py:_norm -- str(value or default).strip().lower().
 * Python TRUTHINESS, not a NULL check: 0, 0.0, "" and false all use the
 * default. Caller frees. */
char *sub_norm(const json_t *value, const char *fallback);

/* nous_subscription.py:_section -- config[key] when it is an object, else a
 * NEW empty object. *created is set to that new object (NULL when the existing
 * section was returned) so the caller knows what to free. */
const json_t *sub_section(const json_t *config, const char *key, json_t **created);

/* nous_subscription.py:_ensure_section -- create/replace the section in place.
 * The result is owned by config; caller frees nothing. */
json_t *sub_ensure_section(json_t *config, const char *key);

/* nous_subscription.py:_provider_is_nous */
int sub_provider_is_nous(const json_t *config);

/* nous_subscription.py:_audio_provider -- the managed nous selection and a
 * gateway account are both served by OpenAI's API. Caller frees. */
char *sub_audio_provider(const json_t *cfg, const char *fallback, int gw);

/* nous_subscription.py:_uses_gateway -- legacy use_gateway opt-in. */
int sub_uses_gateway(const json_t *section);

#endif /* PORT_NOUS_SUBSCRIPTION_H */
