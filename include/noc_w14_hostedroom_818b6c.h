#ifndef NOC_W14_HOSTEDROOM_818B6C_H
#define NOC_W14_HOSTEDROOM_818B6C_H

#include "hermes_json.h"

/* C port of the pure validation leaves in gateway/hosted_rooms_common.py
 * (gate w14_hostedroom_818b6c). Each function mirrors a live helper whose
 * error class/message templates are data; ValueError is supplied by the caller
 * via `error`, so every failure string is byte-identical to the Python one.
 *
 * Every function returns a NEW json_t* of the form
 *   {"ok": bool, "value": <result>|null, "error": <message>|null}
 * so a raised error is observable without exceptions.
 */
json_t *w14_hostedroom_818b6c_identifier(json_t *value, const char *label,
                                         int max_chars, int skip_pattern,
                                         const char *invalid);
json_t *w14_hostedroom_818b6c_bounded_int(json_t *value, const char *message,
                                         long low, int has_high, long high);
json_t *w14_hostedroom_818b6c_exact_fields(json_t *value, const char *label,
                                          json_t *required, json_t *optional,
                                          const char *not_object,
                                          const char *missing_fmt,
                                          const char *unknown_fmt);
json_t *w14_hostedroom_818b6c_text(json_t *value, const char *label,
                                  int max_bytes, int strip);
json_t *w14_hostedroom_818b6c_utf8_len(json_t *parts);

#endif /* NOC_W14_HOSTEDROOM_818B6C_H */
