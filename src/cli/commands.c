/*
 * commands.c — Slash command definitions for Hermes C CLI.
 * Central registry of all slash commands. Phase 51-60: CLI parity.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_cron.h"
#include "hermes_skills.h"
#include "hermes_cli.h"
#include "cli_cmd_system.h"
#include "cli_cmd_help.h"
#include "cli_cmd_session.h"
#include "cli_cmd_config.h"
#include "cli_cmd_misc.h"
#include "cli_cmd_gateway.h"
#include "cli_cmd_skills.h"
#include "cli_cmd_tools.h"
#include "cli_cmd_mcp.h"
#include "cli_cmd_kanban.h"
#include "cli_cmd_parity.h"
#include "cli_command_registry.h"
#include "cli_cmd_security.h"
#include "cli_cmd_memory.h"
#include "cli_cmd_display.h"
#include "commands_shared.h"
#include "pet.h"
/*
 * Port of Python hermes_cli commands: build_info.py, platforms.py,
 * session_recap.py, status.py, cron.py, model_catalog.py, model_switch.py,
 * models.py, providers.py, skills_config.py, fallback_cmd.py, debug.py,
 * partial_compress.py.
 * C commands.c implements 93 slash commands directly.
 */
#include "hermes_secrets.h"
#include "hermes_gateway.h"
#include "hermes_skills_hub.h"
#include "hermes_web_dashboard.h"
#include "mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <sys/utsname.h>

#include "provider.h"
#include "provider_metadata.h"
#include "usage_pricing.h"
#include "hermes_insights.h"
#include "hermes_display.h"
#include "hermes_curator.h"
#include "hermes_skin.h"
#include "skill_usage.h"
#include "hermes_skill_commands.h"
#include "skill_bundles.h"
#include "hermes_auth.h"
#include <ctype.h>

/* Partial-compress keep-count clamp.
 * Port of Python hermes_cli/partial_compress.py:_coerce_keep.
 * Clamps a keep-count token to [1, MAX_KEEP_LAST]; falls back to
 * DEFAULT_KEEP_LAST on non-integer input. */

/* Tool handler declarations (used by session commands) */
extern char *session_search_handler(const char *args_json, const char *task_id);
extern char *session_crud_handler(const char *args_json, const char *task_id);


/* Forward declaration for send_message_handler from tools/send_message.c */
extern char *send_message_handler(const char *args_json, const char *task_id);

/* Forward declaration for delegate_list from tools/delegate.c */
extern void delegate_list(void *result);


