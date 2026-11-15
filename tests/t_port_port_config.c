/* t_port_port_config.c -- harness for the hermes_cli/config.py port.
 *
 * Emits one JSON object per case on stdout; tests/oracle_config.py reads that
 * and diffs it against the real Python. The harness stays dumb: no Python, no
 * expected values, just the C result.
 */
#include "port_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else putchar(*p);
        }
    }
    putchar('"');
}

int main(void)
{
    static const char *const KEYS[] = {
        "", "a", "a.b", "a.b.c", "a\\.b", "a\\.b.c", "a.b\\.c",
        "providers.myprov.models.grok-4\\.6.context_length",
        "models.grok-4\\.6.supports_vision", "qwen3.5-397b-wafer",
        ".leading", "trailing.", "..", "a..b", "\\", "a\\", "\\\\.",
        "deep.a.b.c.d.e.f", "x\\.y\\.z", "X.Y", "1.2", "_._",
    };
    static const char *const POLICY_KEYS[] = {
        "PATH", "path", "Path", "HERMES_HOME", "hermes_home",
        "LD_PRELOAD", "ld_preload", "Mixed_Case_Key", "_LEAD", "9BAD",
    };
    static const char *const ENV_KEYS[] = {
        "OPENAI_API_KEY", "openai_api_key", "OPENROUTER_API_KEY", "GITHUB_TOKEN",
        "SUDO_PASSWORD", "TERMINAL_SSH_HOST", "TERMINAL_SSH_USER",
        "TERMINAL_SSH_KEY", "SOMETHING_TOKEN", "SOMETHING_SECRET", "X_API_KEY",
        "API_KEY", "PLAIN", "A.B", "a.b", "HONCHO_API_KEY", "SLACK_APP_TOKEN",
        "TOOL_GATEWAY_DOMAIN", "FIRECRAWL_GATEWAY_URL", "NOT_A_CRED",
    };
    static const char *const VALIDATE_KEYS[] = {
        "GOOD_NAME", "good_name", "_LEAD", "9BAD", "-BAD", "HAS SPACE", "",
        "PATH", "LD_PRELOAD", "LD_ANYTHING", "DYLD_X", "GIT_CONFIG_X",
        "PYTHONPATH", "HERMES_HOME", "HERMES_YOLO_MODE", "BROWSER", "EDITOR",
        "BASH_ENV", "ENV", "CLASSPATH", "RUSTFLAGS", "SOMETHING_TOKEN",
        "TERMINAL_SSH_HOST", "GITHUB_TOKEN", "A.B",
    };

#define EMIT(FN, ARG, WIN)                                                    \
    do {                                                                      \
        printf("{\"fn\":\"%s\",\"arg\":", FN);                                \
        jstr(ARG);                                                            \
        printf(",\"win\":%d,\"out\":", WIN);                                   \
    } while (0)

    for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        char *segs[64];
        int n = cfg_split_key_path(KEYS[i], segs, 64);
        EMIT("split", KEYS[i], 0);
        if (n < 0) {
            fputs("\"OVERFLOW\"}", stdout);
        } else {
            fputs("[", stdout);
            for (int k = 0; k < n; k++) {
                if (k) putchar(',');
                jstr(segs[k]);
                free(segs[k]);
            }
            fputs("]}", stdout);
        }
        putchar('\n');
    }

    for (size_t i = 0; i < sizeof(POLICY_KEYS) / sizeof(POLICY_KEYS[0]); i++) {
        for (int w = 0; w <= 1; w++) {
            char out[512];
            cfg_env_var_policy_name(POLICY_KEYS[i], w, out, sizeof(out));
            EMIT("policy", POLICY_KEYS[i], w);
            jstr(out);
            fputs("}\n", stdout);
        }
    }

    for (size_t i = 0; i < sizeof(ENV_KEYS) / sizeof(ENV_KEYS[0]); i++) {
        int got = cfg_is_env_config_key(ENV_KEYS[i]);
        EMIT("envkey", ENV_KEYS[i], 0);
        printf("%d}\n", got);
    }

    for (size_t i = 0; i < sizeof(VALIDATE_KEYS) / sizeof(VALIDATE_KEYS[0]); i++) {
        for (int w = 0; w <= 1; w++) {
            int got = cfg_validate_env_var_name(VALIDATE_KEYS[i], w);
            EMIT("validate", VALIDATE_KEYS[i], w);
            printf("%d}\n", got);
        }
    }


    /* ── install-method helpers ── */
    {
        static const char *const METHODS[] = {
            "", "nix", "nixos", "home-manager", "docker", "apt", "git",
            "unknown", "NIX", "NixOS", "snap", "  nix  ", "nix-manager",
        };
        for (size_t i = 0; i < sizeof(METHODS) / sizeof(METHODS[0]); i++) {
            char cmd[1024];
            cfg_recommended_update_command_for_method(METHODS[i], cmd, sizeof(cmd));
            printf("{\"fn\":\"nixmethod\",\"arg\":");
            jstr(METHODS[i]);
            printf(",\"out\":%d}\n", cfg_is_nix_install_method(METHODS[i]));
            printf("{\"fn\":\"updatecmd\",\"arg\":");
            jstr(METHODS[i]);
            printf(",\"out\":");
            jstr(cmd);
            fputs("}\n", stdout);
            char warn[256];
            cfg_format_unsupported_install_warning(METHODS[i], warn, sizeof(warn));
            printf("{\"fn\":\"unsupwarn\",\"arg\":");
            jstr(METHODS[i]);
            printf(",\"out\":");
            jstr(warn);
            fputs("}\n", stdout);
        }

        printf("{\"fn\":\"uvinstall\",\"arg\":\"\",\"out\":%d}\n",
               cfg_is_uv_tool_install());
        printf("{\"fn\":\"unsupmethod\",\"arg\":\"nix\",\"out\":%d}\n",
               cfg_is_unsupported_install_method("nix"));
        printf("{\"fn\":\"unsupmethod\",\"arg\":\"\",\"out\":%d}\n",
               cfg_is_unsupported_install_method(""));
    }


    {
        static const char *const CPS[] = {
            "null", "\"str\"", "42", "true", "{}",
            "{\"base_url\":\"u\"}",
            "{\"base_url\":\"u\",\"api_key\":\"k\",\"api_mode\":\"x\"}",
            "[]", "[{\"name\":\"n\",\"base_url\":\"u\"}]",
            "[{\"name\":\"n\"}]", "[null]", "[\"str\"]", "[42]",
            "[{\"name\":\"n\",\"base_url\":\"u\"},{\"base_url\":\"u\"}]",
        };
        for (size_t i = 0; i < sizeof(CPS) / sizeof(CPS[0]); i++) {
            json_t *cp = json_parse(CPS[i], NULL);
            json_t *issues = json_array();
            if (cp && issues) {
                ac_validate_custom_providers(issues, cp);
                char *out = json_serialize(issues);
                printf("{\"fn\":\"valcp\",\"arg\":");
                jstr(CPS[i]);
                printf(",\"out\":");
                jstr(out ? out : "");
                fputs("}\n", stdout);
                free(out);
            }
            json_free(cp);
            json_free(issues);
        }
        static const char *const FBS[] = {
            "null", "\"str\"", "42", "true", "{}",
            "{\"provider\":\"openrouter\",\"model\":\"m\"}",
            "{\"provider\":\"openrouter\"}", "{\"model\":\"m\"}",
            "[]", "[{\"provider\":\"p\",\"model\":\"m\"}]",
            "[{\"provider\":\"p\"}]", "[null]", "[\"str\"]", "[{}]",
        };
        for (size_t i = 0; i < sizeof(FBS) / sizeof(FBS[0]); i++) {
            json_t *fb = json_parse(FBS[i], NULL);
            json_t *issues = json_array();
            if (fb && issues) {
                ac_validate_fallback_model(issues, fb);
                char *out = json_serialize(issues);
                printf("{\"fn\":\"valfb\",\"arg\":");
                jstr(FBS[i]);
                printf(",\"out\":");
                jstr(out ? out : "");
                fputs("}\n", stdout);
                free(out);
            }
            json_free(fb);
            json_free(issues);
        }
    }
    return 0;
}
