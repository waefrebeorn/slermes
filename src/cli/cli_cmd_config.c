/*
 * cli_cmd_config.c — Config slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "cli_cmd_config.h"
#include "commands_shared.h"
#include "hermes.h"

/* P23: Config category groups — /config groups lists all.
 * Canonical definition (shared by the facade via commands_shared.h extern).
 * The cfg_category_t type lives in commands_shared.h. */
const cfg_category_t CFG_CATEGORIES[] = {
    {"model",       "Provider, model, API connection settings",    "model.",       0},
    {"display",     "UI theme, skin, streaming, language",        "display.",     0},
    {"agent",       "Iterations, verbosity, system prompt",       "agent.",       0},
    {"tools",       "Terminal, approvals, vision settings",       "tools.",       0},
    {"delegation",  "Subagent spawning and child config",         "delegation.",  0},
    {"browser",     "CDP browser engine settings",                "browser.",     0},
    {"memory",      "Memory provider, char limits, TTL",          "memory.",      0},
    {"compression", "Context compression strategy and thresholds","compression.", 0},
    {"cron",        "Scheduler directory, job limits",            "cron.",        0},
    {"notification","Completion/error notification settings",     "notification.",0},
    {"security",    "Tirith, URL safety, redaction",              "security.",    0},
    {"sessions",    "DB path, retention, auto-save",              "session.",     0},
    {"plugin",      "Plugin directories and enabled plugins",      "plugin.",      0},
    {"mcp",         "MCP server timeout, auth, tool limit",       "mcp.",         0},
    {"auxiliary",   "Auxiliary LLM routing (vision, web_extract, etc.)","auxiliary.",  0},
    {"tts",         "Text-to-speech configuration",                "tts.",        0},
    {"stt",         "Speech-to-text configuration",                "stt.",        0},
    {"voice",       "Voice input recording settings",              "voice.",      0},
    {NULL, NULL, NULL, 0}
};

/* ── Backup ────────────────────────────────────────────────── */
/* AG26: Port of Python hermes_cli/main.py:cmd_backup(). */
void cmd_backup(const char *args, agent_state_t *state) {
    (void)state;
    bool full = false;
    if (args) {
        while (*args == ' ') args++;
        if (strcmp(args, "full") == 0) full = true;
    }

    const char *home = state->hermes_home;
    if (!home || !home[0]) {
        home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
    }

    /* Timestamp for backup filename */
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", tm_now);

    /* Build backup path */
    char backup_dir[1024];
    snprintf(backup_dir, sizeof(backup_dir), "%s/backups", home);
    mkdir(backup_dir, 0700);

    char backup_path[1024];
    snprintf(backup_path, sizeof(backup_path), "%s/slermes-backup-%s", backup_dir, ts);

    printf("\n=== Slermes Backup ===\n\n");
    printf("  Destination: %s/\n", backup_dir);

    /* Copy config.yaml */
    char src[1024], dst[1024];
    snprintf(src, sizeof(src), "%s/config.yaml", home);
    snprintf(dst, sizeof(dst), "%s/config.yaml", backup_path);
    if (access(src, F_OK) == 0) {
        mkdir(backup_path, 0700);
        FILE *in = fopen(src, "rb");
        FILE *out = fopen(dst, "wb");
        if (in && out) {
            char buf[8192];
            size_t n;
            size_t total = 0;
            while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
                if (fwrite(buf, 1, n, out) != n) break;
                total += n;
            }
            fclose(in); fclose(out);
            printf("  [config]    %s/config.yaml (%zu bytes)\n", backup_path, total);
        } else {
            if (in) fclose(in);
            if (out) fclose(out);
            printf("  [config]    Error backing up config.yaml\n");
        }
    } else {
        printf("  [config]    No config.yaml found — skipped\n");
    }

    /* Copy .env */
    snprintf(src, sizeof(src), "%s/.env", home);
    snprintf(dst, sizeof(dst), "%s/.env", backup_path);
    if (access(src, F_OK) == 0) {
        FILE *in = fopen(src, "rb");
        FILE *out = fopen(dst, "wb");
        if (in && out) {
            char buf[8192];
            size_t n;
            size_t total = 0;
            while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
                if (fwrite(buf, 1, n, out) != n) break;
                total += n;
            }
            fclose(in); fclose(out);
            printf("  [.env]      %s/.env (%zu bytes)\n", backup_path, total);
        } else {
            if (in) fclose(in);
            if (out) fclose(out);
            printf("  [.env]      Error backing up .env\n");
        }
    } else {
        printf("  [.env]      No .env found — skipped\n");
    }

    if (full) {
        /* Copy sessions directory */
        char sessions_src[1024], sessions_dst[1024];
        snprintf(sessions_src, sizeof(sessions_src), "%s/sessions", home);
        snprintf(sessions_dst, sizeof(sessions_dst), "%s/sessions", backup_path);
        if (access(sessions_src, F_OK) == 0) {
            mkdir(sessions_dst, 0700);
            DIR *d = opendir(sessions_src);
            if (d) {
                struct dirent *entry;
                int count = 0;
                while ((entry = readdir(d)) != NULL) {
                    if (entry->d_type != DT_REG) continue;
                    char sf[1024], df[1024];
                    snprintf(sf, sizeof(sf), "%s/%s", sessions_src, entry->d_name);
                    snprintf(df, sizeof(df), "%s/%s", sessions_dst, entry->d_name);
                    FILE *in = fopen(sf, "rb");
                    FILE *out = fopen(df, "wb");
                    if (in && out) {
                        char buf[8192];
                        size_t n;
                        while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
                            if (fwrite(buf, 1, n, out) != n) break;
                        fclose(in); fclose(out);
                        count++;
                    } else {
                        if (in) fclose(in);
                        if (out) fclose(out);
                    }
                }
                closedir(d);
                printf("  [sessions]  %d session file(s) backed up\n", count);
            }
        } else {
            printf("  [sessions]  No sessions directory — skipped\n");
        }

        /* Copy plugins directory */
        char plugins_src[1024], plugins_dst[1024];
        snprintf(plugins_src, sizeof(plugins_src), "%s/plugins", home);
        snprintf(plugins_dst, sizeof(plugins_dst), "%s/plugins", backup_path);
        if (access(plugins_src, F_OK) == 0) {
            mkdir(plugins_dst, 0700);
            DIR *d = opendir(plugins_src);
            if (d) {
                struct dirent *entry;
                int count = 0;
                while ((entry = readdir(d)) != NULL) {
                    if (entry->d_type != DT_REG && entry->d_type != DT_LNK) continue;
                    char sf[1024], df[1024];
                    snprintf(sf, sizeof(sf), "%s/%s", plugins_src, entry->d_name);
                    snprintf(df, sizeof(df), "%s/%s", plugins_dst, entry->d_name);
                    FILE *in = fopen(sf, "rb");
                    FILE *out = fopen(df, "wb");
                    if (in && out) {
                        char buf[8192];
                        size_t n;
                        while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
                            if (fwrite(buf, 1, n, out) != n) break;
                        fclose(in); fclose(out);
                        count++;
                    } else {
                        if (in) fclose(in);
                        if (out) fclose(out);
                    }
                }
                closedir(d);
                printf("  [plugins]   %d plugin file(s) backed up\n", count);
            }
        } else {
            printf("  [plugins]   No plugins directory — skipped\n");
        }
    }

    printf("\n=== Backup complete ===\n");
    printf("  Location: %s/\n", backup_path);
    printf("  To restore, copy files back to %s/\n", home);
    printf("  Then run: slermes /restore %s\n", backup_path);
}

