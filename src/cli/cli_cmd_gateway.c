/*
 * cli_cmd_gateway.c — Gateway slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "hermes_agent.h"
#include "cli_cmd_gateway.h"
#include "commands_shared.h"
#include "hermes_core_types.h"

/* format_directory_for_display is defined in port_gateway_channel_directory.c
 * (same dir, non-static) and renders the cached channel directory. */
char *format_directory_for_display(void);

/* /gateway: Gateway management command with subcommands */
void cmd_gateway(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /gateway [status|list|stop|setup|restart]\n");
        printf("  status   Show gateway connection status\n");
        printf("  list     List configured platforms\n");
        printf("  stop     Stop gateway and exit\n");
        printf("  setup    Configure gateway platforms\n");
        printf("  restart  Save session and restart\n");
        return;
    }

    if (strcmp(args, "status") == 0 || strcmp(args, "list") == 0) {
        /* Show gateway platform status */
        hermes_config_t cfg;
        hermes_config_load(&cfg, NULL);
        printf("Gateway status:\n");
        const char *gw = getenv("HERMES_GATEWAY_PLATFORMS");
        if (gw)
            printf("  Platforms (env):  %s\n", gw);
        else if (cfg.gateway_platforms[0])
            printf("  Platforms (config): %s\n", cfg.gateway_platforms);
        else
            printf("  Platforms:        (none configured)\n");
        printf("  All 19 C platforms compiled in.\n");
        printf("  Set gateway.platforms in config.yaml to activate.\n");
        if (strcmp(args, "list") == 0) {
            /* Render the actual cached channel directory (was an orphaned
             * ported formatter; the hardcoded list below is replaced). */
            char *dir_txt = format_directory_for_display();
            if (dir_txt) {
                printf("\n%s\n", dir_txt);
                free(dir_txt);
            } else {
                printf("\nNo channel directory cached yet.\n");
            }
        }
        return;
    }

    if (strcmp(args, "stop") == 0) {
        printf("Stopping gateway...\n");
        /* Shutdown all gateway platforms */
        gw_platform_shutdown_all();
        /* Save session state */
        if (state->db) {
            agent_save_session(state);
            agent_close_db(state);
        }
        printf("Gateway stopped. Exiting.\n");
        exit(0);
    }

    if (strcmp(args, "setup") == 0) {
        printf("Gateway setup:\n");
        printf("\n");
        printf("  Available platforms and their required env vars:\n");
        printf("\n");
         const char *platforms[][2] = {
            {"telegram", "TELEGRAM_BOT_TOKEN"},
            {"discord",  "DISCORD_BOT_TOKEN"},
            {"slack",    "SLACK_BOT_TOKEN"},
            {"signal",   "SIGNAL_NUMBER"},
            {"sms",      "TWILIO_ACCOUNT_SID"},
            {"matrix",   "MATRIX_HOMESERVER"},
            {"email",    "EMAIL_HOST"},
            {"whatsapp", "WHATSAPP_PHONE_NUMBER_ID"},
            {"feishu",   "FEISHU_APP_ID"},
            {"wecom",    "WECOM_CORP_ID"},
            {"dingtalk", "DINGTALK_WEBHOOK_TOKEN"},
            {"homeassistant", "HASS_TOKEN"},
            {"mattermost","MATTERMOST_URL"},
            {"bluebubbles","BLUEBUBBLES_PASSWORD"},
            {NULL, NULL}
        };
        for (int i = 0; platforms[i][0]; i++) {
            const char *val = getenv(platforms[i][1]);
            printf("  %-14s %s (%s)\n",
                   platforms[i][0],
                   val ? "[ready]" : "[missing]",
                   platforms[i][1]);
        }
        printf("\n");
        printf("  Current config: ");
        hermes_config_t cfg;
        hermes_config_load(&cfg, NULL);
        if (cfg.gateway_platforms[0])
            printf("%s\n", cfg.gateway_platforms);
        else
            printf("(none)\n");
        printf("\n");
        printf("  To enable platforms:\n");
        printf("    1. Set the required env vars in ~/.hermes/.env\n");
        printf("    2. Run: /platform resume <platform_name>\n");
        printf("    3. Run: /gateway restart\n");
        printf("  Or edit gateway.platforms in config.yaml directly.\n");
        return;
    }

    if (strcmp(args, "restart") == 0) {
        /* Reuse restart logic from /restart */
        cmd_restart("", state);
        return;
    }

    printf("Unknown subcommand: '%s'. Use: /gateway status|list|stop|setup|restart\n", args);
}

