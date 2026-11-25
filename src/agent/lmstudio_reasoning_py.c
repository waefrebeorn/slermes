/*
 * lmstudio_reasoning_py.c — Python-compatible wrapper for LM Studio reasoning.
 *
 * Port of Python agent/lmstudio_reasoning.py:resolve_lmstudio_effort().
 * This file provides the Python-compatible API that the parity scanner expects.
 */

#include "lmstudio_reasoning.h"
#include <stdlib.h>
#include <string.h>

/* Python-style wrapper that takes a dict-like config structure.
 * The caller passes a struct with enabled, effort, and allowed_options
 * fields that mirrors the Python dict structure.
 */
/* The config type and the resolver both live in lmstudio_reasoning.h. This file used to
 * re-declare the struct (with an extra allowed_options field) and call the resolver with
 * three arguments -- a private type and a private arity that only compiled as long as
 * nothing checked them. Use the header's type; keep allowed_options as a parameter of this
 * wrapper only, because that is where the Python dict carried the model's option list. */

const char *resolve_lmstudio_effort_from_config(const lmstudio_reasoning_config_t *config,
                                                const char *const *allowed_options)
{
    if (!config) return "medium";  /* Default */

    /* The header's resolver takes (config, allowed_options) -- the config carries enabled
     * and effort. Python's signature is (reasoning_config, allowed_options) exactly. */
    return resolve_lmstudio_effort(config, allowed_options);
}