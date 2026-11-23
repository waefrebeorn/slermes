#ifndef NOC_W8_SETUPQUICK_C8D6F5_H
#define NOC_W8_SETUPQUICK_C8D6F5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_setupquick_c8d6f5_minimize_config @ hermes_cli/setup_quick.py:_blank_slate_minimize_config
 * Applies the Blank Slate "everything OFF" knobs to a config dict (setdefault
 * semantics) and returns the resulting object. */
struct json_t *w8_setupquick_c8d6f5_minimize_config(const struct json_t *config);

/* PoP: w8_setupquick_c8d6f5_messaging_platform @ hermes_cli/setup_quick.py:_run_quick_setup
 * First-match lookup over _MESSAGING_PLATFORMS: returns ["<label>","<emoji>"]
 * for the first (needle, label, emoji) whose needle is a substring of name,
 * else JSON null. Case-sensitive, order matters. */
struct json_t *w8_setupquick_c8d6f5_messaging_platform(const char *var_name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_SETUPQUICK_C8D6F5_H */