/* /config: Full command with groups and set support */
void cmd_config(const char *args, agent_state_t *state) {
    /* Load config to access full config struct */
    hermes_config_t cfg;
    if (!hermes_config_load(&cfg, state->hermes_home)) {
        printf("Failed to load configuration.\n");
        return;
    }

    if (!args || !args[0]) {
        /* Show summary */
        printf("Configuration (config_version: %d):\n",
               cfg.config_version > 0 ? cfg.config_version : HERMES_CONFIG_VERSION);
        printf("  model:          %s\n", cfg.provider_cfg.model);
        printf("  provider:       %s\n", cfg.provider_cfg.provider);
        printf("  base_url:       %s\n", cfg.provider_cfg.base_url);
        printf("  api_mode:       %s\n", cfg.provider_cfg.api_mode);
        printf("  max_tokens:     %d\n", cfg.provider_cfg.max_tokens);
        printf("  temperature:    %.1f\n", (double)cfg.provider_cfg.temperature);
        printf("  top_p:          %.1f\n", (double)cfg.provider_cfg.top_p);
        printf("  display.skin:   %s\n", cfg.display.skin[0] ? cfg.display.skin : "(default)");
        printf("  display.stream: %s\n", cfg.display.stream ? "yes" : "no");
        printf("  agent.turns:    %d\n", cfg.agent.max_iterations);
        printf("  agent.verbose:  %d\n", cfg.agent.verbose_level);
        printf("  approvals.mode: %s\n", cfg.tools.approval_mode);
        printf("  terminal.timeout: %d\n", cfg.tools.terminal_timeout);
        return;
    }

    /* Subcommands */
    if (strcmp(args, "validate") == 0 || strcmp(args, "-v") == 0) {
        config_validation_t result;
        bool valid = hermes_config_validate(&cfg, &result);
        if (valid) {
            printf("Configuration valid.\n");
        } else {
            printf("Configuration issues (%d):\n", result.count);
            for (int i = 0; i < result.count; i++)
                printf("  [%s] %s\n", result.issues[i].key, result.issues[i].message);
        }
        return;
    }

    if (strcmp(args, "diff") == 0) {
        cfg_diff_t diff;
        if (hermes_config_diff(&cfg, &diff)) {
            printf("Differences from defaults (%d):\n", diff.count);
            for (int i = 0; i < diff.count; i++) {
                const char *type_str;
                switch (diff.entries[i].type) {
                    case CFG_DIFF_ADDED:   type_str = "+"; break;
                    case CFG_DIFF_CHANGED: type_str = "~"; break;
                    case CFG_DIFF_MISSING: type_str = "-"; break;
                    default:               type_str = "?"; break;
                }
                printf("  %s %s: \"%s\" -> \"%s\"\n", type_str,
                       diff.entries[i].key,
                       diff.entries[i].default_value,
                       diff.entries[i].active_value);
            }
        } else {
            printf("Configuration matches defaults.\n");
        }
        return;
    }

    if (strcmp(args, "export") == 0) {
        hermes_config_export(&cfg, NULL);
        return;
    }

    if (strcmp(args, "migrate") == 0) {
        if (hermes_config_migrate(&cfg, state->hermes_home)) {
            printf("Config migrated to v%d.\n", HERMES_CONFIG_VERSION);
            /* Reload from migrated file */
            hermes_config_load(&cfg, state->hermes_home);
        } else {
            printf("Config already at v%d. No migration needed.\n", HERMES_CONFIG_VERSION);
        }
        return;
    }

    if (strcmp(args, "groups") == 0) {
        list_config_groups();
        return;
    }

    if (strcmp(args, "schema") == 0) {
        char *schema = hermes_config_schema();
        if (schema) {
            printf("Config schema (JSON Schema draft-07):\n%s\n", schema);
            free(schema);
        } else {
            printf("Failed to generate config schema.\n");
        }
        return;
    }

    if (strncmp(args, "profile ", 8) == 0) {
        const char *profile_name = args + 8;
        if (strcmp(profile_name, "list") == 0) {
            /* List available profiles */
            char profiles_dir[HERMES_PATH_MAX];
            hermes_resolve_path("profiles", profiles_dir, sizeof(profiles_dir));
            DIR *d = opendir(profiles_dir);
            if (!d) {
                printf("No profiles directory found at %s\n", profiles_dir);
                return;
            }
            printf("Available profiles:\n");
            struct dirent *de;
            int count = 0;
            while ((de = readdir(d)) != NULL) {
                size_t len = strlen(de->d_name);
                if (len > 5 && strcmp(de->d_name + len - 5, ".yaml") == 0) {
                    de->d_name[len - 5] = '\0';
                    printf("  %s\n", de->d_name);
                    count++;
                }
            }
            closedir(d);
            if (count == 0) printf("  (none)\n");
            return;
        }

        /* profile clone <name> --from <source> */
        if (strncmp(profile_name, "clone ", 6) == 0) {
            const char *clone_name = profile_name + 6;
            const char *from_name = NULL;
            /* Parse --from <source> */
            const char *from_arg = strstr(clone_name, " --from ");
            char clone_buf[256] = "";
            if (from_arg) {
                from_name = from_arg + 8;
                /* Terminate clone_name at --from */
                size_t len = (size_t)(from_arg - clone_name);
                if (len >= sizeof(clone_buf)) len = sizeof(clone_buf) - 1;
                memcpy(clone_buf, clone_name, len);
                clone_buf[len] = '\0';
                clone_name = clone_buf;
            }

            if (!clone_name || !clone_name[0]) {
                printf("Usage: /config profile clone <name> --from <source>\n");
                return;
            }

            char profiles_dir[HERMES_PATH_MAX];
            hermes_resolve_path("profiles", profiles_dir, sizeof(profiles_dir));

            /* Determine source profile */
            const char *src = from_name ? from_name : "default";
            char src_path[HERMES_PATH_MAX];
            snprintf(src_path, sizeof(src_path), "%s/%s.yaml", profiles_dir, src);

            /* Check source exists */
            struct stat st;
            if (stat(src_path, &st) != 0) {
                printf("Source profile '%s' not found at %s\n", src, src_path);
                return;
            }

            /* Check target doesn't already exist */
            char dst_path[HERMES_PATH_MAX];
            snprintf(dst_path, sizeof(dst_path), "%s/%s.yaml", profiles_dir, clone_name);
            if (stat(dst_path, &st) == 0) {
                printf("Profile '%s' already exists. Delete it first or use a different name.\n", clone_name);
                return;
            }

            /* Copy the file */
            FILE *src_f = fopen(src_path, "r");
            if (!src_f) {
                printf("Failed to read source profile '%s'\n", src);
                return;
            }
            FILE *dst_f = fopen(dst_path, "w");
            if (!dst_f) {
                fclose(src_f);
                printf("Failed to create profile '%s'\n", clone_name);
                return;
            }
            char buf[8192];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), src_f)) > 0) {
                fwrite(buf, 1, n, dst_f);
            }
            fclose(src_f);
            fclose(dst_f);
            printf("Profile '%s' cloned from '%s'.\n", clone_name, src);
            return;
        }

        /* profile delete <name> */
        if (strncmp(profile_name, "delete ", 7) == 0) {
            const char *del_name = profile_name + 7;
            if (!del_name || !del_name[0]) {
                printf("Usage: /config profile delete <name>\n");
                return;
            }
            char profiles_dir[HERMES_PATH_MAX];
            hermes_resolve_path("profiles", profiles_dir, sizeof(profiles_dir));
            char del_path[HERMES_PATH_MAX];
            snprintf(del_path, sizeof(del_path), "%s/%s.yaml", profiles_dir, del_name);

            struct stat st;
            if (stat(del_path, &st) != 0) {
                printf("Profile '%s' not found.\n", del_name);
                return;
            }
            if (unlink(del_path) != 0) {
                printf("Failed to delete profile '%s'.\n", del_name);
                return;
            }
            printf("Profile '%s' deleted.\n", del_name);
            return;
        }

        /* Load named profile */
        hermes_config_t pcfg;
        if (!hermes_config_load_profile(&pcfg, profile_name, state->hermes_home)) {
            printf("Profile '%s' not found. Use /config profile list to see available profiles.\n", profile_name);
            return;
        }
        hermes_set_profile(profile_name);
        printf("Profile '%s' activated (takes effect on next run).\n", profile_name);
        return;
    }

    /* Show specific section */
    if (strncmp(args, "show ", 5) == 0) {
        const char *section = args + 5;
        if (!show_config_section(&cfg, section))
            printf("Unknown section: %s. Use /config groups to list all.\n", section);
        return;
    }

    /* Get a single key */
    if (strncmp(args, "get ", 4) == 0) {
        const char *key = args + 4;
        /* Show the section the key belongs to */
        if (strstr(key, "model") == key || strcmp(key, "provider") == 0)
            show_section_model(&cfg);
        else if (strstr(key, "display") == key)
            show_section_display(&cfg);
        else if (strstr(key, "agent") == key)
            show_section_agent(&cfg);
        else if (strstr(key, "tools") == key || strstr(key, "approvals") == key || strstr(key, "terminal") == key)
            show_section_tools(&cfg);
        else if (strstr(key, "delegation") == key)
            show_section_delegation(&cfg);
        else if (strstr(key, "memory") == key)
            show_section_memory(&cfg);
        else if (strstr(key, "compression") == key)
            show_section_compression(&cfg);
        else if (strstr(key, "security") == key)
            show_section_security(&cfg);
        else if (strstr(key, "cron") == key)
            show_section_cron(&cfg);
        else if (strstr(key, "notification") == key)
            show_section_notification(&cfg);
        else if (strstr(key, "browser") == key)
            show_section_browser(&cfg);
        else if (strstr(key, "sessions") == key)
            show_section_sessions(&cfg);
        else if (strstr(key, "plugin") == key)
            show_section_plugin(&cfg);
        else if (strstr(key, "mcp") == key)
            show_section_mcp(&cfg);
        else
            printf("Unknown key: %s\n", key);
        return;
    }

    /* Set a key=value */
    if (strncmp(args, "set ", 4) == 0) {
        config_set_key(&cfg, args + 4);
        return;
    }

    printf("Usage: /config [validate|diff|export|migrate|groups|schema|profile <name>|profile list|profile clone <name> --from <source>|profile delete <name>|show <group>|get <key>|set <key>=<value>]\n");
}

