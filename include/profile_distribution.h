/*
 * profile_distribution.h — install/update/describe surface of
 * hermes_cli/profile_distribution.py (plan_install / install_distribution /
 * update_distribution / describe_distribution).
 */
#ifndef SLERMES_PROFILE_DISTRIBUTION_H
#define SLERMES_PROFILE_DISTRIBUTION_H

#include "hermes_json.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PD_MANIFEST_FILENAME       "distribution.yaml"
#define PD_ENV_TEMPLATE_FILENAME   ".env.template"
#define PD_ENV_EXAMPLE_FILENAME    ".env.EXAMPLE"

/* Parsed distribution.yaml. `raw` keeps the full document so downstream
 * consumers (distribution_owned allowlist, env_requires template) see every
 * declared field, mirroring Python's DistributionManifest dataclass. */
typedef struct {
    char *name;
    char *version;
    char *source;
    char *hermes_requires;
    char *description;
    json_t *raw;
} pd_manifest_t;

/* InstallPlan @ hermes_cli/profile_distribution.py:InstallPlan */
typedef struct {
    pd_manifest_t manifest;
    char *staged_dir;
    char *provenance;
    char *target_dir;
    int existing;          /* target profile already exists (update path) */
    int preserves_config;  /* leave user's config.yaml alone */
    int has_cron;
    int has_skills;
} pd_install_plan_t;

void pd_manifest_free(pd_manifest_t *m);
void pd_install_plan_free(pd_install_plan_t *plan);

/* plan_install @ hermes_cli/profile_distribution.py:plan_install — stage
 * *source* under *workdir* and produce the plan describing what install would
 * do. Returns 0 on success; on failure returns nonzero with *err set to a
 * malloc'd DistributionError message. */
int pd_plan_install(const char *source, const char *workdir,
                    const char *override_name, pd_install_plan_t *out,
                    char **err);

/* install_distribution @ ...:install_distribution — stage into a private temp
 * dir and install into a new profile. Fails when the profile exists unless
 * *force*. create_alias registers a wrapper alias when collision-free. */
int pd_install_distribution(const char *source, const char *name, int force,
                            int create_alias, pd_install_plan_t *out,
                            char **err);

/* update_distribution @ ...:update_distribution — re-pull the distribution
 * recorded in an existing profile's distribution.yaml `source:` field.
 * Distribution-owned files overwritten; user-owned data untouched;
 * config.yaml preserved unless *force_config*. */
int pd_update_distribution(const char *profile_name, int force_config,
                           pd_install_plan_t *out, char **err);

/* describe_distribution @ ...:describe_distribution — structured view of a
 * profile's distribution metadata. Empty JSON object if the profile exists
 * but has no manifest; nonzero return with *err when it doesn't exist. */
json_t *pd_describe_distribution(const char *profile_name, char **err);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_PROFILE_DISTRIBUTION_H */