/* Registry — mirroring Python Hermes COMMAND_REGISTRY */
/* Registry — mirroring Python Hermes COMMAND_REGISTRY */
const command_def_t COMMANDS[] = {
    {.name="/new", .alias="/n", .description="Start a new conversation session", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_new},
    {.name="/clear", .alias="/c", .description="Clear conversation context", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_clear, .cli_only=true, .gateway_only=false},
    {.name="/undo", .alias="/u", .description="Remove the last assistant response", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_undo},
    {.name="/save", .alias="/s", .description="Save current session", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_save, .cli_only=true, .gateway_only=false},
    {.name="/load", .alias=NULL, .description="Load a session: /load <session_id>", .category="Session", .args_hint="<session_id>", .subcommands=NULL, .handler=cmd_load},
    {.name="/sessions", .alias=NULL, .description="List saved sessions", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_sessions},
    {.name="/stats", .alias=NULL, .description="Show current session statistics", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_stats},
    {.name="/recap", .alias=NULL, .description="Summarize recent session activity", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_recap},
    {.name="/conv", .alias=NULL, .description="Show recent conversation messages", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_conv},
    {.name="/history", .alias=NULL, .description="Show full conversation history", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_history, .cli_only=true, .gateway_only=false},
    {.name="/model", .alias="/m", .description="Model mgmt: list, show, providers, set", .category="Config", .args_hint="[list|show|providers|set]", .subcommands="list,show,providers,set", .handler=cmd_model},
    {.name="/config", .alias="/cfg", .description="Show or edit configuration", .category="Config", .args_hint="[key] [val]", .subcommands=NULL, .handler=cmd_config, .cli_only=true, .gateway_only=false},
    {.name="/setup", .alias=NULL, .description="Setup wizard: provider, model, API key. Sections: model|tts|terminal|gateway|tools|agent", .category="Config", .args_hint="[--quick|--non-interactive|--reset|--portal|section]", .subcommands="model,tts,terminal,gateway,tools,agent", .handler=cmd_setup, .cli_only=true, .gateway_only=false},
    {.name="/uninstall", .alias=NULL, .description="Uninstall Slermes: removes binary, config, .env", .category="Config", .args_hint=NULL, .subcommands=NULL, .handler=cmd_uninstall, .cli_only=true, .gateway_only=false},
    {.name="/backup", .alias=NULL, .description="Backup config, .env, and sessions", .category="Config", .args_hint="[config|full]", .subcommands="config,full", .handler=cmd_backup, .cli_only=true, .gateway_only=false},
    {.name="/topic", .alias="/t", .description="Set the system topic/personality", .category="Config", .args_hint="<text>", .subcommands=NULL, .handler=cmd_topic},
    {.name="/tools", .alias=NULL, .description="List available tools and their status", .category="Tools", .args_hint=NULL, .subcommands=NULL, .handler=cmd_tools, .cli_only=true, .gateway_only=false},
    {.name="/tools-verify", .alias=NULL, .description="Verify all expected tools are registered", .category="Tools", .args_hint=NULL, .subcommands=NULL, .handler=cmd_tools_verify, .cli_only=true, .gateway_only=false},
    {.name="/commands", .alias="/cmds", .description="List all available slash commands", .category="Tools", .args_hint=NULL, .subcommands=NULL, .handler=cmd_commands, .cli_only=true, .gateway_only=false},
    {.name="/skills-hub", .alias="/hub", .description="Skills hub: search, show, list, sync", .category="Skills", .args_hint="[search|show|list|sync]", .subcommands="search,show,list,sync", .handler=cmd_skills_hub},
    {.name="/help", .alias="/h", .description="Show help for commands", .category="Help", .args_hint="[command]", .subcommands=NULL, .handler=cmd_help},
    {.name="/exit", .alias="/quit", .description="Exit the program", .category="System", .args_hint=NULL, .subcommands=NULL, .handler=cmd_exit},
    {.name="/reset", .alias="/r", .description="Reset session: clear all messages and start fresh", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_reset},
    {.name="/retry", .alias=NULL, .description="Retry the last LLM call", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_retry},
    {.name="/compress", .alias="/cctx", .description="Compress conversation context", .category="Session", .args_hint="<keep_count>", .subcommands=NULL, .handler=cmd_compress},
    {.name="/branch", .alias=NULL, .description="Branch from current session", .category="Session", .args_hint="[message_index]", .subcommands=NULL, .handler=cmd_branch},
    {.name="/snapshot", .alias="/snap", .description="Save a named snapshot", .category="Session", .args_hint="[name]", .subcommands=NULL, .handler=cmd_snapshot, .cli_only=true, .gateway_only=false},
    {.name="/status", .alias="/st", .description="Show session status and configuration info", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_status},
    {.name="/stop", .alias=NULL, .description="Kill all running background processes", .category="System", .args_hint=NULL, .subcommands=NULL, .handler=cmd_stop},
    {.name="/approve", .alias=NULL, .description="Approve a pending dangerous command", .category="Security", .args_hint=NULL, .subcommands=NULL, .handler=cmd_approve, .cli_only=false, .gateway_only=true},
    {.name="/deny", .alias=NULL, .description="Deny a pending dangerous command", .category="Security", .args_hint=NULL, .subcommands=NULL, .handler=cmd_deny, .cli_only=false, .gateway_only=true},
    {.name="/title", .alias=NULL, .description="Set a title for the current session", .category="Session", .args_hint="<title>", .subcommands=NULL, .handler=cmd_title},
    {.name="/resume", .alias=NULL, .description="Resume a previously-named session", .category="Session", .args_hint="<id>", .subcommands=NULL, .handler=cmd_resume},
    {.name="/yolo", .alias=NULL, .description="Toggle YOLO mode (skip dangerous command approvals)", .category="Config", .args_hint=NULL, .subcommands=NULL, .handler=cmd_yolo},
    {.name="/usage", .alias=NULL, .description="Show token usage and session statistics", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_usage},
    {.name="/plugins", .alias=NULL, .description="Plugin mgmt: list, show, install, remove", .category="Plugins", .args_hint="[list|show|install|remove]", .subcommands="list,show,install,remove", .handler=cmd_plugins, .cli_only=true, .gateway_only=false},
    {.name="/platforms", .alias=NULL, .description="Show gateway/messaging platform status", .category="Gateway", .args_hint=NULL, .subcommands=NULL, .handler=cmd_platforms, .cli_only=false, .gateway_only=true},
    {.name="/redraw", .alias=NULL, .description="Force a full UI repaint", .category="Display", .args_hint=NULL, .subcommands=NULL, .handler=cmd_redraw, .cli_only=true, .gateway_only=false},
    {.name="/background", .alias="/bg", .description="Run a prompt in the background", .category="Session", .args_hint="<prompt>", .subcommands=NULL, .handler=cmd_background},
    {.name="/verbose", .alias=NULL, .description="Toggle tool progress display verbosity", .category="Display", .args_hint=NULL, .subcommands=NULL, .handler=cmd_verbose, .cli_only=true, .gateway_only=false},
    {.name="/skin", .alias=NULL, .description="Show or change the display skin/theme", .category="Display", .args_hint="<name>", .subcommands=NULL, .handler=cmd_skin, .cli_only=true, .gateway_only=false},
    {.name="/personality", .alias="/p", .description="Set a predefined personality system message", .category="Config", .args_hint="<name>", .subcommands=NULL, .handler=cmd_personality},
    {.name="/whoami", .alias=NULL, .description="Show your slash command access level", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_whoami, .cli_only=true, .gateway_only=false},
    {.name="/profile", .alias=NULL, .description="Show active profile name and home directory", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_profile, .cli_only=true, .gateway_only=false},
    {.name="/goal", .alias=NULL, .description="Set a standing goal", .category="Session", .args_hint="<text>", .subcommands=NULL, .handler=cmd_goal},
    {.name="/agents", .alias=NULL, .description="Show active subagents and running tasks", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_agents},
    {.name="/reasoning", .alias="/re", .description="Manage reasoning effort and display", .category="Config", .args_hint="[level|show|hide]", .subcommands="on,off,show,hide,low,medium,high", .handler=cmd_reasoning},
    {.name="/toolsets", .alias=NULL, .description="List available toolsets", .category="Tools", .args_hint=NULL, .subcommands=NULL, .handler=cmd_toolsets, .cli_only=true, .gateway_only=false},
    {.name="/skills", .alias=NULL, .description="Search and manage installed skills", .category="Skills", .args_hint=NULL, .subcommands=NULL, .handler=cmd_skills, .cli_only=true, .gateway_only=false},
    {.name="/secrets", .alias=NULL, .description="Manage secrets: list, get, sync, status", .category="Security", .args_hint="[list|get|sync|status]", .subcommands="list,get,sync,status", .handler=cmd_secrets},
    {.name="/auth", .alias=NULL, .description="Provider auth status: status, providers", .category="Security", .args_hint="[status|providers]", .subcommands="status,providers", .handler=cmd_auth},
    {.name="/doctor", .alias=NULL, .description="System diagnostics: all, config, env, keys, system", .category="System", .args_hint="[all|config|env|keys|system]", .subcommands="all,config,env,keys,system", .handler=cmd_doctor, .cli_only=true, .gateway_only=false},
    {.name="/webhook", .alias=NULL, .description="Manage webhook subscriptions: list, add, remove", .category="Gateway", .args_hint="[list|add|remove]", .subcommands="list,add,remove", .handler=cmd_webhook, .cli_only=false, .gateway_only=true},
    {.name="/memory", .alias=NULL, .description="Memory setup: status, providers, setup", .category="Memory", .args_hint="[status|providers|setup]", .subcommands="status,providers,setup", .handler=cmd_memory},
    {.name="/gateway", .alias=NULL, .description="Manage gateway: status, list, stop, setup, restart", .category="Gateway", .args_hint="[status|list|stop|setup|restart]", .subcommands="status,list,stop,setup,restart", .handler=cmd_gateway},
    {.name="/completions", .alias=NULL, .description="Generate shell completions: bash, zsh, fish", .category="System", .args_hint="[bash|zsh|fish]", .subcommands="bash,zsh,fish", .handler=cmd_completions, .cli_only=true, .gateway_only=false},
    {.name="/cron", .alias=NULL, .description="Manage scheduled tasks: list, status", .category="Cron", .args_hint="[list|status]", .subcommands="list,status", .handler=cmd_cron, .cli_only=true, .gateway_only=false},
    {.name="/dashboard", .alias=NULL, .description="Launch web dashboard: status, start, stop, url", .category="System", .args_hint="[start|stop|status|url]", .subcommands="start,stop,status,url", .handler=cmd_dashboard},
    {.name="/fast", .alias=NULL, .description="Toggle fast mode for priority processing", .category="Config", .args_hint=NULL, .subcommands=NULL, .handler=cmd_fast},
    {.name="/reload", .alias=NULL, .description="Reload .env variables into running session", .category="System", .args_hint=NULL, .subcommands=NULL, .handler=cmd_reload, .cli_only=true, .gateway_only=false},
    {.name="/rollback", .alias=NULL, .description="List or restore state snapshots", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_rollback},
    {.name="/copy", .alias=NULL, .description="Copy the last assistant response to clipboard", .category="System", .args_hint=NULL, .subcommands=NULL, .handler=cmd_copy, .cli_only=true, .gateway_only=false},
    {.name="/queue", .alias=NULL, .description="Queue a prompt for the next turn", .category="Session", .args_hint="<prompt>", .subcommands=NULL, .handler=cmd_queue},
    {.name="/restart", .alias=NULL, .description="Gracefully restart the gateway", .category="Gateway", .args_hint=NULL, .subcommands=NULL, .handler=cmd_restart},
    {.name="/subgoal", .alias=NULL, .description="Add or manage extra criteria on the active goal", .category="Session", .args_hint="<text>", .subcommands=NULL, .handler=cmd_subgoal},
    {.name="/sethome", .alias=NULL, .description="Set this chat as the home channel", .category="Gateway", .args_hint=NULL, .subcommands=NULL, .handler=cmd_sethome, .cli_only=false, .gateway_only=true},
    {.name="/handoff", .alias=NULL, .description="Hand off this session to a messaging platform", .category="Gateway", .args_hint=NULL, .subcommands=NULL, .handler=cmd_handoff, .cli_only=true, .gateway_only=false},
    {.name="/platform", .alias="/pf", .description="Pause, resume, or list gateway platforms", .category="Gateway", .args_hint=NULL, .subcommands=NULL, .handler=cmd_platform},
    {.name="/bundles", .alias=NULL, .description="List skill bundles", .category="Skills", .args_hint=NULL, .subcommands=NULL, .handler=cmd_bundles},
    {.name="/curator", .alias=NULL, .description="Background skill maintenance status", .category="Skills", .args_hint=NULL, .subcommands=NULL, .handler=cmd_curator},
    {.name="/image", .alias=NULL, .description="Attach a local image file for next prompt", .category="Tools", .args_hint="<path>", .subcommands=NULL, .handler=cmd_image},
    {.name="/paste", .alias=NULL, .description="Attach clipboard image from clipboard", .category="Tools", .args_hint=NULL, .subcommands=NULL, .handler=cmd_paste, .cli_only=true, .gateway_only=false},
    {.name="/insights", .alias=NULL, .description="Show usage insights and analytics", .category="Session", .args_hint="[--days N]", .subcommands=NULL, .handler=cmd_insights},
    {.name="/indicator", .alias=NULL, .description="Pick the TUI busy-indicator style", .category="Display", .args_hint=NULL, .subcommands=NULL, .handler=cmd_indicator, .cli_only=true, .gateway_only=false},
    {.name="/statusbar", .alias=NULL, .description="Toggle the context/model status bar", .category="Display", .args_hint=NULL, .subcommands=NULL, .handler=cmd_statusbar, .cli_only=true, .gateway_only=false},
    {.name="/footer", .alias=NULL, .description="Toggle gateway metadata footer on replies", .category="Config", .args_hint="[on|off|status]", .subcommands="on,off,status", .handler=cmd_footer},
    {.name="/busy", .alias=NULL, .description="Control what Enter does while Hermes is working", .category="Display", .args_hint="[queue|steer|interrupt|status]", .subcommands="queue,steer,interrupt,status", .handler=cmd_busy, .cli_only=true, .gateway_only=false},
    {.name="/reload-mcp", .alias=NULL, .description="Reload MCP servers from config", .category="MCP", .args_hint=NULL, .subcommands=NULL, .handler=cmd_reload_mcp},
    {.name="/mcp", .alias=NULL, .description="MCP mgmt: status, list, test", .category="MCP", .args_hint="[status|list|test <name>]", .subcommands="status,list,test", .handler=cmd_mcp},
    {.name="/reload-skills", .alias=NULL, .description="Re-scan skills directory for changes", .category="Skills", .args_hint=NULL, .subcommands=NULL, .handler=cmd_reload_skills},
    {.name="/browser", .alias=NULL, .description="Connect browser tools to Chromium via CDP", .category="Tools", .args_hint=NULL, .subcommands=NULL, .handler=cmd_browser, .cli_only=true, .gateway_only=false},
    {.name="/voice", .alias=NULL, .description="Toggle voice input/output mode", .category="Config", .args_hint=NULL, .subcommands=NULL, .handler=cmd_voice},
    {.name="/steer", .alias=NULL, .description="Inject a message after the next tool call", .category="Session", .args_hint="<text>", .subcommands=NULL, .handler=cmd_steer},
    {.name="/kanban", .alias=NULL, .description="Kanban board management: show, list, create", .category="Kanban", .args_hint="[show|list|create]", .subcommands="show,list,create", .handler=cmd_kanban},
    {.name="/update", .alias=NULL, .description="Update Hermes Agent to the latest version", .category="System", .args_hint=NULL, .subcommands=NULL, .handler=cmd_update},
    {.name="/debug", .alias=NULL, .description="Upload debug report and get shareable link", .category="System", .args_hint=NULL, .subcommands=NULL, .handler=cmd_debug, .cli_only=true, .gateway_only=false},
    {.name="/session-search", .alias=NULL, .description="Search sessions: <query> [--limit N]", .category="Session", .args_hint="<query> [--limit N]", .subcommands=NULL, .handler=cmd_session_search},
    {.name="/session-export", .alias=NULL, .description="Export session: json or markdown", .category="Session", .args_hint="<session_id> [json|markdown]", .subcommands="json,markdown", .handler=cmd_session_export},
    {.name="/session-import", .alias=NULL, .description="Import session from JSON file", .category="Session", .args_hint="<filepath>", .subcommands=NULL, .handler=cmd_session_import},
    {.name="/logs", .alias=NULL, .description="View agent logs", .category="System", .args_hint="[errors|gateway] [-n N]", .subcommands="errors,gateway", .handler=cmd_logs},
    {.name="/dump", .alias=NULL, .description="Dump system debug info for support context", .category="System", .args_hint=NULL, .subcommands=NULL, .handler=cmd_dump, .cli_only=true, .gateway_only=false},
    {.name="/send", .alias=NULL, .description="Send a message: [target] <message>", .category="System", .args_hint="[target] <message>", .subcommands=NULL, .handler=cmd_send},
    {.name="/key", .alias=NULL, .description="Manage API keys: list, set, show, unset", .category="Security", .args_hint="[list|set <provider>|show <provider>|unset <provider>]", .subcommands="list,set,show,unset", .handler=cmd_key},
    {.name="/deps", .alias=NULL, .description="Install third-party Python bridge dependencies", .category="System", .args_hint=NULL, .subcommands=NULL, .handler=cmd_deps, .cli_only=true, .gateway_only=false},
    {.name="/pet", .alias=NULL, .description="Petdex: info, gallery, select, remove, disable, scale", .category="Display", .args_hint="[info|gallery|select <slug>|remove <slug>|disable|scale <n>]", .subcommands="info,gallery,select,remove,disable,scale", .handler=cmd_pet},
    /* ── Command-surface parity (Python COMMAND_REGISTRY 1:1) ── */
    {.name="/battery", .alias=NULL, .description="Toggle a color-coded battery indicator in the status bar", .category="Configuration", .args_hint="[on|off|status]", .subcommands="on,off,status", .handler=cmd_battery, .cli_only=true, .gateway_only=false},
    {.name="/blueprint", .alias="/bp", .description="Set up an automation from a blueprint template", .category="Tools & Skills", .args_hint="[name] [slot=value ...]", .subcommands=NULL, .handler=cmd_blueprint},
    {.name="/codex-runtime", .alias="/codex_runtime", .description="Toggle codex app-server runtime for OpenAI/Codex models", .category="Configuration", .args_hint="[auto|codex_app_server]", .subcommands=NULL, .handler=cmd_codex_runtime},
    {.name="/egress", .alias=NULL, .description="Show Docker egress proxy status", .category="Session", .args_hint="[status]", .subcommands="status", .handler=cmd_egress},
    {.name="/hatch", .alias="/generate-pet", .description="Generate a new petdex pet from a description", .category="Tools & Skills", .args_hint="[description]", .subcommands=NULL, .handler=cmd_hatch, .cli_only=true, .gateway_only=false},
    {.name="/journey", .alias="/learning", .description="Open the learning journey timeline", .category="Session", .args_hint="[list|delete <id>|edit <id>]", .subcommands="list,delete,edit", .handler=cmd_journey, .cli_only=true, .gateway_only=false},
    {.name="/learn", .alias=NULL, .description="Learn a reusable skill from anything you describe", .category="Tools & Skills", .args_hint="<what to learn from>", .subcommands=NULL, .handler=cmd_learn},
    {.name="/moa", .alias=NULL, .description="Run one prompt through the default Mixture of Agents preset, then restore your model", .category="Session", .args_hint="<prompt>", .subcommands=NULL, .handler=cmd_moa},
    {.name="/prompt", .alias=NULL, .description="Compose your next prompt in $EDITOR (markdown), then send it", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_prompt},
    {.name="/start", .alias=NULL, .description="Acknowledge platform start pings without a reply", .category="Session", .args_hint=NULL, .subcommands=NULL, .handler=cmd_start},
    {.name="/subscription", .alias="/upgrade", .description="View your Nous plan and change it in the browser", .category="Info", .args_hint=NULL, .subcommands=NULL, .handler=cmd_subscription, .cli_only=true, .gateway_only=false},
    {.name="/suggestions", .alias="/suggest", .description="Review suggested automations (accept/dismiss)", .category="Tools & Skills", .args_hint="[accept|dismiss N|catalog|clear]", .subcommands="accept,dismiss,catalog,clear", .handler=cmd_suggestions},
    {.name="/timestamps", .alias="/ts", .description="Toggle [HH:MM] timestamps on messages and /history", .category="Configuration", .args_hint="[on|off|status]", .subcommands="on,off,status", .handler=cmd_timestamps, .cli_only=true, .gateway_only=false},
    {.name="/topup", .alias=NULL, .description="Show your Nous balance and manage billing on the portal", .category="Info", .args_hint=NULL, .subcommands=NULL, .handler=cmd_topup},
    {.name=NULL, .alias=NULL, .description=NULL, .category=NULL, .args_hint=NULL, .subcommands=NULL, .handler=NULL},
};

