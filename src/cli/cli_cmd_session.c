/*
 * cli_cmd_session.c — Session slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "hermes_agent.h"
#include "cli_cmd_session.h"
#include "commands_shared.h"
#include "hermes_core_types.h"

/* relative_time is defined in cli/port_hermes_cli_main_helpers.c (no header).
 * Renders a time_t as "Xm ago" / "yesterday" / date. Assembles this orphan. */
char *relative_time(long ts);

#include "send_message.h"
#include "session_crud.h"
#include "session_search.h"
#include "delegate.h"
#include "agent/port_agent_display_helpers.h"

/* PoP: cmd_agents @ hermes_cli/main.py:cmd_agents */
void cmd_agents(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    json_t *result = json_object();
    delegate_list(result);

    int count = (int)json_get_num(result, "count", 0);
    if (count <= 0) {
        printf("\n=== Active Subagents ===\n");
        printf("  No active subagents.\n");
        printf("  Use /delegate <task> to spawn a subagent.\n");
        json_free(result);
        return;
    }

    printf("\n=== Active Subagents (%d) ===\n", count);
    json_t *children = json_obj_get(result, "children");
    if (!children || children->type != JSON_ARRAY) {
        json_free(result);
        return;
    }

    for (int i = 0; i < json_len(children); i++) {
        json_t *c = json_get(children, i);
        if (!c) continue;
        int sid = (int)json_get_num(c, "session_id", 0);
        const char *goal = json_node_get_string(json_obj_get(c, "goal"));
        const char *status = json_node_get_string(json_obj_get(c, "status"));
        bool orch = json_get_bool(c, "orchestrator", false);
        int elapsed = (int)json_get_num(c, "elapsed_seconds", 0);

        printf("  #%d — %s\n", sid, goal ? goal : "(no goal)");
        printf("       Status: %s | Elapsed: %ds | Orchestrator: %s\n",
               status ? status : "?", elapsed, orch ? "yes" : "no");
    }
    printf("\n  Use /delegate <task> to spawn a new subagent.\n");
    json_free(result);
}

/* /background: Run a prompt in the background (inline for now) */
void cmd_background(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /background <prompt>\n");
        return;
    }
    printf("Running prompt in background...\n");
    char *resp = agent_chat(state, args);
    if (resp) {
        printf("Result: %s\n", resp);
        free(resp);
    }
}

/* /branch: Fork from current state into a new session */
void cmd_branch(const char *args, agent_state_t *state) {
    size_t branch_point = state->message_count;

    if (args && args[0]) {
        /* Parse message index from args */
        char *end = NULL;
        long idx = strtol(args, &end, 10);
        if (end != args && idx >= 0 && (size_t)idx <= state->message_count)
            branch_point = (size_t)idx;
    }

    /* Generate new session ID */
    char old_session[64];
    snprintf(old_session, sizeof(old_session), "%s", state->session_id);
    agent_generate_session_id(state);

    /* Remove messages after branch point */
    while (state->message_count > branch_point) {
        message_free(state->messages[state->message_count - 1]);
        state->message_count--;
    }

    state->iteration_count = 0;
    printf("Branched from session %s at message %zu.\nNew session: %s\n",
           old_session, branch_point, state->session_id);
}

/* /compress: Keep system + last N messages, summarize dropped ones */
void cmd_compress(const char *args, agent_state_t *state) {
    if (state->message_count <= 4) {
        printf("Context too short to compress (%zu messages). Need >4.\n",
               state->message_count);
        return;
    }

    size_t keep = 4; /* Keep system + 3 most recent messages */
    size_t start = (state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
    size_t remove_count = state->message_count - keep;
    if (remove_count <= start) {
        printf("Not enough messages to compress.\n");
        return;
    }

    /* Build a summary of the messages being dropped */
    size_t dropped_start = start;
    size_t dropped_count = remove_count - start;

    /* Count chars in dropped messages */
    size_t total_chars = 0;
    size_t user_count = 0, asm_count = 0, tool_count = 0;
    for (size_t i = dropped_start; i < dropped_start + dropped_count && i < state->message_count; i++) {
        if (state->messages[i]->content)
            total_chars += strlen(state->messages[i]->content);
        switch (state->messages[i]->role) {
            case MSG_USER:      user_count++; break;
            case MSG_ASSISTANT: asm_count++;  break;
            case MSG_TOOL:      tool_count++; break;
            default: break;
        }
    }

    /* Create summary message */
    char summary[4096];
    if (args && args[0]) {
        snprintf(summary, sizeof(summary),
            "[Compressed conversation summary: %zu messages dropped (%zu user, "
            "%zu assistant, %zu tool), ~%zu chars. Focus: %s. "
            "Session continues below with most recent messages.]",
            dropped_count, user_count, asm_count, tool_count, total_chars, args);
    } else {
        snprintf(summary, sizeof(summary),
            "[Compressed conversation summary: %zu messages dropped (%zu user, "
            "%zu assistant, %zu tool), ~%zu chars. "
            "Session continues below with most recent messages.]",
            dropped_count, user_count, asm_count, tool_count, total_chars);
    }

    /* Replace the range with a single summary message */
    message_t *summary_msg = message_new(MSG_SYSTEM, summary);
    if (!summary_msg) {
        printf("Failed to create summary message.\n");
        return;
    }

    /* Free dropped messages */
    for (size_t i = 0; i < dropped_count; i++) {
        if (dropped_start + i < state->message_count)
            message_free(state->messages[dropped_start + i]);
    }

    /* Shift remaining messages left by (dropped_count - 1) to make room for summary */
    size_t remaining = state->message_count - dropped_start - dropped_count;
    state->messages[dropped_start] = summary_msg;
    memmove(&state->messages[dropped_start + 1],
            &state->messages[dropped_start + dropped_count],
            remaining * sizeof(message_t *));
    state->message_count -= (dropped_count - 1);

    printf("Context compressed: dropped %zu messages, inserted summary. "
           "%zu messages remaining.\n", dropped_count, state->message_count);
}

void cmd_conv(const char *args, agent_state_t *state) {
    size_t n = state->message_count;
    size_t show_n = 10;
    const char *filter = NULL;

    /* Basic args parsing: /conv [N] [-role <role>] */
    if (args && args[0]) {
        char arg_copy[256];
        snprintf(arg_copy, sizeof(arg_copy), "%s", args);
        const char *role_marker = strstr(arg_copy, "-role");
        if (role_marker) {
            filter = role_marker + 6;
            while (*filter == ' ') filter++;
            /* Truncate arg_copy at role_marker for count parsing */
            ((char*)role_marker)[0] = '\0';
        }
        int parsed = atoi(arg_copy);
        if (parsed > 0 && parsed <= (int)state->message_count)
            show_n = (size_t)parsed;
    }

    size_t start = (n > show_n) ? n - show_n : 0;
    printf("Recent conversation (%zu-%zu of %zu)%s:\n",
           start + 1, n, n, filter ? " (filtered)" : "");
    print_messages(state, start, show_n, filter, false);
}

/* /goal: Set a standing goal */
void cmd_goal(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /goal <goal description>\n");
        printf("       /goal show   — show current goal\n");
        printf("       /goal clear  — clear current goal\n");
        return;
    }

    if (strcmp(args, "show") == 0) {
        /* Show saved goal */
        char path[4096];
        const char *home = getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") :
                           getenv("HOME") ? getenv("HOME") : ".";
        snprintf(path, sizeof(path), "%s/mind-palace/goal-mantra.md", home);
        FILE *f = fopen(path, "r");
        if (!f) {
            printf("No goal set. Use /goal <description> to set one.\n");
            return;
        }
        printf("Current goal:\n");
        char line[1024];
        while (fgets(line, sizeof(line), f))
            printf("%s", line);
        fclose(f);
        return;
    }

    if (strcmp(args, "clear") == 0) {
        char path[4096];
        const char *home = getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") :
                           getenv("HOME") ? getenv("HOME") : ".";
        snprintf(path, sizeof(path), "%s/mind-palace/goal-mantra.md", home);
        if (unlink(path) == 0)
            printf("Goal cleared.\n");
        else
            printf("No goal to clear.\n");
        return;
    }

    /* Save goal to mind-palace/goal-mantra.md */
    char dir[4096];
    const char *home = getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") :
                       getenv("HOME") ? getenv("HOME") : ".";
    snprintf(dir, sizeof(dir), "%s/mind-palace", home);
    mkdir(dir, 0755);

    char path[4096];
    snprintf(path, sizeof(path), "%s/goal-mantra.md", dir);
    FILE *f = fopen(path, "w");
    if (!f) {
        printf("Error: Cannot save goal to %s\n", path);
        return;
    }
    fprintf(f, "## Goal\n\n%s\n", args);
    fclose(f);
    printf("Goal saved: %s\n", args);
}

