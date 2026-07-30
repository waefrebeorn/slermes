/*
 * config_profile.c -- extracted from cli/config.c monolith.
 * Real implementation of one config-lifecycle concern; public
 * hermes_config_* protos stay in include/hermes_core_types.h.
 */

#include "hermes_core_types.h"
#include "config_schema.h"
#include "hermes_yaml.h"
#include "hermes_json.h"
#include "hermes_auth.h"
#include "provider_metadata.h"
#include "curses_widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/stat.h>

bool hermes_config_load_profile(hermes_config_t *cfg, const char *profile_name, const char *config_dir) {
    if (!profile_name || profile_name[0] == '\0') return false;

    char profile_path[HERMES_PATH_MAX];
    if (config_dir && config_dir[0])
        snprintf(profile_path, sizeof(profile_path), "%s/profiles/%s.yaml", config_dir, profile_name);
    else {
        char profiles_sub[HERMES_PATH_MAX];
        snprintf(profiles_sub, sizeof(profiles_sub), "profiles/%s.yaml", profile_name);
        hermes_resolve_path(profiles_sub, profile_path, sizeof(profile_path));
    }

    /* Check file exists */
    struct stat st;
    if (stat(profile_path, &st) != 0) return false;

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(profile_path, &err);
    if (!doc) {
        if (err) fprintf(stderr, "Warning: profile '%s' parse error: %s\n", profile_name, err);
        if (err) free(err);
        return false;
    }

    /* Merge profile settings — only override what's set in profile */
    /* Model */
    const char *v;
    v = yaml_get_string(doc, "model.default");
    if (v) snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", v);
    v = yaml_get_string(doc, "model.provider");
    if (v) snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", v);
    v = yaml_get_string(doc, "model.base_url");
    if (v) snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", v);
    v = yaml_get_string(doc, "model.api_key");
    if (v) snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", v);
    v = yaml_get_string(doc, "model.api_mode");
    if (v) snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "%s", v);

    /* Agent */
    int iv = yaml_get_int(doc, "agent.max_turns", 0);
    if (iv > 0) { cfg->agent.max_iterations = iv; cfg->max_turns = iv; }
    iv = yaml_get_int(doc, "agent.verbose", 0);
    if (iv >= 0 && iv <= 2) { cfg->agent.verbose_level = iv; cfg->verbose = iv; }

    /* Display */
    v = yaml_get_string(doc, "display.skin");
    if (v) snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", v);

    yaml_free(doc);
    return true;
}
void hermes_config_defaults(hermes_config_t *cfg) {
    hermes_config_load(cfg, NULL);
    /* Don't touch env_path/config_path — caller sets those */
}