void cmd_fast(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0] || strcmp(args, "status") == 0) {
        printf("Fast mode: %s\n", g_fast_mode ? "FAST" : "NORMAL");
        printf("  Usage: /fast [normal|fast|status]\n");
        return;
    }
    if (strcmp(args, "fast") == 0 || strcmp(args, "on") == 0) {
        g_fast_mode = 1;
        printf("Fast mode enabled.\n");
    } else if (strcmp(args, "normal") == 0 || strcmp(args, "off") == 0) {
        g_fast_mode = 0;
        printf("Fast mode disabled.\n");
    } else {
        printf("Unknown argument: %s\n", args);
        printf("  Usage: /fast [normal|fast|status]\n");
    }
}

void cmd_footer(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0] || strcmp(args, "status") == 0) {
        printf("Footer: %s\n", g_footer_on ? "shown" : "hidden");
        printf("  Usage: /footer [on|off|status]\n");
        return;
    }
    if (strcmp(args, "on") == 0) {
        g_footer_on = 1;
        printf("Footer shown.\n");
    } else if (strcmp(args, "off") == 0) {
        g_footer_on = 0;
        printf("Footer hidden.\n");
    } else {
        printf("Unknown argument: %s\n", args);
        printf("  Usage: /footer [on|off|status]\n");
    }
}

void cmd_model(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        /* No args: show current model */
        printf("Model:        %s\n", state->llm.model);
        printf("Provider:     %s\n", state->llm.provider[0] ? state->llm.provider : "(auto)");
        printf("Base URL:     %s\n", state->llm.base_url[0] ? state->llm.base_url : "(default)");
        printf("Max turns:    %d\n", state->max_iterations);
        printf("Tools:        %zu available\n", state->tools.count);
        printf("Usage: /model list [--cap NAME] | /model show <name> | /model providers | /model set <name>\n");
        return;
    }

    /* Parse subcommand */
    char arg_copy[256];
    snprintf(arg_copy, sizeof(arg_copy), "%s", args);
    const char *cmd = strtok(arg_copy, " ");
    if (!cmd) {
        printf("Usage: /model list [--cap NAME] | /model show <name> | /model providers | /model set <name>\n");
        return;
    }

    if (strcmp(cmd, "list") == 0 || strcmp(cmd, "ls") == 0) {
        /* /model list [--cap NAME] */
        const char *cap_str = strstr(args, "--cap");
        if (cap_str && cap_str[6]) {
            cap_str += 6;
            while (*cap_str == '=' || *cap_str == ' ') cap_str++;
            model_capability_t caps = model_capability_parse(cap_str);
            char *json = model_metadata_list_filtered_json(caps);
            if (json) {
                printf("Models with capability '%s':\n%s\n", cap_str, json);
                free(json);
            }
        } else {
            char *json = model_metadata_list_json();
            if (json) {
                printf("Known models:\n%s\n", json);
                free(json);
            }
        }
        return;
    }

    if (strcmp(cmd, "show") == 0 || strcmp(cmd, "info") == 0) {
        const char *name = strtok(NULL, " ");
        if (!name) {
            printf("Usage: /model show <model_name>\n");
            return;
        }
        const model_metadata_t *m = model_metadata_find(name);
        if (!m) {
            printf("Model '%s' not found in local metadata.\n", name);
            return;
        }
        char caps_str[128];
        model_capability_format(m->caps, caps_str, sizeof(caps_str));
        printf("Model:        %s\n", m->model_prefix);
        printf("Family:       %s\n", m->family);
        printf("Context:      %d tokens\n", m->context_window);
        printf("Max output:   %d tokens\n", m->max_output);
        printf("Capabilities: %s\n", caps_str[0] ? caps_str : "(none)");
        printf("Pricing:      $%.4f/$%.4f per 1M in/out\n", m->input_per_1m, m->output_per_1m);
        return;
    }

    if (strcmp(cmd, "providers") == 0 || strcmp(cmd, "prov") == 0) {
        char *json = provider_metadata_list_json();
        if (json) {
            printf("Known providers:\n%s\n", json);
            free(json);
        }
        return;
    }

    if (strcmp(cmd, "set") == 0) {
        const char *model_name = strtok(NULL, " ");
        if (!model_name) {
            printf("Usage: /model set <model_name>\n");
            return;
        }
        snprintf(state->llm.model, sizeof(state->llm.model), "%s", model_name);
        printf("Model set to: %s\n", state->llm.model);
        return;
    }

    /* Unknown subcommand */
    printf("Unknown: /model %s\n", cmd);
    printf("Usage: /model list [--cap NAME] | /model show <name> | /model providers | /model set <name>\n");
}

