/*
 * cli_cmd_skills.c — Skills slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "libyaml/yaml.h"
#include "hermes_agent.h"
#include "cli_cmd_skills.h"
#include "commands_shared.h"
#include "hermes_core_types.h"
#include "hermes_skills.h"

/* /bundles: List skill bundles. Reads yaml files from skill-bundles dir. */
void cmd_bundles(const char *args, agent_state_t *state) {
    (void)args;
    const char *home = state->hermes_home[0] ? state->hermes_home : getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) { printf("Cannot determine home directory.\n"); return; }

    char bundles_dir[HERMES_PATH_MAX + 64];
    snprintf(bundles_dir, sizeof(bundles_dir), "%s/skill-bundles", home);

    DIR *dir = opendir(bundles_dir);
    if (!dir) {
        printf("No skill bundles found.\n");
        printf("  Create YAML files in %s/skill-bundles/\n", home);
        printf("  Format: { name, description, skills: [list] }\n");
        return;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len > 5 && strcmp(name + len - 5, ".yaml") == 0) {
            count++;
            char path[HERMES_PATH_MAX + 128];
            snprintf(path, sizeof(path), "%s/%s", bundles_dir, name);

            char *err = NULL;
            yaml_doc_t *doc = yaml_parse_file(path, &err);
            if (!doc) {
                printf("  %s: (parse error: %s)\n", name, err ? err : "unknown");
                free(err);
                continue;
            }

            const char *bname = yaml_get_string(doc, "name");
            const char *desc  = yaml_get_string(doc, "description");
            printf("  %s\n", bname ? bname : name);
            if (desc) printf("    Description: %s\n", desc);

            /* List skills within the bundle */
            size_t sc = yaml_list_count(doc, "skills");
            if (sc > 0) {
                printf("    Skills (%zu): ", sc);
                int first_skill = 1;
                for (size_t si = 0; si < sc && si < 50; si++) {
                    const char *sk = yaml_list_get(doc, "skills", si);
                    if (sk) {
                        printf("%s%s", first_skill ? "" : ", ", sk);
                        first_skill = 0;
                    }
                }
                printf("\n");
            }

            yaml_free(doc);
        }
    }
    closedir(dir);

    if (count == 0) {
        printf("No skill bundles found.\n");
        printf("  Create YAML files in %s/skill-bundles/\n", home);
        printf("  Format: { name, description, skills: [list] }\n");
    } else {
        printf("\n%d bundle(s) found in %s/skill-bundles/\n", count, home);
    }
}

/* /curator: Background skill maintenance */
void cmd_curator(const char *args, agent_state_t *state) {
    (void)state;
    bool show_help = false;

    /* Parse subcommand */
    const char *sub = NULL;
    if (args) {
        while (*args == ' ') args++;
        if (*args) sub = args;
    }

    if (sub && strcmp(sub, "--help") == 0) show_help = true;

    if (show_help || (sub && strcmp(sub, "help") == 0)) {
        printf("Usage: /curator [status|run|pause|resume|help]\n");
        printf("  status   - Show curator state (default)\n");
        printf("  run      - Trigger a curator run\n");
        printf("  pause    - Pause curator auto-runs\n");
        printf("  resume   - Resume curator auto-runs\n");
        return;
    }

    /* Load curator state */
    curator_state_t cs;
    load_state(&cs);

    if (sub && strcmp(sub, "pause") == 0) {
        cs.paused = true;
        save_state(&cs);
        printf("Curator paused.\n");
        return;
    }

    if (sub && strcmp(sub, "resume") == 0) {
        cs.paused = false;
        save_state(&cs);
        printf("Curator resumed.\n");
        return;
    }

    if (sub && strcmp(sub, "run") == 0) {
        printf("Triggering curator run...\n");
        /* Wire llm_background_review: review last tool result in session */
        char *review = NULL;
        double duration = 0.0;
        if (state && state->message_count > 0) {
            /* Find the last tool result message */
            char *tool_result = NULL;
            char *tool_name = NULL;
            for (int i = (int)state->message_count - 1; i >= 0; i--) {
                if (state->messages[i]->role == MSG_TOOL) {
                    tool_result = state->messages[i]->content;
                    /* Look backward for the tool name from assistant message */
                    for (int j = i - 1; j >= 0; j--) {
                        if (state->messages[j]->role == MSG_ASSISTANT &&
                            state->messages[j]->tool_call_id &&
                            strcmp(state->messages[j]->tool_call_id,
                                   state->messages[i]->tool_call_id) == 0) {
                            tool_name = state->messages[j]->tool_name;
                            break;
                        }
                    }
                    if (!tool_name && state->messages[i]->tool_call_id) {
                        /* Use tool_call_id as fallback name */
                        tool_name = state->messages[i]->tool_call_id;
                    }
                    break;
                }
            }
            if (tool_result) {
                review = llm_background_review(&state->llm,
                    tool_name ? tool_name : "unknown",
                    "{}", tool_result);
            }
        }
        if (review) {
            record_run(&cs, duration, review);
            printf("Curator run complete. Review summary:\n  %s\n", review);
            free(review);
        } else {
            record_run(&cs, duration,
                "No tool results to review in current session");
            printf("Curator run complete (no tool results found).\n");
        }
        return;
    }

    /* Default: status display */
    const char *status_str = "ENABLED";
    if (cs.paused) status_str = "PAUSED";
    else if (!cs.enabled) status_str = "DISABLED";

    char time_buf[64], duration_buf[64];
    format_reltime(cs.last_run_at, time_buf, sizeof(time_buf));
    format_duration(cs.last_run_duration, duration_buf, sizeof(duration_buf));

    printf("curator: %s\n", status_str);
    printf("  runs:           %d\n", cs.run_count);
    printf("  last run:       %s\n", time_buf);
    printf("  last duration:  %s\n", duration_buf);
    if (cs.last_run_summary[0])
        printf("  summary:        %s\n", cs.last_run_summary);

    /* Also show skill usage stats if available */
    skill_usage_map_t sumap;
    skill_usage_load(NULL, &sumap);
    int total = sumap.count;
    int agent_created = 0;
    for (int i = 0; i < total; i++) {
        if (strcmp(sumap.records[i].created_by, "agent") == 0) agent_created++;
    }
    printf("  skills tracked: %d (%d agent-created)\n", total, agent_created);
}

