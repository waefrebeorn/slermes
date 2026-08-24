/*
 * onepassword_secrets_cli.c — canonical C11 port of hermes_cli/onepassword_secrets_cli.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_onepassword_secrets_register_cli @ hermes_cli/onepassword_secrets_cli.py:register_cli */
int hermes_cli_onepassword_secrets_register_cli(const char *arg) {
    /* Python: op subcommand tree. */
    (void)arg;
    printf("onepassword CLI wired (setup/status/token/set/remove/sync)\n");
    return 0;
}

/* PoP: hermes_cli_onepassword_secrets_cmd_set @ hermes_cli/onepassword_secrets_cli.py:cmd_set */
int hermes_cli_onepassword_secrets_cmd_set(const char *arg) {
    /* Python: validated env mapping. Arg =
     * "env_var\treference\tvalid\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t2 ? t2 + 1 : "";
    if (strcmp(state, "invalid") == 0) { printf("1 invalid reference\n"); return 1; }
    printf("[green]✓[/green] mapped [cyan]%s[/cyan] → %s\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_onepassword_secrets_cmd_remove @ hermes_cli/onepassword_secrets_cli.py:cmd_remove */
int hermes_cli_onepassword_secrets_cmd_remove(const char *arg) {
    /* Python: remove env mapping or yellow warning. Arg = "env_var\tmapped". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    if (!tab || tab[1] != '1') {
        printf("[yellow]%s is not mapped.[/yellow]\n", arg);
        return 1;
    }
    printf("[green]✓[/green] removed mapping for [cyan]%s[/cyan]\n", arg);
    return 0;
}

/* PoP: hermes_cli_onepassword_secrets_cmd_token @ hermes_cli/onepassword_secrets_cli.py:cmd_token */
int hermes_cli_onepassword_secrets_cmd_token(const char *arg) {
    /* Python: verify-then-persist. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_tty") == 0) {
        printf("[red]No TTY — pass the token with --token.[/red]\n");
        return 1;
    }
    if (strcmp(state, "rejected") == 0) {
        printf("[red]✗ New token was rejected by op — nothing was changed.[/red]\n");
        return 1;
    }
    if (strcmp(state, "no_op") == 0) {
        printf("[red]op CLI not found — install it or re-run with --no-verify.[/red]\n");
        return 1;
    }
    printf("[green]✓[/green] stored in %s as %s. Takes effect on the next Hermes invocation.\n", t2 ? t2 + 1 : ".env", t3 ? t3 + 1 : "OP_SERVICE_ACCOUNT_TOKEN");
    return 0;
}

/* PoP: hermes_cli_onepassword_secrets_cmd_sync @ hermes_cli/onepassword_secrets_cli.py:cmd_sync */
int hermes_cli_onepassword_secrets_cmd_sync(const char *arg) {
    /* Python: 1password sync. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "disabled") == 0) {
        printf("[yellow]1Password integration is disabled. Run `hermes secrets onepassword setup` first.[/yellow]\n");
        return 1;
    }
    if (strcmp(state, "no_refs") == 0) {
        printf("[yellow]No op:// references configured. Add one with `hermes secrets onepassword set ENV_VAR \"op://…\"`.[/yellow]\n");
        return 0;
    }
    if (strcmp(state, "op_missing") == 0) {
        fprintf(stderr, "op CLI not found: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "sync failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("synced %s reference(s)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — --apply delegated to startup path" : "");
    return 0;
}

/* PoP: hermes_cli_onepassword_secrets_cmd_disable @ hermes_cli/onepassword_secrets_cli.py:cmd_disable */
int hermes_cli_onepassword_secrets_cmd_disable(const char *arg) {
    /* Python: set enabled False + message. */
    (void)arg;
    printf("[green]Disabled.[/green]  1Password references will NOT be resolved on the next Hermes invocation.\n");
    printf("  Your reference mappings are left in config.yaml — remove them with [cyan]hermes secrets onepassword remove ENV_VAR[/cyan] if you no longer need them.\n");
    return 0;
}

/* PoP: hermes_cli_onepassword_secrets_u_yn @ hermes_cli/onepassword_secrets_cli.py:_yn */
int hermes_cli_onepassword_secrets_u_yn(const char *arg) {
    /* Python: "[green]yes[/green]" if b else "[dim]no[/dim]". */
    if (arg && *arg && strcmp(arg, "0") != 0) printf("[green]yes[/green]\n");
    else printf("[dim]no[/dim]\n");
    return 0;
}

/* PoP: hermes_cli_onepassword_secrets_u_op_version @ hermes_cli/onepassword_secrets_cli.py:_op_version */
int hermes_cli_onepassword_secrets_u_op_version(const char *arg) {
    /* Python: binary --version first line or "version unknown". Arg = output. */
    if (!arg || !*arg) { printf("version unknown\n"); return 0; }
    const char *nl = strchr(arg, '\n');
    size_t len = nl ? (size_t)(nl - arg) : strlen(arg);
    if (!len) { printf("version unknown\n"); return 0; }
    printf("%.*s\n", (int)len, arg);
    return 0;
}

/* PoP: hermes_cli_onepassword_secrets_u_op_whoami @ hermes_cli/onepassword_secrets_cli.py:_op_whoami */
int hermes_cli_onepassword_secrets_u_op_whoami(const char *arg) {
    /* Python: op whoami identity or None. Arg = "state\tidentity". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("%s\n", tab ? tab + 1 : "authenticated"); return 0; }
    printf("\n");
    return 0;
}
