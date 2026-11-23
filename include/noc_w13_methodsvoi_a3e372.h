#ifndef NOC_W13_METHODSVOI_A3E372_H
#define NOC_W13_METHODSVOI_A3E372_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: methodvoi_voice_cfg_number @ tui_gateway/methods_voice.py:_voice_cfg_number
 * value if it is a non-bool number else dflt. Returns a NEW json value. */
struct json_t *methodvoi_voice_cfg_number(const struct json_t *value, const struct json_t *dflt);

/* PoP: methodvoi_frame_fields @ tui_gateway/methods_voice.py:_frame_fields
 * {"sample_rate": frame.get("sample_rate",16000),
 *  "frame_length": frame.get("frame_length",1280)} (dict.get: present-null kept). */
struct json_t *methodvoi_frame_fields(const struct json_t *frame);

/* PoP: methodvoi_fd_barge_params @ tui_gateway/methods_voice.py:_fd_barge_params
 * JSON array [threshold_multiplier (float), grace_ms (int)] from cfg. */
struct json_t *methodvoi_fd_barge_params(const struct json_t *cfg);

/* PoP: methodvoi_owner_result @ tui_gateway/methods_voice.py:_owner_result
 * {field: ok, "reason": null if truthy(ok) else "not_owner", **extra}.
 * The stubbed _ok is identity; extra keys are applied last (may override). */
struct json_t *methodvoi_owner_result(const char *field, const struct json_t *ok,
                                      const struct json_t *extra);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_METHODSVOI_A3E372_H */
