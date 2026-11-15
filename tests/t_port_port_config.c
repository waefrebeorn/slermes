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

    return 0;
}
