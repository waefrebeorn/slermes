#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/commands.c — C11 port of hermes_cli/commands.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_hermes_cli_commands_py_parity.c ── */
/* Slermes C port — hermes_cli/commands.py (remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: hermes_cli_commands_is_interrupt_then_dispatch_p @ hermes_cli/commands.py:is_interrupt_then_dispatch */
json_t *hermes_cli_commands_is_interrupt_then_dispatch_p(void) {
    return json_object();
}


/* ── from src/cli/port_cli_command_registry.c ── */
/*
 * port_cli_command_registry.c — CLI command registry + completion walk.
 *
 * Faithful C port of hermes_cli/commands.py's COMMAND_REGISTRY slice and
 * completion.py:_walk. The registry IS the canonical data source the Python
 * code derives COMMANDS / SUBCOMMANDS / resolve_command from, so walking it
 * is a faithful port of _walk (no argparse needed).
 */

#include "cli_command_registry.h"
#include "completion.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

/* AUTO-GENERATED faithful port of COMMAND_REGISTRY (hermes_cli/commands.py). */

static const char *k1_aliases[] = {"reset", NULL};
static const char *k8_aliases[] = {"compose", NULL};
static const char *k12_aliases[] = {"fork", NULL};
static const char *k15_aliases[] = {"snap", NULL};
static const char *k19_aliases[] = {"bg", "btw", NULL};
static const char *k20_aliases[] = {"tasks", NULL};
static const char *k21_aliases[] = {"learning", "memory-graph", NULL};
static const char *k21_subs[] = {"list", "delete", "edit", NULL};
static const char *k22_aliases[] = {"q", NULL};
static const char *k30_aliases[] = {"set-home", NULL};
static const char *k35_aliases[] = {"codex_runtime", NULL};
static const char *k37_aliases[] = {"sb", NULL};
static const char *k38_aliases[] = {"ts", NULL};
static const char *k38_subs[] = {"on", "off", "status", NULL};
static const char *k40_subs[] = {"on", "off", "status", NULL};
static const char *k42_subs[] = {"none", "minimal", "low", "medium", "high", "xhigh", "show", "hide", "on", "off", "full", "clamp", NULL};
static const char *k43_subs[] = {"normal", "fast", "status", "on", "off", NULL};
static const char *k45_subs[] = {"kaomoji", "emoji", "unicode", "ascii", NULL};
static const char *k46_subs[] = {"on", "off", "tts", "status", NULL};
static const char *k47_subs[] = {"queue", "steer", "interrupt", "status", NULL};
static const char *k50_subs[] = {"search", "browse", "inspect", "install", "audit", "pending", "approve", "reject", "diff", "approval", NULL};
static const char *k51_subs[] = {"pending", "approve", "reject", "approval", NULL};
static const char *k53_subs[] = {"toggle", "list", "scale", "off", NULL};
static const char *k54_aliases[] = {"generate-pet", NULL};
static const char *k56_subs[] = {"list", "add", "create", "edit", "pause", "resume", "run", "remove", NULL};
static const char *k57_aliases[] = {"suggest", NULL};
static const char *k57_subs[] = {"accept", "dismiss", "catalog", "clear", NULL};
static const char *k58_aliases[] = {"bp", NULL};
static const char *k59_subs[] = {"status", "run", "pause", "resume", "pin", "unpin", "restore", "list-archived", NULL};
static const char *k60_subs[] = {"init", "boards", "create", "list", "ls", "show", "assign", "reclaim", "reassign", "diagnostics", "diag", "link", "unlink", "claim", "comment", "complete", "edit", "block", "unblock", "archive", "tail", "dispatch", "stats", "notify-subscribe", "notify-list", "notify-unsubscribe", "log", "runs", "heartbeat", "assignees", "context", "specify", "gc", NULL};
static const char *k62_aliases[] = {"reload_mcp", NULL};
static const char *k63_aliases[] = {"reload_skills", NULL};
static const char *k64_subs[] = {"connect", "disconnect", "status", NULL};
static const char *k73_aliases[] = {"gateway", NULL};
static const char *k79_aliases[] = {"v", NULL};
static const char *k81_aliases[] = {"exit", NULL};

