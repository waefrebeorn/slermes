/*
 * cli_cmd_security.c — Security slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "cli_cmd_security.h"
#include "commands_shared.h"
#include "hermes.h"

/* /auth: Provider auth status overview */
void cmd_auth(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Auth credential status.\n");
        printf("Usage: /auth status              — Show all provider credential status\n");
        printf("       /auth providers            — List known auth providers\n");
        return;
    }
    const char *sub = args;
    while (*sub == ' ') sub++;

    if (strcmp(sub, "status") == 0 || strcmp(sub, "st") == 0) {
        printf("=== Provider Credential Status ===\n\n");
        /* Check API key env vars */
        typedef struct { const char *provider; const char *env_var; const char *desc; } cred_check_t;
        cred_check_t checks[] = {
            {"OpenAI",    "OPENAI_API_KEY",       "ChatGPT/OpenAI models"},
            {"Anthropic", "ANTHROPIC_API_KEY",     "Claude models"},
            {"Anthropic", "ANTHROPIC_TOKEN",       "Claude OAuth setup token"},
            {"OpenRouter", "OPENROUTER_API_KEY",   "OpenRouter aggregation"},
            {"DeepSeek",  "DEEPSEEK_API_KEY",      "DeepSeek models"},
            {"Google",    "GOOGLE_API_KEY",        "Gemini models"},
            {"xAI",       "XAI_API_KEY",           "Grok models"},
            {"Azure",     "AZURE_API_KEY",         "Azure OpenAI"},
            {"AWS",       "AWS_ACCESS_KEY_ID",     "AWS Bedrock (w/ AWS_SECRET_ACCESS_KEY)"},
            {"Nous",      "NOUS_API_KEY",          "NousResearch inference"},
            {"HF",        "HF_TOKEN",              "Hugging Face models"},
            {"Groq",      "GROQ_API_KEY",          "Groq inference"},
            {"Together",  "TOGETHER_API_KEY",      "Together AI"},
            {"Perplexity","PERPLEXITY_API_KEY",    "Perplexity models"},
            {"Cohere",    "COHERE_API_KEY",        "Cohere models"},
            {"Fireworks", "FIREWORKS_API_KEY",     "Fireworks AI"},
            {"Mistral",   "MISTRAL_API_KEY",       "Mistral models"},
            {NULL, NULL, NULL}
        };
        int count = 0;
        for (int i = 0; checks[i].provider; i++) {
            const char *val = getenv(checks[i].env_var);
            bool present = (val && val[0]);
            if (present) {
                size_t vlen = strlen(val);
                printf("  %-14s  %-30s  ✓ (%zu chars)\n",
                       checks[i].provider, checks[i].env_var, vlen);
                count++;
            }
        }
        if (count == 0)
            printf("  No API key credentials found in environment.\n");
        printf("\n  Total: %d credential(s) detected\n", count);

        /* OAuth status */
        printf("\n=== OAuth / Token Status ===\n");
        const char *cc_oauth = getenv("CLAUDE_CODE_OAUTH_TOKEN");
        printf("  Claude Code OAuth: %s\n",
               (cc_oauth && *cc_oauth) ? "configured" : "not set");
        const char *bws = getenv("BWS_ACCESS_TOKEN");
        if (!bws) bws = getenv("SLERMES_BWS_TOKEN");
        printf("  Bitwarden (BSM):  %s\n", (bws && *bws) ? "configured" : "not set");

        printf("\n=== Config Status ===\n");
        const char *home = state->hermes_home[0] ? state->hermes_home : NULL;
        if (!home) home = getenv("SLERMES_HOME");
        if (!home) {
            home = getenv("HOME");
            printf("  Config home:     %s\n", home ? home : "(not found)");
        } else {
            printf("  Config home:     %s\n", home);
        }
        char env_path[1024];
        snprintf(env_path, sizeof(env_path), "%s/.env", home ? home : "");
        printf("  .env file:       %s\n",
               (home && access(env_path, F_OK) == 0) ? "present" : "not found");
        char cfg_path[1024];
        snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", home ? home : "");
        printf("  config.yaml:     %s\n",
               (home && access(cfg_path, F_OK) == 0) ? "present" : "not found");

        /* Show OAuth token status */
        if (home) {
            int oauth_count = 0;
            auth_entry_t *entries = auth_store_load(home, &oauth_count);
            if (entries && oauth_count > 0) {
                printf("\n  OAuth Tokens:\n");
                time_t now = time(NULL);
                for (int i = 0; i < oauth_count; i++) {
                    const char *status = "expired";
                    if (entries[i].token.expires_at > 0) {
                        if (entries[i].token.expires_at > (double)now)
                            status = "valid";
                        else if (entries[i].token.refresh_token && entries[i].token.refresh_token[0])
                            status = "expired (refreshable)";
                    } else if (entries[i].token.access_token) {
                        status = "valid (no expiry)";
                    }
                    printf("    %-20s %s", entries[i].provider, status);
                    if (entries[i].token.expires_at > 0) {
                        time_t et = (time_t)entries[i].token.expires_at;
                        printf("  (expires %s", ctime(&et));
                    } else {
                        printf("\n");
                    }
                }
                auth_store_free(entries, oauth_count);
            }
        }

        printf("\n  For details, use: /secrets status\n");
        printf("  For auth login flows, use: /auth login <provider> [key]\n");
        printf("  To refresh OAuth tokens: /auth refresh [provider]\n");
        return;
    }

    if (strcmp(sub, "providers") == 0) {
        printf("Known auth providers:\n");
        printf("  openai        API key ($OPENAI_API_KEY)\n");
        printf("  anthropic     API key ($ANTHROPIC_API_KEY) or OAuth ($ANTHROPIC_TOKEN)\n");
        printf("  openrouter    API key ($OPENROUTER_API_KEY)\n");
        printf("  deepseek      API key ($DEEPSEEK_API_KEY)\n");
        printf("  google        API key ($GOOGLE_API_KEY)\n");
        printf("  xai           API key ($XAI_API_KEY) or OAuth (xAI OAuth flow)\n");
        printf("  azure         API key ($AZURE_API_KEY)\n");
        printf("  bedrock       AWS credentials ($AWS_ACCESS_KEY_ID + secret)\n");
        printf("  nous          API key ($NOUS_API_KEY) or OAuth (device code flow)\n");
        printf("  groq          API key ($GROQ_API_KEY)\n");
        printf("  together       API key ($TOGETHER_API_KEY)\n");
        printf("  mistral       API key ($MISTRAL_API_KEY)\n");
        printf("  fireworks     API key ($FIREWORKS_API_KEY)\n");
        printf("  perplexity    API key ($PERPLEXITY_API_KEY)\n");
        printf("  cohere        API key ($COHERE_API_KEY)\n");
        printf("\nTo add credentials: set the env var in .env or use /auth login <provider> [key].\n");
        printf("  /auth login openai sk-xxx...\n");
        printf("  /auth login anthropic   - prints instructions\n");
        return;
    }

    if (strcmp(sub, "login") == 0 || strncmp(sub, "login ", 6) == 0) {
        const char *rest = sub + 5;
        while (*rest == ' ') rest++;
        if (!*rest) {
            printf("Usage: /auth login <provider> [api_key]\n");
            printf("If no api_key provided, prints setup instructions.\n");
            printf("If api_key provided, writes directly to .env.\n\n");
            printf("Providers: openai, anthropic, openrouter, deepseek,\n");
            printf("           google, xai, azure, bedrock, nous, hf,\n");
            printf("           groq, together, mistral, fireworks,\n");
            printf("           perplexity, cohere\n");
            return;
        }
        const char *provider = rest;
        const char *key_value = NULL;
        const char *sp = strchr(provider, ' ');
        if (sp) {
            char pbuf[64];
            size_t plen = (size_t)(sp - provider);
            snprintf(pbuf, sizeof(pbuf), "%.*s", (int)plen, provider);
            provider = pbuf;
            key_value = sp + 1;
            while (*key_value == ' ') key_value++;
        }
        typedef struct { const char *n; const char *e; } pm_t;
         const pm_t PM[] = {
            {"openai","OPENAI_API_KEY"}, {"anthropic","ANTHROPIC_API_KEY"},
            {"openrouter","OPENROUTER_API_KEY"}, {"deepseek","DEEPSEEK_API_KEY"},
            {"google","GOOGLE_API_KEY"}, {"xai","XAI_API_KEY"},
            {"azure","AZURE_API_KEY"}, {"bedrock","AWS_ACCESS_KEY_ID"},
            {"nous","NOUS_API_KEY"}, {"hf","HF_TOKEN"},
            {"groq","GROQ_API_KEY"}, {"together","TOGETHER_API_KEY"},
            {"mistral","MISTRAL_API_KEY"}, {"fireworks","FIREWORKS_API_KEY"},
            {"perplexity","PERPLEXITY_API_KEY"}, {"cohere","COHERE_API_KEY"},
            {NULL,NULL}
        };
        const char *env_var = NULL;
        for (int i = 0; PM[i].n; i++) {
            if (strcasecmp(provider, PM[i].n) == 0) { env_var = PM[i].e; break; }
        }
        if (!env_var) { printf("Unknown provider. Run /auth providers\n"); return; }

        if (key_value) {
            const char *h = state->hermes_home[0] ? state->hermes_home : getenv("HOME");
            if (!h) h = ".";
            char ep[1024]; snprintf(ep, sizeof(ep), "%s/.env", h);
            FILE *f = fopen(ep, "a");
            if (!f) { printf("Cannot write %s\n", ep); return; }
            fprintf(f, "\n# %s via /auth login\n%s=%s\n", provider, env_var, key_value);
            fclose(f);
            printf("Wrote %s=%s to %s\n", env_var, key_value, ep);
            printf("Restart slermes or /reload to apply.\n");
        } else if (strcasecmp(provider, "nous") == 0) {
            /* Device code flow for Nous Portal */
            const char *home = state->hermes_home[0] ? state->hermes_home : getenv("HOME");
            if (!home) { printf("Cannot determine home directory.\n"); return; }

            oauth_token_t *tok = nous_device_code_login(30);
            if (!tok) return;

            /* Save token to auth store */
            auth_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            strncpy(entry.provider, "nous-oauth", sizeof(entry.provider) - 1);
            entry.token = *tok;  /* Transfer ownership — don't free separately */

            if (auth_store_save(home, &entry)) {
                printf("\n✅ Nous Portal OAuth token saved.\n");
                printf("   Provider: nous-oauth\n");
                if (tok->expires_at > 0) {
                    printf("   Expires:  %s", ctime(&(time_t){ (time_t)tok->expires_at }));
                }
                printf("\nRestart slermes or /reload to apply.\n");
            } else {
                printf("\n❌ Failed to save OAuth token: %s\n", oauth_last_error());
                oauth_token_free(tok);
            }
        } else if (strcasecmp(provider, "xai-oauth") == 0 ||
                   strcasecmp(provider, "xai") == 0) {
            /* PKCE loopback callback flow for xAI OAuth */
            const char *home = state->hermes_home[0] ? state->hermes_home : getenv("HOME");
            if (!home) { printf("Cannot determine home directory.\n"); return; }

            printf("Starting xAI OAuth loopback login...\n");
            fflush(stdout);
            oauth_token_t *tok = xai_oauth_callback_login(120);
            if (!tok) {
                printf("\n❌ xAI OAuth login failed: %s\n", oauth_last_error());
                return;
            }

            /* Save token to auth store */
            auth_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            strncpy(entry.provider, "xai-oauth", sizeof(entry.provider) - 1);
            entry.token = *tok;

            printf("\nSaving xAI OAuth token to auth store...\n");
            if (auth_store_save(home, &entry)) {
                printf("✅ xAI OAuth token saved.\n");
                if (tok->expires_at > 0) {
                    time_t exp = (time_t)tok->expires_at;
                    printf("   Expires: %s", ctime(&exp));
                }
                printf("\nRestart slermes or /reload to apply.\n");
                printf("Set model.provider to xai-oauth in config to use.\n");
            } else {
                printf("\n❌ Failed to save OAuth token: %s\n", oauth_last_error());
                oauth_token_free(tok);
            }
        } else {
            printf("To configure %s:\n", provider);
            printf("  export %s=<your_key>\n", env_var);
            printf("  /auth login %s <your_key>\n", provider);
        }
        return;
    }

    if (strcmp(sub, "refresh") == 0 || strncmp(sub, "refresh ", 8) == 0) {
        const char *target = sub + 7;
        while (*target == ' ') target++;
        const char *home = state->hermes_home[0] ? state->hermes_home : getenv("HOME");
        if (!home) { printf("Cannot determine home directory.\n"); return; }

        int count = 0;
        auth_entry_t *entries = auth_store_load(home, &count);
        if (!entries || count == 0) {
            printf("No OAuth tokens found in auth store.\n");
            auth_store_free(entries, count);
            return;
        }

        int refreshed = 0;
        for (int i = 0; i < count; i++) {
            if (target[0] && strcmp(entries[i].provider, target) != 0)
                continue;
            if (!entries[i].token.refresh_token || !entries[i].token.refresh_token[0]) {
                if (target[0]) printf("  %s: no refresh token\n", entries[i].provider);
                continue;
            }
            /* Determine token endpoint from provider name */
            const char *endpoint = NULL;
            const char *client_id = "hermes-cli";
            if (strcmp(entries[i].provider, "nous-oauth") == 0) {
                endpoint = NOUS_OAUTH_TOKEN_ENDPOINT;
            } else if (strcmp(entries[i].provider, "xai-oauth") == 0) {
                endpoint = "https://auth.x.ai/oauth2/token";
                client_id = XAI_OAUTH_CLIENT_ID;
            }
            if (!endpoint) {
                printf("  %s: unknown token endpoint, skipping\n", entries[i].provider);
                continue;
            }

            printf("  Refreshing %s... ", entries[i].provider);
            fflush(stdout);
            oauth_token_t *tok = oauth_refresh_token(endpoint, client_id,
                entries[i].token.refresh_token, 30);
            if (!tok) {
                printf("failed: %s\n", oauth_last_error());
                continue;
            }
            /* Save updated token */
            auth_entry_t new_entry;
            memset(&new_entry, 0, sizeof(new_entry));
            strncpy(new_entry.provider, entries[i].provider, sizeof(new_entry.provider) - 1);
            new_entry.token = *tok;
            if (auth_store_save(home, &new_entry)) {
                printf("ok\n");
                refreshed++;
            } else {
                printf("save failed\n");
                oauth_token_free(tok);
            }
        }
        auth_store_free(entries, count);
        printf("\nRefreshed %d token(s).\n", refreshed);
        return;
    }

    if (strcmp(sub, "tokens") == 0) {
        const char *home = state->hermes_home[0] ? state->hermes_home : getenv("HOME");
        if (!home) { printf("Cannot determine home directory.\n"); return; }
        int count = 0;
        auth_entry_t *entries = auth_store_load(home, &count);
        if (!entries || count == 0) {
            printf("No OAuth tokens stored.\n");
            auth_store_free(entries, count);
            return;
        }
        printf("Stored OAuth tokens:\n");
        time_t now = time(NULL);
        for (int i = 0; i < count; i++) {
            const char *status = "expired";
            if (entries[i].token.expires_at > 0) {
                if (entries[i].token.expires_at > (double)now)
                    status = "valid";
                else if (entries[i].token.refresh_token && entries[i].token.refresh_token[0])
                    status = "expired (refreshable)";
            } else if (entries[i].token.access_token) {
                status = "valid (no expiry)";
            }
            printf("  %-20s %s", entries[i].provider, status);
            if (entries[i].token.expires_at > 0) {
                time_t et = (time_t)entries[i].token.expires_at;
                printf("  (expires %s", ctime(&et));
            } else {
                printf("\n");
            }
            printf("                 refresh_token: %s\n",
                   entries[i].token.refresh_token && entries[i].token.refresh_token[0] ? "yes" : "no");
        }
        auth_store_free(entries, count);
        printf("\nUse /auth refresh [provider] to refresh expiring tokens.\n");
        return;
    }

    if (strcmp(sub, "validate") == 0 || strncmp(sub, "validate ", 9) == 0) {
        const char *target = sub + 8;
        while (*target == ' ') target++;
        if (!*target) {
            printf("Usage: /auth validate <provider>\n");
            printf("Tests API key. Supported: openai/ anthropic/ openrouter/ deepseek/ xai/ groq/ together/ google\n");
            return;
        }
        typedef struct { const char *n; const char *e; const char *u; int m; } val_t;
         const val_t V[] = {
            {"openai","OPENAI_API_KEY","https://api.openai.com/v1/models",0},
            {"openrouter","OPENROUTER_API_KEY","https://openrouter.ai/api/v1/models",0},
            {"deepseek","DEEPSEEK_API_KEY","https://api.deepseek.com/chat/completions",0},
            {"xai","XAI_API_KEY","https://api.x.ai/v1/models",0},
            {"groq","GROQ_API_KEY","https://api.groq.com/openai/v1/models",0},
            {"together","TOGETHER_API_KEY","https://api.together.xyz/v1/models",0},
            {"google","GOOGLE_API_KEY","https://generativelanguage.googleapis.com/v1beta/models",1},
            {"anthropic","ANTHROPIC_API_KEY","https://api.anthropic.com/v1/messages",2},
            {NULL,NULL,NULL,0}
        };
        int found = 0;
        for (int i = 0; V[i].n; i++) {
            if (strcasecmp(target, V[i].n) != 0) continue;
            found = 1;
            const char *key = getenv(V[i].e);
            if (!key||!*key) { printf("*** env var not set (%s)\n", V[i].e); break; }
            printf("Testing %s", V[i].n); printf("... ");
            fflush(stdout);
            http_client_t *c = http_new(15);
            bool ok = false;
            if (c) {
                if (V[i].m == 1) {
                    char url[1024];
                    char gpf[] = "?key=";
                    memcpy(url, V[i].u, strlen(V[i].u) + 1);
                    strcat(url, gpf);
                    strcat(url, key);
                    http_resp_t *r = http_get(c,url,NULL);
                    ok = r && (r->status==200||r->status==403||r->status==400);
                    http_resp_free(r);
                } else if (V[i].m == 2) {
                    char hdr[512];
                    char ap[] = "x-api-key: ";
                    char aver[] = "\r\nanthropic-version: 2023-06-01";
                    memcpy(hdr, ap, strlen(ap) + 1);
                    strcat(hdr, key);
                    strcat(hdr, aver);
                    const char *msg = "{\"model\":\"claude-3-5-sonnet-20241022\",\"max_tokens\":1,\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}";
                    http_resp_t *r = http_post_json_auth(c, V[i].u, hdr, msg);
                    ok = r && (r->status==200||r->status==400||r->status==401);
                    http_resp_free(r);
                } else {
                    char hdr[512];
                    char bp[] = "Authorization: Bearer ";
                    strcpy(hdr, bp);
                    strcat(hdr, key);
                    http_resp_t *r = http_get(c, V[i].u, hdr);
                    ok = r && (r->status == 200 || r->status == 401);
                    http_resp_free(r);
                }
                http_free(c);
            }
            printf(ok ? "valid\n" : "invalid\n");
            break;
        }
        if (!found) printf("Unknown provider\n");
        return;
    }

    printf("Unknown subcommand: '%s'\n", sub);
    printf("Usage: /auth status | providers | login | tokens | refresh | validate\n");
}

