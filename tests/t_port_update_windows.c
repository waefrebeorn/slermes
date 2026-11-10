/*
 * t_port_update_windows.c -- oracle harness for port_update_windows.c
 */
#include "port_update_windows.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else putchar(*p);
    }
    putchar('"');
}

static void emit_str(const char *fn, const char *label, const char *v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(v);
    printf("}\n");
}

static void emit_bool(const char *fn, const char *label, int v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char *s;

    /* ---- _lower_dir_prefix ---- */
    s = uw_lower_dir_prefix("C:\\Hermes\\Install", NULL);
    emit_str("_lower_dir_prefix", "plain", s); free(s);
    s = uw_lower_dir_prefix("C:\\Hermes\\Install\\", NULL);
    emit_str("_lower_dir_prefix", "trailing", s); free(s);
    s = uw_lower_dir_prefix("C:\\Hermes", "C:\\Hermes\\Install");
    emit_str("_lower_dir_prefix", "resolved", s); free(s);
    s = uw_lower_dir_prefix(NULL, NULL);
    emit_str("_lower_dir_prefix", "null", s); free(s);

    /* ---- _cwd_prefix ---- */
    s = uw_cwd_prefix("C:\\Hermes\\Install");
    emit_str("_cwd_prefix", "plain", s); free(s);
    s = uw_cwd_prefix("C:\\Hermes\\Install\\");
    emit_str("_cwd_prefix", "trailing", s); free(s);
    s = uw_cwd_prefix("");
    emit_str("_cwd_prefix", "empty", s); free(s);
    s = uw_cwd_prefix(NULL);
    emit_str("_cwd_prefix", "null", s); free(s);

    /* ---- _gateway_command_subcommand ---- */
    s = uw_gateway_subcommand("python -m hermes_cli.main gateway run");
    emit_str("_gateway_command_subcommand", "run", s); free(s);
    s = uw_gateway_subcommand("python -m hermes_cli.main gateway status");
    emit_str("_gateway_command_subcommand", "status", s); free(s);
    s = uw_gateway_subcommand("hermes gateway");
    emit_str("_gateway_command_subcommand", "bare", s); free(s);
    s = uw_gateway_subcommand("python -m tui_gateway server");
    emit_str("_gateway_command_subcommand", "tui", s); free(s);
    s = uw_gateway_subcommand("hermes-gateway.exe --foo");
    emit_str("_gateway_command_subcommand", "dedicated", s); free(s);
    s = uw_gateway_subcommand("python -m hermes_cli.main --profile work gateway run");
    emit_str("_gateway_command_subcommand", "profile", s); free(s);
    s = uw_gateway_subcommand("python -m hermes_cli.main -p gateway run");
    emit_str("_gateway_command_subcommand", "p-consumes", s); free(s);
    s = uw_gateway_subcommand("");
    emit_str("_gateway_command_subcommand", "empty", s); free(s);

    /* ---- _is_pausable_gateway ---- */
    emit_bool("_is_pausable_gateway", "run",   uw_is_pausable_gateway("python -m hermes_cli.main gateway run"));
    emit_bool("_is_pausable_gateway", "status",uw_is_pausable_gateway("python -m hermes_cli.main gateway status"));
    emit_bool("_is_pausable_gateway", "none",  uw_is_pausable_gateway("python -m tui_gateway server"));
    emit_bool("_is_pausable_gateway", "empty", uw_is_pausable_gateway(""));

    /* ---- _hermes_holder_subcommand ---- */
    s = uw_holder_subcommand("python -m hermes_cli.main --profile work gateway run");
    emit_str("_hermes_holder_subcommand", "gateway", s); free(s);
    s = uw_holder_subcommand("C:\\venv\\Scripts\\hermes.exe serve");
    emit_str("_hermes_holder_subcommand", "serve", s); free(s);
    s = uw_holder_subcommand("C:\\venv\\Scripts\\hermes.exe kanban --preserve-cache");
    emit_str("_hermes_holder_subcommand", "kanban", s); free(s);
    s = uw_holder_subcommand("python -m hermes_cli.main --model opus chat");
    emit_str("_hermes_holder_subcommand", "value-flag", s); free(s);
    s = uw_holder_subcommand("python some-other-script.py");
    emit_str("_hermes_holder_subcommand", "none", s); free(s);

    /* ---- _venv_holder_kind ---- */
    s = uw_holder_kind("python -m hermes_cli.main gateway run");
    emit_str("_venv_holder_kind", "gateway", s); free(s);
    s = uw_holder_kind("C:\\venv\\Scripts\\hermes.exe serve");
    emit_str("_venv_holder_kind", "backend-serve", s); free(s);
    s = uw_holder_kind("C:\\venv\\Scripts\\hermes.exe dashboard");
    emit_str("_venv_holder_kind", "backend-dash", s); free(s);
    s = uw_holder_kind("C:\\venv\\Scripts\\hermes.exe kanban --preserve-cache");
    emit_str("_venv_holder_kind", "hermes-kanban", s); free(s);
    s = uw_holder_kind("python some-other-script.py");
    emit_str("_venv_holder_kind", "python", s); free(s);

    return 0;
}