/* /platform: List gateway platforms */
void cmd_platform(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Gateway platforms (configured via config.yaml gateway.platforms):\n");
        printf("  All 19 C platforms: telegram, discord, slack, matrix,\n");
        printf("  mattermost, webhook, whatsapp, email, signal, sms,\n");
        printf("  homeassistant, feishu, wecom, dingtalk, qqbot,\n");
        printf("  bluebubbles, msgraph_webhook, weixin, yuanbao\n");
        printf("  Use /platform list to show active status.\n");
        return;
    }
    if (strcmp(args, "list") == 0) {
        /* Show platforms from config */
        const char *gw = getenv("HERMES_GATEWAY_PLATFORMS");
        if (gw) {
            printf("Gateway platforms (env): %s\n", gw);
        } else {
            hermes_config_t cfg;
            hermes_config_load(&cfg, state->hermes_home);
            if (cfg.gateway_platforms[0])
                printf("Gateway platforms (config): %s\n", cfg.gateway_platforms);
            else
                printf("Gateway platforms: none configured\n");
        }
        printf("\nAll 19 C gateway modules compiled in.\n");
        printf("Active platforms determined by gateway.platforms config key.\n");
    } else if (strncmp(args, "pause", 5) == 0) {
        const char *pname = args + 5;
        while (*pname == ' ') pname++;
        if (!pname[0])
            printf("Usage: /platform pause <platform_name>\n");
        else {
            /* Read current platforms from config, remove the named platform */
            hermes_config_t cfg;
            hermes_config_load(&cfg, NULL);
            char new_platforms[256] = "";
            const char *src = cfg.gateway_platforms;
            if (src && src[0]) {
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s", src);
                char *tok = strtok(tmp, ",");
                int first = 1;
                while (tok) {
                    /* Remove leading/trailing whitespace from token */
                    while (*tok == ' ') tok++;
                    char *end = tok + strlen(tok);
                    while (end > tok && *(end-1) == ' ') end--;
                    *end = '\0';
                    if (strcasecmp(tok, pname) != 0) {
                        if (!first) strncat(new_platforms, ",", sizeof(new_platforms) - strlen(new_platforms) - 1);
                        strncat(new_platforms, tok, sizeof(new_platforms) - strlen(new_platforms) - 1);
                        first = 0;
                    }
                    tok = strtok(NULL, ",");
                }
            }
            if (hermes_config_set_platforms(&cfg, new_platforms[0] ? new_platforms : NULL)) {
                printf("Platform '%s' disabled (removed from gateway.platforms).\n", pname);
                printf("  Run /restart or restart slermes for the change to take effect.\n");
            } else {
                printf("Error: Could not update config.yaml.\n");
            }
        }
    } else if (strncmp(args, "resume", 6) == 0) {
        const char *pname = args + 6;
        while (*pname == ' ') pname++;
        if (!pname[0])
            printf("Usage: /platform resume <platform_name>\n");
        else {
            /* Read current platforms, add the named platform if not already present */
            hermes_config_t cfg;
            hermes_config_load(&cfg, NULL);
            char new_platforms[256] = "";
            int found = 0;
            if (cfg.gateway_platforms[0]) {
                snprintf(new_platforms, sizeof(new_platforms), "%s", cfg.gateway_platforms);
                /* Check if already present */
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s", cfg.gateway_platforms);
                char *tok = strtok(tmp, ",");
                while (tok) {
                    while (*tok == ' ') tok++;
                    if (strcasecmp(tok, pname) == 0) { found = 1; break; }
                    tok = strtok(NULL, ",");
                }
            }
            if (found) {
                printf("Platform '%s' is already enabled.\n", pname);
            } else {
                if (new_platforms[0])
                    strncat(new_platforms, ",", sizeof(new_platforms) - strlen(new_platforms) - 1);
                strncat(new_platforms, pname, sizeof(new_platforms) - strlen(new_platforms) - 1);
                if (hermes_config_set_platforms(&cfg, new_platforms)) {
                    printf("Platform '%s' enabled (added to gateway.platforms).\n", pname);
                    printf("  Run /restart or restart slermes for the change to take effect.\n");
                } else {
                    printf("Error: Could not update config.yaml.\n");
                }
            }
        }
    } else {
        printf("Unknown: %s. Use: list\n", args);
    }
}

