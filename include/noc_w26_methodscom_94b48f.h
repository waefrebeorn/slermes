/*
 * noc_w26_methodscom_94b48f.h - C surface for upstream
 * tui_gateway/methods_complete_helpers.py (gate w26_methodscom_94b48f).
 *
 * The pure ranking core of the complete.* RPCs: the @-mention / Cmd-P fuzzy
 * ranker, the non-git walk filter and the cache/exclude budgets.  The repo
 * listing itself (git ls-files, os.walk) is the seam and stays in the caller.
 */
#ifndef NOC_W26_METHODSCOM_94B48F_H
#define NOC_W26_METHODSCOM_94B48F_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W26_METHODSCOM_94B48F_TIER_EXACT 0
#define W26_METHODSCOM_94B48F_TIER_PREFIX 1
#define W26_METHODSCOM_94B48F_TIER_WORD 2
#define W26_METHODSCOM_94B48F_TIER_SUBSTRING 3
#define W26_METHODSCOM_94B48F_TIER_SUBSEQUENCE 4

/* req: {"name": <str>, "query": <str|falsy>} -> [tier, len(name)] or null. */
json_t *w26_methodscom_94b48f_rank(const json_t *req);

/* req: {"dirnames": [<str>, ...]} -> the names os.walk may descend into
 * (not in the fallback excludes and not dot-prefixed). */
json_t *w26_methodscom_94b48f_keep_dirnames(const json_t *req);

/* The fuzzy-cache TTL / file cap / fallback-exclude budget, as an object. */
json_t *w26_methodscom_94b48f_constants(void);

#ifdef __cplusplus
}
#endif
#endif
