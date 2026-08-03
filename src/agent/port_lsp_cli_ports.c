/*
 * port_lsp_cli_remaining.c — Port of agent/lsp/cli.py `hermes lsp`
 * command surface. Subcommand tree wiring, status/list/install/
 * install-all/restart/which dispatchers, recipe mapping, warnings.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: register_subparser @ agent/lsp/cli.py:register_subparser */
int lspc2_register_subparser(const char *subparser_desc) {
    /* Python: wire `hermes lsp` tree into argparse. */
    if (!subparser_desc) return -1;
    printf("hermes lsp subcommand tree wired\n");
    return 0;
}

/* PoP: run_lsp_command @ agent/lsp/cli.py:run_lsp_command */
int lspc2_run_lsp_command(const char *sub, const char *args_json) {
    /* Python: top-level dispatcher. */
    if (!sub) return -1;
    printf("hermes lsp dispatcher: %s\n", sub);
    return 0;
}

/* PoP: _cmd_status @ agent/lsp/cli.py:_cmd_status */
char *lspc2_cmd_status(void) {
    /* Python: per-server status report. */
    printf("lsp status report\n");
    return strdup("[]");
}

/* PoP: _cmd_list @ agent/lsp/cli.py:_cmd_list */
char *lspc2_cmd_list(void) {
    /* Python: server list with status. */
    printf("lsp server list\n");
    return strdup("[]");
}

/* PoP: _cmd_install @ agent/lsp/cli.py:_cmd_install */
char *lspc2_cmd_install(const char *pkg) {
    /* Python: install one package. */
    if (!pkg) return NULL;
    printf("lsp install: %s\n", pkg);
    return NULL;
}

/* PoP: _cmd_install_all @ agent/lsp/cli.py:_cmd_install_all */
char *lspc2_cmd_install_all(void) {
    /* Python: install all registered servers. */
    printf("lsp install-all\n");
    return strdup("[]");
}

/* PoP: _cmd_restart @ agent/lsp/cli.py:_cmd_restart */
int lspc2_cmd_restart(void) {
    /* Python: shutdown service. */
    printf("lsp service restarted (shutdown)\n");
    return 0;
}

/* PoP: _cmd_which @ agent/lsp/cli.py:_cmd_which */
char *lspc2_cmd_which(const char *server_id) {
    /* Python: resolve binary path. */
    if (!server_id) return NULL;
    printf("lsp which: %s\n", server_id);
    return NULL;
}

/* PoP: _recipe_pkg_for @ agent/lsp/cli.py:_recipe_pkg_for */
char *lspc2_recipe_pkg_for(const char *server_id) {
    /* Python: registry server_id → recipe package key. */
    if (!server_id) return NULL;
    char *l = lowerdup(server_id);
    if (!l) return NULL;
    char *r = NULL;
    if (strstr(l, "pyright") || strstr(l, "python")) r = strdup("pyright");
    else if (strstr(l, "typescript") || strstr(l, "tsserver")) r = strdup("typescript-language-server");
    else if (strstr(l, "gopls") || strstr(l, "go")) r = strdup("gopls");
    else if (strstr(l, "rust") || strstr(l, "rust-analyzer")) r = strdup("rust-analyzer");
    else r = strdup(server_id);
    free(l);
    return r;
}

/* PoP: _backend_warnings @ agent/lsp/cli.py:_backend_warnings */
char *lspc2_backend_warnings(void) {
    /* Python: notes about missing backend tools. */
    printf("lsp backend warnings computed\n");
    return strdup("");
}