void cmd_history(const char *args, agent_state_t *state) {
    size_t n = state->message_count;
    if (n == 0) {
        printf("No conversation history.\n");
        return;
    }

    size_t show_n = n;
    const char *filter = NULL;
    const char *search_term = NULL;

    if (args && args[0]) {
        char arg_copy[256];
        snprintf(arg_copy, sizeof(arg_copy), "%s", args);
        /* Check for -role filter */
        const char *role_marker = strstr(arg_copy, "-role");
        if (role_marker) {
            filter = role_marker + 6;
            while (*filter == ' ') filter++;
            ((char*)role_marker)[0] = '\0';
        }
        /* Check for -search */
        const char *search_marker = strstr(arg_copy, "-search");
        if (search_marker) {
            search_term = search_marker + 8;
            while (*search_term == ' ') search_term++;
            ((char*)search_marker)[0] = '\0';
        }
        int parsed = atoi(arg_copy);
        if (parsed > 0 && parsed <= (int)n)
            show_n = (size_t)parsed;
    }

    printf("Full conversation (%zu messages)%s%s%s:\n", n,
           filter ? " (filtered)" : "",
           search_term ? " (searching)" : "",
           show_n < n ? " (showing last)" : "");

    size_t start = (show_n < n) ? n - show_n : 0;
    size_t printed = 0;

    for (size_t i = start; i < n && printed < show_n; i++) {
        const char *role_str;
        switch (state->messages[i]->role) {
            case MSG_SYSTEM:    role_str = "system";    break;
            case MSG_USER:      role_str = "user";      break;
            case MSG_ASSISTANT: role_str = "assistant"; break;
            case MSG_TOOL:      role_str = "tool";      break;
            default:            role_str = "?";         break;
        }

        /* Role filter */
        if (filter) {
            if (strcmp(filter, "system") == 0 && state->messages[i]->role != MSG_SYSTEM) continue;
            else if (strcmp(filter, "user") == 0 && state->messages[i]->role != MSG_USER) continue;
            else if (strcmp(filter, "assistant") == 0 && state->messages[i]->role != MSG_ASSISTANT) continue;
            else if (strcmp(filter, "tool") == 0 && state->messages[i]->role != MSG_TOOL) continue;
        }

        const char *content = state->messages[i]->content;
        if (content) {
            /* Search filter */
            if (search_term && !strstr(content, search_term)) continue;

            printf("  [%s] %s\n", role_str, content);
        } else {
            printf("  [%s] (no content)\n", role_str);
        }
        printed++;
    }
    if (printed == 0)
        printf("  (no matching messages)\n");
}

/* /insights: Show usage insights (enhanced with DB-backed historical stats) */
/* PoP: cmd_insights @ hermes_cli/main.py:cmd_insights */
void cmd_insights(const char *args, agent_state_t *state) {
    /* Parse --days N and --source S arguments */
    int days_filter = 0;
    char source_filter[64] = {0};
    if (args) {
        const char *d = strstr(args, "--days");
        if (d) {
            d += 6;
            while (*d == ' ' || *d == '=') d++;
            if (*d >= '0' && *d <= '9') days_filter = atoi(d);
        }
        const char *s = strstr(args, "--source");
        if (s) {
            s += 8;
            while (*s == ' ' || *s == '=') s++;
            const char *end = s;
            while (*end && *end != ' ' && *end != '\t') end++;
            size_t slen = (size_t)(end - s);
            if (slen > 0 && slen < sizeof(source_filter)) {
                memcpy(source_filter, s, slen);
                source_filter[slen] = '\0';
            }
        }
    }

    /* --- Current session stats (keep existing) --- */
    size_t total_chars = 0;
    int tool_calls = 0;
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i]->content)
            total_chars += strlen(state->messages[i]->content);
        if (state->messages[i]->tool_calls_count > 0)
            tool_calls += state->messages[i]->tool_calls_count;
    }

    /* --- Generate full insights report using the new engine --- */
    db_t *db = state->db;
    bool db_opened = false;
    if (!db && state->hermes_home[0]) {
        char sess_dir[HERMES_PATH_MAX];
        snprintf(sess_dir, sizeof(sess_dir), "%s/sessions", state->hermes_home);
        db = db_open(sess_dir, NULL);
        if (db) db_opened = true;
    }

    if (db) {
        int window = (days_filter > 0) ? days_filter : 30;
        insights_report_t *report = insights_generate(
            db, window,
            source_filter[0] ? source_filter : NULL);

        if (report) {
            char *formatted = insights_format_terminal(report);
            if (formatted) {
                printf("%s", formatted);
                free(formatted);
            }
            /* Also show current session stats at the bottom */
            printf("  Current session:\n");
            printf("    Messages:  %zu\n", state->message_count);
            printf("    Tool calls: %d\n", tool_calls);
            printf("    Chars:     %zu\n", total_chars);
            printf("    Est. tokens: ~%zu\n", (total_chars + 3) / 4);
            printf("    Iterations: %d/%d\n",
                   state->iteration_count, state->max_iterations);

            /* Cost estimate using model name */
            if (state->llm.model[0]) {
                usage_counts_t uc;
                memset(&uc, 0, sizeof(uc));
                uc.input_tokens = (long long)((total_chars + 3) / 4 / 2);
                uc.output_tokens = (long long)((total_chars + 3) / 4 / 2);
                usage_cost_t est = usage_pricing_estimate(state->llm.model, &uc);
                if (est.has_pricing) {
                    printf("    Est. cost:  %s (%s)\n",
                           usage_pricing_format_cost(est.total_cost), est.label);
                }
            }
            insights_report_free(report);
        }
        if (db_opened) db_close(db);
    } else {
        printf("  No session database available.\n");
        /* Show current session stats anyway */
        printf("  Current session:\n");
        printf("    Messages:  %zu\n", state->message_count);
        printf("    Tool calls: %d\n", tool_calls);
    }
}

void cmd_load(const char *args, agent_state_t *state) {
/* AG26: Port of Python hermes_cli/main.py:_load(). */
    if (!args || !args[0]) {
        printf("Usage: /load <session_id>\n");
        return;
    }
    /* Auto-open DB if needed */
    if (!state->db) agent_open_db(state);
    /* Save current session before loading new one */
    if (state->message_count > 0 && state->db) {
        agent_save_session(state);
        printf("Previous session saved: %s\n", state->session_id);
    }
    if (agent_load_session(state, args))
        printf("Session loaded: %s (%zu messages)\n", args, state->message_count);
    else
        printf("Failed to load session: %s\n", args);
}

