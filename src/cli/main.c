/*
 * main.c — CLI-specific entry module for Hermes C.
 * CLI configuration and setup before delegating to cli_main().
 * Separate from src/main.c (the global entry point).
 */


/* PoP: CLI entry point (port of cli.py) */

#include "hermes_core_types.h"
#include "hermes_display.h"
#include "hermes_logger.h"
#include <stdio.h>

#include "cli.h"

/* CLI-specific initialization */
void cli_init(void) {
    display_init();
    setup_logging();
    hermes_log(LOG_INFO, "cli", "CLI initialized");
}

/* Port of Python gateway/platforms/qqbot/adapter.py:_cleanup(). */
/* Port of Python tools/environments/base.py:cleanup, tools/environments/daytona.py:cleanup, tools/environments/docker.py:cleanup, tools/environments/local.py:cleanup, tools/environments/managed_modal.py:cleanup, tools/environments/modal.py:cleanup, tools/environments/singularity.py:cleanup, tools/environments/ssh.py:cleanup(). */
/* CLI-specific cleanup */
void cli_cleanup(void) {
/* PoP: reset @ cli.py:reset */
    display_reset();
}

/* Additional CLI entry point called from main.c */
int hermes_cli_entry(int argc, char **argv) {
    cli_init();
    int rc = cli_main(argc, argv);
    cli_cleanup();
    return rc;
}
