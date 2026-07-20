/*
 * t_port_setup_probes.c — faithful verification harness for the provider/model
 * setup probes (ports of hermes_cli/setup.py and agent/google_oauth.py).
 *
 * One fixture file, one op token per line. The harness exercises the REAL
 * functions with REAL inputs (controlled PATH, real env vars, a real
 * hermes_config_t) — no mocks. The Python oracle
 * (tests/sta_oracle_setup_probes.py) recomputes the expected result from the
 * same fixture; the runner diffs them.
 *
 * Fixture line grammar:
 *   espeak        <have|no>
 *   xai           <set|unset>
 *   reasoning
 *   model_config  <model-string>
 *   model_creds   <provider> <set|unset>
 *   goauth        client_id <set|unset>
 *   goauth        secret    <set|unset>
 */

#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Forward declarations — defined in src/cli/config_setup.c and
 * src/provider/google_oauth.c. We avoid pulling hermes.h (libdb chain). */
bool setup_check_espeak_ng(void);
bool setup_xai_oauth_logged_in(void);
void setup_model_config_dict(const hermes_config_t *cfg, char *out_default, size_t out_size);
const char *setup_current_reasoning_effort(void);
bool setup_model_section_has_credentials(const hermes_config_t *cfg, const char *provider);
const char *require_client_id(void);
const char *get_client_secret(void);

/* Emit a JSON string with the minimal escaping the oracle uses. */
static void emit_json_string(const char *s) {
    putchar('"');
    for (const char *p = s ? s : ""; *p; p++) {
        if (*p == '"') fputs("\\\"", stdout);
        else if (*p == '\\') fputs("\\\\", stdout);
        else if (*p == '\n') fputs("\\n", stdout);
        else if (*p == '\r') fputs("\\r", stdout);
        else if (*p == '\t') fputs("\\t", stdout);
        else putchar(*p);
    }
    putchar('"');
}

static void set_env_or_clear(const char *name, int set_it) {
    if (set_it) setenv(name, "present", 1);
    else unsetenv(name);
}

/* env var the C side checks for a given provider (mirrors
 * setup_model_section_has_credentials in config_setup.c). Used so the
 * harness and oracle agree on which var gates each provider. */
