/**
 * @file hermes_xai_retirement.h
 * @brief L04: Detect retired xAI models in Hermes config.
 *
 * xAI retired several models on May 15, 2026. This module walks
 * a hermes_config_t and returns a list of issues for any reference
 * to a retired xAI model.
 */
#ifndef HERMES_XAI_RETIREMENT_H
#define HERMES_XAI_RETIREMENT_H

#include "hermes_core_types.h"

/* ================================================================
 *  Constants
 * ================================================================ */
#define XAI_RETIREMENT_DATE "May 15, 2026"
#define XAI_MIGRATION_GUIDE_URL "https://docs.x.ai/developers/migration/may-15-retirement"

/* ================================================================
 *  Types
 * ================================================================ */
typedef struct {
    char config_path[128];      /* e.g. "principal.model" or "auxiliary.vision.model" */
    char current_model[128];    /* exact value from config */
    char replacement[64];       /* recommended xAI replacement model */
    char reasoning_effort[32];  /* set if non-reasoning variant needs effort=none */
    char note[256];             /* disambiguation note */
} xai_retirement_issue_t;

#define MAX_RETIREMENT_ISSUES 32

typedef struct {
    xai_retirement_issue_t issues[MAX_RETIREMENT_ISSUES];
    int count;
} xai_retirement_result_t;

/* ================================================================
 *  API
 * ================================================================ */

/**
 * Scan config for retired xAI model references.
 * Returns a result struct with zero or more issues.
 */
xai_retirement_result_t hermes_xai_find_retired(const hermes_config_t *cfg);

/**
 * Format a single retirement issue into a human-readable string.
 * Returns malloc'd string. Caller must free().
 */
char *hermes_xai_format_issue(const xai_retirement_issue_t *issue);

/**
 * Check if a model string looks like an xAI model (starts with "grok-").
 */
bool hermes_xai_looks_like(const char *model);

/**
 * Normalize a model ID: strip x-ai/ or xai/ prefix, lowercase.
 * Returns pointer to static buffer — not thread-safe.
 */
const char *hermes_xai_normalize(const char *model);

/**
 * Resolve a dotted slot path (e.g. "auxiliary.vision.model") to
 * (parent_mapping, leaf_key), faithful to xai_retirement._walk_to_parent.
 * Returns 0 on success, -1 if path has <1 parent, -2 if a segment/leaf
 * is missing or an intermediate node is not a mapping.
 */
int hermes_xai_walk_to_parent(const json_t *yaml_doc, const char *dotted_path,
                              json_t **out_parent, const char **out_leaf);

#endif /* HERMES_XAI_RETIREMENT_H */
