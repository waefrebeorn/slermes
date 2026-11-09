/*
 * port_codex_models.h -- pure Codex catalog helpers ported from
 * hermes_cli/codex_models.py.
 *
 * The API fetch, the on-disk cache and the -900k variant synthesis stay in
 * Python.
 */
#ifndef PORT_CODEX_MODELS_H
#define PORT_CODEX_MODELS_H

#include <stddef.h>

/* codex_models.py:_dedupe -- order-preserving dedupe. Returns a newly
 * allocated NULL-terminated array of BORROWED string pointers; free the ARRAY
 * only. *out_count receives the length. */
const char **cm_dedupe(const char *const *model_ids, int *out_count);

/* codex_models.py:_add_forward_compat_models -- append synthetic slugs whose
 * template is already present. Same ownership contract as cm_dedupe. */
const char **cm_add_forward_compat(const char *const *model_ids, int *out_count);

#endif /* PORT_CODEX_MODELS_H */
