/*
 * migrate.c - C11 port of upstream hermes_cli/subcommands/migrate.py
 * (gate w23_migrate_73e740).
 *
 * PoP: w23_migrate_73e740_surface @ hermes_cli/subcommands/migrate.py:build_migrate_parser
 * PoP: w23_migrate_73e740_surface @ hermes_cli/subcommands/migrate.py:_add_agent_import_parsers
 * PoP: w23_migrate_73e740_subcommand_names @ hermes_cli/subcommands/migrate.py:build_migrate_parser
 *
 * build_migrate_parser is pure parser construction: given a subparsers object it
 * declares one `migrate` command, two migrations (xai, relay) and one importer
 * per agent-importers source. That declaration IS the behavior, so it is ported
 * as data: every help/description string, every option in declaration order,
 * every default it sets. argparse, the handler callables and IMPORT_SOURCES are
 * the seams.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_migrate_73e740.h"

/* option strings, exactly as the live parser declares them */
static const char *const MIG_O_APPLY[] = {"--apply"};
static const char *const MIG_O_NO_BACKUP[] = {"--no-backup"};
static const char *const MIG_O_ALL_PROFILES[] = {"--all-profiles"};
static const char *const MIG_O_NO_VALIDATE[] = {"--no-validate"};
static const char *const MIG_O_SOURCE[] = {"--source"};
static const char *const MIG_O_DRY_RUN[] = {"--dry-run"};
static const char *const MIG_O_OVERWRITE[] = {"--overwrite"};
static const char *const MIG_O_YES[] = {"-y", "--yes"};
static const char *const MIG_O_MIGRATE_SECRETS[] = {"--migrate-secrets"};
static const char *const MIG_O_ACTIVATE_ROUTINES[] = {"--activate-routines"};
static const char *const MIG_O_BOT[] = {"--bot"};

/* Option defaults: a store_true flag or an unset value. */
#define MIG_DEF_FALSE 0
#define MIG_DEF_NONE 1

typedef struct {
    const char *dest;
    const char *const *opts;
    size_t nopts;
    const char *action;
    int nargs;              /* < 0 means argparse's None */
    const char *metavar;    /* NULL means None */
    const char *help;
    int def_kind;
} mig_opt;

typedef struct {
    const char *name;
    const char *label;      /* NULL for the two migrations */
    const char *help;       /* literal, or NULL when composed from label */
    const char *description;
    const mig_opt *opts;
    size_t nopts;
    const char *func;       /* set_defaults(func=...) */
    int agent;              /* also sets migrate_agent=<name> */
} mig_cmd;

static const mig_opt MIG_XAI_OPTS[] = {
    {"apply", MIG_O_APPLY, 1, "store_true", 0, NULL,
     "Rewrite config.yaml in-place (default: dry-run, no writes)", MIG_DEF_FALSE},
    {"no_backup", MIG_O_NO_BACKUP, 1, "store_true", 0, NULL,
     "Skip the timestamped backup of config.yaml when applying", MIG_DEF_FALSE},
};

static const mig_opt MIG_RELAY_OPTS[] = {
    {"all_profiles", MIG_O_ALL_PROFILES, 1, "store_true", 0, NULL,
     "Migrate the default home and every named profile (what `hermes update` does)", MIG_DEF_FALSE},
    {"no_validate", MIG_O_NO_VALIDATE, 1, "store_true", 0, NULL,
     "Skip activating the generated file through Relay's validator before writing it", MIG_DEF_FALSE},
};

static const mig_opt MIG_AGENT_OPTS[] = {
    {"source", MIG_O_SOURCE, 1, "store", -1, NULL,
     "Where the agent's setup (or, for Grok Bot, its export file) is", MIG_DEF_NONE},
    {"dry_run", MIG_O_DRY_RUN, 1, "store_true", 0, NULL, "Preview only", MIG_DEF_FALSE},
    {"overwrite", MIG_O_OVERWRITE, 1, "store_true", 0, NULL,
     "Replace existing items on name conflicts", MIG_DEF_FALSE},
    {"yes", MIG_O_YES, 2, "store_true", 0, NULL, "Skip the confirmation prompt", MIG_DEF_FALSE},
};