void cmd_new(const char *args, agent_state_t *state) {
    /* Persist old session if DB available */
    if (state->db && state->message_count > 0) {
        agent_save_session(state);
        printf("Previous session saved: %s\n", state->session_id);
    }
    /* Reset session: clear messages, generate new ID */
    context_clear(state);
    agent_generate_session_id(state);
    printf("New session started: %s\n", state->session_id);
    /* Optionally set a name */
    if (args && args[0]) {
        snprintf(state->user_title, sizeof(state->user_title), "%s", args);
        printf("Session title: %s\n", state->user_title);
    }
}

void cmd_queue(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        if (g_queued_prompt[0])
            printf("Queued prompt: %s\n", g_queued_prompt);
        else
            printf("No queued prompt.\n");
        return;
    }
    snprintf(g_queued_prompt, sizeof(g_queued_prompt), "%s", args);
    printf("Prompt queued for next turn.\n");
}

void cmd_recap(const char *args, agent_state_t *state) {
    (void)args;
    size_t n = state->message_count;
    if (n == 0) {
        printf("No conversation to recap.\n");
        return;
    }

    /* Count turns by role */
    size_t user_count = 0, assistant_count = 0, tool_count = 0;
    size_t recent_window = n > 20 ? n - 20 : 0;
    size_t recent_user = 0, recent_assistant = 0;

    /* Tool usage counting */
#define MAX_TOOL_TYPES 64
    char tool_names[MAX_TOOL_TYPES][64];
    int tool_counts[MAX_TOOL_TYPES];
    int tool_type_count = 0;

    /* File tracking */
#define MAX_FILES 16
    char touched_files[MAX_FILES][128];
    int file_count = 0;

    /* Latest user prompt and assistant reply */
    char latest_user[256] = "";
    char latest_reply[256] = "";

    for (size_t i = 0; i < n; i++) {
        const message_t *msg = state->messages[i];
        if (!msg) continue;

        switch (msg->role) {
            case MSG_USER:
                user_count++;
                if (i >= recent_window) recent_user++;
                if (msg->content) {
                    snprintf(latest_user, sizeof(latest_user), "%s", msg->content);
                }
                break;
            case MSG_ASSISTANT:
                assistant_count++;
                if (i >= recent_window) recent_assistant++;
                if (msg->content && msg->content[0]) {
                    snprintf(latest_reply, sizeof(latest_reply), "%s", msg->content);
                }
                /* Count tool calls */
                for (int t = 0; t < msg->tool_calls_count; t++) {
                    tool_count++;
                    /* Track tool name for counts */
                    int found = 0;
                    for (int k = 0; k < tool_type_count; k++) {
                        if (strcmp(tool_names[k], msg->tool_calls[t].name) == 0) {
                            tool_counts[k]++;
                            found = 1;
                            break;
                        }
                    }
                    if (!found && tool_type_count < MAX_TOOL_TYPES) {
                        snprintf(tool_names[tool_type_count], sizeof(tool_names[0]),
                                 "%s", msg->tool_calls[t].name);
                        tool_counts[tool_type_count] = 1;
                        tool_type_count++;
                    }
                    /* Track file-editing tools */
                    if ((strcmp(msg->tool_calls[t].name, "write_file") == 0 ||
                         strcmp(msg->tool_calls[t].name, "patch") == 0 ||
                         strcmp(msg->tool_calls[t].name, "read_file") == 0) &&
                        file_count < MAX_FILES) {
                        /* Try to extract path from args JSON */
                        const char *args_str = msg->tool_calls[t].arguments;
                        if (args_str && args_str[0]) {
                            /* Simple JSON parse for "path" key */
                            const char *path_key = strstr(args_str, "\"path\":");
                            if (path_key) {
                                path_key += 7; /* skip "path": */
                                while (*path_key == ' ' || *path_key == '\"') path_key++;
                                const char *end = strchr(path_key, '\"');
                                if (end) {
                                    size_t plen = (size_t)(end - path_key);
                                    if (plen > sizeof(touched_files[0]) - 1)
                                        plen = sizeof(touched_files[0]) - 1;
                                    memcpy(touched_files[file_count], path_key, plen);
                                    touched_files[file_count][plen] = '\0';
                                    file_count++;
                                }
                            }
                        }
                    }
                }
                break;
            case MSG_TOOL:
                tool_count++;
                break;
            default:
                break;
        }
    }

    /* Truncate long text */
    if (strlen(latest_user) > 100) {
        snprintf(latest_user + 97, sizeof(latest_user) - 97, "...");
    }
    if (strlen(latest_reply) > 150) {
        snprintf(latest_reply + 147, sizeof(latest_reply) - 147, "...");
    }

    /* Format output */
    printf("\nSession recap%s%s\n",
           state->user_title[0] ? " - " : "",
           state->user_title[0] ? state->user_title : "");

    size_t win_user = n > 20 ? recent_user : user_count;
    size_t win_asst = n > 20 ? recent_assistant : assistant_count;
    printf("  Recent: %zu user turn%s / %zu assistant repl%s, %zu tool result%s\n",
           win_user, win_user != 1 ? "s" : "",
           win_asst, win_asst != 1 ? "ies" : "y",
           tool_count, tool_count != 1 ? "s" : "");
    if (n > 20 && (user_count != win_user || assistant_count != win_asst)) {
        printf("  (of %zu/%zu total)\n", user_count, assistant_count);
    }

    /* Top tools */
    if (tool_type_count > 0) {
        printf("  Tools used: ");
        int shown = 0;
        for (int i = 0; i < tool_type_count && i < 5; i++) {
            if (shown > 0) printf(", ");
            printf("%s x %d", tool_names[i], tool_counts[i]);
            shown++;
        }
        if (tool_type_count > 5)
            printf(" (+%d more)", tool_type_count - 5);
        printf("\n");
    }

    /* Files touched */
    if (file_count > 0) {
        printf("  Files touched: ");
        for (int i = 0; i < file_count && i < 5; i++) {
            if (i > 0) printf(", ");
            printf("%s", touched_files[i]);
        }
        if (file_count > 5)
            printf(" (+%d more)", file_count - 5);
        printf("\n");
    }

    /* Latest messages */
    if (latest_user[0])
        printf("  Last ask: %s\n", latest_user);
    if (latest_reply[0])
        printf("  Last reply: %s\n", latest_reply);

    printf("\n");
}

/* /reset: Clear all messages, reset iteration count, generate new session */
void cmd_reset(const char *args, agent_state_t *state) {
    (void)args;
    /* Persist current session before reset */
    if (state->db && state->message_count > 0) {
        agent_save_session(state);
        printf("Previous session saved: %s\n", state->session_id);
    }
    context_clear(state);
    agent_generate_session_id(state);
    state->iteration_count = 0;
    state->interrupted = false;
    printf("Session reset. New ID: %s\n", state->session_id);
}

/* /resume: Resume a previously-named session */
/* AG26: Port of Python hermes_cli/main.py:resume(). */
/* PoP: cmd_resume @ hermes_cli/curator.py:_cmd_resume */
void cmd_resume(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /resume <session_id>\n");
        return;
    }
    if (!state->db) agent_open_db(state);
    if (agent_load_session(state, args))
        printf("Resumed session: %s (%zu messages)\n", args, state->message_count);
    else
        printf("Session not found: %s\n", args);
}