/* /key: Manage API keys */
/* AG26: Port of Python hermes_cli/main.py:_key(). */
void cmd_key(const char *args, agent_state_t *state) {
    if (!args || args[0] == '\0') {
        printf("Usage: /key [list|set <provider>|show <provider>|unset <provider>]\n");
        printf("  list              - Show all configured API keys (masked)\n");
        printf("  set <provider>    - Interactively set a provider API key\n");
        printf("  show <provider>   - Show a specific provider's key (masked)\n");
        printf("  unset <provider>  - Remove a provider's key from .env\n");
        printf("\nKnown providers: ");
        for (int i = 0; PROVIDER_KEY_MAP[i].provider; i++) {
            if (i > 0) printf(", ");
            printf("%s", PROVIDER_KEY_MAP[i].provider);
        }
        printf("\n");
        return;
    }

    /* Parse subcommand */
    char subcmd[128];
    const char *p = args;
    while (*p == ' ') p++;
    int i = 0;
    while (*p && *p != ' ' && i < (int)sizeof(subcmd) - 1)
        subcmd[i++] = *p++;
    subcmd[i] = '\0';
    while (*p == ' ') p++;
    const char *param = (*p) ? p : NULL;

    if (strcmp(subcmd, "list") == 0) {
        key_list_all();
        return;
    }

    if (strcmp(subcmd, "set") == 0) {
        if (!param) {
            printf("Usage: /key set <provider>\n");
            return;
        }
        key_wizard(state->hermes_home, param);
        return;
    }

    if (strcmp(subcmd, "show") == 0) {
        if (!param) {
            printf("Usage: /key show <provider>\n");
            return;
        }
        key_show(param);
        return;
    }

    if (strcmp(subcmd, "unset") == 0) {
        if (!param) {
            printf("Usage: /key unset <provider>\n");
            return;
        }
        if (key_unset(state->hermes_home, param) == 0) {
            printf("Key cleared for %s. Run '/reload env' to apply.\n", param);
        } else {
            printf("Failed to clear key for %s.\n", param);
        }
        return;
    }

    printf("Unknown subcommand: %s. Use '/key' for help.\n", subcmd);
}

