#ifndef NOC_W14_SETUPPLATF_2D6BDC_H
#define NOC_W14_SETUPPLATF_2D6BDC_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_setupplatf_2d6bdc_valid_telegram_token @ hermes_cli/setup_platforms.py:_is_valid_telegram_bot_token */
struct json_t *w14_setupplatf_2d6bdc_valid_telegram_token(const struct json_t *in);

/* PoP: w14_setupplatf_2d6bdc_is_progress @ hermes_cli/setup_platforms.py:_is_progress */
struct json_t *w14_setupplatf_2d6bdc_is_progress(const struct json_t *in);

/* PoP: w14_setupplatf_2d6bdc_allowlist @ hermes_cli/setup_platforms.py:_save_allowlist
 * users.replace(" ", "") — the allowlist normalization shared by the wizards. */
struct json_t *w14_setupplatf_2d6bdc_allowlist(const struct json_t *in);

/* PoP: w14_setupplatf_2d6bdc_save_port @ hermes_cli/setup_platforms.py:_save_port
 * int(value) port persistence decision: {"action","value"}. */
struct json_t *w14_setupplatf_2d6bdc_save_port(const struct json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SETUPPLATF_2D6BDC_H */