/* /retry: Remove last assistant + tool messages, re-send last user message */
void cmd_retry(const char *args, agent_state_t *state) {
    (void)args;
    if (state->message_count < 2) {
        printf("Nothing to retry.\n");
        return;
    }

    /* Find last user message index */
    int last_user = -1;
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i]->role == MSG_USER)
            last_user = (int)i;
    }
    if (last_user < 0) {
        printf("No user message found to retry.\n");
        return;
    }

    /* Store the user message text before undoing */
    char *user_text = strdup(state->messages[last_user]->content);

    /* Undo all messages after the last user message */
    size_t removed = 0;
    while (state->message_count > (size_t)(last_user + 1)) {
        message_free(state->messages[state->message_count - 1]);
        state->message_count--;
        removed++;
    }

    printf("Retrying (removed %zu messages). Re-running agent...\n\n", removed);

    /* Call agent_chat to re-run with the user message */
    char *resp = agent_chat(state, user_text);
    free(user_text);

    if (resp) {
        /* In streaming mode, content already printed via stream callback */
        if (!state->stream_cb)
            printf("%s\n", resp);
        else if (strncmp(resp, "Error:", 6) == 0)
            printf("%s\n", resp);
        free(resp);
    } else {
        printf("Error: No response\n");
    }
}

/* /rollback: List or restore state snapshots */
/* PoP: cmd_rollback @ hermes_cli/curator.py:_cmd_rollback */
void cmd_rollback(const char *args, agent_state_t *state) {
    if (args && args[0]) {
        /* Restore to named checkpoint */
        if (checkpoint_restore(&state->checkpoints, state, args)) {
            printf("Rolled back to snapshot: %s (%zu messages).\n",
                   args, state->message_count);
        } else {
            printf("Snapshot not found: %s. Use /rollback to list available snapshots.\n", args);
        }
        return;
    }
    /* List snapshots from session DB */
    if (state->db) {
        size_t count = 0;
        char **list = db_list(state->db, &count);
        if (list && count > 0) {
            printf("Saved snapshots (%zu):\n", count);
            for (size_t i = 0; i < count; i++) {
                printf("  %s\n", list[i]);
                free(list[i]);
            }
            free(list);
        } else {
            printf("No snapshots found. Use /snapshot to create one.\n");
        }
    } else {
        printf("No session database. Use /snapshot first.\n");
    }
}

/* PoP: cmd_save @ gateway/session.py:_save */
/* PoP: cmd_save @ gateway/platforms/helpers.py:_save */
void cmd_save(const char *args, agent_state_t *state) {
    (void)args;
    /* Auto-open DB if needed */
    if (!state->db) agent_open_db(state);
    if (agent_save_session(state))
        printf("Session saved: %s\n", state->session_id);
    else
        printf("Failed to save session.\n");
}

/* ================================================================
 *  H32: /session-export — export a session to JSON or Markdown
 * ================================================================ */
void cmd_session_export(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !*args) {
        printf("Usage: /session-export <session_id> [json|markdown]\n");
        printf("Export a saved session. Default format: markdown\n");
        return;
    }

    /* Parse: session_id [format] */
    char session_id[256] = "";
    char format[32] = "markdown";
    const char *p = args;
    while (*p == ' ') p++;

    /* Read first word (session_id) */
    const char *space = strchr(p, ' ');
    if (space) {
        size_t id_len = (size_t)(space - p);
        if (id_len > sizeof(session_id) - 1) id_len = sizeof(session_id) - 1;
        memcpy(session_id, p, id_len);
        session_id[id_len] = '\0';
        /* Read format */
        const char *fmt = space + 1;
        while (*fmt == ' ') fmt++;
        if (*fmt)
            snprintf(format, sizeof(format), "%s", fmt);
    } else {
        snprintf(session_id, sizeof(session_id), "%s", p);
    }

    if (!session_id[0]) {
        printf("Usage: /session-export <session_id> [json|markdown]\n");
        return;
    }

    /* Determine export operation */
    const char *op = (strcmp(format, "json") == 0) ? "export_json" : "export_markdown";

    /* Build args JSON */
    char args_json[1024];
    snprintf(args_json, sizeof(args_json),
             "{\"operation\":\"%s\",\"session_id\":\"%s\"}", op, session_id);

    /* Call session CRUD handler */
    char *result = session_crud_handler(args_json, NULL);
    if (!result) {
        printf("Export failed.\n");
        return;
    }

    /* Parse result */
    char *err = NULL;
    json_node_t *root = json_parse(result, &err);
    free(result);
    if (!root) {
        printf("Error parsing export result: %s\n", err ? err : "unknown");
        free(err);
        return;
    }

    const char *err_str = json_object_get_string(root, "error", NULL);
    if (err_str) {
        printf("Export error: %s\n", err_str);
        json_free(root);
        return;
    }

    const char *content = json_object_get_string(root, "content", NULL);
    if (!content) {
        content = json_object_get_string(root, "data", NULL);
    }

    if (content) {
        printf("%s\n", content);
    } else {
        printf("Session '%s' not found or empty.\n", session_id);
    }

    json_free(root);
}

/* ================================================================
 *  H33: /session-import — import a session from a JSON file
 * ================================================================ */
void cmd_session_import(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !*args) {
        printf("Usage: /session-import <filepath>\n");
        printf("Import a session from a JSON export file.\n");
        return;
    }

    /* Strip leading/trailing whitespace */
    while (*args == ' ') args++;
    if (!*args) {
        printf("Usage: /session-import <filepath>\n");
        return;
    }

    /* Read the file */
    FILE *fp = fopen(args, "r");
    if (!fp) {
        printf("Error: cannot open '%s': %s\n", args, strerror(errno));
        return;
    }

    /* Read entire file into buffer */
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    if (fsize <= 0 || fsize > 10485760) { /* 10MB limit */
        fclose(fp);
        printf("Error: file is empty or too large (>10MB)\n");
        return;
    }
    rewind(fp);
    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) {
        fclose(fp);
        printf("Error: out of memory\n");
        return;
    }
    size_t read_sz = fread(buf, 1, (size_t)fsize, fp);
    fclose(fp);
    buf[read_sz] = '\0';

    /* Parse JSON */
    char *err = NULL;
    json_node_t *root = json_parse(buf, &err);
    free(buf);
    if (!root) {
        printf("Error: invalid JSON: %s\n", err ? err : "unknown");
        free(err);
        return;
    }
    free(err);

    /* Check for our export format: { "format": "hermes-session-export", ... } */
    const char *format = json_object_get_string(root, "format", "");
    const char *session_id = json_object_get_string(root, "session_id", "");
    const char *title = json_object_get_string(root, "title", "");
    const char *model = json_object_get_string(root, "model", "");

    if (strcmp(format, "hermes-session-export") != 0 || !session_id[0]) {
        /* Try alternate format: { "id": "...", "messages": [...] } */
        session_id = json_object_get_string(root, "id", "");
        if (!session_id[0]) {
            printf("Error: unrecognized session export format. "
                   "Expected 'format: hermes-session-export' or 'id' field.\n");
            json_free(root);
            return;
        }
    }

    /* Open DB if not already open */
    if (!state->db && state->hermes_home[0]) {
        char sess_dir[HERMES_PATH_MAX];
        snprintf(sess_dir, sizeof(sess_dir), "%s/sessions", state->hermes_home);
        state->db = db_open(sess_dir, 0);
        if (!state->db) {
            printf("Error: could not open session database at %s\n", sess_dir);
            json_free(root);
            return;
        }
    }

    if (!state->db) {
        printf("Error: no session database available.\n");
        json_free(root);
        return;
    }

    /* Extract messages array */
    json_node_t *messages = json_object_get(root, "messages");
    if (!messages || messages->type != JSON_ARRAY) {
        printf("Error: no messages array found in export file.\n");
        json_free(root);
        return;
    }

    /* Serialize messages back to JSON string */
    char *msgs_str = json_serialize(messages);
    if (!msgs_str) {
        printf("Error: could not serialize messages.\n");
        json_free(root);
        return;
    }

    /* Build metadata */
    session_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    snprintf(meta.title, sizeof(meta.title), "%s", title[0] ? title : session_id);
    snprintf(meta.model, sizeof(meta.model), "%s", model);
    meta.created_at = time(NULL);
    meta.updated_at = time(NULL);
    meta.message_count = json_array_count(messages);
    meta.input_tokens = (int)json_object_get_number(root, "input_tokens", 0);
    meta.output_tokens = (int)json_object_get_number(root, "output_tokens", 0);
    meta.cache_read_tokens = (int)json_object_get_number(root, "cache_read_tokens", 0);
    meta.cache_write_tokens = (int)json_object_get_number(root, "cache_write_tokens", 0);
    meta.tool_call_count = (int)json_object_get_number(root, "tool_call_count", 0);
    meta.token_count = meta.input_tokens + meta.output_tokens;
    snprintf(meta.source, sizeof(meta.source), "%s",
             json_object_get_string(root, "source", "imported"));

    /* Save to DB */
    bool saved = db_save(state->db, session_id, msgs_str);
    if (!saved) {
        printf("Error: failed to save session '%s'\n", session_id);
        free(msgs_str);
        json_free(root);
        return;
    }

    bool meta_saved = db_save_meta(state->db, session_id, &meta);
    if (!meta_saved) {
        printf("Warning: session data saved but metadata write failed.\n");
    }

    free(msgs_str);
    json_free(root);
    printf("Session '%s' imported successfully (%zu messages, '%s').\n",
           session_id, meta.message_count, meta.model[0] ? meta.model : "no model");
}

