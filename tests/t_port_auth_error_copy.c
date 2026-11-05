/*
 * t_port_auth_error_copy.c — Oracle harness for hermes_cli/auth_error_copy.py
 * (ported to src/cli/port_auth_error_copy.c).
 */
#include "port_auth_error_copy.h"
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

static void emit_lines(const char *name, const char *family, json_t *arr)
{
    printf("{\"case\":\"%s\",\"family\":\"%s\",\"lines\":", name, family);
    if (!arr) { fputs("null", stdout); }
    else {
        char *s = json_serialize(arr);
        fputs(s ? s : "null", stdout);
        free(s);
        json_free(arr);
    }
    printf("}\n");
}

static void emit_text(const char *name, const char *family, char *text)
{
    printf("{\"case\":\"%s\",\"family\":\"%s\",\"text\":", name, family);
    emit_json_str(text);
    printf("}\n");
    free(text);
}

static void emit_hint(const char *name, const char *provider)
{
    printf("{\"case\":\"%s\",\"family\":\"hint\",\"provider\":", name);
    emit_json_str(provider);
    printf(",\"text\":");
    emit_json_str(afc_unknown_provider_hint(provider));
    printf("}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    afc_error_t cancelled = { AFC_CANCELLED, "", "KeyboardInterrupt", NULL, NULL, 0 };
    afc_error_t eof       = { AFC_CANCELLED, "", "EOFError", NULL, NULL, 0 };
    afc_error_t sysexit130 = { AFC_OTHER, "", "SystemExit", NULL, NULL, 130 };
    afc_error_t sysexit_none = { AFC_OTHER, "", "SystemExit", NULL, NULL, -1 };
    afc_error_t sysexit3  = { AFC_OTHER, "", "SystemExit", NULL, NULL, 3 };
    afc_error_t net       = { AFC_OTHER, "connection refused", "ConnectError", NULL, NULL, 0 };
    afc_error_t net_stdlib = { AFC_OTHER, "reset by peer", "ConnectionResetError", NULL, NULL, 0 };
    afc_error_t generic   = { AFC_OTHER, "boom", "RuntimeError", NULL, NULL, 0 };
    afc_error_t generic_empty = { AFC_OTHER, "", "RuntimeError", NULL, NULL, 0 };
    afc_error_t copy      = { AFC_SIGNIN_COPY, "The sign-in code expired.\n  Details: expired_token: gone",
                              "SignInCopyError", "expired_token", NULL, 0 };
    afc_error_t copy_single = { AFC_SIGNIN_COPY, "Sign-in did not complete.",
                                "SignInCopyError", "", NULL, 0 };
    afc_error_t dev_exp   = { AFC_DEVICE_EXPIRED, "device code timed out",
                              "DeviceCodeExpired", NULL, "device_code_timeout", 0 };
    afc_error_t dev_exp2  = { AFC_OTHER, "timed out", "AuthError", NULL, "timeout", 0 };

    struct { const char *name; const afc_error_t *e; } signin[] = {
        { "cancelled", &cancelled },
        { "eof", &eof },
        { "sysexit130", &sysexit130 },
        { "sysexit_none", &sysexit_none },
        { "sysexit3", &sysexit3 },
        { "network", &net },
        { "network_stdlib", &net_stdlib },
        { "generic", &generic },
        { "generic_empty", &generic_empty },
        { "signin_copy", &copy },
        { "signin_copy_single", &copy_single },
        { "device_expired", &dev_exp },
        { "device_expired_code2", &dev_exp2 },
    };
    for (size_t i = 0; i < sizeof(signin) / sizeof(signin[0]); i++) {
        emit_lines(signin[i].name, "signin",
                   afc_sign_in_failure_lines(signin[i].e,
                                             "portal.nousresearch.com",
                                             "hermes portal"));
        emit_lines(signin[i].name, "provider",
                   afc_provider_setup_failure_lines(signin[i].e, "hermes model"));
    }

    /* custom host/retry */
    emit_lines("custom_host", "signin",
               afc_sign_in_failure_lines(&net, "example.test", "hermes login"));

    emit_text("device_expired_token", "device",
              afc_device_flow_error("expired_token", "code expired", "hermes portal"));
    emit_text("device_access_denied", "device",
              afc_device_flow_error("access_denied", "user said no", "hermes portal"));
    emit_text("device_invalid_grant", "device",
              afc_device_flow_error("invalid_grant", "bad grant", "hermes portal"));
    emit_text("device_invalid_client", "device",
              afc_device_flow_error("invalid_client", "unknown app", "hermes portal"));
    emit_text("device_unknown_code", "device",
              afc_device_flow_error("weird_code", "server exploded", "hermes portal"));
    emit_text("device_empty_code", "device",
              afc_device_flow_error("", "no code given", "hermes portal"));

    emit_hint("hint_opencode_free", "opencode-free");
    emit_hint("hint_free_upper", "  FREE ");
    emit_hint("hint_opencode_underscore", "opencode_free");
    emit_hint("hint_other", "openai");
    emit_hint("hint_null", NULL);
    return 0;
}