/* /personality: Set a predefined personality system message */
void cmd_personality(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /personality <system prompt text>\n");
        printf("Sets or replaces the system message (personality).\n");
        return;
    }
    context_set_system(state, args);
    printf("Personality set.\n");
}

/* /reasoning: Manage reasoning effort */
void cmd_reasoning(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /reasoning [level|show|hide]\n");
        printf("Levels: none, minimal, low, medium, high, xhigh, on, off\n");
        printf("Current: %s\n",
               state->llm.reasoning_effort[0] ? state->llm.reasoning_effort : "(default)");
        return;
    }

    /* Handle special commands */
    if (strcmp(args, "show") == 0) {
        printf("Current reasoning effort: %s\n",
               state->llm.reasoning_effort[0] ? state->llm.reasoning_effort : "(not set, provider default)");
        return;
    }
    if (strcmp(args, "hide") == 0) {
        printf("Reasoning display hidden (content still sent to provider if configured).\n");
        return;
    }

    /* Map common aliases to canonical values */
    const char *value = args;
    if (strcmp(args, "on") == 0) value = "medium";
    else if (strcmp(args, "off") == 0) value = "none";

    /* Validate value */
     const char *valid[] = {"none", "minimal", "low", "medium", "high", "xhigh", NULL};
    bool ok = false;
    for (int i = 0; valid[i]; i++) {
        if (strcmp(value, valid[i]) == 0) { ok = true; break; }
    }
    if (!ok) {
        printf("Invalid reasoning level: %s\n", args);
        printf("Valid: none, minimal, low, medium, high, xhigh, on, off\n");
        return;
    }

    strncpy(state->llm.reasoning_effort, value, sizeof(state->llm.reasoning_effort) - 1);
    state->llm.reasoning_effort[sizeof(state->llm.reasoning_effort) - 1] = '\0';
    printf("Reasoning effort set to: %s\n", value);
}

/* /setup: Interactive setup wizard — port of Python hermes_cli/setup.py */
/* Usage: /setup [model|tts|terminal|gateway|tools|agent] [--quick] [--non-interactive] [--reset] [--reconfigure] [--portal] */
void cmd_setup(const char *args, agent_state_t *state) {
    const char *home = state->hermes_home;
    if (!home || !home[0]) {
        home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
    }

    /* Parse args for flags and section */
    bool non_interactive = false;
    bool quick_mode = false;
    bool reset_mode = false;
    bool portal_mode = false;
    bool reconfigure = false;
    char section[32] = "";

    if (args && args[0]) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s", args);
        char *save = NULL;
        const char *tok = strtok_r(buf, " ", &save);
        while (tok) {
            if (strcmp(tok, "--non-interactive") == 0 || strcmp(tok, "-n") == 0)
                non_interactive = true;
            else if (strcmp(tok, "--quick") == 0 || strcmp(tok, "-q") == 0)
                quick_mode = true;
            else if (strcmp(tok, "--reset") == 0 || strcmp(tok, "-r") == 0)
                reset_mode = true;
            else if (strcmp(tok, "--reconfigure") == 0)
                reconfigure = true;
            else if (strcmp(tok, "--portal") == 0 || strcmp(tok, "-p") == 0)
                portal_mode = true;
            else if (strcmp(tok, "model") == 0 || strcmp(tok, "tts") == 0 ||
                     strcmp(tok, "terminal") == 0 || strcmp(tok, "gateway") == 0 ||
                     strcmp(tok, "tools") == 0 || strcmp(tok, "agent") == 0)
                snprintf(section, sizeof(section), "%s", tok);
            tok = strtok_r(NULL, " ", &save);
        }
    }

    /* Handle --portal: one-shot Nous Portal setup */
    if (portal_mode) {
        hermes_config_setup_portal();
        return;
    }

    /* Handle --reset: delete config */
    if (reset_mode) {
        char path[4096];
        snprintf(path, sizeof(path), "%s/config.yaml", home);
        if (unlink(path) == 0)
            printf("✅ Config reset. Run 'slermes setup' to reconfigure.\n");
        else
            printf("No config to reset.\n");
        return;
    }

    /* Handle section-specific setup */
    if (section[0]) {
        hermes_config_setup_section(home, section);
        return;
    }

    /* Handle non-interactive: generate config from env vars */
    if (non_interactive) {
        hermes_config_setup_noninteractive(home);
        return;
    }

    /* Handle --quick: only prompt for missing/unset items */
    if (quick_mode) {
        hermes_config_setup_quick(home);
        return;
    }

    /* Handle --reconfigure: show current values as defaults */
    if (reconfigure) {
        hermes_config_setup_interactive(home);
        return;
    }

    /* Default: full interactive wizard */
    hermes_config_setup_interactive(home);
}

void cmd_topic(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /topic <system prompt text>\n");
        return;
    }
    /* Set or replace system message */
    if (state->message_count > 0 && state->messages[0]->role == MSG_SYSTEM) {
        /* Replace existing system message */
        free(state->messages[0]->content);
        state->messages[0]->content = strdup(args);
    } else {
        /* Insert new system message at front */
        message_t *sys = (message_t *)calloc(1, sizeof(message_t));
        if (sys) {
            sys->role = MSG_SYSTEM;
            sys->content = strdup(args);
            /* Shift messages right */
            for (size_t i = state->message_count; i > 0; i--)
                state->messages[i] = state->messages[i - 1];
            state->messages[0] = sys;
            state->message_count++;
        }
    }
    printf("Topic set.\n");
}

/* ── Uninstall ───────────────────────────────────────────────── */
/* AG26: Port of Python hermes_cli/uninstall.py:uninstall().
 * AG26: Port of Python hermes_cli/main.py:cmd_uninstall().
 */