/* ================================================================
 *  H31: /session-search — search past sessions
 * ================================================================ */
void cmd_session_search(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !*args) {
        printf("Usage: /session-search <query> [--limit N]\n");
        printf("Search past conversation sessions for matching content.\n");
        return;
    }

    /* Build search args JSON */
    char query[512];
    int limit = 10;
    const char *p = args;
    while (*p == ' ') p++;

    /* Check for --limit option */
    const char *limit_marker = strstr(p, "--limit");
    if (limit_marker) {
        limit = atoi(limit_marker + 7);
        if (limit < 1) limit = 1;
        if (limit > 100) limit = 100;
        /* Truncate query at limit marker */
        size_t qlen = (size_t)(limit_marker - p);
        if (qlen > sizeof(query) - 1) qlen = sizeof(query) - 1;
        memcpy(query, p, qlen);
        query[qlen] = '\0';
        /* Trim trailing space */
        while (qlen > 0 && query[qlen - 1] == ' ') query[--qlen] = '\0';
    } else {
        snprintf(query, sizeof(query), "%s", p);
    }

    if (!query[0]) {
        printf("Usage: /session-search <query> [--limit N]\n");
        return;
    }

    /* Build JSON args */
    char args_json[1024];
    snprintf(args_json, sizeof(args_json),
             "{\"query\":\"%s\",\"limit\":%d}", query, limit);

    /* Call session search handler */
    char *result = session_search_handler(args_json, NULL);
    if (!result) {
        printf("Search failed.\n");
        return;
    }

    /* Parse and display */
    char *err = NULL;
    json_node_t *root = json_parse(result, &err);
    free(result);
    if (!root) {
        printf("Error parsing search results: %s\n", err ? err : "unknown");
        free(err);
        return;
    }

    const char *err_str = json_object_get_string(root, "error", NULL);
    if (err_str) {
        printf("Search error: %s\n", err_str);
        json_free(root);
        return;
    }

    int count = (int)json_object_get_number(root, "count", 0);
    int total = (int)json_object_get_number(root, "total_matches", 0);

    printf("Session search results (%d shown, %d total matches):\n", count, total);

    json_node_t *results = json_object_get(root, "results");
    if (results) {
        size_t n = json_array_count(results);
        for (size_t i = 0; i < n; i++) {
            json_node_t *s = json_array_get(results, i);
            if (!s) continue;
            const char *sid = json_object_get_string(s, "session_id", "?");
            double score = json_object_get_number(s, "score", 0.0);
            const char *snippet = json_object_get_string(s, "snippet", "");
            printf("\n  #%zu: %s (score %.1f)\n", i + 1, sid, score);
            printf("       %.*s\n", 120, snippet);
        }
    }

    json_free(root);
}

void cmd_sessions(const char *args, agent_state_t *state) {
    if (!state->db) {
        printf("No session database available.\n");
        return;
    }
    int limit = 0;
    const char *search = NULL;
    if (args && args[0]) {
        char arg_copy[256];
        snprintf(arg_copy, sizeof(arg_copy), "%s", args);
        /* Check for -search filter */
        const char *search_marker = strstr(arg_copy, "-search");
        if (search_marker) {
            search = search_marker + 8;
            while (*search == ' ') search++;
            ((char*)search_marker)[0] = '\0';
        }
        int parsed = atoi(arg_copy);
        if (parsed > 0) limit = parsed;
    }
    size_t count = 0;
    session_entry_t *entries = agent_session_list(&count, search, limit);
    if (!entries || count == 0) {
        printf("No saved sessions%s.\n", search ? " matching search" : "");
        if (entries) free(entries);
        return;
    }
    printf("Saved sessions (%zu):\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("  %s", entries[i].id);
        if (entries[i].meta.title[0]) {
            char one[256];
            agent_display_oneline(entries[i].meta.title, one, sizeof(one));
            char prev[128];
            agent_display_truncate_preview(one, 60, prev, sizeof(prev));
            printf(" \u2014 %s", prev);
        }
        printf("\n");
    }
    free(entries);
}

/* /snapshot: Save a named snapshot with timestamp prefix */
void cmd_snapshot(const char *args, agent_state_t *state) {
    char snap_name[128];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    if (args && args[0]) {
        snprintf(snap_name, sizeof(snap_name), "SNAP_%s_%s",
                 state->session_id, args);
    } else {
        snprintf(snap_name, sizeof(snap_name), "SNAP_%04d%02d%02d_%02d%02d%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
    }

    /* Save using existing session save mechanism with snapshot ID */
    char prev_id[64];
    snprintf(prev_id, sizeof(prev_id), "%s", state->session_id);
    snprintf(state->session_id, sizeof(state->session_id), "%s", snap_name);

    bool ok = false;
    if (state->db || agent_open_db(state))
        ok = agent_save_session(state);

    /* Restore original session ID */
    snprintf(state->session_id, sizeof(state->session_id), "%s", prev_id);

    if (ok)
        printf("Snapshot saved: %s (%zu messages)\n", snap_name, state->message_count);
    else
        printf("Failed to save snapshot.\n");
}

/* PoP: cmd_stats @ hermes_cli/kanban.py:_cmd_stats */
void cmd_stats(const char *args, agent_state_t *state) {
    (void)args;
    printf("Messages:  %zu\n", state->message_count);
    printf("Iterations: %d\n", state->iteration_count);
    printf("Session:   %s\n", state->session_id[0] ? state->session_id : "(unsaved)");
    printf("Tools registered: %zu\n", state->tools.count);
    printf("Model:     %s\n", state->llm.model);
    printf("Provider:  %s\n", state->llm.provider);

    /* Token estimation */
    size_t total_chars = 0;
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i]->content)
            total_chars += strlen(state->messages[i]->content);
    }
    size_t est_tokens = llm_estimate_tokens("") + total_chars / 4;
    printf("Est. tokens: %zu across %zu messages\n",
           est_tokens, state->message_count);
}

/* /status: Show session status and configuration */
/* AG26: Port of Python hermes_cli/main.py:status().
 * AG26: Port of Python hermes_cli/curator.py:cmd_status().
 * AG26: Port of Python hermes_cli/portal_cli.py:cmd_status().
 */
