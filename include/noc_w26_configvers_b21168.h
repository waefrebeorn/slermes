/*
 * noc_w26_configvers_b21168.h - C surface for upstream
 * hermes_cli/config_version_stamp.py (gate w26_configvers_b21168).
 *
 * The raw ``_config_version`` read behind ``check_config_version()``: how a
 * stamped value is coerced, what (stamp, latest) a config document yields, and
 * the message a too-new stamp gets.  Reading config.yaml is a seam -- the
 * document arrives already parsed, its existence and parse failure as flags.
 */
#ifndef NOC_W26_CONFIGVERS_B21168_H
#define NOC_W26_CONFIGVERS_B21168_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The schema version this build knows (DEFAULT_CONFIG["_config_version"]). */
#define W26_CONFIGVERS_B21168_LATEST 54

/* _coerce_config_version: bool/None/unparsable read as legacy 0, negatives
 * clamp to 0, numeric strings and floats truncate like int(). */
json_t *w26_configvers_b21168_coerce(const json_t *value);

/* read_config_version_stamp -> [stamp, latest]; stamp is null when the document
 * parsed but carries no ``_config_version`` key.  A parse failure or a
 * non-mapping root is [latest, latest] for a tolerant caller and the
 * InvalidUserConfigError seam ({"error": ...}) when `raise` is set. */
json_t *w26_configvers_b21168_read_stamp(const json_t *state);

/* check_config_version -> [current, latest]: a missing stamp reads as 0. */
json_t *w26_configvers_b21168_check_version(const json_t *state);

/* newer_stamp_message(stamp). */
json_t *w26_configvers_b21168_newer_message(int stamp);

/* The module's literal constants, as an object. */
json_t *w26_configvers_b21168_constants(void);

#ifdef __cplusplus
}
#endif
#endif