static const mig_opt MIG_OPENCLAW_OPTS[] = {
    {"source", MIG_O_SOURCE, 1, "store", -1, NULL,
     "Where the agent's setup (or, for Grok Bot, its export file) is", MIG_DEF_NONE},
    {"dry_run", MIG_O_DRY_RUN, 1, "store_true", 0, NULL, "Preview only", MIG_DEF_FALSE},
    {"overwrite", MIG_O_OVERWRITE, 1, "store_true", 0, NULL,
     "Replace existing items on name conflicts", MIG_DEF_FALSE},
    {"yes", MIG_O_YES, 2, "store_true", 0, NULL, "Skip the confirmation prompt", MIG_DEF_FALSE},
    {"migrate_secrets", MIG_O_MIGRATE_SECRETS, 1, "store_true", 0, NULL,
     "Also import OpenClaw's allowlisted API keys and bot tokens", MIG_DEF_FALSE},
};

static const mig_opt MIG_ROUTINE_OPTS[] = {
    {"source", MIG_O_SOURCE, 1, "store", -1, NULL,
     "Where the agent's setup (or, for Grok Bot, its export file) is", MIG_DEF_NONE},
    {"dry_run", MIG_O_DRY_RUN, 1, "store_true", 0, NULL, "Preview only", MIG_DEF_FALSE},
    {"overwrite", MIG_O_OVERWRITE, 1, "store_true", 0, NULL,
     "Replace existing items on name conflicts", MIG_DEF_FALSE},
    {"yes", MIG_O_YES, 2, "store_true", 0, NULL, "Skip the confirmation prompt", MIG_DEF_FALSE},
    {"activate_routines", MIG_O_ACTIVATE_ROUTINES, 1, "store_true", 0, NULL,
     "Create the imported cron jobs running instead of paused", MIG_DEF_FALSE},
    {"bot", MIG_O_BOT, 1, "append", -1, "NAME",
     "Only this Bot or agent, by name (repeatable)", MIG_DEF_NONE},
};

#define MIG_N(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Declaration order is the order argparse lists them in. */
static const mig_cmd MIG_COMMANDS[W23_MIGRATE_73E740_SUBCOMMANDS] = {
    {"xai", NULL, "Migrate xAI models scheduled for retirement on May 15, 2026",
     "Scan config.yaml for references to xAI models retiring on "
     "May 15, 2026 and, with --apply, rewrite them in-place to the "
     "official replacements per the xAI migration guide. The original "
     "config.yaml is backed up before any rewrite.",
     MIG_XAI_OPTS, MIG_N(MIG_XAI_OPTS), "cmd_migrate_xai", 0},
    {"relay", NULL,
     "Convert legacy HERMES_NEMO_RELAY_ATIF_*/ATOF_* exporter vars into relay-plugins.toml",
     "The NeMo Relay cutover stopped reading the legacy exporter variables. Generate "
     "<hermes home>/relay-plugins.toml from them, point HERMES_NEMO_RELAY_PLUGINS_TOML at it, "
     "and comment the legacy lines out. `hermes update` runs this for every profile automatically.",
     MIG_RELAY_OPTS, MIG_N(MIG_RELAY_OPTS), "_cmd_migrate_relay", 0},
    {"openclaw", "OpenClaw", NULL, NULL,
     MIG_OPENCLAW_OPTS, MIG_N(MIG_OPENCLAW_OPTS), "cmd_migrate_agent", 1},
    {"claude-code", "Claude Code", NULL, NULL,
     MIG_AGENT_OPTS, MIG_N(MIG_AGENT_OPTS), "cmd_migrate_agent", 1},
    {"codex", "Codex CLI", NULL, NULL,
     MIG_AGENT_OPTS, MIG_N(MIG_AGENT_OPTS), "cmd_migrate_agent", 1},
    {"cursor", "Cursor", NULL, NULL,
     MIG_AGENT_OPTS, MIG_N(MIG_AGENT_OPTS), "cmd_migrate_agent", 1},
    {"grok-bot", "Grok Bot", NULL, NULL,
     MIG_ROUTINE_OPTS, MIG_N(MIG_ROUTINE_OPTS), "cmd_migrate_agent", 1},
    {"openbot", "OpenBot", NULL, NULL,
     MIG_ROUTINE_OPTS, MIG_N(MIG_ROUTINE_OPTS), "cmd_migrate_agent", 1},
};

static const char *const MIG_ROOT_HELP =
    "Move over from another agent, or migrate retired models and deprecated settings";