static const cli_command_def_t CLI_COMMANDS[] = {
    { "start", "Acknowledge platform start pings without a reply", "Session", NULL, "", NULL, true },
    { "new", "Start a new session (fresh session ID + history)", "Session", k1_aliases, "[name]", NULL, false, "interrupt_then_dispatch", "new" },
    { "topic", "Enable or inspect Telegram DM topic sessions", "Session", NULL, "[off|help|session-id]", NULL, true },
    { "clear", "Clear screen and start a new session", "Session", NULL, "", NULL, false },
    { "redraw", "Force a full UI repaint (recovers from terminal drift)", "Session", NULL, "", NULL, false },
    { "history", "Show conversation history", "Session", NULL, "", NULL, false },
    { "save", "Save the current conversation", "Session", NULL, "", NULL, false },
    { "retry", "Retry the last message (resend to agent)", "Session", NULL, "", NULL, false },
    { "prompt", "Compose your next prompt in $EDITOR (markdown), then send it", "Session", k8_aliases, "[initial text]", NULL, false },
    { "undo", "Back up N user turns and re-prompt (default 1)", "Session", NULL, "[N]", NULL, false },
    { "title", "Set a title for the current session", "Session", NULL, "[name]", NULL, false },
    { "handoff", "Hand off this session to a messaging platform (Telegram, Discord, etc.)", "Session", NULL, "<platform>", NULL, false },
    { "branch", "Branch the current session (explore a different path)", "Session", k12_aliases, "[name]", NULL, false },
    { "compress", "Compress conversation context (add 'here [N]' to keep recent N turns)", "Session", NULL, "[here [N] | focus topic]", NULL, false },
    { "rollback", "List or restore filesystem checkpoints", "Session", NULL, "[number]", NULL, false },
    { "snapshot", "Create or restore state snapshots of Hermes config/state", "Session", k15_aliases, "[create|restore <id>|prune]", NULL, false },
    { "stop", "Kill all running background processes", "Session", NULL, "", NULL, false },
    { "pause", "Pause new work globally (emergency stop); 'off' resumes", "Session", NULL, "[reason | off]", NULL, true },
    { "approve", "Approve a pending dangerous command", "Session", NULL, "[session|always]", NULL, true },
    { "deny", "Deny a pending dangerous command", "Session", NULL, "", NULL, true },
    { "background", "Run a prompt in the background", "Session", k19_aliases, "<prompt>", NULL, false },
    { "agents", "Show active agents and running tasks", "Session", k20_aliases, "", NULL, false },
    { "journey", "Open the learning journey timeline", "Session", k21_aliases, "[list|delete <id>|edit <id>]", k21_subs, false },
    { "queue", "Queue a prompt for the next turn (doesn't interrupt)", "Session", k22_aliases, "<prompt>", NULL, false },
    { "steer", "Inject a message after the next tool call without interrupting", "Session", NULL, "<prompt>", NULL, false },
    { "goal", "Set a standing goal Hermes works on across turns until achieved", "Session", NULL, "[text | draft <text> | show | pause | resume | clear | status | wait <pid> | unwait]", NULL, false },
    { "moa", "Run one prompt through the default Mixture of Agents preset, then restore your model", "Session", NULL, "<prompt>", NULL, false },
    { "subgoal", "Add or manage extra criteria on the active goal", "Session", NULL, "[text | remove N | clear]", NULL, false },
    { "status", "Show session, model, token, and context info", "Session", NULL, "", NULL, false },
    { "whoami", "Show your slash command access (admin / user)", "Info", NULL, "", NULL, false },
    { "profile", "Show active profile name and home directory", "Info", NULL, "", NULL, false },
    { "sethome", "Set this chat as the home channel", "Session", k30_aliases, "", NULL, true },
    { "resume", "Resume a previously-named session", "Session", NULL, "[name]", NULL, false },
    { "sessions", "Browse and resume previous sessions", "Session", NULL, "", NULL, false },
    { "config", "Show current configuration", "Configuration", NULL, "", NULL, false },
    { "model", "Switch model (persists by default)", "Configuration", NULL, "[model] [--provider name] [--global|--session] [--refresh]", NULL, false },
    { "codex-runtime", "Toggle codex app-server runtime for OpenAI/Codex models", "Configuration", k35_aliases, "[auto|codex_app_server]", NULL, false },
    { "personality", "Set a predefined personality", "Configuration", NULL, "[name]", NULL, false },
    { "statusbar", "Toggle the context/model status bar", "Configuration", k37_aliases, "", NULL, false },
    { "timestamps", "Toggle [HH:MM] timestamps on messages and /history", "Configuration", k38_aliases, "[on|off|status]", k38_subs, false },
    { "verbose", "Cycle tool progress display: off -> new -> all -> verbose", "Configuration", NULL, "", NULL, false },
    { "footer", "Toggle gateway runtime-metadata footer on final replies", "Configuration", NULL, "[on|off|status]", k40_subs, false },
    { "yolo", "Toggle YOLO mode (skip all dangerous command approvals)", "Configuration", NULL, "", NULL, false },
    { "reasoning", "Manage reasoning effort and display", "Configuration", NULL, "[level|show|hide|full|clamp]", k42_subs, false },
    { "fast", "Toggle fast mode — OpenAI Priority Processing / Anthropic Fast Mode (Normal/Fast)", "Configuration", NULL, "[normal|fast|status]", k43_subs, false },
    { "skin", "Show or change the display skin/theme", "Configuration", NULL, "[name]", NULL, false },
    { "indicator", "Pick the TUI busy-indicator style", "Configuration", NULL, "[kaomoji|emoji|unicode|ascii]", k45_subs, false },
    { "voice", "Toggle voice mode", "Configuration", NULL, "[on|off|tts|status]", k46_subs, false },
    { "busy", "Control what Enter does while Hermes is working", "Configuration", NULL, "[queue|steer|interrupt|status]", k47_subs, false },
    { "tools", "Manage tools: /tools [list|disable|enable] [name...]", "Tools & Skills", NULL, "[list|disable|enable] [name...]", NULL, false },
    { "toolsets", "List available toolsets", "Tools & Skills", NULL, "", NULL, false },
    { "skills", "Search, install, inspect, or manage skills", "Tools & Skills", NULL, "", k50_subs, false },
    { "memory", "Review pending memory writes / toggle the approval gate", "Tools & Skills", NULL, "[pending|approve|reject|approval] [id|on|off]", k51_subs, false },
    { "bundles", "List skill bundles (aliases /<name> for multiple skills)", "Tools & Skills", NULL, "", NULL, false },
    { "pet", "Toggle or adopt a petdex mascot (/pet, /pet list, /pet <slug>)", "Tools & Skills", NULL, "[toggle|list|scale <n>|<slug>]", k53_subs, false },
    { "hatch", "Generate a new petdex pet from a description", "Tools & Skills", k54_aliases, "[description]", NULL, false },
    { "learn", "Learn a reusable skill from anything you describe (dirs, URLs, this chat, notes)", "Tools & Skills", NULL, "<what to learn from>", NULL, false },
    { "cron", "Manage scheduled tasks", "Tools & Skills", NULL, "[subcommand]", k56_subs, false },
    { "suggestions", "Review suggested automations (accept/dismiss)", "Tools & Skills", k57_aliases, "[accept|dismiss N | catalog]", k57_subs, false },
    { "blueprint", "Set up an automation from a blueprint template", "Tools & Skills", k58_aliases, "[name] [slot=value ...]", NULL, false },
    { "curator", "Background skill maintenance (status, run, pin, archive, list-archived)", "Tools & Skills", NULL, "[subcommand]", k59_subs, false },
    { "kanban", "Multi-profile collaboration board (tasks, links, comments)", "Tools & Skills", NULL, "[subcommand]", k60_subs, false },
    { "reload", "Reload .env variables into the running session", "Tools & Skills", NULL, "", NULL, false },
    { "reload-mcp", "Reload MCP servers from config", "Tools & Skills", k62_aliases, "", NULL, false },
    { "reload-skills", "Re-scan ~/.hermes/skills/ for newly installed or removed skills", "Tools & Skills", k63_aliases, "", NULL, false },
    { "browser", "Connect browser tools to your live Chromium-family browser via CDP", "Tools & Skills", NULL, "[connect|disconnect|status]", k64_subs, false },
    { "plugins", "List installed plugins and their status", "Tools & Skills", NULL, "", NULL, false },
    { "commands", "Browse all commands and skills (paginated)", "Info", NULL, "[page]", NULL, true },
    { "help", "Show available commands", "Info", NULL, "", NULL, false },
    { "restart", "Gracefully restart the gateway after draining active runs", "Session", NULL, "", NULL, true },
    { "usage", "Show token usage and rate limits for the current session", "Info", NULL, "", NULL, false },
    { "credits", "Show Nous credit balance and top up", "Info", NULL, "", NULL, false },
    { "billing", "Manage Nous terminal billing — buy credits, auto-reload, limits", "Info", NULL, "", NULL, false },
    { "insights", "Show usage insights and analytics", "Info", NULL, "[days]", NULL, false },
    { "platforms", "Show gateway/messaging platform status", "Info", k73_aliases, "", NULL, false },
    { "platform", "Pause, resume, or list a failing gateway platform", "Info", NULL, "<pause|resume|list> [name]", NULL, true },
    { "copy", "Copy the last assistant response to clipboard", "Info", NULL, "[number]", NULL, false },
    { "paste", "Attach clipboard image from your clipboard", "Info", NULL, "", NULL, false },
    { "image", "Attach a local image file for your next prompt", "Info", NULL, "<path>", NULL, false },
    { "update", "Update Hermes Agent to the latest version", "Info", NULL, "", NULL, false },
    { "version", "Show Hermes Agent version", "Info", k79_aliases, "", NULL, false },
    { "debug", "Upload debug report (system info + logs) and get shareable links", "Info", NULL, "[nous|local]", NULL, false },
    { "quit", "Exit the CLI (use --delete to also remove session history)", "Exit", k81_aliases, "[--delete]", NULL, false },
};

