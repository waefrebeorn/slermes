#ifndef NOC_W14_HOSTEDROOM_765F11_H
#define NOC_W14_HOSTEDROOM_765F11_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w14_hostedroom_765f11_identifier @ session_rules/hosted_rooms.py:identifier */
struct json_t *w14_hostedroom_765f11_identifier(const struct json_t *value, const struct json_t *label,
        const struct json_t *max_chars, const struct json_t *pattern_mode, const struct json_t *invalid);
/* PoP: w14_hostedroom_765f11_bounded_int @ session_rules/hosted_rooms.py:bounded_int */
struct json_t *w14_hostedroom_765f11_bounded_int(const struct json_t *value, const struct json_t *message,
        const struct json_t *low, const struct json_t *high);
/* PoP: w14_hostedroom_765f11_text @ session_rules/hosted_rooms.py:text */
struct json_t *w14_hostedroom_765f11_text(const struct json_t *value, const struct json_t *label,
        const struct json_t *max_bytes, const struct json_t *strip);
/* PoP: w14_hostedroom_765f11_validate_context @ session_rules/hosted_rooms.py:validate_context */
struct json_t *w14_hostedroom_765f11_validate_context(const struct json_t *purpose, const struct json_t *notes,
        const struct json_t *routing, const struct json_t *lead_member_id);
/* PoP: w14_hostedroom_765f11_context_from_payload @ session_rules/hosted_rooms.py:context_from_payload */
struct json_t *w14_hostedroom_765f11_context_from_payload(const struct json_t *payload);
/* PoP: w14_hostedroom_765f11_retry_fields @ session_rules/hosted_rooms.py:retry_fields */
struct json_t *w14_hostedroom_765f11_retry_fields(const struct json_t *context, const struct json_t *original);
/* PoP: w14_hostedroom_765f11_retained_fields @ session_rules/hosted_rooms.py:retained_fields */
struct json_t *w14_hostedroom_765f11_retained_fields(const struct json_t *table);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_HOSTEDROOM_765F11_H */