static const char *const MIG_ROOT_DESC =
    "With no type: find another agent's setup on this computer (OpenClaw, Claude Code, "
    "Codex CLI, Cursor, Grok Bot, OpenBot) and offer to import it. Name an agent to import from it; "
    "xai and relay rewrite deprecated settings in the active config.yaml.";

static void mig_help_of(const mig_cmd *c, char *buf, size_t cap) {
    if (c->agent) snprintf(buf, cap, "Import your %s setup into Hermes", c->label);
    else snprintf(buf, cap, "%s", c->help ? c->help : "");
}

static void mig_desc_of(const mig_cmd *c, char *buf, size_t cap) {
    if (c->agent) {
        snprintf(buf, cap,
                 "Import your %s setup into the active profile through the "
                 "agent-importers plugin (installed from the plugin catalog when missing). Always "
                 "previews first and asks before changing anything.",
                 c->label);
    } else {
        snprintf(buf, cap, "%s", c->description ? c->description : "");
    }
}

static json_t *mig_option_json(const mig_opt *o) {
    json_t *j = json_object();
    json_t *opts = json_array();
    size_t i;

    for (i = 0; i < o->nopts; i++) json_append(opts, json_string(o->opts[i]));
    json_set(j, "dest", json_string(o->dest));
    json_set(j, "option_strings", opts);
    json_set(j, "action", json_string(o->action));
    json_set(j, "default", o->def_kind == MIG_DEF_FALSE ? json_bool(false) : json_null());
    json_set(j, "nargs", o->nargs < 0 ? json_null() : json_number((double)o->nargs));
    json_set(j, "metavar", o->metavar ? json_string(o->metavar) : json_null());
    json_set(j, "help", json_string(o->help ? o->help : ""));
    return j;
}

static json_t *mig_unknown(void) {
    json_t *o = json_object();
    json_set(o, "error", json_string("unknown_command"));
    return o;
}

json_t *w23_migrate_73e740_subcommand_names(void) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < W23_MIGRATE_73E740_SUBCOMMANDS; i++)
        json_append(out, json_string(MIG_COMMANDS[i].name));
    return out;
}

json_t *w23_migrate_73e740_surface(const json_t *path) {
    const mig_cmd *cmd = NULL;
    json_t *out, *options, *subs, *defaults;
    char help_buf[256], desc_buf[1024];
    size_t i, len = 0;

    if (path && path->type != JSON_NULL) {
        if (path->type != JSON_ARRAY) return mig_unknown();
        len = json_len(path);
        if (len > 1) return mig_unknown();
        if (len == 1) {
            const json_t *name = json_get(path, 0);
            if (!name || name->type != JSON_STRING || !name->str_val) return mig_unknown();
            for (i = 0; i < W23_MIGRATE_73E740_SUBCOMMANDS; i++)
                if (strcmp(name->str_val, MIG_COMMANDS[i].name) == 0) cmd = &MIG_COMMANDS[i];
            if (!cmd) return mig_unknown();
        }
    }

    out = json_object();
    options = json_array();
    defaults = json_object();

    if (!cmd) {
        subs = json_array();
        json_set(out, "help", json_string(MIG_ROOT_HELP));
        json_set(out, "description", json_string(MIG_ROOT_DESC));
        json_set(out, "options", options);
        for (i = 0; i < W23_MIGRATE_73E740_SUBCOMMANDS; i++) {
            json_t *row = json_object();
            mig_help_of(&MIG_COMMANDS[i], help_buf, sizeof(help_buf));
            json_set(row, "name", json_string(MIG_COMMANDS[i].name));
            json_set(row, "help", json_string(help_buf));
            json_append(subs, row);
        }
        json_set(out, "subcommands", subs);
        json_set(defaults, "func", json_string("cmd_migrate"));
        json_set(out, "defaults", defaults);
        return out;
    }

    mig_help_of(cmd, help_buf, sizeof(help_buf));
    mig_desc_of(cmd, desc_buf, sizeof(desc_buf));
    json_set(out, "help", json_string(help_buf));
    json_set(out, "description", json_string(desc_buf));
    for (i = 0; i < cmd->nopts; i++) json_append(options, mig_option_json(&cmd->opts[i]));
    json_set(out, "options", options);
    json_set(out, "subcommands", json_null());
    json_set(defaults, "func", json_string(cmd->func));
    if (cmd->agent) json_set(defaults, "migrate_agent", json_string(cmd->name));
    json_set(out, "defaults", defaults);
    return out;
}