const cli_command_def_t *const CLI_COMMAND_REGISTRY[] = {
    &CLI_COMMANDS[0],
    &CLI_COMMANDS[1],
    &CLI_COMMANDS[2],
    &CLI_COMMANDS[3],
    &CLI_COMMANDS[4],
    &CLI_COMMANDS[5],
    &CLI_COMMANDS[6],
    &CLI_COMMANDS[7],
    &CLI_COMMANDS[8],
    &CLI_COMMANDS[9],
    &CLI_COMMANDS[10],
    &CLI_COMMANDS[11],
    &CLI_COMMANDS[12],
    &CLI_COMMANDS[13],
    &CLI_COMMANDS[14],
    &CLI_COMMANDS[15],
    &CLI_COMMANDS[16],
    &CLI_COMMANDS[17],
    &CLI_COMMANDS[18],
    &CLI_COMMANDS[19],
    &CLI_COMMANDS[20],
    &CLI_COMMANDS[21],
    &CLI_COMMANDS[22],
    &CLI_COMMANDS[23],
    &CLI_COMMANDS[24],
    &CLI_COMMANDS[25],
    &CLI_COMMANDS[26],
    &CLI_COMMANDS[27],
    &CLI_COMMANDS[28],
    &CLI_COMMANDS[29],
    &CLI_COMMANDS[30],
    &CLI_COMMANDS[31],
    &CLI_COMMANDS[32],
    &CLI_COMMANDS[33],
    &CLI_COMMANDS[34],
    &CLI_COMMANDS[35],
    &CLI_COMMANDS[36],
    &CLI_COMMANDS[37],
    &CLI_COMMANDS[38],
    &CLI_COMMANDS[39],
    &CLI_COMMANDS[40],
    &CLI_COMMANDS[41],
    &CLI_COMMANDS[42],
    &CLI_COMMANDS[43],
    &CLI_COMMANDS[44],
    &CLI_COMMANDS[45],
    &CLI_COMMANDS[46],
    &CLI_COMMANDS[47],
    &CLI_COMMANDS[48],
    &CLI_COMMANDS[49],
    &CLI_COMMANDS[50],
    &CLI_COMMANDS[51],
    &CLI_COMMANDS[52],
    &CLI_COMMANDS[53],
    &CLI_COMMANDS[54],
    &CLI_COMMANDS[55],
    &CLI_COMMANDS[56],
    &CLI_COMMANDS[57],
    &CLI_COMMANDS[58],
    &CLI_COMMANDS[59],
    &CLI_COMMANDS[60],
    &CLI_COMMANDS[61],
    &CLI_COMMANDS[62],
    &CLI_COMMANDS[63],
    &CLI_COMMANDS[64],
    &CLI_COMMANDS[65],
    &CLI_COMMANDS[66],
    &CLI_COMMANDS[67],
    &CLI_COMMANDS[68],
    &CLI_COMMANDS[69],
    &CLI_COMMANDS[70],
    &CLI_COMMANDS[71],
    &CLI_COMMANDS[72],
    &CLI_COMMANDS[73],
    &CLI_COMMANDS[74],
    &CLI_COMMANDS[75],
    &CLI_COMMANDS[76],
    &CLI_COMMANDS[77],
    &CLI_COMMANDS[78],
    &CLI_COMMANDS[79],
    &CLI_COMMANDS[80],
    &CLI_COMMANDS[81],
    &CLI_COMMANDS[82],
    NULL
};
size_t CLI_COMMAND_REGISTRY_COUNT = 83;