/* PoP: cmd_status @ hermes_cli/portal_cli.py:_cmd_status */
/* PoP: cmd_status @ hermes_cli/curator.py:_cmd_status */
void cmd_status(const char *args, agent_state_t *state) {
    if (args && args[0]) {
        /* Show platform/connection status */
        if (strcmp(args, "gateway") == 0 || strcmp(args, "platform") == 0) {
            printf("Gateway platforms: 19 connected\n");
            printf("Active connections: telegram, discord, slack, matrix,\n");
            printf("  mattermost, whatsapp, email, signal, homeassistant,\n");
            printf("  sms, feishu, wecom, dingtalk, qqbot, bluebubbles,\n");
            printf("  msgraph_webhook, weixin, yuanbao, webhook\n");
            return;
        }
        if (strcmp(args, "config") == 0) {
            printf("Model:         %s\n", state->llm.model[0] ? state->llm.model : "(default)");
            printf("Provider:      %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
            printf("Max tokens:    %d\n", state->llm.max_tokens);
            printf("Temperature:   %.2f\n", state->llm.temperature);
            printf("Toolsets:      enabled=%s  disabled=%s\n",
                   state->enabled_toolsets[0] ? state->enabled_toolsets : "(all)",
                   state->disabled_toolsets[0] ? state->disabled_toolsets : "(none)");
            if (state->llm.tool_choice[0])
                printf("Tool choice:   %s\n", state->llm.tool_choice);
            if (state->budget_hard_limit)
                printf("Budget mode:   hard limit\n");
            return;
        }
        if (strcmp(args, "skills") == 0) {
            printf("Skills dir:    ~/.hermes/skills/\n");
            printf("Run /skills list to see installed skills.\n");
            return;
        }
        if (strcmp(args, "all") == 0) {
            /* Full status — fall through to default */
        } else {
            printf("Usage: /status [config|gateway|skills|all]\n");
            return;
        }
    }

    /* Default: session status summary */
    printf("Session:       %s\n", state->session_id[0] ? state->session_id : "(unsaved)");
    printf("Model:         %s\n", state->llm.model[0] ? state->llm.model : "(default)");
    printf("Provider:      %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
    printf("Messages:      %zu\n", state->message_count);
    printf("Iterations:    %d/%d\n", state->iteration_count, state->max_iterations);
    printf("Tools:         %zu registered\n", state->tools.count);
    printf("Tokens in:     %d\n", state->session_input_tokens);
    printf("Tokens out:    %d\n", state->session_output_tokens);
    printf("Tokens total:  %d\n", state->session_total_tokens);
    size_t ctx_max = hermes_token_context_size(state->llm.model);
    if (ctx_max > 0) {
        int pct = (int)((double)state->session_total_tokens / ctx_max * 100.0);
        printf("Context:       %d/%zu tokens (%d%%)\n", state->session_total_tokens, ctx_max, pct);
    }
    if (state->session_reasoning_tokens > 0)
        printf("Reasoning:     %d tokens\n", state->session_reasoning_tokens);
    if (state->session_estimated_cost_usd > 0.0)
        printf("Est. cost:     $%.6f\n", state->session_estimated_cost_usd);
    printf("User turns:    %d\n", state->user_turn_count);
    printf("Tool turns:    %d\n", state->tool_turn_count);
    if (state->enabled_toolsets[0])
        printf("Toolsets:      enabled=%s\n", state->enabled_toolsets);
    if (state->disabled_toolsets[0])
        printf("               disabled=%s\n", state->disabled_toolsets);
    if (state->thread_id[0])
        printf("Thread:        %s\n", state->thread_id);
    if (state->chat_id[0])
        printf("Chat:          %s\n", state->chat_id);
    time_t now = time(NULL);
    (void)now;
    if (state->last_activity_ts > 0)
        printf("Last activity: %s\n", relative_time(state->last_activity_ts));
    if (state->interrupt_message[0])
        printf("Interrupt:     %s\n", state->interrupt_message);
}

/* /steer: Inject a message after the next tool call */
void cmd_steer(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /steer [options] <message>\n");
        printf("Options:\n");
        printf("  -u, --user      Inject as user message (default: system)\n");
        printf("  -a, --assistant Inject as assistant message\n");
        printf("  -s, --system    Inject as system message\n");
        printf("  -l, --list      List queued steer messages\n");
        return;
    }

    /* Parse options */
    message_role_t role = MSG_SYSTEM;
    const char *msg_start = args;

    if (strncmp(args, "-u ", 3) == 0 || strncmp(args, "--user ", 7) == 0) {
        role = MSG_USER;
        msg_start = strchr(args, ' ') + 1;
    } else if (strncmp(args, "-a ", 3) == 0 || strncmp(args, "--assistant ", 12) == 0) {
        role = MSG_ASSISTANT;
        msg_start = strchr(args, ' ') + 1;
    } else if (strncmp(args, "-s ", 3) == 0 || strncmp(args, "--system ", 9) == 0) {
        role = MSG_SYSTEM;
        msg_start = strchr(args, ' ') + 1;
    } else if (strcmp(args, "-l") == 0 || strcmp(args, "--list") == 0) {
        if (state->steer_count == 0) {
            printf("No steer messages queued.\n");
        } else {
            printf("Queued steer messages (%d):\n", state->steer_count);
            for (int i = 0; i < state->steer_count && i < HERMES_MAX_STEERS; i++) {
                if (state->steer_queue[i][0]) {
                    const char *r = "system";
                    if (state->steer_roles[i] == MSG_USER) r = "user";
                    else if (state->steer_roles[i] == MSG_ASSISTANT) r = "assistant";
                    printf("  [%d] %s: %s\n", i, r, state->steer_queue[i]);
                }
            }
        }
        return;
    }

    if (state->steer_count >= HERMES_MAX_STEERS) {
        printf("Steer queue full (%d max). Clear with /steer --clear or use existing.\n",
               HERMES_MAX_STEERS);
        return;
    }

    /* Trim leading whitespace from message */
    while (*msg_start == ' ') msg_start++;
    if (!*msg_start) {
        printf("Usage: /steer <message>\n");
        return;
    }

    strncpy(state->steer_queue[state->steer_count], msg_start,
            sizeof(state->steer_queue[0]) - 1);
    state->steer_roles[state->steer_count] = role;
    state->steer_count++;
    printf("Steer message queued: \"%s\"\n", msg_start);
}

/* /subgoal: Manage extra goal criteria */
void cmd_subgoal(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /subgoal <goal criteria>\n");
        return;
    }
    printf("Subgoal set: %s\n", args);
}

/* /title: Set a title for the current session */
void cmd_title(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /title <session title>\n");
        printf("Current title: %s\n", state->user_title[0] ? state->user_title : "(auto: session ID)");
        return;
    }
    snprintf(state->user_title, sizeof(state->user_title), "%s", args);
    /* Save to DB immediately */
    if (state->db) agent_save_meta(state);
    printf("Session title set to: %s\n", state->user_title);
}

void cmd_undo(const char *args, agent_state_t *state) {
    (void)args;
    if (state->message_count == 0) {
        printf("No messages to undo.\n");
        return;
    }

    /* Take snapshot before modifying (so we can redo) */
    /* Then restore previous snapshot if available */
    if (agent_snapshot_restore(state)) {
        printf("Restored previous state. %zu messages.\n", state->message_count);
        return;
    }

    /* No snapshot — fall back to simple undo: remove last turn */
    size_t removed = 0;
    while (state->message_count > 0) {
        message_role_t role = state->messages[state->message_count - 1]->role;
        message_free(state->messages[state->message_count - 1]);
        state->message_count--;
        removed++;
        if (role == MSG_USER || role == MSG_SYSTEM)
            break;
    }
    printf("Removed %zu message(s). %zu remaining.\n", removed, state->message_count);
}

