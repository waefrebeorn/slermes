#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/web_routers/git.py: 2 missing functions ── */

/* PoP: gh_auth_status_route @ hermes_cli/web_routers/git.py:gh_auth_status_route */
char *gh_auth_status_route(void) { return strdup(""); }

/* PoP: git_pr_list_route @ hermes_cli/web_routers/git.py:git_pr_list_route */
char *git_pr_list_route(void) { return strdup(""); }
