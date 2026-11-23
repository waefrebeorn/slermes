#ifndef NOC_W14_MCPELICITA_0BEAE8_H
#define NOC_W14_MCPELICITA_0BEAE8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_mcpelicita_0beae8_options @ tools/mcp_elicitation_form.py:_options */
struct json_t *w14_mcpelicita_0beae8_options(const struct json_t *spec);

/* PoP: w14_mcpelicita_0beae8_field_kind @ tools/mcp_elicitation_form.py:_field_kind */
struct json_t *w14_mcpelicita_0beae8_field_kind(const struct json_t *spec);

/* PoP: w14_mcpelicita_0beae8_match_option @ tools/mcp_elicitation_form.py:_match_option */
struct json_t *w14_mcpelicita_0beae8_match_option(const struct json_t *spec,
                                                  const char *text);

/* PoP: w14_mcpelicita_0beae8_coerce_boolean @ tools/mcp_elicitation_form.py:_coerce_boolean */
struct json_t *w14_mcpelicita_0beae8_coerce_boolean(const char *text);

/* PoP: w14_mcpelicita_0beae8_format_error @ tools/mcp_elicitation_form.py:_format_error */
const char *w14_mcpelicita_0beae8_format_error(const char *fmt, const char *text);

/* PoP: w14_mcpelicita_0beae8_coerce_string @ tools/mcp_elicitation_form.py:_coerce_string */
struct json_t *w14_mcpelicita_0beae8_coerce_string(const struct json_t *spec,
                                                   const char *text);

/* PoP: w14_mcpelicita_0beae8_coerce_number @ tools/mcp_elicitation_form.py:_coerce_number */
struct json_t *w14_mcpelicita_0beae8_coerce_number(const struct json_t *spec,
                                                   const char *text);

/* PoP: w14_mcpelicita_0beae8_coerce_multi @ tools/mcp_elicitation_form.py:_coerce_multi */
struct json_t *w14_mcpelicita_0beae8_coerce_multi(const struct json_t *spec,
                                                  const struct json_t *raw);

/* PoP: w14_mcpelicita_0beae8_coerce_answer @ tools/mcp_elicitation_form.py:coerce_answer */
struct json_t *w14_mcpelicita_0beae8_coerce_answer(const struct json_t *spec,
                                                   const struct json_t *raw,
                                                   int required);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_MCPELICITA_0BEAE8_H */
