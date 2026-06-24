#ifndef HERMES_SETUP_WIZARD_H
#define HERMES_SETUP_WIZARD_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run the interactive setup wizard.
 * Returns 0 on success.
 */
int setup_wizard_run(void);

/**
 * Run setup in non-interactive mode.
 * Uses defaults and environment variables.
 */
int setup_wizard_noninteractive(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SETUP_WIZARD_H */