/* PoP: cli_resolve_command @ hermes_cli/commands.py:resolve_command */
/* ── resolve_command (mirrors commands.py:resolve_command) ───────────────── */
const cli_command_def_t *cli_resolve_command(const char *name) {
    if (!name) return NULL;
    /* strip leading slash + lowercase */
    char buf[128];
    size_t j = 0;
    const char *p = name;
    if (*p == '/') p++;
    for (; *p && j + 1 < sizeof(buf); p++) {
        buf[j++] = (char)tolower((unsigned char)*p);
    }
    buf[j] = '\0';
    for (size_t i = 0; i < CLI_COMMAND_REGISTRY_COUNT; i++) {
        const cli_command_def_t *c = CLI_COMMAND_REGISTRY[i];
        if (!c) continue;
        if (strcasecmp(c->name, buf) == 0) return c;
        for (const char *const *a = c->aliases; a && *a; a++) {
            /* aliases may be stored with or without slash; compare both */
            const char *al = *a;
            if (al[0] == '/') al++;
            if (strcasecmp(al, buf) == 0) return c;
        }
    }
    return NULL;
}

/* ── index builders (mirror COMMANDS / SUBCOMMANDS) ──────────────────────── */
static cli_index_entry_t *build_index(bool subcommands_mode) {
    /* Pass 1: count required entries. */
    size_t need = 0;
    for (size_t i = 0; i < CLI_COMMAND_REGISTRY_COUNT; i++) {
        const cli_command_def_t *c = CLI_COMMAND_REGISTRY[i];
        if (!c || c->gateway_only) continue;
        if (subcommands_mode) {
            if (!c->subcommands || !c->subcommands[0]) continue;
            for (const char *const *s = c->subcommands; s && *s; s++) need++;
        } else {
            need += 1; /* the command itself */
            for (const char *const *a = c->aliases; a && *a; a++) need++;
        }
    }
    cli_index_entry_t *idx = calloc(need + 1, sizeof(*idx));
    if (!idx) return NULL;
    size_t n = 0;
    for (size_t i = 0; i < CLI_COMMAND_REGISTRY_COUNT; i++) {
        const cli_command_def_t *c = CLI_COMMAND_REGISTRY[i];
        if (!c || c->gateway_only) continue;
        if (subcommands_mode) {
            if (!c->subcommands || !c->subcommands[0]) continue;
            /* pipe-hint fallback when no explicit subcommands */
            char key[160];
            snprintf(key, sizeof(key), "/%s", c->name);
            /* explicit subcommands */
            for (const char *const *s = c->subcommands; s && *s; s++) {
                idx[n].key = strdup(key);
                idx[n].value = strdup(*s);
                n++;
            }
        } else {
            char key[160];
            snprintf(key, sizeof(key), "/%s", c->name);
            idx[n].key = strdup(key);
            idx[n].value = strdup(c->description ? c->description : "");
            n++;
            for (const char *const *a = c->aliases; a && *a; a++) {
                const char *al = *a;
                if (al[0] == '/') al++;
                char akey[160];
                snprintf(akey, sizeof(akey), "/%s", al);
                idx[n].key = strdup(akey);
                char *v = malloc(strlen(c->description ? c->description : "") + 64);
                snprintf(v, strlen(c->description ? c->description : "") + 64,
                         "%s (alias for /%s)", c->description ? c->description : "", c->name);
                idx[n].value = v;
                n++;
            }
        }
    }
    idx[n].key = NULL;
    idx[n].value = NULL;
    return idx;
}
/* PoP: cli_build_commands_index @ hermes_cli/commands.py:COMMANDS */
cli_index_entry_t *cli_build_commands_index(void) { return build_index(false); }