/* /restart: Gracefully restart via exec */
void cmd_restart(const char *args, agent_state_t *state) {
    (void)args;
    /* Save session state before restart */
    if (state->db) {
        printf("Saving session...\n");
        fflush(stdout);
        agent_save_session(state);
        agent_close_db(state);
    }

    printf("Restarting...\n");
    fflush(stdout);

    /* Re-exec with saved arguments (if available) or default */
    char *argv[256];
    int argc = 0;
    char exe[4096];
    ssize_t exe_len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (exe_len > 0) {
        exe[exe_len] = '\0';
        argv[argc++] = exe;
        /* Try to preserve original args from /proc */
        FILE *cmdline = fopen("/proc/self/cmdline", "rb");
        if (cmdline) {
            char cl_buf[4096];
            int n = (int)fread(cl_buf, 1, sizeof(cl_buf) - 1, cmdline);
            fclose(cmdline);
            if (n > 0) {
                cl_buf[n] = '\0';
                /* Parse null-separated args */
                char *p = cl_buf;
                /* Skip argv[0] */
                p += strlen(p) + 1;
                while (*p && argc < 255) {
                    argv[argc++] = p;
                    p += strlen(p) + 1;
                }
            }
        }
        argv[argc] = NULL;
        execv(exe, argv);
        /* If exec fails */
        fprintf(stderr, "Restart failed: %s. Use /exit and re-launch.\n", strerror(errno));
    } else {
        fprintf(stderr, "Cannot find executable path. Use /exit and re-launch.\n");
    }
}

/* ── Webhook subscription management ──────────────────────────── */
/* AG26: Port of Python hermes_cli/main.py:cmd_webhook(). */
void cmd_webhook(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Webhook subscription management.\n");
        printf("Usage: /webhook list\n");
        printf("       /webhook add <url> [secret]\n");
        printf("       /webhook remove <id>\n");
        return;
    }

    if (strcmp(args, "list") == 0 || strcmp(args, "ls") == 0) {
        int count = webhook_subscription_count();
        if (count == 0) {
            printf("No webhook subscriptions. Add one with /webhook add <url> [secret]\n");
            return;
        }
        printf("Webhook subscriptions (%d):\n", count);
        webhook_subscription_t subs[64];
        int n = webhook_subscription_list(subs, 64);
        for (int i = 0; i < n; i++) {
            printf("  #%d: %s\n", i, subs[i].endpoint);
            if (subs[i].max_retries > 0)
                printf("      retries=%d backoff=%dms\n",
                       subs[i].max_retries, subs[i].backoff_ms);
            if (subs[i].header_count > 0) {
                printf("      headers: ");
                for (int j = 0; j < subs[i].header_count; j++)
                    printf("%s=%s ", subs[i].headers[j].key, subs[i].headers[j].value);
                printf("\n");
            }
        }
        return;
    }

    if (strncmp(args, "add ", 4) == 0) {
        const char *url = args + 4;
        const char *secret = NULL;
        char url_only[1024] = {0};
        const char *sp = strchr(url, ' ');
        if (sp) {
            size_t ulen = (size_t)(sp - url);
            if (ulen >= sizeof(url_only)) ulen = sizeof(url_only) - 1;
            memcpy(url_only, url, ulen);
            url_only[ulen] = '\0';
            secret = sp + 1;
            if (!*secret) secret = NULL;
        } else {
            snprintf(url_only, sizeof(url_only), "%s", url);
        }

        if (!url_only[0]) {
            printf("Error: URL required. Usage: /webhook add <url> [secret]\n");
            return;
        }

        int idx = webhook_subscription_add(url_only, secret, 3, 1000);
        if (idx >= 0) {
            printf("Subscription #%d added for %s\n", idx, url_only);
            if (secret) printf("  Secret: %s\n", secret);
        } else {
            printf("Error: Failed to add subscription (max %d reached?)\n", WEBHOOK_SUBS_MAX);
        }
        return;
    }

    if (strncmp(args, "remove ", 7) == 0) {
        int idx = atoi(args + 7);
        if (webhook_subscription_remove(idx)) {
            printf("Subscription #%d removed.\n", idx);
        } else {
            printf("Error: Subscription #%d not found.\n", idx);
        }
        return;
    }

    printf("Unknown subcommand: %s\n", args);
    printf("Usage: /webhook list | add <url> [secret] | remove <id>\n");
}

