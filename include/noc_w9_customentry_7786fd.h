#ifndef NOC_W9_CUSTOMENTRY_7786FD_H
#define NOC_W9_CUSTOMENTRY_7786FD_H

#include <hermes_json.h>

/* C11 ports of the pure credential-ownership helpers in
 * agent/custom_entry_pool.py. Every symbol is prefixed w9_customentry_7786fd_
 * and takes one borrowed json_t *args object, returning a NEW json_t *.
 * Registry / OpenRouter values are injected via the args object so the C
 * side never reads live config or environment. */
json_t *w9_customentry_7786fd_entry_declares_credential(json_t *a);
json_t *w9_customentry_7786fd_own_seeded_sources(json_t *a);
json_t *w9_customentry_7786fd_builtin_pool_address(json_t *a);
json_t *w9_customentry_7786fd_recorded_address(json_t *a);

#endif /* NOC_W9_CUSTOMENTRY_7786FD_H */