static const char *provider_env_var(const char *prov) {
    if (strcmp(prov, "nous") == 0) return "NOUS_API_KEY";
    if (strcmp(prov, "openai") == 0) return "OPENAI_API_KEY";
    if (strcmp(prov, "anthropic") == 0) return "ANTHROPIC_API_KEY";
    if (strcmp(prov, "google") == 0) return "GOOGLE_API_KEY";
    if (strcmp(prov, "deepseek") == 0) return "DEEPSEEK_API_KEY";
    if (strcmp(prov, "xai") == 0) return "XAI_API_KEY";
    if (strcmp(prov, "openrouter") == 0) return "OPENROUTER_API_KEY";
    if (strcmp(prov, "azure") == 0) return "AZURE_API_KEY";
    if (strcmp(prov, "bedrock") == 0) return "AWS_ACCESS_KEY_ID";
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.txt>\n", argv[0]); return 2; }
    /* Remember the real PATH so we can restore it before running cleanup
     * commands (rm) that would otherwise be unfindable on the isolated PATH. */
    const char *orig_path = getenv("PATH");
    char orig_path_buf[4096];
    if (orig_path) {
        strncpy(orig_path_buf, orig_path, sizeof(orig_path_buf) - 1);
        orig_path_buf[sizeof(orig_path_buf) - 1] = '\0';
    } else {
        orig_path_buf[0] = '\0';
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0) continue;

        char *op = line;
        char *rest = strchr(op, ' ');
        if (rest) { *rest++ = '\0'; while (*rest == ' ') rest++; } else rest = (char *)"";

        if (strcmp(op, "espeak") == 0) {
            /* Fully isolated PATH: a private temp dir that contains only
             * `which` (symlinked from the real location) and, for the "have"
             * case, a fake `espeak-ng`. This keeps setup_check_espeak_ng's
             * `popen("which espeak-ng ...")` self-contained and avoids the
             * real system espeak. */
            char tmppath[1024];
            snprintf(tmppath, sizeof(tmppath), "/tmp/espeak_probe_%d", (int)getpid());
            mkdir(tmppath, 0755);
            /* bring `which` into the isolated dir */
            char whichlink[1400];
            snprintf(whichlink, sizeof(whichlink), "ln -sf /usr/bin/which '%s/which' 2>/dev/null", tmppath);
            system(whichlink);
            if (strcmp(rest, "have") == 0) {
                char bin[1200];
                snprintf(bin, sizeof(bin), "%s/espeak-ng", tmppath);
                FILE *bf = fopen(bin, "w");
                if (bf) { fputs("#!/bin/sh\necho ok\n", bf); fclose(bf); chmod(bin, 0755); }
            }
            setenv("PATH", tmppath, 1);
            bool r = setup_check_espeak_ng();
            printf("{\"op\":\"espeak\",\"case\":\"%s\",\"found\":%s}\n", rest, r ? "true" : "false");
            /* restore PATH so the cleanup `rm` (a system command) is findable */
            if (orig_path_buf[0]) setenv("PATH", orig_path_buf, 1);
            char rm[1400]; snprintf(rm, sizeof(rm), "rm -rf '%s'", tmppath); system(rm);

        } else if (strcmp(op, "xai") == 0) {
            set_env_or_clear("XAI_API_KEY", strcmp(rest, "set") == 0);
            bool r = setup_xai_oauth_logged_in();
            printf("{\"op\":\"xai\",\"case\":\"%s\",\"logged_in\":%s}\n", rest, r ? "true" : "false");

        } else if (strcmp(op, "reasoning") == 0) {
            const char *r = setup_current_reasoning_effort();
            printf("{\"op\":\"reasoning\",\"out\":"); emit_json_string(r ? r : ""); printf("}\n");

        } else if (strcmp(op, "model_config") == 0) {
            hermes_config_t cfg;
            memset(&cfg, 0, sizeof(cfg));
            strncpy(cfg.model, rest, sizeof(cfg.model) - 1);
            char out[512];
            memset(out, 0, sizeof(out));
            setup_model_config_dict(&cfg, out, sizeof(out));
            printf("{\"op\":\"model_config\",\"in\":");
            emit_json_string(rest);
            printf(",\"out\":");
            emit_json_string(out);
            printf("}\n");

        } else if (strcmp(op, "model_creds") == 0) {
            char *prov = rest;
            char *mode = strchr(prov, ' ');
            if (!mode) continue;
            *mode++ = '\0';
            while (*mode == ' ') mode++;
            const char *env = provider_env_var(prov);
            set_env_or_clear(env ? env : "NONE_UNMAPPED", env && strcmp(mode, "set") == 0);
            bool r = setup_model_section_has_credentials(NULL, prov);
            printf("{\"op\":\"model_creds\",\"provider\":\"%s\",\"has_creds\":%s}\n", prov, r ? "true" : "false");

        } else if (strcmp(op, "goauth") == 0) {
            char *which = rest;
            char *mode = strchr(which, ' ');
            if (!mode) continue;
            *mode++ = '\0';
            while (*mode == ' ') mode++;
            if (strcmp(which, "client_id") == 0) {
                set_env_or_clear("GOOGLE_CLIENT_ID", strcmp(mode, "set") == 0);
                const char *r = require_client_id();
                printf("{\"op\":\"goauth\",\"which\":\"client_id\",\"val\":");
                emit_json_string(r);
                printf("}\n");
            } else if (strcmp(which, "secret") == 0) {
                set_env_or_clear("GOOGLE_CLIENT_SECRET", strcmp(mode, "set") == 0);
                const char *r = get_client_secret();
                printf("{\"op\":\"goauth\",\"which\":\"secret\",\"val\":");
                emit_json_string(r);
                printf("}\n");
            }
        }
    }
    fclose(f);
    return 0;
}