/* ================================================================
 *  Command resolution
 * ================================================================ */

const command_def_t *commands_resolve(const char *input) {
    if (!input || input[0] != '/') return NULL;

    /* Exact match (including aliases) — case-insensitive */
    for (int i = 0; COMMANDS[i].name; i++) {
        if (strcasecmp(input, COMMANDS[i].name) == 0 ||
            (COMMANDS[i].alias && strcasecmp(input, COMMANDS[i].alias) == 0))
            return &COMMANDS[i];
    }

    /* Partial match: check if command name is a prefix of input followed by space or end */
    size_t inlen = strlen(input);
    const command_def_t *first_partial = NULL;
    int partial_count = 0;
    for (int i = 0; COMMANDS[i].name; i++) {
        size_t cmdlen = strlen(COMMANDS[i].name);

        /* Command is a prefix of input (e.g. "/sessions" matches "/sessions --active") */
        if (inlen > cmdlen && input[cmdlen] == ' ' &&
            strncasecmp(input, COMMANDS[i].name, cmdlen) == 0) {
            if (!first_partial) first_partial = &COMMANDS[i];
            partial_count++;
        }

        /* Input is a prefix of command (e.g. "/se" matches "/sessions") */
        if (inlen <= cmdlen && strncasecmp(COMMANDS[i].name, input, inlen) == 0) {
            if (!first_partial) first_partial = &COMMANDS[i];
            partial_count++;
        }
    }

    /* Only return if exactly one match (no ambiguity) */
    if (partial_count == 1 && first_partial)
        return first_partial;

    /* Multi-alias fallback: the faithful COMMAND_REGISTRY port carries the
     * full alias sets (journey→learning/memory-graph, new→reset,
     * background→bg/btw, ...). Resolve the bare name there and map the
     * canonical name back to a live handler. */
    {
        const char *bare = input;
        if (bare[0] == '/') bare++;
        const cli_command_def_t *reg = cli_resolve_command(bare);
        if (reg) {
            char canonical[256];
            snprintf(canonical, sizeof(canonical), "/%s", reg->name);
            for (int i = 0; COMMANDS[i].name; i++) {
                if (strcasecmp(COMMANDS[i].name, canonical) == 0)
                    return &COMMANDS[i];
            }
        }
    }

    return NULL;
}

