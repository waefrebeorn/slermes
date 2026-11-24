/*
 * noc_w31_shared_97a29d_p2.h - C surface for upstream
 * gateway/platforms/_shared.py (gate w31_shared_97a29d_p2).
 *
 * The delivery/payload shaping each adapter shares: what a standalone send's
 * result dict proves about delivery, the ids it put in the chat, the failure
 * envelope stamped with that history, the multi-message success fields, and the
 * table-driven YAML->extra seeding (`apply_yaml_bridge` / `_YAML_KINDS`) that
 * decides which adapter settings a config.yaml key contributes.
 */
#ifndef NOC_W31_SHARED_97A29D_P2_H
#define NOC_W31_SHARED_97A29D_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* delivery_state: "delivered" / "partial" / "uncertain" / "none". */
#define W31_SHARED_97A29D_P2_DELIVERED "delivered"
#define W31_SHARED_97A29D_P2_PARTIAL   "partial"
#define W31_SHARED_97A29D_P2_UNCERTAIN "uncertain"
#define W31_SHARED_97A29D_P2_NONE      "none"

/* result -> one of the four DELIVERED/PARTIAL/UNCERTAIN/NONE strings above.
 * A non-dict, or a dict with no truthy "error", is DELIVERED; an explicit
 * "delivery" wins; else `retryable is True` means NONE, anything else
 * UNCERTAIN. */
json_t *w31_shared_97a29d_p2_delivery_state(const json_t *result);

/* result -> [str, ...].  Every landed chunk id a standalone send reports,
 * [] when none are known (missing / falsy / non-dict). */
json_t *w31_shared_97a29d_p2_delivered_message_ids(const json_t *result);

/* (error, message_ids, has_landed, is_ambiguous, never_sent) -> dict.
 * DeliveryOutcome.failure: the error envelope stamped with the whole attempt
 * history's delivery state, its landed ids merged in, and `retryable` only
 * when NONE. */
json_t *w31_shared_97a29d_p2_outcome_failure(const json_t *error, const json_t *message_ids,
                                            const json_t *has_landed, const json_t *is_ambiguous,
                                            const json_t *never_sent);

/* message_ids -> {} or {"delivered_message_ids": [...]}.  Success-envelope
 * fields of a send split into several messages (more than one landed id). */
json_t *w31_shared_97a29d_p2_split_fields(const json_t *message_ids);

/* (cfg, key, kind) -> bool.  The applies-when predicate of _YAML_KINDS:
 * lower/json bridge whenever the key is present (a YAML `none` still writes),
 * str skips null/blank, csv skips null only.  json_null() for an unknown kind
 * (upstream would raise KeyError). */
json_t *w31_shared_97a29d_p2_yaml_kind_applies(const json_t *cfg, const char *key,
                                               const char *kind);

/* (cfg, spec) -> dict or null.  apply_yaml_bridge's seeded `extra` for a
 * [(yaml_key, ENV_VAR, kind), ...] spec: the original YAML value of every row
 * whose kind applies, null when nothing matched.  The env write itself (and
 * the `json` kind's json.dumps encode) is the seam. */
json_t *w31_shared_97a29d_p2_yaml_bridge_seed(const json_t *cfg, const json_t *spec);

#ifdef __cplusplus
}
#endif
#endif
