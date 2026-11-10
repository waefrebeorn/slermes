/*
 * config_env_file.h — home-dir + .env file helpers for the config subsystem.
 *
 * Module-internal header (config.c <-> config_env_file.c).
 */
#ifndef CONFIG_ENV_FILE_H
#define CONFIG_ENV_FILE_H

#include <stddef.h>
#include "hermes_core_types.h"   /* hermes_config_t */

void get_slermes_home(char *buf, size_t sz);
void parse_env_file(const char *path, hermes_config_t *cfg);

#endif /* CONFIG_ENV_FILE_H */