/* /reload-skills: Re-scan skills directory */
/* Port of Python skill_commands: reload_skills */
/* PoP: _reload_skills @ cli.py:_reload_skills */
/* Port of Python cli.py:_reload_skills(). */
void cmd_reload_skills(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) { printf("Cannot determine home directory.\n"); return; }

    char skills_dir[512];
    snprintf(skills_dir, sizeof(skills_dir), "%s/.slermes/skills", home);

    struct stat st;
    if (stat(skills_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("Skills directory not found: %s\n", skills_dir);
        printf("Create it to install skills.\n");
        return;
    }

    /* Count skill files */
    DIR *dir = opendir(skills_dir);
    if (!dir) { printf("Cannot open skills directory: %s\n", skills_dir); return; }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        /* Check for .md or directory */
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", skills_dir, entry->d_name);
        struct stat fst;
        if (stat(full, &fst) == 0 && (S_ISDIR(fst.st_mode) || strstr(entry->d_name, ".md")))
            count++;
    }
    closedir(dir);

    printf("Skills directory scanned: %s\n", skills_dir);
    printf("Found %d skill(s). Use /skills search-hub <query> to find more.\n", count);
}

/* PoP: cli_hermes_cli_skills_config_skills_command @ hermes_cli/skills_config.py:skills_command */
/* /skills: List installed skills */
/* PoP: cmd_skills @ hermes_cli/main.py:cmd_skills */
void cmd_skills(const char *args, agent_state_t *state) {
    (void)state;
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    char skills_dir[512];
    if (home) snprintf(skills_dir, sizeof(skills_dir), "%s/.slermes/skills", home);
    printf("Skills directory: %s\n", skills_dir);

    if (args && args[0]) {
        /* Parse subcommand */
        char cmd[128], arg[256];
        cmd[0] = arg[0] = '\0';
        if (sscanf(args, "%127s %255[^\n]", cmd, arg) < 1) {
            printf("Usage: /skills [search-hub <query> | install <slug>]\n");
            return;
        }

        if (strcmp(cmd, "search-hub") == 0 || strcmp(cmd, "search") == 0) {
            size_t count = 0;
            skill_search_result_t *sr = skill_search_hub(arg, &count, 20);
            if (!sr || count == 0) {
                printf("No results from browse.sh hub.\n");
                if (sr) skill_search_hub_free(sr, count);
                return;
            }
            printf("Browse.sh hub results (%zu):\n", count);
            for (size_t i = 0; i < count; i++)
                printf("  %s  (slug: %s, score: %.2f)\n",
                       sr[i].name, sr[i].path + 10, sr[i].score);
            skill_search_hub_free(sr, count);

        } else if (strcmp(cmd, "install") == 0) {
            if (!arg[0]) {
                printf("Usage: /skills install <slug>\n");
                return;
            }
            char error[512] = "";
            bool ok = skill_install_from_hub(arg, error, sizeof(error));
            if (ok)
                printf("Installed '%s' from browse.sh hub.\n", arg);
            else
                printf("Failed: %s\n", error);

        } else {
            printf("Unknown subcommand '%s'. Use: search-hub <query> | install <slug>\n", cmd);
        }
        return;
    }

    printf("Skills management:\n");
    printf("  /skills search-hub <query>   — Search browse.sh skills hub\n");
    printf("  /skills install <slug>       — Install skill from browse.sh hub\n");
    printf("  /skills list                 — List local skills\n");
    printf("  Use skill_view/skill_manage tools for detailed management.\n");
}