void cmd_uninstall(const char *args, agent_state_t *state) {
    (void)args;
    const char *home = state->hermes_home;
    if (!home || !home[0]) {
        home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
    }

    printf("\n=== Slermes Uninstall ===\n\n");

    /* Step 1: Find and remove binary */
    const char *paths[] = {
        "/usr/local/bin/slermes",
        "/usr/bin/slermes",
        NULL
    };
    char local_path[1024];
    const char *local_bin = getenv("HOME");
    if (local_bin) {
        snprintf(local_path, sizeof(local_path), "%s/.local/bin/slermes", local_bin);
    }
    int found = 0;
    for (int i = 0; paths[i]; i++) {
        if (access(paths[i], F_OK) == 0) {
            if (remove(paths[i]) == 0) {
                printf("  Removed: %s\n", paths[i]);
                found++;
            } else {
                printf("  Error removing %s (permission denied?)\n", paths[i]);
            }
        }
    }
    if (local_bin && access(local_path, F_OK) == 0) {
        if (remove(local_path) == 0) {
            printf("  Removed: %s\n", local_path);
            found++;
        } else {
            printf("  Error removing %s\n", local_path);
        }
    }
    if (found == 0) {
        printf("  Binary not found — already clean.\n");
    }

    /* Step 2: Check for config and .env */
    char cfg_path[1024];
    char env_path[1024];
    snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", home);
    snprintf(env_path, sizeof(env_path), "%s/.env", home);

    bool has_cfg = (access(cfg_path, F_OK) == 0);
    bool has_env = (access(env_path, F_OK) == 0);

    printf("\n  Config:     %s %s\n", cfg_path, has_cfg ? "EXISTS" : "not found");
    printf("  .env:       %s %s\n", env_path, has_env ? "EXISTS" : "not found");

    if (has_cfg || has_env) {
        printf("\n  To remove config and .env, run: slermes -c 'rm %s/config.yaml %s/.env'\n", home, home);
        printf("  Or manually delete:\n");
        if (has_cfg) printf("    rm %s\n", cfg_path);
        if (has_env) printf("    rm %s\n", env_path);
    }

    printf("\n=== Uninstall complete ===\n");
    printf("  Skills, plugins, and sessions preserved in %s/\n", home);
    printf("  To fully remove all data: rm -rf %s/.hermes\n", home);
}

void cmd_voice(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Voice CLI — mode and configuration\n");
        printf("Usage: /voice status          — Show voice mode + config\n");
        printf("       /voice on              — Enable voice input/output\n");
        printf("       /voice off             — Disable voice mode\n");
        printf("       /voice tts             — Enable TTS output mode\n");
        printf("       /voice config          — Show voice config settings\n");
        printf("       /voice key <binding>   — Set record key (e.g. ctrl+b)\n");
        return;
    }
    const char *sub = args;
    while (*sub == ' ') sub++;

    if (strcmp(sub, "status") == 0 || strcmp(sub, "st") == 0) {
        printf("Voice mode: %s\n", g_voice_mode ? "ENABLED" : "DISABLED");
        hermes_config_t cfg;
        char home_dir[1024];
        memset(home_dir, 0, sizeof(home_dir));
        if (state->hermes_home[0]) {
            snprintf(home_dir, sizeof(home_dir), "%s", state->hermes_home);
        } else {
            const char *home_env = getenv("SLERMES_HOME");
            if (!home_env) home_env = getenv("HOME");
            if (home_env) {
                snprintf(home_dir, sizeof(home_dir), "%s/.slermes", home_env);
                if (access(home_dir, F_OK) != 0)
                    snprintf(home_dir, sizeof(home_dir), "%s/.hermes", home_env);
            }
        }
        if (home_dir[0] && access(home_dir, F_OK) == 0 && hermes_config_load(&cfg, home_dir)) {
            printf("\nVoice config:\n");
            show_cfg_val("record_key", "str", cfg.voice.record_key);
            char key_buf[64] = "Ctrl+B"; /* Default display */
            if (cfg.voice.record_key[0]) {
                /* Format key binding: "ctrl+b" -> "Ctrl+B" */
                if (strncmp(cfg.voice.record_key, "c-", 2) == 0)
                    snprintf(key_buf, sizeof(key_buf), "Ctrl+%c", toupper((unsigned char)cfg.voice.record_key[2]));
                else if (strncmp(cfg.voice.record_key, "a-", 2) == 0)
                    snprintf(key_buf, sizeof(key_buf), "Alt+%c", toupper((unsigned char)cfg.voice.record_key[2]));
            }
            printf("  record_key_formatted: %s\n", key_buf);
            show_cfg_val_int("max_recording_seconds", cfg.voice.max_recording_seconds);
            show_cfg_val_bool("auto_tts", cfg.voice.auto_tts);
            show_cfg_val_bool("beep_enabled", cfg.voice.beep_enabled);
            show_cfg_val_int("silence_threshold", cfg.voice.silence_threshold);
            show_cfg_val_float("silence_duration", cfg.voice.silence_duration);
            printf("\nTTS provider:\n");
            show_cfg_val("provider", "str", cfg.tts.provider);
        } else {
            printf("(config not loaded)\n");
        }
        return;
    }
    if (strcmp(sub, "on") == 0) {
        g_voice_mode = 1;
        printf("Voice mode ENABLED. voice_listen/voice_speak tools are available.\n");
        return;
    }
    if (strcmp(sub, "tts") == 0) {
        g_voice_mode = 1;
        printf("Voice (TTS) mode ENABLED. voice_speak tool available.\n");
        return;
    }
    if (strcmp(sub, "off") == 0) {
        g_voice_mode = 0;
        printf("Voice mode DISABLED.\n");
        return;
    }
    if (strcmp(sub, "config") == 0) {
        hermes_config_t cfg;
        char home_dir[1024];
        memset(home_dir, 0, sizeof(home_dir));
        if (state->hermes_home[0]) {
            snprintf(home_dir, sizeof(home_dir), "%s", state->hermes_home);
        } else {
            const char *home_env = getenv("SLERMES_HOME");
            if (!home_env) home_env = getenv("HOME");
            if (home_env) {
                snprintf(home_dir, sizeof(home_dir), "%s/.slermes", home_env);
                if (access(home_dir, F_OK) != 0)
                    snprintf(home_dir, sizeof(home_dir), "%s/.hermes", home_env);
            }
        }
        if (home_dir[0] && access(home_dir, F_OK) == 0 && hermes_config_load(&cfg, home_dir)) {
            show_section_voice(&cfg);
        } else {
            printf("Error: Could not load config\n");
        }
        return;
    }
    if (strncmp(sub, "key ", 4) == 0) {
        const char *binding = sub + 4;
        while (*binding == ' ') binding++;
        if (!*binding) {
            printf("Usage: /voice key <binding>\n");
            printf("  Example: /voice key ctrl+b\n");
            printf("  Format: ctrl+<key> or alt+<key>\n");
            return;
        }
        hermes_config_t cfg;
        char home_dir[1024];
        memset(home_dir, 0, sizeof(home_dir));
        if (state->hermes_home[0]) {
            snprintf(home_dir, sizeof(home_dir), "%s", state->hermes_home);
        } else {
            const char *home_env = getenv("SLERMES_HOME");
            if (!home_env) home_env = getenv("HOME");
            if (home_env) {
                snprintf(home_dir, sizeof(home_dir), "%s/.slermes", home_env);
                if (access(home_dir, F_OK) != 0)
                    snprintf(home_dir, sizeof(home_dir), "%s/.hermes", home_env);
            }
        }
        if (!home_dir[0] || access(home_dir, F_OK) != 0) {
            printf("Error: Cannot determine Hermes home\n");
            return;
        }
        if (hermes_config_load(&cfg, home_dir)) {
            printf("To set voice record key to '%s':\n", binding);
            printf("  Edit ~/.slermes/config.yaml:\n");
            printf("    voice:\n");
            printf("      record_key: %s\n", binding);
        } else {
            printf("Error: Could not load config\n");
        }
        return;
    }

    printf("Unknown argument: %s\n", sub);
    printf("Usage: /voice [on|off|tts|status|config|key <binding>]\n");
}

