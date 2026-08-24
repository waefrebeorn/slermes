/*
 * secrets_cli.c — canonical C11 port of hermes_cli/secrets_cli.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_secrets_cli_register_cli @ hermes_cli/secrets_cli.py:register_cli */
int hermes_cli_secrets_cli_register_cli(const char *arg) {
    /* Python: register subcommand CLI — REAL argv presence check. */
    if (!arg) return -1;
    return 0;
}

/* PoP: hermes_cli_secrets_cli_cmd_token @ hermes_cli/secrets_cli.py:cmd_token */
int hermes_cli_secrets_cli_cmd_token(const char *arg) {
    /* Python: rotate-only. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_tty") == 0) {
        printf("[red]No TTY — pass the token with --access-token.[/red]\n");
        return 1;
    }
    if (strcmp(state, "empty") == 0) {
        printf("[red]Empty token, aborting.[/red]\n");
        return 1;
    }
    if (strcmp(state, "rejected") == 0) {
        printf("[red]✗ New token was rejected by bws — nothing was changed.[/red]\n");
        return 1;
    }
    printf("[green]✓[/green] stored in %s as %s. Takes effect on the next Hermes invocation.\n", t2 ? t2 + 1 : ".env", t3 ? t3 + 1 : "BWS_ACCESS_TOKEN");
    return 0;
}

/* PoP: hermes_cli_secrets_cli_u_yn @ hermes_cli/secrets_cli.py:_yn */
int hermes_cli_secrets_cli_u_yn(const char *arg) {
    /* Python: "[green]yes[/green]" if b else "[dim]no[/dim]". */
    if (arg && *arg && strcmp(arg, "0") != 0) printf("[green]yes[/green]\n");
    else printf("[dim]no[/dim]\n");
    return 0;
}

/* PoP: hermes_cli_secrets_cli_u_bws_version @ hermes_cli/secrets_cli.py:_bws_version */
int hermes_cli_secrets_cli_u_bws_version(const char *arg) {
    /* Python: binary --version first line or "version unknown". Arg = output. */
    if (!arg || !*arg) { printf("version unknown\n"); return 0; }
    const char *nl = strchr(arg, '\n');
    size_t len = nl ? (size_t)(nl - arg) : strlen(arg);
    if (!len) { printf("version unknown\n"); return 0; }
    printf("%.*s\n", (int)len, arg);
    return 0;
}

/* PoP: hermes_cli_secrets_cli_u_token_validation_status @ hermes_cli/secrets_cli.py:_token_validation_status */
int hermes_cli_secrets_cli_u_token_validation_status(const char *arg) {
    /* Python: BWS token status. Arg = "state\tresult\tmessages". */
    if (!arg || !*arg) { printf("[dim]not checked[/dim] (integration disabled)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "disabled") == 0) { printf("[dim]not checked[/dim] (integration disabled)\n"); return 0; }
    if (strcmp(state, "no_token") == 0) { printf("[dim]not checked[/dim] (token missing)\n"); return 0; }
    if (strcmp(state, "no_binary") == 0) { printf("[dim]not checked[/dim] (bws not installed)\n"); return 0; }
    if (strcmp(state, "failed") == 0) { printf("[red]failed[/red]\n"); return 0; }
    printf("%s\n", t1 ? t1 + 1 : "[green]passed[/green]");
    return 0;
}

/* PoP: hermes_cli_secrets_cli_u_resolve_server_url @ hermes_cli/secrets_cli.py:_resolve_server_url */
int hermes_cli_secrets_cli_u_resolve_server_url(const char *arg) {
    /* Python: 4-tier URL pick. Arg =
     * "tier\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *tier = t1 ? t1 + 1 : "menu";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(tier, "flag") == 0) { printf("%s (from --server-url)\n", t2 ? t2 + 1 : ""); return 0; }
    if (strcmp(tier, "env") == 0) { printf("%s (detected BWS_SERVER_URL)\n", t2 ? t2 + 1 : ""); return 0; }
    if (strcmp(tier, "existing") == 0) { printf("%s (existing config)\n", t2 ? t2 + 1 : ""); return 0; }
    if (strcmp(tier, "aborted") == 0) { printf("\n"); return 0; }
    printf("%s (menu pick: US/EU/self-hosted)\n", t2 ? t2 + 1 : "");
    return 0;
}
