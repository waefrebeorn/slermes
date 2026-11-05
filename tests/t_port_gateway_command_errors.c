/*
 * t_port_gateway_command_errors.c — Oracle harness for
 * hermes_cli/gateway_command_errors.py (ported to
 * src/cli/port_gateway_command_errors.c).
 *
 * Emits one JSON line per case with the extracted verb or the guidance
 * lines (plus the argv/returncode that produced them), so the Python
 * oracle can replay the LIVE module and diff.
 */
#include "port_gateway_command_errors.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_str(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

static void emit_verb(const char *name, const char *cmd)
{
    printf("{\"case\":\"%s\",\"cmd\":", name);
    emit_json_str(cmd);
    printf(",\"verb\":");
    emit_json_str(gce_verb_for(cmd));
    printf("}\n");
}

static void emit_argv(const char *const *argv, int argc)
{
    fputc('[', stdout);
    for (int i = 0; i < argc; i++) {
        if (i) fputs(", ", stdout);
        emit_json_str(argv[i]);
    }
    fputc(']', stdout);
}

static void emit_explain(const char *name, int kind, const char *verb,
                         const char *const *argv, int argc, int rc)
{
    char *details = (argv && argc) ? gce_called_process_error_text(argv, argc, rc) : NULL;
    json_t *arr = gce_explain_service_failure(kind, verb, details);

    printf("{\"case\":\"%s\",\"kind\":%d,\"verb\":", name, kind);
    emit_json_str(verb);
    printf(",\"argv\":");
    if (argv) emit_argv(argv, argc); else fputs("null", stdout);
    printf(",\"rc\":%d,\"details\":", rc);
    emit_json_str(details);
    printf(",\"lines\":");
    if (!arr) {
        fputs("null", stdout);
    } else {
        char *ser = json_serialize(arr);
        fputs(ser ? ser : "null", stdout);
        free(ser);
        json_free(arr);
    }
    printf("}\n");
    free(details);
}

int main(void)
{
    static const char *ARGV_START[]   = { "systemctl", "--user", "start", "hermes-gateway" };
    static const char *ARGV_STOP[]    = { "systemctl", "--user", "stop", "hermes-gateway" };
    static const char *ARGV_RESTART[] = { "systemctl", "--user", "restart", "hermes-gateway" };
    static const char *ARGV_NOVERB[]  = { "systemctl", "--user", "status", "hermes-gateway" };

    setvbuf(stdout, NULL, _IONBF, 0);

    emit_verb("verb_start", "systemctl --user start hermes-gateway");
    emit_verb("verb_stop", "systemctl --user stop hermes-gateway");
    emit_verb("verb_restart", "systemctl --user restart hermes-gateway");
    emit_verb("verb_none", "systemctl --user status hermes-gateway");
    emit_verb("verb_substring", "restarting-gateway");
    emit_verb("verb_null", NULL);

    emit_explain("no_systemctl", GCE_FAULT_NO_SYSTEMCTL, "start", NULL, 0, 0);
    emit_explain("unknown_fault", GCE_FAULT_NONE, "start", NULL, 0, 0);

    emit_explain("failed_start", GCE_FAULT_SYSTEMCTL_FAILED, "start", ARGV_START, 4, 1);
    emit_explain("failed_stop", GCE_FAULT_SYSTEMCTL_FAILED, "stop", ARGV_STOP, 4, 3);
    emit_explain("failed_restart", GCE_FAULT_SYSTEMCTL_FAILED, "restart", ARGV_RESTART, 4, 1);
    emit_explain("failed_noverb", GCE_FAULT_SYSTEMCTL_FAILED, "start", ARGV_NOVERB, 4, 5);
    return 0;
}