void cmd_yolo(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    g_yolo_mode = !g_yolo_mode;
    printf("YOLO mode %s. Dangerous command approvals will be %s.\n",
           g_yolo_mode ? "ENABLED" : "DISABLED",
           g_yolo_mode ? "skipped" : "enforced");
}

/* Set a config key: parse key=value, validate, print new value */
bool config_set_key(hermes_config_t *cfg, const char *args) {
    /* Find '=' or space delimiter */
    const char *eq = strchr(args, '=');
    if (!eq) {
        printf("Usage: /config set <key>=<value>\n");
        return false;
    }
    size_t key_len = (size_t)(eq - args);
    const char *val = eq + 1;
    while (*val == ' ') val++;

    char key[128];
    snprintf(key, sizeof(key), "%.*s", (int)key_len, args);

    /* Try to match keys and set */
    bool found = false;

    if (strcmp(key, "model") == 0 || strcmp(key, "model.default") == 0) {
        snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", val);
        found = true;
    }
    else if (strcmp(key, "provider") == 0 || strcmp(key, "model.provider") == 0) {
        snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", val);
        found = true;
    }
    else if (strcmp(key, "base_url") == 0 || strcmp(key, "model.base_url") == 0) {
        snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", val);
        found = true;
    }
    else if (strcmp(key, "api_mode") == 0 || strcmp(key, "model.api_mode") == 0) {
        snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "%s", val);
        found = true;
    }
    else if (strcmp(key, "max_tokens") == 0 || strcmp(key, "model.max_tokens") == 0) {
        int iv = atoi(val);
        if (iv > 0) { cfg->provider_cfg.max_tokens = iv; found = true; }
        else printf("Invalid integer: %s\n", val);
    }
    else if (strcmp(key, "temperature") == 0 || strcmp(key, "model.temperature") == 0) {
        float fv = (float)atof(val);
        if (fv >= 0.0f && fv <= 2.0f) { cfg->provider_cfg.temperature = fv; found = true; }
        else printf("Temperature must be 0.0-2.0\n");
    }
    else if (strcmp(key, "top_p") == 0 || strcmp(key, "model.top_p") == 0) {
        float fv = (float)atof(val);
        if (fv >= 0.0f && fv <= 1.0f) { cfg->provider_cfg.top_p = fv; found = true; }
        else printf("top_p must be 0.0-1.0\n");
    }
    else if (strcmp(key, "max_turns") == 0 || strcmp(key, "agent.max_turns") == 0) {
        int iv = atoi(val);
        if (iv > 0 && iv <= 10000) { cfg->agent.max_iterations = iv; found = true; }
        else printf("max_turns must be 1-10000\n");
    }
    else if (strcmp(key, "verbose") == 0 || strcmp(key, "agent.verbose") == 0) {
        int iv = atoi(val);
        if (iv >= 0 && iv <= 2) { cfg->agent.verbose_level = iv; found = true; }
        else printf("verbose must be 0-2\n");
    }
    else if (strcmp(key, "display.skin") == 0 || strcmp(key, "skin") == 0) {
        snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", val);
        found = true;
    }
    else if (strcmp(key, "display.streaming") == 0 || strcmp(key, "streaming") == 0) {
        cfg->display.stream = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        found = true;
    }
    else if (strcmp(key, "approvals.mode") == 0) {
        if (strcmp(val, "off") == 0 || strcmp(val, "manual") == 0 || strcmp(val, "auto") == 0) {
            snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "%s", val);
            found = true;
        } else printf("approvals.mode must be off/manual/auto\n");
    }
    else if (strcmp(key, "approvals.timeout") == 0) {
        int iv = atoi(val);
        if (iv >= 0 && iv <= 86400) { cfg->tools.approval_timeout = iv; found = true; }
        else printf("approvals.timeout must be 0-86400\n");
    }
    else if (strcmp(key, "terminal.timeout") == 0) {
        int iv = atoi(val);
        if (iv > 0 && iv <= 86400) { cfg->tools.terminal_timeout = iv; found = true; }
        else printf("terminal.timeout must be 1-86400\n");
    }
    else if (strcmp(key, "yolo") == 0) {
        cfg->yolo_mode = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        if (cfg->yolo_mode) snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "off");
        found = true;
    }

    if (!found) {
        printf("Unknown or unsupported key: %s\n", key);
        printf("Use /config groups to list available groups.\n");
        printf("Common keys: model, provider, base_url, max_tokens, temperature, top_p,\n");
        printf("  max_turns, verbose, display.skin, streaming, approvals.mode,\n");
        printf("  approvals.timeout, terminal.timeout, yolo\n");
        return false;
    }

    /* Sync flat fields */
    snprintf(cfg->model, sizeof(cfg->model), "%s", cfg->provider_cfg.model);
    snprintf(cfg->provider, sizeof(cfg->provider), "%s", cfg->provider_cfg.provider);
    snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", cfg->provider_cfg.base_url);
    cfg->max_turns = cfg->agent.max_iterations;
    cfg->verbose = cfg->agent.verbose_level;

    /* Write updated config back */
    if (hermes_config_export(cfg, cfg->config_path)) {
        printf("Set %s = %s (written to %s)\n", key, val, cfg->config_path);
    } else {
        printf("Set %s = %s (in-memory only, write failed)\n", key, val);
    }

    return true;
}

/* List all config groups */
void list_config_groups(void) {
    printf("Config groups (18):\n");
    for (int i = 0; CFG_CATEGORIES[i].name; i++)
        printf("  %-15s  %s\n", CFG_CATEGORIES[i].name, CFG_CATEGORIES[i].desc);
    printf("\nUse /config show <group> to view keys in a group.\n");
}

/* Dispatch show_section by name */
bool show_config_section(const hermes_config_t *cfg, const char *section) {
    if (strcmp(section, "model") == 0 || strcmp(section, "provider") == 0)
        { show_section_model(cfg); return true; }
    if (strcmp(section, "display") == 0)
        { show_section_display(cfg); return true; }
    if (strcmp(section, "agent") == 0)
        { show_section_agent(cfg); return true; }
    if (strcmp(section, "tools") == 0 || strcmp(section, "approvals") == 0)
        { show_section_tools(cfg); return true; }
    if (strcmp(section, "delegation") == 0)
        { show_section_delegation(cfg); return true; }
    if (strcmp(section, "browser") == 0)
        { show_section_browser(cfg); return true; }
    if (strcmp(section, "memory") == 0)
        { show_section_memory(cfg); return true; }
    if (strcmp(section, "compression") == 0)
        { show_section_compression(cfg); return true; }
    if (strcmp(section, "cron") == 0)
        { show_section_cron(cfg); return true; }
    if (strcmp(section, "notification") == 0)
        { show_section_notification(cfg); return true; }
    if (strcmp(section, "security") == 0)
        { show_section_security(cfg); return true; }
    if (strcmp(section, "sessions") == 0)
        { show_section_sessions(cfg); return true; }
    if (strcmp(section, "plugin") == 0)
        { show_section_plugin(cfg); return true; }
    if (strcmp(section, "mcp") == 0)
        { show_section_mcp(cfg); return true; }
    if (strcmp(section, "auxiliary") == 0)
        { show_section_auxiliary(cfg); return true; }
    if (strcmp(section, "tts") == 0)
        { show_section_tts(cfg); return true; }
    if (strcmp(section, "stt") == 0)
        { show_section_stt(cfg); return true; }
    if (strcmp(section, "voice") == 0)
        { show_section_voice(cfg); return true; }
    return false;
}

