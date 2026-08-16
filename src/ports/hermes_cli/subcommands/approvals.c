#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/subcommands/approvals.c — C11 port of hermes_cli/subcommands/approvals.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/cli/port_subcommands_parsers.c ── */
/*
 * port_subcommands_parsers.c — C port of hermes_cli/subcommands/*_parser
 *
 * Faithful port of the argparse builder functions. Python's build_*_parser
 * attach a subcommand (name, help, description, sub-actions, arguments) to
 * the `hermes` CLI's argparse subparsers. The C CLI drives dispatch through
 * its own command registry, so the faithful equivalent here is a function
 * that emits the SAME structural spec argparse records — as a JSON object:
 *
 *   { "name", "help", "description", "subcommands": [ {name, help,
 *       "args":[{name, dest, metavar, action, choices, default, nargs,
 *                help}]} ], "args":[...], "epilog" }
 *
 * Callers (help screens, web schema) render from this spec. No runtime I/O
 * happens here — exactly like Python, which only builds the parser object.
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "json.h"

static json_t *cmd_arg(const char *name, const char *dest, const char *metavar,
                       const char *action, const char *choices, const char *dflt,
                       const char *nargs, const char *help)
{
    json_t *a = json_object();
    json_set(a, "name", json_string(name ? name : ""));
    if (dest) json_set(a, "dest", json_string(dest));
    if (metavar) json_set(a, "metavar", json_string(metavar));
    if (action) json_set(a, "action", json_string(action));
    if (choices) json_set(a, "choices", json_string(choices));
    if (dflt) json_set(a, "default", json_string(dflt));
    if (nargs) json_set(a, "nargs", json_string(nargs));
    if (help) json_set(a, "help", json_string(help));
    return a;
}

static json_t *cmd_sub(const char *name, const char *help)
{
    json_t *s = json_object();
    json_set(s, "name", json_string(name ? name : ""));
    if (help) json_set(s, "help", json_string(help));
    json_set(s, "args", json_array());
    return s;
}

static char *emit_spec(json_t *spec)
{
    char *ser = json_serialize(spec);
    json_free(spec);
    return ser ? ser : strdup("{}");
}

/* PoP: subcmd_build_approvals_parser @ hermes_cli/subcommands/approvals.py:build_approvals_parser */
char *subcmd_build_approvals_parser(void)
{
    json_t *spec = json_object();
    json_set(spec, "name", json_string("approvals"));
    json_set(spec, "help", json_string("Approval-prompt tools (mine history into allowlist proposals)"));
    json_set(spec, "description", json_string(
        "Tools for the dangerous-command approval system. `hermes approvals suggest` mines past approval decisions from the session database and proposes command_allowlist entries so repeatedly-approved commands stop prompting."));
    json_set(spec, "subcommand_dest", json_string("approvals_command"));
    json_set(spec, "metavar", json_string("<subcommand>"));
    json_t *subs = json_array();
    json_t *suggest = cmd_sub("suggest", "Propose command_allowlist entries from past approvals");
    json_t *sargs = json_obj_get(suggest, "args");
    json_append(sargs, cmd_arg("--apply", "apply_indices", "N[,M...]", NULL, NULL, NULL, NULL,
        "Merge the numbered proposals (from a prior run) into command_allowlist in config.yaml"));
    json_append(sargs, cmd_arg("--json", NULL, NULL, "store_true", NULL, NULL, NULL,
        "Emit machine-readable JSON instead of human-readable text"));
    json_set(suggest, "subs_are_args", json_bool(true));
    json_append(subs, suggest);
    json_set(spec, "subcommands", subs);
    return emit_spec(spec);
}

/* PoP: subcmd_build_import_agent_parser @ hermes_cli/subcommands/import_agent.py:build_import_agent_parser */
char *subcmd_build_import_agent_parser(void)
{
    json_t *spec = json_object();
    json_set(spec, "name", json_string("import-agent"));
    json_set(spec, "help", json_string("Import a Claude Code or Codex CLI setup into Hermes"));
    json_set(spec, "description", json_string(
        "One-command import of another coding agent's setup into Hermes. Maps CLAUDE.md/AGENTS.md instructions, permission allowlists, MCP servers, skills, and memories into their Hermes equivalents. Always shows a preview before making changes. API keys and credentials are never imported — run 'hermes setup' for those."));
    json_t *args = json_array();
    json_append(args, cmd_arg("agent", NULL, NULL, NULL, "claude-code,codex", NULL, "?",
        "Which agent to import from (default: auto-detect ~/.claude or ~/.codex)"));
    json_append(args, cmd_arg("--source", NULL, NULL, NULL, NULL, NULL, NULL,
        "Path to the agent's config directory (default: ~/.claude or ~/.codex)"));
    json_append(args, cmd_arg("--dry-run", NULL, NULL, "store_true", NULL, NULL, NULL,
        "Preview only — stop after showing what would be imported"));
    json_append(args, cmd_arg("--overwrite", NULL, NULL, "store_true", NULL, NULL, NULL,
        "Overwrite existing Hermes items on name conflicts (default: skip)"));
    json_append(args, cmd_arg("--yes", NULL, NULL, "store_true", NULL, NULL, NULL,
        "Skip confirmation prompts"));
    json_set(spec, "args", args);
    json_set(spec, "func", json_string("cmd_import_agent"));
    return emit_spec(spec);
}

