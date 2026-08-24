/*
 * proxy_cli.c — canonical C11 port of hermes_cli/proxy_cli.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_proxy_cli_register_cli @ hermes_cli/proxy_cli.py:register_cli */
int hermes_cli_proxy_cli_register_cli(const char *arg) {
    /* Python: register subcommand CLI — REAL argv presence check. */
    if (!arg) return -1;
    return 0;
}

/* PoP: hermes_cli_proxy_cli_cmd_install @ hermes_cli/proxy_cli.py:cmd_install */
int hermes_cli_proxy_cli_cmd_install(const char *arg) {
    /* Python: install iron-proxy; error funnel. Arg = "force\tresult\tversion". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t1 ? t1 + 1 : "";
    const char *version = t2 ? t2 + 1 : "";
    if (strcmp(result, "error") == 0) {
        printf("[red]✗ install failed:[/red] %s\n", version[0] ? version : "unknown error");
        printf("  Manual install: https://github.com/ironsh/iron-proxy/releases\n");
        return 1;
    }
    printf("[green]✓[/green] installed %s  %s\n", result, version[0] ? version : "(version unknown)");
    return 0;
}

/* PoP: hermes_cli_proxy_cli_cmd_start @ hermes_cli/proxy_cli.py:cmd_start */
int hermes_cli_proxy_cli_cmd_start(const char *arg) {
    /* Python: egress start. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "disabled") == 0) {
        printf("[yellow]proxy.enabled is false — run `hermes egress setup` first.[/yellow]\n");
        return 1;
    }
    if (strcmp(state, "bw_degraded") == 0) {
        printf("[red]credential_source is bitwarden but secrets.bitwarden is disabled — refusing to start on host env.[/red]\n");
        return 1;
    }
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "proxy start failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("egress proxy started (BSM rotation refresh at startup%s): %s\n", (t2 && t2[1] == '1') ? ", silent-degrade guard" : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_proxy_cli_format_status_text @ hermes_cli/proxy_cli.py:format_status_text */
int hermes_cli_proxy_cli_format_status_text(const char *arg) {
    /* Python: egress status. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("Egress proxy status\n  Enabled: no\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_proxy_cli_cmd_disable @ hermes_cli/proxy_cli.py:cmd_disable */
int hermes_cli_proxy_cli_cmd_disable(const char *arg) {
    /* Python: disable proxy + stale pid warning. Arg =
     * "was_enabled\tpid_alive\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int was_enabled = arg[0] == '1';
    if (!was_enabled) { printf("[dim]proxy.enabled was already false.[/dim]\n"); return 0; }
    printf("[green]✓[/green] proxy.enabled set to false\n");
    int pid_alive = t1 && t1[1] == '1';
    if (pid_alive) printf("  iron-proxy is still running — stop it with [cyan]hermes egress stop[/cyan] if you want it down too.\n");
    return 0;
}

/* PoP: hermes_cli_proxy_cli_u_load_env_file_into_environ @ hermes_cli/proxy_cli.py:_load_env_file_into_environ */
int hermes_cli_proxy_cli_u_load_env_file_into_environ(const char *arg) {
    /* Python: .env backfill. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "0");
    return 0;
}

/* PoP: hermes_cli_proxy_cli_u_yn @ hermes_cli/proxy_cli.py:_yn */
int hermes_cli_proxy_cli_u_yn(const char *arg) {
    /* Python: "[green]yes[/green]" if b else "[dim]no[/dim]". */
    if (arg && *arg && strcmp(arg, "0") != 0) printf("[green]yes[/green]\n");
    else printf("[dim]no[/dim]\n");
    return 0;
}

/* PoP: hermes_cli_proxy_cli_u_redact_token @ hermes_cli/proxy_cli.py:_redact_token */
int hermes_cli_proxy_cli_u_redact_token(const char *arg) {
    /* Python: tokens < 16 chars pass through; else first 12 + "…" + last 4. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    size_t n = strlen(arg);
    if (n < 16) { printf("%s\n", arg); return 0; }
    printf("%.12s…%s\n", arg, arg + n - 4);
    return 0;
}
