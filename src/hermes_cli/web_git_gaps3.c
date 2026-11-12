#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/web_git.py: 2 missing functions ── */

/* PoP: review_pr_list @ hermes_cli/web_git.py:review_pr_list */
char *review_pr_list(void) { return strdup(""); }