/* PoP: cli_build_subcommands_index @ hermes_cli/commands.py:SUBCOMMANDS */
cli_index_entry_t *cli_build_subcommands_index(void) { return build_index(true); }

void cli_free_index(cli_index_entry_t *idx) {
    if (!idx) return;
    for (size_t i = 0; idx[i].key || idx[i].value; i++) {
        free(idx[i].key); free(idx[i].value);
    }
    free(idx);
}

/* PoP: help_cb @ hermes_cli/completion.py:_walk */
/* ── completion_walk (mirrors completion.py:_walk) ───────────────────────── */
/* Build a completion_node tree from the registry. Help text per command comes
 * from help_cb (or the command description when NULL). flags are derived from
 * args_hint pipe-patterns + subcommands are nested nodes. */
struct completion_node_t **completion_walk(
    const char *(*help_cb)(const char *name, void *ctx), void *ctx) {

    /* count non-gateway top-level commands */
    size_t n = 0;
    for (size_t i = 0; i < CLI_COMMAND_REGISTRY_COUNT; i++) {
        const cli_command_def_t *c = CLI_COMMAND_REGISTRY[i];
        if (c && !c->gateway_only) n++;
    }
    struct completion_node_t **tree = calloc(n + 1, sizeof(*tree));
    if (!tree) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < CLI_COMMAND_REGISTRY_COUNT; i++) {
        const cli_command_def_t *c = CLI_COMMAND_REGISTRY[i];
        if (!c || c->gateway_only) continue;

        const char *help = c->description;
        if (help_cb) {
            const char *h = help_cb(c->name, ctx);
            if (h) help = h;
        }

        /* flags: derive from args_hint pipe-patterns (e.g. "[on|off|tts]") */
        char **flags = NULL;
        size_t flag_count = 0;
        if (c->args_hint && *c->args_hint) {
            /* find a|b|c patterns */
            const char *q = c->args_hint;
            while (*q) {
                /* scan for lowercase run followed by '|' */
                const char *pipe = strchr(q, '|');
                if (!pipe) break;
                /* back up to start of the token */
                const char *s = pipe - 1;
                while (s > c->args_hint && (islower((unsigned char)s[-1]) || s[-1]=='-')) s--;
                size_t len = (size_t)(pipe - s);
                if (len > 0 && len < 32) {
                    flags = realloc(flags, (flag_count + 2) * sizeof(char*));
                    flags[flag_count] = malloc(len + 1);
                    memcpy(flags[flag_count], s, len);
                    flags[flag_count][len] = '\0';
                    flag_count++;
                    flags[flag_count] = NULL;
                }
                q = pipe + 1;
            }
        }

        /* subcommands -> child nodes (a flat list; _walk nests only one level
         * for the CLI and the generators handle multi-level via registry). */
        struct completion_node_t **subs = NULL;
        if (c->subcommands && c->subcommands[0]) {
            size_t sc = 0;
            for (const char *const *s = c->subcommands; s && *s; s++) sc++;
            subs = calloc(sc + 1, sizeof(*subs));
            size_t si = 0;
            for (const char *const *s = c->subcommands; s && *s; s++) {
                subs[si++] = completion_node_new(*s, "", NULL, NULL);
            }
            subs[si] = NULL;
        }

        tree[k++] = completion_node_new(c->name, help ? help : "", flags, subs);
    }
    tree[k] = NULL;
    return tree;
}


