/*
 * noc_w29_adapter_d36d4e_p1.h - C surface for the module-level pure helpers of
 * upstream plugins/platforms/signal/adapter.py (gate w29_adapter_d36d4e_p1).
 *
 * Comma-list config parsing, magic-byte extension guessing (with the shared
 * tools/audio_container sniff), image/audio extension predicates, the
 * extension→mime table, mention placeholder rendering, Signal service-id and
 * E.164 predicates, and the connect-time config gate.  Nothing here opens a
 * socket or touches the filesystem.
 */
#ifndef NOC_W29_ADAPTER_D36D4E_P1_H
#define NOC_W29_ADAPTER_D36D4E_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* " a, b ,," -> ["a","b"]; non-string input is the Python AttributeError. */
json_t *w29_adapter_d36d4e_p1_parse_comma_list(const json_t *value);

/* base64 payload -> ".png"/".jpg"/".gif"/".pdf"/".webp"/container ext/".zip"/".bin". */
json_t *w29_adapter_d36d4e_p1_guess_extension(const json_t *data_b64);

json_t *w29_adapter_d36d4e_p1_is_image_ext(const json_t *ext);
json_t *w29_adapter_d36d4e_p1_is_audio_ext(const json_t *ext);
json_t *w29_adapter_d36d4e_p1_ext_to_mime(const json_t *ext);

/* text with \uFFFC placeholders replaced by @identifier, right to left. */
json_t *w29_adapter_d36d4e_p1_render_mentions(const json_t *text, const json_t *mentions);

json_t *w29_adapter_d36d4e_p1_is_signal_service_id(const json_t *value);
json_t *w29_adapter_d36d4e_p1_looks_like_e164_number(const json_t *value);

/* extra (config.extra) + env (SIGNAL_HTTP_URL/SIGNAL_ACCOUNT) -> bool. */
json_t *w29_adapter_d36d4e_p1_validate_signal_config(const json_t *extra, const json_t *env);

json_t *w29_adapter_d36d4e_p1_check_signal_requirements(void);

#ifdef __cplusplus
}
#endif
#endif