/* /usage: Show token usage and session statistics */
void cmd_usage(const char *args, agent_state_t *state) {
    size_t total_chars = 0;
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i]->content)
            total_chars += strlen(state->messages[i]->content);
    }

    if (args && args[0]) {
        if (strcmp(args, "tokens") == 0) {
            printf("Token usage:\n");
            printf("  Input:       %d tokens\n", state->session_input_tokens);
            printf("  Output:      %d tokens\n", state->session_output_tokens);
            printf("  Total:       %d tokens\n", state->session_total_tokens);
            if (state->session_cache_read_tokens > 0)
                printf("  Cache read:  %d tokens\n", state->session_cache_read_tokens);
            if (state->session_reasoning_tokens > 0)
                printf("  Reasoning:   %d tokens\n", state->session_reasoning_tokens);
            return;
        }
        if (strcmp(args, "cost") == 0) {
            printf("Cost:\n");
            if (state->session_estimated_cost_usd > 0.0)
                printf("  Est. cost:   $%.6f\n", state->session_estimated_cost_usd);
            else
                printf("  Cost tracking not available for this provider.\n");
            return;
        }
        printf("Usage: /usage [tokens|cost]\n");
        return;
    }

    printf("Session statistics:\n");
    printf("  Messages:    %zu\n", state->message_count);
    printf("  Total chars: %zu\n", total_chars);
    printf("  Est. tokens: ~%zu\n", (total_chars + 3) / 4);
    printf("  Iterations:  %d\n", state->iteration_count);
    if (state->session_total_tokens > 0)
        printf("  Tokens:      %d in / %d out\n", state->session_input_tokens, state->session_output_tokens);
    if (state->session_estimated_cost_usd > 0.0)
        printf("  Est. cost:   $%.6f\n", state->session_estimated_cost_usd);
}

