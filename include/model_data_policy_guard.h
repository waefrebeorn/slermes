/*
 * model_data_policy_guard.h — C11 port of
 * hermes_cli/model_data_policy_guard.py.
 *
 * Data-policy confirmation helpers for model selection surfaces: some tiers
 * are cheap because the vendor trains on your prompts. Returns a warning
 * payload the CLI/web flows surface as an explicit confirm step.
 */
#ifndef HERMES_CLI_MODEL_DATA_POLICY_GUARD_H
#define HERMES_CLI_MODEL_DATA_POLICY_GUARD_H

#ifdef __cplusplus
extern "C" {
#endif

/* DataTrainingWarning dataclass (frozen). All strings owned; free with
 * data_training_warning_free. */
typedef struct {
    char *model;
    char *provider;
    char *message;
} DataTrainingWarning;

/* PoP: data_training_warning @ hermes_cli/model_data_policy_guard.py:data_training_warning */
/* Return a malloc'd warning when model_name selects a data-training tier,
 * NULL when no rule matches. provider/base_url may be NULL (base_url is
 * reserved for host-scoped rules, exactly like Python's noqa'd arg). */
DataTrainingWarning *data_training_warning(const char *model_name,
                                           const char *provider,
                                           const char *base_url);

/* Free a warning returned by data_training_warning (NULL-safe). */
void data_training_warning_free(DataTrainingWarning *w);

#endif /* HERMES_CLI_MODEL_DATA_POLICY_GUARD_H */