/* /secrets: Manage Bitwarden secrets */
void cmd_secrets(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || args[0] == '\0') {
        printf("Usage: /secrets [list|get <name>|sync|status]\n");
        printf("  list   - Show available secret references\n");
        printf("  get    - Resolve and display a single secret\n");
        printf("  sync   - Force re-fetch from Bitwarden\n");
        printf("  status - Show Bitwarden integration status\n");
        return;
    }

    /* Parse subcommand */
    char subcmd[256];
    const char *p = args;
    while (*p == ' ') p++;
    int i = 0;
    while (*p && *p != ' ' && i < (int)sizeof(subcmd) - 1)
        subcmd[i++] = *p++;
    subcmd[i] = '\0';
    while (*p == ' ') p++;
    const char *param = (*p) ? p : NULL;

    if (strcmp(subcmd, "status") == 0) {
        const char *token = getenv("BWS_ACCESS_TOKEN");
        if (!token) token = getenv("SLERMES_BWS_TOKEN");
        printf("Bitwarden Secrets Manager status:\n");
        printf("  Access token: %s\n", (token && token[0]) ? "configured" : "NOT SET");

        const char *bsm_cfg = getenv("HERMES_SECRETS_ENABLED");
        if (!bsm_cfg) bsm_cfg = getenv("SLERMES_SECRETS_ENABLED");
        printf("  Integration: %s\n",
               (bsm_cfg && strcmp(bsm_cfg, "false") != 0) ? "enabled" : "disabled");

        char cmd[256];
        snprintf(cmd, sizeof(cmd), "which bws 2>/dev/null || echo 'not found'");
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char path[512] = "";
            if (fgets(path, sizeof(path), fp)) {
                size_t len = strlen(path);
                if (len > 0 && path[len-1] == '\n') path[len-1] = '\0';
                printf("  bws binary: %s\n", path);
            }
            pclose(fp);
        }

        /* Check for Anthropic OAuth tokens (port of Python anthropic_adapter.py) */
        printf("\nAnthropic OAuth status:\n");
        const char *ant_key = getenv("ANTHROPIC_API_KEY");
        const char *ant_token = getenv("ANTHROPIC_TOKEN");
        const char *cc_oauth = getenv("CLAUDE_CODE_OAUTH_TOKEN");
        bool has_api_key = (ant_key && *ant_key);
        bool has_setup_token = (ant_token && is_oauth_token(ant_token));
        bool has_cc_oauth = (cc_oauth && *cc_oauth);

        printf("  API key:        %s\n", has_api_key ? "✓" : "not set");
        printf("  Setup token:    %s%s\n",
               has_setup_token ? "✓" : "not set",
               has_setup_token ? "" : " (sk-ant-oat* or managed key)");
        printf("  CC OAuth token: %s\n",
               has_cc_oauth ? "✓ (from CLAUDE_CODE_OAUTH_TOKEN)" : "not set");
        printf("  Total:          %d credential(s) detected\n",
               (int)has_api_key + (int)(ant_token && *ant_token) + (int)has_cc_oauth);

    } else if (strcmp(subcmd, "list") == 0) {
        const char *token = getenv("BWS_ACCESS_TOKEN");
        if (!token) token = getenv("SLERMES_BWS_TOKEN");
        if (!token || !*token) {
            printf("BWS_ACCESS_TOKEN not set.\n");
            return;
        }
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
                 "bws secret list 2>/dev/null | "
                 "jq -r '.[] | \"\\(.key): \\(.id)\"' 2>/dev/null || "
                 "bws secret list 2>/dev/null");
        printf("Secrets:\n");
        fflush(stdout);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char line[1024];
            int count = 0;
            while (fgets(line, sizeof(line), fp)) {
                printf("  %s", line);
                count++;
            }
            int rc = pclose(fp);
            if (count == 0)
                printf("  (none — ensure bws is authenticated)\n");
            (void)rc;
        }

    } else if (strcmp(subcmd, "get") == 0 && param) {
        char ref[512];
        snprintf(ref, sizeof(ref), "${BSM:%s}", param);
        char *val = hermes_secrets_resolve(ref);
        if (val) {
            printf("%s: %s\n", param, val);
            free(val);
        } else {
            printf("Secret '%s' not found.\n", param);
        }

    } else if (strcmp(subcmd, "sync") == 0) {
        printf("Syncing secrets...\n");
        if (hermes_secrets_init(NULL))
            printf("Sync complete.\n");
        else
            printf("Sync failed.\n");

    } else {
        printf("Unknown subcommand '%s'. Use: list, get, sync, status\n", subcmd);
    }
}

