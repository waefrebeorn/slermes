/*
 * port_moa_config_pure.h — declarations for the second batch of pure
 * hermes_cli/moa_config.py helpers (see port_moa_config_pure.c).
 */

#ifndef PORT_MOA_CONFIG_PURE_H
#define PORT_MOA_CONFIG_PURE_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"   /* json_t */

#ifdef __cplusplus
extern "C" {
#endif

/* moa_config.py _coerce_float_or_none — returns 1 + *out, or 0 (None). */
/* PoP: moa_coerce_float_or_none @ hermes_cli/moa_config.py:_coerce_float_or_none */
int moa_coerce_float_or_none(const char *value, double *out);

/* moa_config.py _coerce_int */
/* PoP: moa_coerce_int @ hermes_cli/moa_config.py:_coerce_int */
int moa_coerce_int(const char *value, int def);

/* moa_config.py _coerce_int_or_none — returns 1 + *out (positive), or 0. */
/* PoP: moa_coerce_int_or_none @ hermes_cli/moa_config.py:_coerce_int_or_none */
int moa_coerce_int_or_none(const char *value, long *out);

/* moa_config.py _coerce_fanout — returns malloc'd "per_iteration"/"user_turn". */
/* PoP: moa_coerce_fanout @ hermes_cli/moa_config.py:_coerce_fanout */
char *moa_coerce_fanout(const char *value);

/* moa_config.py _clean_reasoning_effort — returns malloc'd effort or NULL. */
/* PoP: moa_clean_reasoning_effort @ hermes_cli/moa_config.py:_clean_reasoning_effort */
char *moa_clean_reasoning_effort(const char *value);

/* moa_config.py _slot_problem — returns malloc'd problem or NULL if valid. */
/* PoP: moa_slot_problem @ hermes_cli/moa_config.py:_slot_problem */
char *moa_slot_problem(const json_t *slot);

/* moa_config.py validate_moa_payload — returns malloc'd JSON array of problems. */
/* PoP: moa_validate_moa_payload @ hermes_cli/moa_config.py:validate_moa_payload */
char *moa_validate_moa_payload(const json_t *raw);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MOA_CONFIG_PURE_H */