/* /skills-hub: Browse.sh skills catalog CLI */
void cmd_skills_hub(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /skills-hub <subcommand> [args]\n");
        printf("  /skills-hub list              — List all hub skills\n");
        printf("  /skills-hub search <query>    — Search hub skills\n");
        printf("  /skills-hub show <slug>       — Show skill details\n");
        printf("  /skills-hub sync              — Refresh catalog from network\n");
        return;
    }
    const char *sub = args;
    while (*sub == ' ') sub++;

    if (strcmp(sub, "list") == 0) {
        skills_hub_fetch_catalog();
        char *s = skills_hub_summary();
        if (s) {
            printf("%s\n", s);
            free(s);
        }
        /* Print first 30 skills */
        hub_skill_meta_t results[50];
        int n = skills_hub_search("", results, 50);
        if (n > 0) {
            printf("\nSkills (%d shown of %d):\n", n > 50 ? 50 : n, n);
            for (int i = 0; i < n && i < 50; i++) {
                printf("  %-28s %s\n", results[i].slug, results[i].title);
            }
            if (n > 50) printf("  ... and %d more\n", n - 50);
        } else {
            printf("No skills found (catalog empty or not loaded).\n");
        }
        return;
    }

    if (strncmp(sub, "search", 6) == 0) {
        const char *query = sub + 6;
        while (*query == ' ') query++;
        if (!*query) { printf("Usage: /skills-hub search <query>\n"); return; }
        skills_hub_fetch_catalog();
        hub_skill_meta_t results[SKILLS_HUB_MAX_RESULTS];
        int n = skills_hub_search(query, results, SKILLS_HUB_MAX_RESULTS);
        if (n > 0) {
            printf("Found %d skills matching \"%s\":\n", n, query);
            for (int i = 0; i < n; i++) {
                printf("  %-28s %s\n", results[i].slug, results[i].title);
            }
        } else {
            printf("No skills found for \"%s\". Try a different query.\n", query);
        }
        return;
    }

    if (strncmp(sub, "show", 4) == 0) {
        const char *slug = sub + 4;
        while (*slug == ' ') slug++;
        if (!*slug) { printf("Usage: /skills-hub show <slug>\n"); return; }
        skills_hub_fetch_catalog();
        hub_skill_meta_t meta;
        if (skills_hub_get_by_slug(slug, &meta)) {
            printf("Slug:    %s\n", meta.slug);
            printf("Name:    %s\n", meta.name[0] ? meta.name : meta.title);
            printf("Title:   %s\n", meta.title);
            printf("Category:%s\n", meta.category);
            printf("Desc:    %s\n", meta.description);
            printf("URL:     %s\n", meta.source_url);
            if (meta.tags[0])
                printf("Tags:    %s\n", meta.tags);
            printf("Installs:%d\n", meta.install_count);
            printf("Proxy:   %s\n", meta.needs_proxy ? "yes" : "no");
            printf("Method:  %s\n", meta.recommended_method[0] ? meta.recommended_method : "(auto)");
        } else {
            printf("Skill not found: %s\n", slug);
        }
        return;
    }

    if (strcmp(sub, "sync") == 0) {
        skills_hub_clear_cache();
        printf("Cleared cache. Fetching catalog...\n");
        if (skills_hub_fetch_catalog()) {
            char *s = skills_hub_summary();
            printf("Catalog refreshed: %s\n", s ? s : "(ok)");
            free(s);
        } else {
            printf("Error: Could not fetch skills catalog.\n");
        }
        return;
    }

    printf("Unknown subcommand: '%s'. Use: /skills-hub list|search|show|sync\n", sub);
}

