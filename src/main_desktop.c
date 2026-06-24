/*
 * main_desktop.c — Entry point for the C11 Desktop App
 *
 * Usage: ./hermes-desktop
 *
 * This is the main entry point for the native C11 desktop application.
 * It replaces the Electron/TypeScript desktop shell with a native
 * ncurses-based UI that uses our own rendering backends.
 *
 * PoP: electron/main.cjs @ apps/desktop/electron/main.cjs
 */

#include "app_desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

static void signal_handler(int sig) {
    (void)sig;
    /* Graceful shutdown handled by atexit */
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    fprintf(stderr, "Slermes Desktop — C11 Edition\n");
    fprintf(stderr, "Starting...\n");

    int rc = app_desktop_run(argc, argv);

    fprintf(stderr, "Slermes Desktop exited with code %d\n", rc);
    return rc;
}
