#ifndef NOC_W13_CLI_VOICEM_324E35_H
#define NOC_W13_CLI_VOICEM_324E35_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_clivoicem_324e35_numeric_or @ hermes_cli/cli_voice_mixin.py:_numeric_or
 * value if a real int/float (NOT bool), else default. Returns a NEW json value. */
struct json_t *w13_clivoicem_324e35_numeric_or(const struct json_t *value,
                                               const struct json_t *dflt);

/* PoP: w13_clivoicem_324e35_voice_stt_model @ hermes_cli/cli_voice_mixin.py:CLIVoiceMixin._voice_stt_model
 * stt config seam: local provider prefers local.model or "base"; else config["model"]. */
struct json_t *w13_clivoicem_324e35_voice_stt_model(const struct json_t *stt_config);

/* PoP: w13_clivoicem_324e35_voice_stt_provider @ hermes_cli/cli_voice_mixin.py:CLIVoiceMixin._voice_stt_provider
 * str(config.get("provider") or "").strip().lower() */
struct json_t *w13_clivoicem_324e35_voice_stt_provider(const struct json_t *stt_config);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_CLI_VOICEM_324E35_H */