int commands_count(void) {
    int count = 0;
    for (int i = 0; COMMANDS[i].name; i++) count++;
    return count;
}

const char *commands_list_json(void) {
    int count = commands_count();
    size_t bufsz = 256 + (size_t)count * 256;
    char *buf = malloc(bufsz);
    if (!buf) return NULL;
    buf[0] = '\0';
    strcat(buf, "[");
    for (int i = 0; COMMANDS[i].name; i++) {
        if (i > 0) strcat(buf, ",");
        strcat(buf, "{\"name\":\"");
        strcat(buf, COMMANDS[i].name);
        strcat(buf, "\",\"cli_only\":");
        strcat(buf, COMMANDS[i].cli_only ? "true" : "false");
        strcat(buf, ",\"gateway_only\":");
        strcat(buf, COMMANDS[i].gateway_only ? "true" : "false");
        strcat(buf, ",\"subcommands\":[");
        if (COMMANDS[i].subcommands) {
            int first = 1;
            const char *p = COMMANDS[i].subcommands;
            while (*p) {
                /* Skip spaces */
                while (*p == ' ') p++;
                if (!*p) break;
                if (!first) strcat(buf, ",");
                first = 0;
                strcat(buf, "\"");
                /* Copy until comma or end */
                while (*p && *p != ',') {
                    char c = *p++;
                    if (c == '\\' || c == '"') { strcat(buf, "\\"); }
                    char tmp[2] = {c, '\0'};
                    strcat(buf, tmp);
                }
                strcat(buf, "\"");
                if (*p == ',') p++;
            }
        }
        strcat(buf, "]}");
    }
    strcat(buf, "]");
    return buf;
}