void show_section_agent(const hermes_config_t *cfg) {
    printf("agent:  Iterations, verbosity, system prompt\n");
    show_cfg_val_int("max_turns", cfg->agent.max_iterations);
    show_cfg_val_int("max_tool_calls_round", cfg->agent.max_tool_calls_round);
    show_cfg_val_int("max_output_tokens", cfg->agent.max_output_tokens);
    show_cfg_val_int("verbose", cfg->agent.verbose_level);
    show_cfg_val_int("api_max_retries", cfg->agent.api_max_retries);
    show_cfg_val_int("clarify_timeout", cfg->agent.clarify_timeout);
    show_cfg_val_float("compress_threshold", cfg->agent.compress_threshold);
    show_cfg_val("system_prompt", "str", cfg->agent.system_prompt);
    show_cfg_val("profile", "str", cfg->agent.profile);
    show_cfg_val("reasoning_effort", "str", cfg->agent.reasoning_effort);
    show_cfg_val_bool("fast", cfg->agent.fast_mode);
    show_cfg_val_bool("quiet", cfg->agent.quiet_mode);
}

void show_section_auxiliary(const hermes_config_t *cfg) {
    printf("auxiliary:  Auxiliary LLM routing\n");
    #define SH_AUX_TASK(task, nm) do {         printf("  " nm ":\n");         show_cfg_val("provider", "str", cfg->auxiliary.task.provider);         show_cfg_val("model", "str", cfg->auxiliary.task.model);         show_cfg_val("base_url", "str", cfg->auxiliary.task.base_url);         show_cfg_val_int("timeout", cfg->auxiliary.task.timeout);     } while(0)
    SH_AUX_TASK(vision, "vision");
    show_cfg_val_int("download_timeout", cfg->auxiliary.vision_download_timeout);
    SH_AUX_TASK(web_extract, "web_extract");
    SH_AUX_TASK(compression, "compression");
    SH_AUX_TASK(skills_hub, "skills_hub");
    SH_AUX_TASK(approval, "approval");
    SH_AUX_TASK(mcp, "mcp");
    SH_AUX_TASK(title_generation, "title_generation");
    SH_AUX_TASK(triage_specifier, "triage_specifier");
    SH_AUX_TASK(kanban_decomposer, "kanban_decomposer");
    SH_AUX_TASK(profile_describer, "profile_describer");
    SH_AUX_TASK(curator, "curator");
    #undef SH_AUX_TASK
}

void show_section_browser(const hermes_config_t *cfg) {
    printf("browser:  CDP engine, viewport, timeout\n");
    show_cfg_val("cdp_url", "str", cfg->browser_cfg.cdp_url);
    show_cfg_val("engine", "str", cfg->browser_cfg.browser_type);
    show_cfg_val_bool("headless", cfg->browser_cfg.headless);
    show_cfg_val_bool("javascript", cfg->browser_cfg.enable_javascript);
    show_cfg_val_int("viewport_width", cfg->browser_cfg.viewport_width);
    show_cfg_val_int("viewport_height", cfg->browser_cfg.viewport_height);
    show_cfg_val_int("command_timeout", cfg->browser_cfg.timeout);
}

void show_section_compression(const hermes_config_t *cfg) {
    printf("compression:  Strategy, threshold, min messages\n");
    show_cfg_val("model", "str", cfg->compression.model);
    show_cfg_val("strategy", "str", cfg->compression.strategy);
    show_cfg_val_float("target_ratio", cfg->compression.target_ratio);
    show_cfg_val_int("min_messages", cfg->compression.min_messages);
    show_cfg_val_bool("preserve_system", cfg->compression.preserve_system);
    show_cfg_val_int("protect_last_n", cfg->compression.protect_last_n);
    show_cfg_val_int("protect_first_n", cfg->compression.protect_first_n);
    show_cfg_val_int("hygiene_hard_message_limit", cfg->compression.hygiene_hard_message_limit);
    show_cfg_val_bool("abort_on_summary_failure", cfg->compression.abort_on_summary_failure);
}

void show_section_cron(const hermes_config_t *cfg) {
    printf("cron:  Scheduler directory, job limits, retention\n");
    show_cfg_val("dir", "str", cfg->cron.dir);
    show_cfg_val_int("max_concurrent_jobs", cfg->cron.max_concurrent_jobs);
    show_cfg_val_int("job_timeout", cfg->cron.job_timeout);
    show_cfg_val_int("retention_days", cfg->cron.retention_days);
    show_cfg_val_bool("notify_on_failure", cfg->cron.notify_on_failure);
}

void show_section_delegation(const hermes_config_t *cfg) {
    printf("delegation:  Subagent spawning and child config\n");
    show_cfg_val_int("max_concurrent_children", cfg->delegation.max_concurrent_children);
    show_cfg_val_int("max_spawn_depth", cfg->delegation.max_spawn_depth);
    show_cfg_val_int("child_timeout", cfg->delegation.child_timeout);
    show_cfg_val_int("child_max_turns", cfg->delegation.child_max_turns);
    show_cfg_val("child_model", "str", cfg->delegation.child_model);
    show_cfg_val("child_provider", "str", cfg->delegation.child_provider);
}

void show_section_display(const hermes_config_t *cfg) {
    printf("display:  UI theme, skin, streaming, language\n");
    show_cfg_val("skin", "str", cfg->display.skin);
    show_cfg_val("banner", "str", cfg->display.banner);
    show_cfg_val("spinner", "str", cfg->display.spinner_style);
    show_cfg_val("indicator", "str", cfg->display.indicator);
    show_cfg_val("language", "str", cfg->display.language);
    show_cfg_val("personality", "str", cfg->display.personality);
    show_cfg_val("footer", "str", cfg->display.footer);
    show_cfg_val_bool("streaming", cfg->display.stream);
    show_cfg_val_bool("show_reasoning", cfg->display.show_reasoning);
    show_cfg_val_bool("compact", cfg->display.compact);
    show_cfg_val_bool("show_cost", cfg->display.show_cost);
    show_cfg_val_bool("timestamps", cfg->display.timestamps);
    show_cfg_val_bool("statusbar", cfg->display.statusbar);
}

void show_section_mcp(const hermes_config_t *cfg) {
    printf("mcp:  Server timeout, auth, max tools\n");
    show_cfg_val_int("timeout", cfg->mcp.timeout);
    show_cfg_val_int("max_tools", cfg->mcp.max_tools);
    show_cfg_val_bool("auth_enabled", cfg->mcp.auth_enabled);
}