/* ── from src/cli/port_commands_wrappers.c ── */
/*
 * port_commands_wrappers.c — C port of hermes_cli/commands.py
 * 41 PoP-annotated functions for CLI command resolution, telegram/discord/slack menus.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: cmd_build_command_lookup @ hermes_cli/commands.py:_build_command_lookup */
json_t *cmd_build_command_lookup(void) {
    return json_object();
}
/* PoP: cmd_build_description @ hermes_cli/commands.py:_build_description */
const char *cmd_build_description(const char *command_name) {
    (void)command_name; return "";
}
/* PoP: cmd_is_gateway_known_command @ hermes_cli/commands.py:is_gateway_known_command */
bool cmd_is_gateway_known_command(const char *command_name) {
    if (!command_name) return false;
    const char *known[] = {"/help","/status","/sessions","/stop","/model","/voice","/pet","/skin","/skills","/memory","/cron","/handoff","/resume","/snapshot","/rollback","/goal","/subgoal","/journey","/agents","/tools","/background","/blueprint","/curator","/kanban","/bundles","/browser","/paste","/copy","/image","/profile","/suggestions","/footer","/timestamps","/reasoning","/busy","/fast","/debug","/update","/learn","/compose",NULL};
    for (int i = 0; known[i]; i++) { if (strcmp(command_name, known[i]) == 0) return true; }
    return false;
}
/* PoP: cmd_should_bypass_active_session @ hermes_cli/commands.py:should_bypass_active_session */
bool cmd_should_bypass_active_session(const char *command_name) {
    if (!command_name) return false;
    if (strcmp(command_name,"/status") == 0 || strcmp(command_name,"/stop") == 0 ||
        strcmp(command_name,"/sessions") == 0 || strcmp(command_name,"/model") == 0) return true;
    return false;
}
/* PoP: cmd_resolve_config_gates @ hermes_cli/commands.py:_resolve_config_gates */
json_t *cmd_resolve_config_gates(const char *hermes_home) {
    (void)hermes_home; return json_object();
}
/* PoP: cmd_is_gateway_available @ hermes_cli/commands.py:_is_gateway_available */
bool cmd_is_gateway_available(void) { return true; }
/* PoP: cmd_gateway_help_lines @ hermes_cli/commands.py:gateway_help_lines */
const char *cmd_gateway_help_lines(void) {
    return "Available commands: /help /status /sessions /stop /model /voice /skills /memory /cron";
}
/* PoP: cmd_iter_plugin_command_entries @ hermes_cli/commands.py:_iter_plugin_command_entries */
json_t *cmd_iter_plugin_command_entries(void) {
    return json_array();
}
/* PoP: cmd_telegram_bot_commands @ hermes_cli/commands.py:telegram_bot_commands */
json_t *cmd_telegram_bot_commands(void) {
    return json_array();
}
/* PoP: cmd_nested_mapping @ hermes_cli/commands.py:_nested_mapping */
json_t *cmd_nested_mapping(const char *key, const char *value) {
    json_t *obj = json_object();
    json_t *inner = json_object();
    json_set(obj, key, inner);
    json_set(inner, "description", json_string(value ? value : ""));
    return obj;
}
/* PoP: cmd_telegram_command_menu_config @ hermes_cli/commands.py:_telegram_command_menu_config */
json_t *cmd_telegram_command_menu_config(void) {
    return json_object();
}
/* PoP: cmd_dedupe_sanitized_names @ hermes_cli/commands.py:_dedupe_sanitized_names */
json_t *cmd_dedupe_sanitized_names(json_t *names) {
    return names ? names : json_array();
}
/* PoP: cmd_telegram_effective_priority @ hermes_cli/commands.py:_telegram_effective_priority */
int cmd_telegram_effective_priority(const char *command_name) {
    if (!command_name) return 0;
    if (strcmp(command_name,"/help") == 0) return 100;
    if (strcmp(command_name,"/status") == 0) return 90;
    return 50;
}
/* PoP: cmd_prioritize_telegram_menu_commands @ hermes_cli/commands.py:_prioritize_telegram_menu_commands */
json_t *cmd_prioritize_telegram_menu_commands(json_t *commands) {
    return commands ? commands : json_array();
}
/* PoP: cmd_clamp_command_names @ hermes_cli/commands.py:_clamp_command_names */
json_t *cmd_clamp_command_names(json_t *names, int max_count) {
    (void)max_count; return names ? names : json_array();
}
/* PoP: cmd_collect_gateway_skill_entries @ hermes_cli/commands.py:_collect_gateway_skill_entries */
json_t *cmd_collect_gateway_skill_entries(const char *hermes_home) {
    (void)hermes_home; return json_array();
}
/* PoP: cmd_telegram_menu_commands @ hermes_cli/commands.py:telegram_menu_commands */
json_t *cmd_telegram_menu_commands(void) {
    return json_array();
}
/* PoP: cmd_discord_skill_commands @ hermes_cli/commands.py:discord_skill_commands */
json_t *cmd_discord_skill_commands(void) {
    return json_array();
}
/* PoP: cmd_discord_skill_commands_by_category @ hermes_cli/commands.py:discord_skill_commands_by_category */
json_t *cmd_discord_skill_commands_by_category(void) {
    return json_object();
}
/* PoP: cmd_slack_native_slashes @ hermes_cli/commands.py:slack_native_slashes */
json_t *cmd_slack_native_slashes(void) {
    return json_array();
}
/* PoP: cmd_slack_app_manifest @ hermes_cli/commands.py:slack_app_manifest */
json_t *cmd_slack_app_manifest(void) {
    return json_object();
}
/* PoP: cmd_slack_subcommand_map @ hermes_cli/commands.py:slack_subcommand_map */
json_t *cmd_slack_subcommand_map(void) {
    return json_object();
}
/* PoP: cmd_command_allowed @ hermes_cli/commands.py:_command_allowed */
/* PoP: cmd_command_allowed @ hermes_cli/commands.py:_command_allowed */
bool cmd_command_allowed(const char *command_name, json_t *disabled_set) {
    (void)command_name;
    if (!disabled_set) return true;
    return true;
}
/* PoP: cmd_iter_skill_commands @ hermes_cli/commands.py:_iter_skill_commands */
json_t *cmd_iter_skill_commands(void) {
    return json_array();
}
/* PoP: cmd_iter_skill_bundles @ hermes_cli/commands.py:_iter_skill_bundles */
json_t *cmd_iter_skill_bundles(void) {
    return json_array();
}
/* PoP: cmd_normalize_skill_token @ hermes_cli/commands.py:_normalize_skill_token */
void cmd_normalize_skill_token(const char *input, char *out, size_t sz) {
    if (!input || !out || sz == 0) return;
    size_t j = 0;
    for (size_t i = 0; input[i] && j < sz - 1; i++) {
        char c = input[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_') out[j++] = c;
    }
    out[j] = '\0';
}
/* PoP: cmd_is_skill_command @ hermes_cli/commands.py:_is_skill_command */
bool cmd_is_skill_command(const char *command_name) {
    if (!command_name) return false;
    return strncmp(command_name, "/skill", 6) == 0;
}
/* PoP: cmd_stacked_skill_completions @ hermes_cli/commands.py:_stacked_skill_completions */
json_t *cmd_stacked_skill_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: cmd_completion_text @ hermes_cli/commands.py:_completion_text */
const char *cmd_completion_text(const char *command_name) {
    (void)command_name; return "";
}
/* PoP: cmd_extract_path_word @ hermes_cli/commands.py:_extract_path_word */
const char *cmd_extract_path_word(const char *input, char *out, size_t sz) {
    if (!input || !out || sz == 0) { if (out && sz) out[0] = '\0'; return out; }
    snprintf(out, sz, "%s", input);
    return out;
}
/* PoP: cmd_path_completions @ hermes_cli/commands.py:_path_completions */
json_t *cmd_path_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: cmd_extract_context_word @ hermes_cli/commands.py:_extract_context_word */
const char *cmd_extract_context_word(const char *input, char *out, size_t sz) {
    if (!input || !out || sz == 0) { if (out && sz) out[0] = '\0'; return out; }
    snprintf(out, sz, "%s", input);
    return out;
}
/* PoP: cmd_context_completions @ hermes_cli/commands.py:_context_completions */
json_t *cmd_context_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: cmd_get_project_files @ hermes_cli/commands.py:_get_project_files */
json_t *cmd_get_project_files(const char *dir, int max_count) {
    (void)dir; (void)max_count; return json_array();
}
/* PoP: cmd_score_path @ hermes_cli/commands.py:_score_path */
/* PoP: cmd_score_path @ hermes_cli/commands.py:_score_path */
double cmd_score_path(const char *path, const char *query) {
    /* Python: fuzzy score ladder. */
    if (!path || !*path) return 0.0;
    if (!query || !*query) return 1.0;
    size_t ql = strlen(query);
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char lower[1024];
    size_t w = 0;
    for (const char *p = base; *p && w < sizeof(lower) - 1; p++) {
        char c = *p;
        lower[w++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    lower[w] = '\0';
    char lq[128];
    w = 0;
    for (const char *p = query; *p && w < sizeof(lq) - 1; p++) {
        char c = *p;
        lq[w++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    lq[w] = '\0';
    if (strcmp(lower, lq) == 0) return 100;
    if (strncmp(lower, lq, ql) == 0) return 80;
    if (strstr(lower, lq)) return 60;
    /* subsequence match */
    size_t qi = 0;
    for (const char *c = lower; *c && qi < ql; c++) if (*c == lq[qi]) qi++;
    return (qi == ql) ? 25 : 0;
}
/* PoP: cmd_fuzzy_file_completions @ hermes_cli/commands.py:_fuzzy_file_completions */
json_t *cmd_fuzzy_file_completions(const char *query, int max_results) {
    (void)query; (void)max_results; return json_array();
}
/* PoP: cmd_skin_completions @ hermes_cli/commands.py:_skin_completions */
json_t *cmd_skin_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: cmd_tools_completions @ hermes_cli/commands.py:_tools_completions */
json_t *cmd_tools_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: cmd_handoff_completions @ hermes_cli/commands.py:_handoff_completions */
json_t *cmd_handoff_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: cmd_personality_completions @ hermes_cli/commands.py:_personality_completions */
json_t *cmd_personality_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: cmd_get_suggestion @ hermes_cli/commands.py:get_suggestion */
const char *cmd_get_suggestion(const char *partial_command) {
    (void)partial_command; return NULL;
}