/* ================================================================
 *  Command execution
 * ================================================================ */

bool commands_dispatch(const char *input, agent_state_t *state) {
    const command_def_t *cmd = commands_resolve(input);
    if (!cmd) return false;

    /* Skip gateway-only commands in CLI mode */
    if (cmd->gateway_only) return false;

    /* Extract args (text after command name). The input may match an
     * ALIAS whose length differs from the canonical name (e.g. "/learning"
     * resolves to "/journey"), so skip the actual input token rather than
     * strlen(cmd->name). */
    const char *args = input;
    while (*args == '/') args++;
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    cmd->handler(args, state);
    return true;
}

/* CL13: Try to dispatch a user-defined quick command from config.
 * Returns true if handled (output printed), false if not a quick command.
 * Uses global config pointer set by cli_main(). */
static const hermes_config_t *g_qc_config = NULL;
void commands_set_quick_config(const hermes_config_t *cfg) { g_qc_config = cfg; }
bool commands_try_quick(const char *input, agent_state_t *state) {
    (void)state;
    if (!input || input[0] != '/' || !g_qc_config) return false;
    const char *qc_json = g_qc_config->quick_commands_json;
    if (!qc_json[0]) return false;
    /* Extract command name */
    const char *cmd_start = input + 1;
    const char *cmd_end = cmd_start;
    while (*cmd_end && *cmd_end != ' ') cmd_end++;
    size_t cmd_len = (size_t)(cmd_end - cmd_start);
    if (cmd_len == 0 || cmd_len > 128) return false;
    char cmd_name[129];
    memcpy(cmd_name, cmd_start, cmd_len);
    cmd_name[cmd_len] = '\0';
    /* Extract user args after command name */
    const char *args = cmd_end;
    while (*args == ' ') args++;
    /* Quick JSON search: look for "cmd_name": pattern */
    char needle[160];
    snprintf(needle, sizeof(needle), "\"%s\":", cmd_name);
    const char *found = strstr(qc_json, needle);
    if (!found) return false;
    /* Extract value: find the next quoted string after the key */
    const char *val = found + strlen(needle);
    /* Skip whitespace and optional quote */
    while (*val == ' ') val++;
    if (*val != '"') return false; /* Not a string value */
    val++; /* skip opening quote */
    /* Find closing quote */
    const char *val_end = val;
    while (*val_end && *val_end != '"') {
        /* Skip escaped quotes */
        if (*val_end == '\\' && *(val_end+1) == '"') val_end += 2;
        else val_end++;
    }
    size_t val_len = (size_t)(val_end - val);
    if (val_len == 0 || val_len > 4096) return false;
    char cmd_exec[4097];
    memcpy(cmd_exec, val, val_len);
    cmd_exec[val_len] = '\0';
    /* Execute shell command */
    printf("\n  [Quick command: %s → `%s`]\n", cmd_name, cmd_exec);
    fflush(stdout);
    FILE *fp = popen(cmd_exec, "r");
    if (!fp) {
        printf("  (failed to execute — popen error)\n");
        return true;
    }
    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        printf("  %s", line);
    }
    int rc = pclose(fp);
    if (rc != 0) {
        printf("  (exited with code %d)\n", rc);
    }
    return true;
}