/* PoP: subcmd_build_monitoring_parser @ hermes_cli/subcommands/monitoring.py:build_monitoring_parser */
char *subcmd_build_monitoring_parser(void)
{
    json_t *spec = json_object();
    json_set(spec, "name", json_string("monitoring"));
    json_set(spec, "help", json_string("Inspect gateway monitoring (health & diagnostics export)"));
    json_set(spec, "description", json_string(
        "Gateway monitoring: service health metrics plus redacted diagnostics, exported over OTLP to an operator-configured endpoint. Content-free by construction — no prompts, messages, tool args/results, or usage analytics. Configure under monitoring.* in config.yaml."));
    json_set(spec, "subcommand_dest", json_string("monitoring_action"));
    json_t *subs = json_array();
    json_t *status = cmd_sub("status", "Show monitoring settings, export state, and redaction posture");
    json_append(subs, status);
    json_set(spec, "subcommands", subs);
    json_set(spec, "func", json_string("cmd_monitoring"));
    return emit_spec(spec);
}

/* PoP: subcmd_build_sync_parser @ hermes_cli/subcommands/sync.py:build_sync_parser */
char *subcmd_build_sync_parser(void)
{
    json_t *spec = json_object();
    json_set(spec, "name", json_string("sync"));
    json_set(spec, "help", json_string("Skill Sync — sync your skills across devices and with your team"));
    json_set(spec, "description", json_string(
        "Skill Sync keeps your skills with you. Personal sync moves your own skills between your devices; if you belong to an organisation, you also get its shared skills and can propose your own back to the team."));
    json_set(spec, "epilog", json_string(
        "Examples:\n  hermes sync status            what is synced, and from where\n  hermes sync enable my-skill   include a skill in your sync\n  hermes sync now               pull, then push\n  hermes sync propose my-skill  share a skill with your team\n"));
    json_set(spec, "subcommand_dest", json_string("sync_command"));
    json_t *subs = json_array();
    json_t *status = cmd_sub("status", "Show what is synced, and from where");
    json_t *pull = cmd_sub("pull", "Pull your synced skills (and your organisation's)");
    json_t *push = cmd_sub("push", "Push your opted-in skills");
    json_t *now = cmd_sub("now", "Reconcile now: pull then push");
    json_t *enable = cmd_sub("enable", "Include a skill in your sync");
    json_t *eargs = json_obj_get(enable, "args");
    json_append(eargs, cmd_arg("skill", NULL, NULL, NULL, NULL, NULL, NULL,
        "Skill name (frontmatter name / directory name)"));
    json_t *disable = cmd_sub("disable", "Exclude a skill from your sync");
    json_t *dargs = json_obj_get(disable, "args");
    json_append(dargs, cmd_arg("skill", NULL, NULL, NULL, NULL, NULL, NULL,
        "Skill name (frontmatter name / directory name)"));
    json_t *device = cmd_sub("device", "Show or set this device's label (shown in the sync console)");
    json_t *devargs = json_obj_get(device, "args");
    json_append(devargs, cmd_arg("--name", "device_name", NULL, NULL, NULL, NULL, NULL,
        "Set a human-friendly label for this device (e.g. \"Ben's Laptop\"). Omit to print the current label."));
    json_t *propose = cmd_sub("propose", "Share a skill with your organisation");
    json_set(propose, "description", json_string(
        "Submit one of your skills to your organisation's shared set. If you are an admin it is added directly; otherwise it becomes a proposal for an admin to review. Accounts that aren't part of a shared organisation don't have this workflow."));
    json_append(subs, status); json_append(subs, pull); json_append(subs, push);
    json_append(subs, now); json_append(subs, enable); json_append(subs, disable);
    json_append(subs, device); json_append(subs, propose);
    json_set(spec, "subcommands", subs);
    return emit_spec(spec);
}

/* PoP: subcmd_build_verify_parser @ hermes_cli/subcommands/verify.py:build_verify_parser */
char *subcmd_build_verify_parser(void)
{
    json_t *spec = json_object();
    json_set(spec, "name", json_string("verify"));
    json_set(spec, "help", json_string("Detect a project's run recipe and smoke-test it"));
    json_set(spec, "description", json_string(
        "Detect how the current project is built, tested, and started (or load the saved manifest at .hermes/environment.json), then run a verification pass: bootstrap -> build -> test -> start in background -> poll readiness -> teardown."));
    json_t *args = json_array();
    json_append(args, cmd_arg("path", NULL, NULL, NULL, NULL, NULL, "?",
        "Project root to verify (default: current directory)"));
    json_append(args, cmd_arg("--detect-only", NULL, NULL, "store_true", NULL, NULL, NULL,
        "Only detect and print the recipe as JSON; run nothing"));
    json_append(args, cmd_arg("--save", NULL, NULL, "store_true", NULL, NULL, NULL,
        "Save the recipe as .hermes/environment.json in the project"));
    json_append(args, cmd_arg("--skip-start", NULL, NULL, "store_true", NULL, NULL, NULL,
        "Run command phases but skip starting the app / readiness poll"));
    json_append(args, cmd_arg("--phase", NULL, NULL, "append", "bootstrap,build,test,start", NULL, NULL,
        "Run only the given phase(s); repeatable"));
    json_append(args, cmd_arg("--port", NULL, NULL, NULL, NULL, NULL, NULL,
        "Override the port used for the readiness poll"));
    json_append(args, cmd_arg("--timeout", NULL, NULL, NULL, NULL, NULL, NULL,
        "Per-phase timeout in seconds"));
    json_append(args, cmd_arg("--ready-timeout", NULL, NULL, NULL, NULL, NULL, NULL,
        "Timeout for the readiness poll"));
    json_set(spec, "args", args);
    return emit_spec(spec);
}

