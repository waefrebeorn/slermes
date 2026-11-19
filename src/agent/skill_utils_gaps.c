#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/skill_utils.py: 4 missing functions ── */

char *get_skill_create_dir(void) { return strdup(""); }

char *display_skill_create_dir(void) { return strdup(""); }

char *project_trusted_dirs_from_config(void) { return strdup("[]"); }

char *candidate_project_skills_dirs(void) { return strdup("[]"); }