/* Show all keys in a config group */
void show_section_model(const hermes_config_t *cfg) {
    printf("model:  Provider, model, API connection\n");
    show_cfg_val("default", "str", cfg->provider_cfg.model);
    show_cfg_val("provider", "str", cfg->provider_cfg.provider);
    show_cfg_val("base_url", "str", cfg->provider_cfg.base_url);
    show_cfg_val("api_mode", "str", cfg->provider_cfg.api_mode);
    show_cfg_val("fallback_model", "str", cfg->provider_cfg.fallback_model);
    show_cfg_val("fallback_providers", "str", cfg->provider_cfg.fallback_providers);
    show_cfg_val("service_tier", "str", cfg->provider_cfg.service_tier);
    show_cfg_val("reasoning_effort", "str", cfg->provider_cfg.reasoning_effort);
    show_cfg_val("default_aux_model", "str", cfg->provider_cfg.default_aux_model);
    show_cfg_val_int("max_tokens", cfg->provider_cfg.max_tokens);
    show_cfg_val_float("temperature", cfg->provider_cfg.temperature);
    show_cfg_val_float("top_p", cfg->provider_cfg.top_p);
}

void show_section_notification(const hermes_config_t *cfg) {
    printf("notification:  Provider, event triggers\n");
    show_cfg_val("provider", "str", cfg->notification.provider);
    show_cfg_val("sound", "str", cfg->notification.sound);
    show_cfg_val_bool("on_complete", cfg->notification.on_complete);
    show_cfg_val_bool("on_error", cfg->notification.on_error);
    show_cfg_val_bool("on_approval", cfg->notification.on_approval);
}

void show_section_plugin(const hermes_config_t *cfg) {
    printf("plugin:  Directories, enabled plugins\n");
    show_cfg_val("dirs", "str", cfg->plugin.dirs);
    show_cfg_val("enabled", "str", cfg->plugin.enabled);
}

void show_section_security(const hermes_config_t *cfg) {
    printf("security:  Tirith, URL safety, redaction\n");
    show_cfg_val("tirith_path", "str", cfg->security.tirith_path);
    show_cfg_val("redact_patterns", "str", cfg->security.redact_patterns);
    show_cfg_val_int("tirith_timeout", cfg->security.tirith_timeout);
    show_cfg_val_bool("tirith_enabled", cfg->security.tirith_enabled);
    show_cfg_val_bool("allow_private_urls", cfg->security.allow_private_urls);
    show_cfg_val_bool("website_blocklist_enabled", cfg->security.website_blocklist_enabled);
}

void show_section_sessions(const hermes_config_t *cfg) {
    printf("sessions:  DB path, retention, auto-save\n");
    show_cfg_val("db_path", "str", cfg->session.db_path);
    show_cfg_val_int("retention_days", cfg->session.retention_days);
    show_cfg_val_int("auto_save_interval", cfg->session.auto_save_interval);
    show_cfg_val_bool("compress", cfg->session.compress);
    show_cfg_val_bool("store_trajectories", cfg->session.store_trajectories);
}

void show_section_stt(const hermes_config_t *cfg) {
    printf("stt:  Speech-to-text configuration\n");
    show_cfg_val_bool("enabled", cfg->stt.enabled);
    show_cfg_val("provider", "str", cfg->stt.provider);
    show_cfg_val("local.model", "str", cfg->stt.local_model);
    show_cfg_val("local.language", "str", cfg->stt.local_language);
    show_cfg_val("openai.model", "str", cfg->stt.openai_model);
    show_cfg_val("mistral.model", "str", cfg->stt.mistral_model);
}

void show_section_tools(const hermes_config_t *cfg) {
    printf("tools:  Terminal, approvals, vision, tool output\n");
    show_cfg_val("approval_mode", "str", cfg->tools.approval_mode);
    show_cfg_val_int("approval_timeout", cfg->tools.approval_timeout);
    show_cfg_val_int("terminal_timeout", cfg->tools.terminal_timeout);
    show_cfg_val_int("max_result_size", cfg->tools.max_result_size);
    show_cfg_val_int("vision_timeout", cfg->tools.vision_timeout);
    show_cfg_val("vision_model", "str", cfg->tools.vision_model);
    show_cfg_val("terminal_backend", "str", cfg->tools.terminal_backend);
    show_cfg_val_bool("persistent_shell", cfg->tools.persistent_shell);
    show_cfg_val("web_backend", "str", cfg->tools.web_backend);
    show_cfg_val("web_search_backend", "str", cfg->tools.web_search_backend);
    show_cfg_val("web_extract_backend", "str", cfg->tools.web_extract_backend);
    show_cfg_val_int("web_search_timeout", cfg->tools.web_search_timeout);
    show_cfg_val_bool("allow_background", cfg->tools.allow_background);
    show_cfg_val_bool("local_process_cleanup", cfg->tools.local_process_cleanup);
}

void show_section_tts(const hermes_config_t *cfg) {
    printf("tts:  Text-to-speech configuration\n");
    show_cfg_val("provider", "str", cfg->tts.provider);
    show_cfg_val("edge.voice", "str", cfg->tts.edge_voice);
    show_cfg_val("elevenlabs.voice_id", "str", cfg->tts.elevenlabs_voice_id);
    show_cfg_val("elevenlabs.model_id", "str", cfg->tts.elevenlabs_model_id);
    show_cfg_val("openai.model", "str", cfg->tts.openai_model);
    show_cfg_val("openai.voice", "str", cfg->tts.openai_voice);
    show_cfg_val("xai.voice_id", "str", cfg->tts.xai_voice_id);
    show_cfg_val("xai.language", "str", cfg->tts.xai_language);
    show_cfg_val_int("xai.sample_rate", cfg->tts.xai_sample_rate);
    show_cfg_val_int("xai.bit_rate", cfg->tts.xai_bit_rate);
    show_cfg_val("mistral.model", "str", cfg->tts.mistral_model);
    show_cfg_val("mistral.voice_id", "str", cfg->tts.mistral_voice_id);
    show_cfg_val("neutts.ref_audio", "str", cfg->tts.neutts_ref_audio);
    show_cfg_val("neutts.ref_text", "str", cfg->tts.neutts_ref_text);
    show_cfg_val("neutts.model", "str", cfg->tts.neutts_model);
    show_cfg_val("neutts.device", "str", cfg->tts.neutts_device);
    show_cfg_val("piper.voice", "str", cfg->tts.piper_voice);
}


void show_section_voice(const hermes_config_t *cfg) {
    printf("voice:  Voice input recording settings\n");
    show_cfg_val("record_key", "str", cfg->voice.record_key);
    show_cfg_val_int("max_recording_seconds", cfg->voice.max_recording_seconds);
    show_cfg_val_bool("auto_tts", cfg->voice.auto_tts);
    show_cfg_val_bool("beep_enabled", cfg->voice.beep_enabled);
    show_cfg_val_int("silence_threshold", cfg->voice.silence_threshold);
    show_cfg_val_float("silence_duration", cfg->voice.silence_duration);
}

void show_section_memory(const hermes_config_t *cfg) {
    printf("memory:  Provider, char limits, TTL\n");
    show_cfg_val("provider", "str", cfg->memory.provider);
    show_cfg_val_int("char_limit", cfg->memory.char_limit);
    show_cfg_val_int("user_char_limit", cfg->memory.user_char_limit);
    show_cfg_val_int("ttl_days", cfg->memory.ttl_days);
    show_cfg_val_bool("auto_save", cfg->memory.auto_save);
}
