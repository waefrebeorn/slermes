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
typedef struct {
    int enabled;              /* 1 if reasoning is enabled, 0 if disabled */
    const char *effort;       /* User's effort choice: "low", "medium", "high" */
    const char *const *allowed_options;  /* NULL-terminated array or NULL */
} lmstudio_reasoning_config_t;

const char *resolve_lmstudio_effort_from_config(const lmstudio_reasoning_config_t *config,
                                                 const char *const *allowed_options)
{
    if (!config) return "medium";  /* Default */
    
    return resolve_lmstudio_effort(config->enabled, config->effort,
                                   allowed_options ? allowed_options : config->allowed_options);
}