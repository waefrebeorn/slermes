/*
 * config_setup_wizard.h — interactive config-setup wizard.
 *
 * Module-internal header (config_setup.c <-> config_setup_wizard.c).
 * Declares the wizard steps the exported setup entries call.
 */
#ifndef CONFIG_SETUP_WIZARD_H
#define CONFIG_SETUP_WIZARD_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_core_types.h"   /* hermes_config_t */

typedef struct {
    const char *name;         /* provider name (env var name) */
    const char *description;  /* display description */
    const char *url;          /* where to get the key */
    const char *prompt_text;  /* prompt text */
    const char *key_env;      /* env var to save to */
} setup_provider_key_info_t;

void setup_agent_settings(hermes_config_t *cfg);
void setup_apply_default_agent_settings(hermes_config_t *cfg);
char ** setup_fetch_provider_models(const char *provider_name, const char *api_key, int *out_count);
void setup_gateway(hermes_config_t *cfg);
bool setup_is_interactive(void);
void setup_nous_model_picker(char *out_model, size_t out_size);
void setup_pick_model(char *out_model, size_t out_size, char **models, int n_models, const char *default_model);
void setup_print_header(const char *title);
void setup_print_noninteractive_guidance(const char *reason);
void setup_print_summary(const hermes_config_t *cfg);
char * setup_prompt(const char *question, const char *default_val);
void setup_prompt_api_key(const setup_provider_key_info_t *info, char *out, size_t out_size);
void setup_prompt_base_url(const char *provider, char *out, size_t out_size);
int setup_prompt_choice(const char *question, const char *choices[], int n_choices, int default_idx);
bool setup_prompt_yes_no(const char *question, bool default_yes);
void setup_sanitize_pasted_input(char *value);
bool setup_save_env(const char *key, const char *value);
void setup_set_provider_endpoint(hermes_config_t *cfg, const char *provider);
void setup_terminal_backend(hermes_config_t *cfg);
void setup_tts(hermes_config_t *cfg);

extern const char *SETUP_OPENROUTER_CURATED[];
extern const char *SETUP_PROVIDERS[];
extern const char **SETUP_FALLBACKS[10];

#endif /* CONFIG_SETUP_WIZARD_H */