/* Try to dispatch a /command as a skill invocation.
 * Returns true if handled (message sent to agent_chat), false if not a skill.
 * Call this when commands_dispatch() returns false and input starts with '/'. */
bool commands_try_skill(const char *input, agent_state_t *state) {
    if (!input || input[0] != '/') return false;

    skill_cmd_scan();
    const char *slug = skill_cmd_resolve(input);
    if (slug) {
        /* Extract user args after the skill name */
        size_t slen = strlen(slug);
        const char *user_args = input;
        if (strncmp(user_args, slug, slen) == 0) {
            user_args += slen;
            while (*user_args == ' ') user_args++;
        } else {
            /* Input is just /slug — find where slug ends */
            user_args = input + 1;
            while (*user_args && *user_args != ' ') user_args++;
            while (*user_args == ' ') user_args++;
        }

        char *msg = skill_cmd_build_message(slug, user_args);
        if (!msg) return false;

        printf("\n  [Invoking skill: %s]\n", slug);
        fflush(stdout);
        char *resp = agent_chat(state, msg);
        if (resp) {
            printf("\n%s\n", resp);
            free(resp);
        }
        free(msg);
        return true;
    }

    /* CL10: Check if input matches a skill bundle name */
    skill_bundle_registry_t bundle_reg;
    memset(&bundle_reg, 0, sizeof(bundle_reg));
    skill_bundles_scan(&bundle_reg);

    /* Extract bundle name: strip leading / and trailing args */
    const char *bname = input + 1; /* skip '/' */
    char bundle_name[256];
    int bn = 0;
    while (bname[bn] && bname[bn] != ' ') {
        if (bn < (int)sizeof(bundle_name) - 1)
            bundle_name[bn] = (char)tolower((unsigned char)bname[bn]);
        bn++;
    }
    bundle_name[bn] = '\0';

    const skill_bundle_t *bundle = skill_bundle_find(&bundle_reg, bundle_name);
    if (bundle) {
        /* Extract user args */
        const char *user_args = bname + bn;
        while (*user_args == ' ') user_args++;

        printf("\n  [Invoking bundle: %s]\n", bundle->name);
        fflush(stdout);

        /* Apply the bundle (load referenced skills) */
        char err[256];
        if (!skill_bundle_apply(bundle, err, sizeof(err))) {
            printf("  Error applying bundle: %s\n", err);
            return true;
        }

        /* Send the remaining args as message to trigger the bundled skills */
        char *resp = agent_chat(state, user_args);
        if (resp) {
            printf("\n%s\n", resp);
            free(resp);
        }
        return true;
    }

    return false;
}

