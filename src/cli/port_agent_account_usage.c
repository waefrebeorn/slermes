/*
 * port_agent_account_usage.c — C port of agent/account_usage.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* build_credits_view — NA_CLI: uses concurrent.futures, hermes_cli.auth, nous_account */
/* Port of Python agent/account_usage.py:build_credits_view — N/A (CLI-specific) */
