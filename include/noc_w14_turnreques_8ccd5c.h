#ifndef NOC_W14_TURNREQUES_8CCD5C_H
#define NOC_W14_TURNREQUES_8CCD5C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_turnreques_8ccd5c_prepare_moa_request @ agent/turn_request_assembly.py:_prepare_moa_request
 *
 * Mirrors _prepare_moa_request(agent, api_messages, pending). The client
 * capability flags / outputs stand in for the injected chat.completions seam
 * (has_rebase -> rebase_prepared_request, has_prepare -> prepare). Returns a
 * JSON array [prepared, api_messages, pending]; caller frees the result. */
struct json_t *w14_turnreques_8ccd5c_prepare_moa_request(
    const struct json_t *api_messages, const struct json_t *pending,
    int has_rebase, int has_prepare,
    const struct json_t *rebase_out, const struct json_t *prepare_out);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_TURNREQUES_8CCD5C_H */
