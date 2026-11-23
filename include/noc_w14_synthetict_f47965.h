#ifndef NOC_W14_SYNTHETICT_F47965_H
#define NOC_W14_SYNTHETICT_F47965_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_synthetict_f47965_synth_turn_armed @ tui_gateway/synthetic_turn.py:synth_turn_armed
 * env_value is the injected HERMES_ISO_CERTIFY_SYNTH_TURN value (Windows CRT env pitfall). */
struct json_t *w14_synthetict_f47965_synth_turn_armed(const struct json_t *env_value);
/* PoP: w14_synthetict_f47965_parse_spec @ tui_gateway/synthetic_turn.py:SyntheticHeavyAgent._parse_spec
 * defaults carries the injected _env_float/_env_int values. */
struct json_t *w14_synthetict_f47965_parse_spec(const struct json_t *message, const struct json_t *defaults);
/* PoP: w14_synthetict_f47965_maybe_build_agent @ tui_gateway/synthetic_turn.py:maybe_build_synthetic_agent
 * armed is the injected synth_turn_armed() result. */
struct json_t *w14_synthetict_f47965_maybe_build_agent(const struct json_t *session_id,
                                     const struct json_t *model_override,
                                     const struct json_t *armed);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SYNTHETICT_F47965_H */