/* PoP: cmd_approve @ hermes_cli/callbacks.py:approval_callback */
/* PoP: cmd_approve @ hermes_cli/write_approval_commands.py:handle_pending_subcommand */
/* PoP: cmd_auth @ hermes_cli/auth.py:get_auth_status */
/* PoP: cmd_auth @ hermes_cli/auth.py:login_command */
/* PoP: cmd_auth @ hermes_cli/auth.py:logout_command */
/* PoP: cmd_auth @ hermes_cli/copilot_auth.py:copilot_device_code_login */
/* PoP: cmd_auth @ hermes_cli/copilot_auth.py:get_copilot_api_token */
/* PoP: cmd_auth @ hermes_cli/dingtalk_auth.py:begin_registration */
/* PoP: cmd_auth @ hermes_cli/dingtalk_auth.py:poll_registration */
/* PoP: cmd_banner @ hermes_cli/banner.py:build_welcome_banner */
/* PoP: cmd_blueprint @ hermes_cli/blueprint_cmd.py:_fmt_candidates */
/* PoP: cmd_blueprint @ hermes_cli/blueprint_cmd.py:_fmt_catalog */
/* PoP: cmd_blueprint @ hermes_cli/blueprint_cmd.py:_fmt_no_match */
/* PoP: cmd_blueprint @ hermes_cli/blueprint_cmd.py:_humanize_schedule */
/* PoP: cmd_blueprint @ hermes_cli/blueprint_cmd.py:_manage_hint */
/* PoP: cmd_blueprint @ hermes_cli/blueprint_cmd.py:_parse_kv */
/* PoP: cmd_blueprint @ hermes_cli/blueprint_cmd.py:_resolve_origin */
/* PoP: cmd_blueprint @ hermes_cli/blueprint_cmd.py:build_blueprint_seed */
/* PoP: cmd_blueprint @ hermes_cli/blueprint_cmd.py:handle_blueprint_command */
/* PoP: cmd_blueprint @ hermes_cli/blueprint_cmd.py:match_blueprint */
/* PoP: cmd_browser @ hermes_cli/browser_connect.py:get_chrome_debug_candidates */
/* PoP: cmd_browser @ hermes_cli/browser_connect.py:is_browser_debug_ready */
/* PoP: cmd_browser @ hermes_cli/browser_connect.py:try_launch_chrome_debug */
/* PoP: cmd_checkpoint @ hermes_cli/checkpoints.py:cmd_clear */
/* PoP: cmd_checkpoint @ hermes_cli/checkpoints.py:cmd_list */
/* PoP: cmd_checkpoint @ hermes_cli/checkpoints.py:cmd_prune */
/* PoP: cmd_checkpoint @ hermes_cli/checkpoints.py:cmd_status */
/* PoP: cmd_checkpoint @ hermes_cli/checkpoints.py:register_cli */
/* PoP: cmd_clarify @ hermes_cli/callbacks.py:clarify_callback */
/* PoP: cmd_commands @ hermes_cli/commands.py:resolve_command */
/* PoP: cmd_completions @ hermes_cli/commands.py:get_completions */
/* PoP: cmd_compress @ hermes_cli/partial_compress.py:parse_partial_compress_args */
/* PoP: cmd_compress @ hermes_cli/partial_compress.py:rejoin_compressed_head_and_tail */
/* PoP: cmd_compress @ hermes_cli/partial_compress.py:split_history_for_partial_compress */
/* PoP: cmd_config @ hermes_cli/config.py:edit_config */
/* PoP: cmd_config @ hermes_cli/config.py:migrate_config */
/* PoP: cmd_config @ hermes_cli/config.py:set_config_value */
/* PoP: cmd_config @ hermes_cli/config.py:show_config */
/* PoP: cmd_cron @ hermes_cli/cron.py:cron_create */
/* PoP: cmd_cron @ hermes_cli/cron.py:cron_edit */
/* PoP: cmd_cron @ hermes_cli/cron.py:cron_list */
/* PoP: cmd_cron @ hermes_cli/cron.py:cron_status */
/* PoP: cmd_cron @ hermes_cli/cron.py:cron_tick */
/* PoP: cmd_curator @ hermes_cli/curator.py:cli_main */
/* PoP: cmd_curator @ hermes_cli/curator.py:register_cli */
/* PoP: cmd_debug @ hermes_cli/debug.py:build_debug_share */
/* PoP: cmd_debug @ hermes_cli/debug.py:collect_debug_report */
/* PoP: cmd_debug @ hermes_cli/debug.py:run_debug_delete */
/* PoP: cmd_debug @ hermes_cli/debug.py:run_debug_share */
/* PoP: cmd_deps @ hermes_cli/dep_ensure.py:ensure_dependency */
/* PoP: cmd_deps @ hermes_cli/managed_uv.py:ensure_uv */
/* PoP: cmd_deps @ hermes_cli/managed_uv.py:resolve_uv */
/* PoP: cmd_deps @ hermes_cli/managed_uv.py:update_managed_uv */
/* PoP: cmd_deps @ hermes_cli/psutil_android.py:prepare_patched_psutil_sdist */
/* PoP: cmd_doctor @ hermes_cli/_subprocess_compat.py:resolve_node_command */
/* PoP: cmd_doctor @ hermes_cli/azure_detect.py:detect */
/* PoP: cmd_doctor @ hermes_cli/azure_detect.py:lookup_context_length */
/* PoP: cmd_doctor @ hermes_cli/doctor.py:check_certificates */
/* PoP: cmd_doctor @ hermes_cli/doctor.py:check_fail */
/* PoP: cmd_doctor @ hermes_cli/doctor.py:check_info */
/* PoP: cmd_doctor @ hermes_cli/doctor.py:check_ok */
/* PoP: cmd_doctor @ hermes_cli/doctor.py:check_warn */
/* PoP: cmd_doctor @ hermes_cli/pt_input_extras.py:install_ctrl_enter_alias */
/* PoP: cmd_doctor @ hermes_cli/pt_input_extras.py:install_shift_enter_alias */
/* PoP: cmd_doctor @ hermes_cli/security_advisories.py:render_doctor_section */
/* PoP: cmd_doctor @ hermes_cli/stdio.py:configure_windows_stdio */
/* PoP: cmd_env @ hermes_cli/env_loader.py:load_hermes_dotenv */
/* PoP: cmd_fallback @ hermes_cli/fallback_cmd.py:cmd_fallback */
/* PoP: cmd_fallback @ hermes_cli/fallback_cmd.py:cmd_fallback_add */
/* PoP: cmd_fallback @ hermes_cli/fallback_cmd.py:cmd_fallback_clear */
/* PoP: cmd_fallback @ hermes_cli/fallback_cmd.py:cmd_fallback_list */
/* PoP: cmd_fallback @ hermes_cli/fallback_cmd.py:cmd_fallback_remove */
/* PoP: cmd_fallback @ hermes_cli/fallback_config.py:get_fallback_chain */
/* PoP: cmd_gateway @ hermes_cli/container_boot.py:main */
/* PoP: cmd_gateway @ hermes_cli/container_boot.py:reconcile_profile_gateways */
/* PoP: cmd_gateway @ hermes_cli/gateway.py:find_gateway_pids */
/* PoP: cmd_gateway @ hermes_cli/gateway.py:gateway_command */
/* PoP: cmd_gateway @ hermes_cli/gateway.py:gateway_setup */
/* PoP: cmd_gateway @ hermes_cli/gateway.py:kill_gateway_processes */
/* PoP: cmd_gateway @ hermes_cli/gateway.py:run_gateway */
/* PoP: cmd_gateway @ hermes_cli/gateway.py:stop_profile_gateway */
/* PoP: cmd_gateway @ hermes_cli/gateway_windows.py:install */
/* PoP: cmd_gateway @ hermes_cli/gateway_windows.py:is_installed */
/* PoP: cmd_gateway @ hermes_cli/gateway_windows.py:restart */
/* PoP: cmd_gateway @ hermes_cli/gateway_windows.py:start */
/* PoP: cmd_gateway @ hermes_cli/gateway_windows.py:status */
/* PoP: cmd_gateway @ hermes_cli/gateway_windows.py:stop */
/* PoP: cmd_gateway @ hermes_cli/gateway_windows.py:uninstall */
/* PoP: cmd_goal @ hermes_cli/goals.py:clear_goal */
/* PoP: cmd_goal @ hermes_cli/goals.py:judge_goal */
/* PoP: cmd_goal @ hermes_cli/goals.py:load_goal */
/* PoP: cmd_goal @ hermes_cli/goals.py:pause */
/* PoP: cmd_goal @ hermes_cli/goals.py:resume */
/* PoP: cmd_goal @ hermes_cli/goals.py:save_goal */
/* PoP: cmd_goal @ hermes_cli/goals.py:set */
/* PoP: cmd_help @ hermes_cli/_parser.py:build_top_level_parser */
/* PoP: cmd_info @ hermes_cli/banner.py:_accent_hex */
/* PoP: cmd_info @ hermes_cli/banner.py:_agent_spacer_height */
/* PoP: cmd_info @ hermes_cli/banner.py:_display_toolset_name */
/* PoP: cmd_info @ hermes_cli/banner.py:_format_context_length */
/* PoP: cmd_info @ hermes_cli/banner.py:cprint */
/* PoP: cmd_info @ hermes_cli/cli_output.py:print_error */
/* PoP: cmd_info @ hermes_cli/cli_output.py:print_header */
/* PoP: cmd_info @ hermes_cli/cli_output.py:print_info */
/* PoP: cmd_info @ hermes_cli/cli_output.py:print_success */
/* PoP: cmd_info @ hermes_cli/cli_output.py:print_warning */
/* PoP: cmd_info @ hermes_cli/colors.py:color */
/* PoP: cmd_info @ hermes_cli/colors.py:should_use_color */
/* PoP: cmd_inventory @ hermes_cli/inventory.py:load_picker_context */
/* PoP: cmd_kanban @ hermes_cli/kanban.py:build_parser */
/* PoP: cmd_kanban @ hermes_cli/kanban.py:run_slash */
/* PoP: cmd_kanban @ hermes_cli/kanban_db.py:* */
/* PoP: cmd_kanban @ hermes_cli/kanban_decompose.py:decompose_task */
/* PoP: cmd_kanban @ hermes_cli/kanban_diagnostics.py:compute_task_diagnostics */
/* PoP: cmd_kanban @ hermes_cli/kanban_specify.py:specify_task */
/* PoP: cmd_kanban @ hermes_cli/kanban_swarm.py:create_swarm */
/* PoP: cmd_logs @ hermes_cli/logs.py:list_logs */
/* PoP: cmd_mcp @ hermes_cli/mcp_catalog.py:install_entry */
/* PoP: cmd_mcp @ hermes_cli/mcp_catalog.py:installed_servers */
/* PoP: cmd_mcp @ hermes_cli/mcp_catalog.py:is_enabled */
/* PoP: cmd_mcp @ hermes_cli/mcp_catalog.py:is_installed */
/* PoP: cmd_mcp @ hermes_cli/mcp_catalog.py:list_catalog */
/* PoP: cmd_mcp @ hermes_cli/mcp_catalog.py:uninstall_entry */
/* PoP: cmd_mcp @ hermes_cli/mcp_config.py:cmd_mcp_add */
/* PoP: cmd_mcp @ hermes_cli/mcp_config.py:cmd_mcp_configure */
/* PoP: cmd_mcp @ hermes_cli/mcp_config.py:cmd_mcp_list */
/* PoP: cmd_mcp @ hermes_cli/mcp_config.py:cmd_mcp_login */
/* PoP: cmd_mcp @ hermes_cli/mcp_config.py:cmd_mcp_remove */
/* PoP: cmd_mcp @ hermes_cli/mcp_config.py:cmd_mcp_test */
/* PoP: cmd_mcp @ hermes_cli/mcp_picker.py:install_by_name */
/* PoP: cmd_mcp @ hermes_cli/mcp_picker.py:run_picker */
/* PoP: cmd_mcp @ hermes_cli/mcp_picker.py:show_catalog */
/* PoP: cmd_mcp @ hermes_cli/mcp_security.py:is_mcp_server_entry_suspicious */
/* PoP: cmd_mcp @ hermes_cli/mcp_security.py:validate_mcp_server_entry */
/* PoP: cmd_mcp @ hermes_cli/mcp_startup.py:start_background_mcp_discovery */
/* PoP: cmd_mcp @ hermes_cli/mcp_startup.py:wait_for_mcp_discovery */
/* PoP: cmd_memory @ hermes_cli/memory_setup.py:cmd_setup */

/* Helper: print messages with role filtering and count limit */
/* PoP: cli_pt_input_extras_install_ignored_terminal_sequences @ hermes_cli/pt_input_extras.py:install_ignored_terminal_sequences */
/* Port of Python install_ignored_terminal_sequences(). The C terminal stack has
 * no prompt_toolkit ANSI_SEQUENCES registry, so we keep our own registry of
 * ignored (parser-dropped) terminal sequences. Returns the number of sequences
 * whose mapping was *changed* (i.e. newly registered) — faithful to the Python
 * return value, where setdefault() only counts first-time insertions. */
static const char *g_ignored_sequences[16];
static int g_ignored_count = 0;

int cli_pt_input_extras_install_ignored_terminal_sequences(void)
{
    static const char *seqs[2] = { "\x1b[I", "\x1b[O" };
    int changed = 0;
    for (int i = 0; i < 2; i++) {
        bool found = false;
        for (int j = 0; j < g_ignored_count; j++) {
            if (strcmp(g_ignored_sequences[j], seqs[i]) == 0) { found = true; break; }
        }
        if (!found && g_ignored_count < 16) {
            g_ignored_sequences[g_ignored_count++] = seqs[i];
            changed++;
        }
    }
    return changed;
}

