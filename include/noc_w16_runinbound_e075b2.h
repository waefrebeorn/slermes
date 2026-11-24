#ifndef NOC_W16_RUNINBOUND_E075B2_H
#define NOC_W16_RUNINBOUND_E075B2_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w16_runinbound_e075b2_hook_choice @ gateway/run_inbound_consumer.py:run_post_admission_hook */
/* The pure decision tail of the hook: first {"action":"handled"} result wins; a
 * whitespace-only or non-str reply is dropped. `mode` is an injected seam for the
 * upstream has_hook/exception paths ("has_hook_false"/"hook_error" -> not handled). */
struct json_t *w16_runinbound_e075b2_hook_choice(const struct json_t *results, const struct json_t *mode);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_RUNINBOUND_E075B2_H */
