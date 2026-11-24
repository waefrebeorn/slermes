/*
 * noc_w30_modelscata_4ee086.h - C surface for upstream
 * hermes_cli/models_catalog_static.py (gate w30_modelscata_4ee086).
 *
 * The curated provider/model catalogue: per-provider curated id lists, the
 * canonical provider registry, display groups, the alias table and the
 * capability sets that gate picker/routing behaviour.  Data only - no network,
 * no filesystem.  Every lookup here is a pure function of the input value and
 * the frozen built-in tables.
 *
 * SEAMS (they stay in the caller and are never entered here): the models.dev
 * disk cache behind _xai_curated_models(), hermes_cli.codex_models behind
 * _codex_curated_models(), and the plugin provider registry that
 * sync_plugin_provider_catalog() folds into the tables at import.
 */
#ifndef NOC_W30_MODELSCATA_4EE086_H
#define NOC_W30_MODELSCATA_4EE086_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The frozen built-in tables as one object: curated provider rows, canonical
 * providers, labels, display groups, slug->group index, the alias table, the
 * aggregator/live-first/models.dev capability sets and the wire prefix tuples. */
json_t *w30_modelscata_4ee086_constants(void);

/* ids -> ids with _XAI_TOP_MODEL pinned to the front (unchanged if absent). */
json_t *w30_modelscata_4ee086_xai_promote_top(const json_t *ids);

/* ids -> ids with the curated xAI extras inserted after the pinned headline. */
json_t *w30_modelscata_4ee086_xai_merge_curated_extras(const json_t *ids);

/* promote_top(merge_curated_extras(ids)) - the xAI catalogue finaliser. */
json_t *w30_modelscata_4ee086_xai_finalize_catalog(const json_t *ids);

/* slug -> its provider group id, or "" when ungrouped. */
json_t *w30_modelscata_4ee086_provider_group_for_slug(const json_t *slug);

/* Flat ordered slug list -> picker rows ({"kind":"single",...} /
 * {"kind":"group",...}), folding grouped slugs at their first position. */
json_t *w30_modelscata_4ee086_group_providers(const json_t *slugs);

/* Plugin profile name -> true when it has no built-in row (picker admission). */
json_t *w30_modelscata_4ee086_plugin_provider_enters_picker(const json_t *name);

/* slug -> curated id list, or null when the slug owns no static row. */
json_t *w30_modelscata_4ee086_provider_models(const json_t *slug);

/* slug -> display label, or null. */
json_t *w30_modelscata_4ee086_provider_label(const json_t *slug);

/* alias -> canonical slug from the built-in alias table, or null. */
json_t *w30_modelscata_4ee086_provider_alias(const json_t *key);

/* slug -> [slug, label, tui_desc], or null when no canonical row exists. */
json_t *w30_modelscata_4ee086_canonical_provider_row(const json_t *slug);

/* group id -> [label, description, [member slugs]], or null. */
json_t *w30_modelscata_4ee086_group_members(const json_t *gid);

#ifdef __cplusplus
}
#endif
#endif