/* Return the full command list for iteration */
const command_def_t *commands_get_all(void) {
    return COMMANDS;
}

/* ================================================================
 *  Handlers
 * ================================================================ */

/* Helper: print messages with role filtering and count limit */
void print_messages(const agent_state_t *state, size_t start, size_t count,
                     const char *filter_role, bool show_full) {
    size_t printed = 0;
    size_t skip_role = 0;
    message_role_t filter = 255; /* no filter */
    if (filter_role) {
        if (strcmp(filter_role, "system") == 0 || strcmp(filter_role, "sys") == 0) filter = MSG_SYSTEM;
        else if (strcmp(filter_role, "user") == 0 || strcmp(filter_role, "usr") == 0) filter = MSG_USER;
        else if (strcmp(filter_role, "assistant") == 0 || strcmp(filter_role, "asm") == 0) filter = MSG_ASSISTANT;
        else if (strcmp(filter_role, "tool") == 0) filter = MSG_TOOL;
    }

    for (size_t i = start; i < state->message_count && printed < count; i++) {
        if (filter != 255 && state->messages[i]->role != filter) {
            skip_role++;
            continue;
        }
        const char *role_str;
        switch (state->messages[i]->role) {
            case MSG_SYSTEM:    role_str = "sys";  break;
            case MSG_USER:      role_str = "usr";  break;
            case MSG_ASSISTANT: role_str = "asm";  break;
            case MSG_TOOL:      role_str = "tool"; break;
            default:            role_str = "?";    break;
        }
        const char *content = state->messages[i]->content;
        if (content) {
            if (show_full) {
                printf("  [%s] %s\n", role_str, content);
            } else {
                char preview[81];
                size_t clen = strlen(content);
                if (clen > 80) {
                    memcpy(preview, content, 77);
                    preview[77] = '.'; preview[78] = '.'; preview[79] = '.';
                    preview[80] = '\0';
                } else {
                    memcpy(preview, content, clen + 1);
                }
                printf("  [%s] %s\n", role_str, preview);
            }
        } else {
            printf("  [%s] (no content)\n", role_str);
        }
        printed++;
    }
    if (printed == 0)
        printf("  (no matching messages)\n");
}

