/*
 * port_moa_config_pure.h — Faithful C11 ports of module-level pure helpers
 * from Python hermes_cli/moa_config.py (REAL_GAP set in the parity
 * battleground). These are the _or_none / fanout / reasoning-effort / slot /
 * payload validators.
 */

#ifndef PORT_MOA_CONFIG_PURE_H
#define PORT_MOA_CONFIG_PURE_H

#include <stddef.h>

typedef struct json_t json_t;

/* _coerce_float_or_none(value, *out) -> 1 if value present+valid (fills *out), 0 if None. */
int moa_coerce_float_or_none(const char *value, double *out);
/* _coerce_int_or_none(value, *out) -> 1 if value present+valid+positive (fills *out), 0 if None. */
int moa_coerce_int_or_none(const char *value, long *out);
/* _coerce_fanout(value, out, out_size) -> normalizes to "per_iteration"|"user_turn". */
void moa_coerce_fanout(const char *value, char *out, size_t out_size);
/* _clean_reasoning_effort(value) -> malloc'd canonical effort ("none"/"low"/...) or NULL. */
char *moa_clean_reasoning_effort(const char *value);
/* _slot_problem(slot) -> malloc'd problem string or NULL when valid. */
char *moa_slot_problem(const json_t *slot);
/* validate_moa_payload(raw, *out_count) -> malloc'd array of problem strings (caller frees). */
char **moa_validate_moa_payload(const json_t *raw, int *out_count);

#endif /* PORT_MOA_CONFIG_PURE_H */
