#ifndef NOC_W8_CLIRENDER_3BF599_H
#define NOC_W8_CLIRENDER_3BF599_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_clirender_3bf599_luminance_from_hex @ hermes_cli/cli_render.py:_luminance_from_hex
 * Returns a JSON number (Rec.709 luma in [0,1]) or JSON null when malformed. */
struct json_t *w8_clirender_3bf599_luminance_from_hex(const char *hex_str);

/* PoP: w8_clirender_3bf599_coerce_output_history_limit @ hermes_cli/cli_render.py:_coerce_output_history_limit
 * Python int() coercion with floor 10 and fallback 200. Returns a JSON number. */
struct json_t *w8_clirender_3bf599_coerce_output_history_limit(const struct json_t *value);

/* PoP: w8_clirender_3bf599_prepend_note_to_message @ hermes_cli/cli_render.py:_prepend_note_to_message
 * Returns a JSON string (str input), a JSON array (list input), or an unchanged
 * copy for unknown shapes, matching the Python function. */
struct json_t *w8_clirender_3bf599_prepend_note_to_message(const struct json_t *message,
                                                          const struct json_t *note);

/* PoP: w8_clirender_3bf599_assistant_content_as_text @ hermes_cli/cli_render.py:_assistant_content_as_text
 * None->"", str->str, list of {"type":"text"} parts joined by newline, else str(). */
struct json_t *w8_clirender_3bf599_assistant_content_as_text(const struct json_t *content);

/* PoP: w8_clirender_3bf599_maybe_remap_for_light_mode @ hermes_cli/cli_render.py:_maybe_remap_for_light_mode
 * Explicit seam: `light` is the injected _detect_light_mode() result (0/1).
 * Returns a malloc'd string; caller frees. */
char *w8_clirender_3bf599_maybe_remap_for_light_mode(const char *hex_color, int light);

/* PoP: w8_clirender_3bf599_detect_light_mode_ex @ hermes_cli/cli_render.py:_detect_light_mode_uncached
 * Explicit seam: `env` is the injected environment object and `osc_bg` the
 * injected _query_osc11_background() result. Returns 0/1 (False/True). */
int w8_clirender_3bf599_detect_light_mode_ex(const struct json_t *env, const char *osc_bg);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_CLIRENDER_3BF599_H */