/* Display a single config key-value pair with type prefix */
void show_cfg_val(const char *key, const char *type, const char *val) {
    printf("  %s (%s): %s\n", key, type, val && val[0] ? val : "(default)");
}

void show_cfg_val_int(const char *key, int val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", val);
    show_cfg_val(key, "int", buf);
}

void show_cfg_val_bool(const char *key, bool val) {
    show_cfg_val(key, "bool", val ? "true" : "false");
}

void show_cfg_val_float(const char *key, float val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", (double)val);
    show_cfg_val(key, "float", buf);
}

/* Show all keys in a config group */

/* ================================================================
 *  Advanced session management commands
 * ================================================================ */

/* ================================================================
 *  Additional commands (CLI parity)
 * ================================================================ */

/* /yolo: Toggle YOLO mode (skip approvals) */
int g_yolo_mode = 0;

void commands_set_yolo(bool enabled) { g_yolo_mode = enabled ? 1 : 0; }
bool commands_get_yolo(void) { return g_yolo_mode != 0; }

/* /verbose: Toggle tool progress display verbosity */
int g_verbose = 0;

void commands_set_verbose(int level) { g_verbose = level; }
int commands_get_verbose(void) { return g_verbose; }

/* /skin: Show or change the display skin/theme */
char g_current_skin[64] = "";

/* Forward declaration from display_core.c */
extern void display_set_skin(void *skin);

/* /fast: Toggle fast mode (normal|fast|status) */
int g_fast_mode = 0;

void commands_set_fast(bool enabled) { g_fast_mode = enabled ? 1 : 0; }
bool commands_get_fast(void) { return g_fast_mode != 0; }

/* /queue: Queue a prompt for the next turn */
char g_queued_prompt[4096] = "";

/* /sethome: Set home channel */
char g_home_channel[256] = "";

/* /handoff: Hand off session to messaging platform
 * Subcommands:
 *   /handoff request <platform> — create a handoff request for another agent
 *   /handoff claim <id>          — claim a pending handoff and resume session
 *   /handoff complete <id>       — mark handoff as complete
 *   /handoff status              — show pending/claimed handoffs
 *   /handoff list                — list all handoff requests */

/* Handoff entry — populated from JSON request files */
/* Simple dynamic array for handoff entries */
static void list_init(list_t *l) {
    l->items = NULL;
    l->count = 0;
    l->capacity = 0;
}

static void list_append(list_t *l, void *item) {
    if (l->count >= l->capacity) {
        l->capacity = l->capacity ? l->capacity * 2 : 8;
        l->items = (void **)realloc(l->items, l->capacity * sizeof(void *));
    }
    l->items[l->count++] = item;
}

static void list_free(list_t *l) {
    if (l->items) free(l->items);
    l->items = NULL;
    l->count = l->capacity = 0;
}


/* Write a handoff request JSON file (declared in commands_shared.h) */
void handoff_write_request(const char *handoff_id, const char *platform,
                           const char *session_id, const char *requester) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    char handoff_dir[8192];
    snprintf(handoff_dir, sizeof(handoff_dir), "%s/.hermes/handoffs", home);

    /* Ensure directory exists */
    char mkdir_cmd[8192];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", handoff_dir);
    system(mkdir_cmd);

    json_node_t *req = json_new_object();
    json_object_set(req, "id", json_new_string(handoff_id));
    json_object_set(req, "platform", json_new_string(platform));
    json_object_set(req, "session_id", json_new_string(session_id));
    json_object_set(req, "requester", json_new_string(requester));
    json_object_set(req, "status", json_new_string("pending"));
    json_object_set(req, "created_at", json_new_number((double)time(NULL)));

    char *json_str = json_serialize(req);
    json_free(req);

    if (json_str) {
        char path[8192];
        snprintf(path, sizeof(path), "%s/%s.json", handoff_dir, handoff_id);
        FILE *f = fopen(path, "w");
        if (f) { fputs(json_str, f); fclose(f); }
        free(json_str);
    }
}

/* Scan handoff directory and return entries */
/* handoff_read_dir is defined in cli_cmd_system.c (declared in cli_cmd_system.h). */

/* /indicator: Pick TUI indicator style — stores in static var */
char g_indicator_style[32] = "default";

/* /statusbar: Toggle status bar (on|off|status) */
int g_statusbar_on = 1;

/* /footer: Toggle footer (on|off|status) */
int g_footer_on = 1;

/* /busy: Control Enter behavior */
/* Busy behavior mode: 0=queue (default), 1=steer, 2=interrupt */
int g_busy_mode = 0;

/* /reload-mcp: Reload MCP servers from config */
/* g_server_count / g_servers are declared extern in commands_shared.h */
extern bool mcp_add_stdio_server(const char *name, const char *command,
                                  char **args, int arg_count);
extern bool mcp_add_sse_server(const char *name, const char *url);
extern bool mcp_remove_server(const char *name);

/* /voice: Toggle voice input/output mode (on|off|tts|status|config|key) */
int g_voice_mode = 0;

/* JSON-escape a string into a fixed-size buffer (for safe JSON injection) */
/* json_escape_arg is provided by cli_cmd_kanban.c (declared in cli_cmd_kanban.h). */

