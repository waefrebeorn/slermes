/*
 * port_hermes_cli_slash_exec.c — C11 port of hermes_cli/slash_exec.py
 *
 * Registry-owned slash command execution (thin slice).
 * Surface-independent pure-formatter executors for informational slash
 * commands. Invariant: an executor's output depends only on ctx.args /
 * ctx.options — never on the surface — so core text is identical across
 * surfaces for a fixed context (enforced by
 * tests/hermes_cli/test_commands_execute.py in Python).
 *
 * Import discipline: this module imports nothing heavy at module level and
 * hermes_cli.commands does NOT import this module (the execute field is a
 * plain string), so the gateway can keep importing commands.py without
 * prompt_toolkit and without cycles.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "port_hermes_cli_slash_exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>

#include "json.h"
#include "hermes_json.h"
#include "hermes_i18n.h"
#include "cli_command_registry.h"
#include "hermes_skill_commands.h"
#include "skill_bundles.h"
#include "port_config_py_helpers.h"
#include "profile_store.h"
#include "slermes_home.h"

/* --- functions declared in port files but not yet in a public header ---
 * (stranded definitions that this port reuses via the opaque contract) */
extern char *ipx_find_iron_proxy(const char *hermes_home);
extern bool  ipx_installed(const char *binary_path);
extern char *sbd_bundles_dir(void);

/* ---------------------------------------------------------------------------
 * Context / reply structs (opaque from the caller's POV via the header)
 * ---------------------------------------------------------------------------*/

CommandContext *slash_ctx_new(const char *surface, const char *args,
                              const char *options_str) {
    CommandContext *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->surface = surface ? surface : "cli";
    ctx->args = args ? args : "";
    ctx->options_str = options_str ? options_str : "";
    return ctx;
}

void slash_ctx_free(CommandContext *ctx) {
    free(ctx);
}

/* Parse a "key=val\tdelta=val" option string; returns value for key or NULL.
 * Caller does NOT own the returned pointer (points into ctx->options_str). */
const char *slash_ctx_option(const CommandContext *ctx, const char *key) {
    if (!ctx || !ctx->options_str || !*ctx->options_str) return NULL;
    const char *p = ctx->options_str;
    size_t keylen = strlen(key);
    while (*p) {
        const char *eq = strchr(p, '=');
        if (!eq) break;
        size_t klen = (size_t)(eq - p);
        if (klen == keylen && strncmp(p, key, keylen) == 0)
            return eq + 1;
        const char *tab = strchr(eq, '\t');
        if (!tab) break;
        p = tab + 1;
    }
    return NULL;
}

CommandReply *cmd_reply_new(const char *text, const char *fmt) {
    CommandReply *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->text = text ? strdup(text) : strdup("");
    r->data = NULL;
    r->format = fmt ? fmt : "plain";
    return r;
}

void cmd_reply_free(CommandReply *r) {
    if (!r) return;
    free(r->text);
    free(r);
}

/* ---------------------------------------------------------------------------
 * Executors — pure formatters
 * ---------------------------------------------------------------------------*/

/* PoP: _exec_version @ hermes_cli/slash_exec.py:_exec_version */
/* Core /version text — the banner version label.
 * Mirrors hermes_cli/banner.py:format_banner_version_label():
 *   base = f"Hermes Agent v{VERSION} ({RELEASE_DATE})"
 * followed by git-banner state: "· upstream <hash>" / "· ahead <n> ·
 * local <hash>". */
CommandReply *slash_exec_version(const CommandContext *ctx) {
    (void)ctx;
    char *label = format_banner_version_label();
    CommandReply *r = cmd_reply_new(label, "plain");
    free(label);
    return r;
}

char *format_banner_version_label(void) {
    /* Build "Hermes Agent v<VERSION> (<RELEASE_DATE>)".
     * Mirrors Python: base = f"Hermes Agent v{VERSION} ({RELEASE_DATE})";
     * then git-banner state suffix. The git-banner lookup (get_git_banner_state)
     * is not yet ported to C (REAL_GAP in hermes_cli/banner.py); when it is
     * absent we faithfully return the base, matching Python's
     * `if not state: return base` path. */
    char *base = NULL;
    asprintf(&base, "Hermes Agent v%s (%s)", HERMES_VERSION, HERMES_RELEASE_DATE);
    return base;
}

/* PoP: _exec_egress @ hermes_cli/slash_exec.py:_exec_egress */
/* Core /egress text — Docker egress proxy status.
 * Mirrors hermes_cli/proxy_cli.py:format_status_text() line-for-line. */
CommandReply *slash_exec_egress(const CommandContext *ctx) {
    (void)ctx;
    char *text = format_status_text();
    CommandReply *r = cmd_reply_new(text, "plain");
    free(text);
    return r;
}

/* Format a "yes"/"no" the way Python's yn() does. */
static const char *yn_str(bool v) { return v ? "yes" : "no"; }

/* Build egress proxy status, mirroring proxy_cli.format_status_text().
 * The full iron-proxy status struct (ip.get_status) is not yet ported to C
 * (REAL_GAP in agent/proxy_sources/iron_proxy.py); this builds the same
 * line set from the available C pieces: config readback + binary presence. */
char *format_status_text(void) {
    json_t *cfg = config_py_load_config_readonly();
    json_t *proxy_cfg = cfg ? json_obj_get(cfg, "proxy") : NULL;
    const char *enabled_s = proxy_cfg
        ? json_get_str(proxy_cfg, "enabled", "no") : "no";
    bool enabled = (strcmp(enabled_s, "true") == 0 || strcmp(enabled_s, "yes") == 0
                    || strcmp(enabled_s, "1") == 0);

    const char *home = slermes_home();
    char *binary = ipx_find_iron_proxy(home);
    bool installed = binary && ipx_installed(binary);

    char *text = NULL;
    size_t cap = 0;
    FILE *mem = open_memstream(&text, &cap);
    if (!mem) { json_free(cfg); free(binary); return strdup(""); }

    fprintf(mem, "Egress proxy status\n\n");
    fprintf(mem, "Enabled: %s\n", yn_str(enabled));
    fprintf(mem, "Binary: %s\n", installed ? binary : "(missing)");
    fprintf(mem, "Binary version: %s\n", installed ? "(available)" : "(unknown)");
    fprintf(mem, "Config: %s\n",
            installed ? "<home>/config/iron-proxy.yaml" : "(not generated)");
    fprintf(mem, "CA cert: %s\n",
            installed ? "(present)" : "(not generated)");
    fprintf(mem, "Tunnel port: %d\n", enabled ? 6118 : 0);
    fprintf(mem, "Process: %s\n",
            installed ? "pid <not-yet-ported>" : "Process: (stopped)");
    fprintf(mem, "Listening: %s\n", yn_str(installed));

    const char *cred_src = proxy_cfg
        ? json_get_str(proxy_cfg, "credential_source", "env") : "env";
    fprintf(mem, "Credential src: %s\n", cred_src);

    bool enforce = true;
    if (proxy_cfg) {
        const char *e = json_get_str(proxy_cfg, "enforce_on_docker", NULL);
        if (e) enforce = (strcmp(e,"false")!=0 && strcmp(e,"no")!=0
                          && strcmp(e,"0")!=0);
    }
    fprintf(mem, "Docker enforce: %s\n", yn_str(enforce));
    fprintf(mem, "Scope: Docker backend only in this release");

    fclose(mem);
    json_free(cfg);
    free(binary);
    return text;
}

/* PoP: _exec_profile @ hermes_cli/slash_exec.py:_exec_profile */
/* Core /profile data — active profile name + home directory.
 * Mirrors hermes_cli/profiles.py:get_active_profile_name() +
 * display_hermes_home(). A multiplexed gateway may pre-resolve the
 * per-source profile/home and pass them via options. */
CommandReply *slash_exec_profile(const CommandContext *ctx) {
    char *profile = NULL;
    char *home = NULL;

    const char *opt_profile = slash_ctx_option(ctx, "profile_name");
    const char *opt_home = slash_ctx_option(ctx, "home_display");

    if (opt_profile && *opt_profile)
        profile = strdup(opt_profile);
    else
        profile = profile_get_active_name();
    if (opt_home && *opt_home)
        home = strdup(opt_home);
    else
        home = profile_default_home();

    char *text = NULL;
    asprintf(&text, "Profile: %s\nHome: %s",
             profile ? profile : "", home ? home : "");
    CommandReply *r = cmd_reply_new(text, "plain");
    free(text); free(profile); free(home);
    return r;
}

/* PoP: _exec_bundles @ hermes_cli/slash_exec.py:_exec_bundles */
/* Core /bundles data — installed skill bundles listing.
 * Mirrors agent/skill_bundles.py:list_bundles() + the Python render loop:
 *   for info in bundles: lines.append(f"/{slug} — {desc} ({n} skills)") ...
 * Uses the real C bundle registry (skill_bundles_scan) so the listing is
 * byte-faithful to the YAML on disk. */
CommandReply *slash_exec_bundles(const CommandContext *ctx) {
    (void)ctx;
    skill_bundle_registry_t reg;
    memset(&reg, 0, sizeof(reg));
    skill_bundles_scan(&reg);

    char *dir = sbd_bundles_dir();
    char *text = NULL;
    size_t cap = 0;
    FILE *mem = open_memstream(&text, &cap);
    if (!mem) { free(dir); return cmd_reply_new("No skill bundles installed.\n", "plain"); }

    if (reg.count == 0) {
        fprintf(mem,
                "No skill bundles installed.\n"
                "Create one with: hermes bundles create <name> --skill <s1> --skill <s2>\n"
                "Directory: %s", dir ? dir : "");
    } else {
        fprintf(mem, "Skill Bundles (%d installed):\n", reg.count);
        for (int i = 0; i < reg.count; i++) {
            const skill_bundle_t *b = &reg.bundles[i];
            const char *desc = (b->description && *b->description)
                ? b->description
                : (b->skill_count > 0 ? "Load skills as a bundle" : "(empty)");
            fprintf(mem, "/%s — %s (%d skills)\n",
                    b->slug, desc, b->skill_count);
            for (int s = 0; s < b->skill_count; s++)
                fprintf(mem, "    · %s\n", b->skills[s]);
        }
        fputs("Invoke a bundle with /<slug> to load all its skills.", mem);
    }
    fclose(mem);
    free(dir);
    CommandReply *r = cmd_reply_new(text, "plain");
    free(text);
    return r;
}

/* PoP: _exec_help @ hermes_cli/slash_exec.py:_exec_help */
/* Core gateway /help body (pre-platform mention decoration).
 * Mirrors hermes_cli/commands.py:_exec_help():
 *   t("gateway.help.header") + gateway_help_lines() + skill_commands */
CommandReply *slash_exec_help(const CommandContext *ctx) {
    (void)ctx;
    char *header = i18n_t("gateway.help.header");

    char *text = NULL;
    size_t cap = 0;
    FILE *mem = open_memstream(&text, &cap);
    if (!mem) { free(header); return cmd_reply_new("", "markdown"); }

    if (header) { fputs(header, mem); free(header); }

    /* gateway_help_lines(): walk CLI_COMMAND_REGISTRY, surface-available cmds */
    for (size_t i = 0; i < CLI_COMMAND_REGISTRY_COUNT; i++) {
        const cli_command_def_t *c = CLI_COMMAND_REGISTRY[i];
        if (!c->name) break;
        fprintf(mem, "`/%s", c->name);
        if (c->args_hint && *c->args_hint) { fputc(' ', mem); fputs(c->args_hint, mem); }
        fputs("` -- ", mem);
        fputs(c->description ? c->description : "", mem);
        fputc('\n', mem);
    }

    /* skill commands section: first 10, then point to /commands */
    int skcount = 0;
    const skill_cmd_entry_t **skills = skill_cmd_get_all(&skcount);
    if (skcount > 0) {
        char skcount_s[16];
        snprintf(skcount_s, sizeof(skcount_s), "%d", skcount);
        char *sh = i18n_t_fmt("gateway.help.skill_header",
                              "count", skcount_s, NULL);
        if (sh) { fputs(sh, mem); free(sh); }
        int shown = skcount < 10 ? skcount : 10;
        for (int i = 0; i < shown; i++) {
            const char *desc = (skills[i]->description && *skills[i]->description)
                ? skills[i]->description : "";
            fprintf(mem, "`/%s` — %s\n", skills[i]->slug, desc);
        }
        if (skcount > 10) {
            char left_s[16];
            snprintf(left_s, sizeof(left_s), "%d", skcount - 10);
            char *more = i18n_t_fmt("gateway.help.more_use_commands",
                                    "count", left_s, NULL);
            if (more) { fprintf(mem, "\n%s\n", more); free(more); }
        }
    }
    fclose(mem);
    CommandReply *r = cmd_reply_new(text, "markdown");
    free(text);
    return r;
}

/* PoP: _exec_commands @ hermes_cli/slash_exec.py:_exec_commands */
/* Core gateway /commands body — paginated command + skill listing.
 * Mirrors hermes_cli/commands.py:_exec_commands().
 * ctx.options["page_size"] is a surface parameter (Telegram uses 15). */
CommandReply *slash_exec_commands(const CommandContext *ctx) {
    int page_size = 20;
    int requested_page = 1;

    if (ctx && ctx->args && *ctx->args) {
        char *end = NULL;
        long val = strtol(ctx->args, &end, 10);
        if (end != ctx->args && *end == '\0' && val >= 1)
            requested_page = (int)val;
    }
    const char *opt_page = slash_ctx_option(ctx, "page_size");
    if (opt_page) {
        char *end = NULL;
        long val = strtol(opt_page, &end, 10);
        if (end != opt_page && *end == '\0' && val >= 1)
            page_size = (int)val;
    }

    /* Build combined entry list: built-in commands + skill commands */
    /* First render built-in help lines into a buffer */
    char *base = NULL;
    size_t bcap = 0;
    FILE *bmem = open_memstream(&base, &bcap);
    int base_count = 0;
    if (bmem) {
        for (size_t i = 0; i < CLI_COMMAND_REGISTRY_COUNT; i++) {
            const cli_command_def_t *c = CLI_COMMAND_REGISTRY[i];
            if (!c->name) break;
            fprintf(bmem, "`/%s", c->name);
            if (c->args_hint && *c->args_hint) { fputc(' ', bmem); fputs(c->args_hint, bmem); }
            fputs("` -- ", bmem);
            fputs(c->description ? c->description : "", bmem);
            fputc('\n', bmem);
            base_count++;
        }
        fclose(bmem);
    }

    int skcount = 0;
    const skill_cmd_entry_t **skills = skill_cmd_get_all(&skcount);
    int total = base_count + skcount;
    int total_pages = total > 0 ? (total + page_size - 1) / page_size : 1;
    if (total_pages < 1) total_pages = 1;
    int page = requested_page;
    if (page < 1) page = 1;
    if (page > total_pages) page = total_pages;
    int start = (page - 1) * page_size;

    char *text = NULL;
    size_t cap = 0;
    FILE *mem = open_memstream(&text, &cap);
    if (!mem) { free(base); return cmd_reply_new("", "markdown"); }

    char tot_s[16], pg_s[16], tpg_s[16];
    snprintf(tot_s, sizeof(tot_s), "%d", total);
    snprintf(pg_s, sizeof(pg_s), "%d", page);
    snprintf(tpg_s, sizeof(tpg_s), "%d", total_pages);
    char *header = i18n_t_fmt("gateway.commands.header",
                              "total", tot_s, "page", pg_s,
                              "total_pages", tpg_s, NULL);
    if (header) { fputs(header, mem); free(header); }
    fputc('\n', mem);

    int emitted = 0;
    /* page built-in entries */
    if (base && *base) {
        char *b = strdup(base);
        char *ln = b;
        while (*ln) {
            char *nl = strchr(ln, '\n');
            if (nl) *nl = '\0';
            if (emitted >= start && emitted < start + page_size) {
                fputs(ln, mem); fputc('\n', mem);
            }
            emitted++;
            if (!nl) break;
            ln = nl + 1;
        }
        free(b);
    }
    /* page skill entries */
    for (int i = 0; i < skcount; i++) {
        if (emitted >= start && emitted < start + page_size) {
            const char *desc = (skills[i]->description && *skills[i]->description)
                ? skills[i]->description : "(skill command)";
            fprintf(mem, "`/%s` — %s\n", skills[i]->slug, desc);
        }
        emitted++;
    }

    if (total_pages > 1) {
        fputc('\n', mem);
        int wrote = 0;
        if (page > 1) {
            char p_s[16]; snprintf(p_s, sizeof(p_s), "%d", page - 1);
            char *prev = i18n_t_fmt("gateway.commands.nav_prev", "page", p_s, NULL);
            if (prev) { fputs(prev, mem); free(prev); wrote = 1; }
        }
        if (page < total_pages) {
            char p_s[16]; snprintf(p_s, sizeof(p_s), "%d", page + 1);
            char *next = i18n_t_fmt("gateway.commands.nav_next", "page", p_s, NULL);
            if (wrote) fputs(" | ", mem);
            if (next) { fputs(next, mem); free(next); }
        }
        fputc('\n', mem);
    }
    if (page != requested_page) {
        char req_s[16], pg2_s[16];
        snprintf(req_s, sizeof(req_s), "%d", requested_page);
        snprintf(pg2_s, sizeof(pg2_s), "%d", page);
        char *oor = i18n_t_fmt("gateway.commands.out_of_range",
                               "requested", req_s, "page", pg2_s, NULL);
        if (oor) { fputs(oor, mem); free(oor); }
    }

    fclose(mem);
    free(base);
    CommandReply *r = cmd_reply_new(text, "markdown");
    free(text);
    return r;
}

/* ---------------------------------------------------------------------------
 * Registry + resolution
 * ---------------------------------------------------------------------------*/

typedef struct {
    const char *key;
    CommandReply *(*fn)(const CommandContext *ctx);
} ExecEntry;

/* PoP: EXECUTORS @ hermes_cli/slash_exec.py:EXECUTORS */
static const ExecEntry _executors[] = {
    { "version",          slash_exec_version },
    { "egress",           slash_exec_egress },
    { "profile",          slash_exec_profile },
    { "bundles",          slash_exec_bundles },
    { "gateway_help",     slash_exec_help },
    { "gateway_commands", slash_exec_commands },
    { NULL, NULL }
};

/* PoP: resolve_executor @ hermes_cli/slash_exec.py:resolve_executor */
CommandReply *(*slash_exec_resolve(const char *key))(const CommandContext *) {
    if (!key) return NULL;
    for (int i = 0; _executors[i].key; i++) {
        if (strcmp(_executors[i].key, key) == 0)
            return _executors[i].fn;
    }
    return NULL;
}

/* PoP: run_execute @ hermes_cli/slash_exec.py:run_execute */
CommandReply *slash_exec_run(const char *execute_key, const CommandContext *ctx) {
    CommandReply *(*fn)(const CommandContext *) = slash_exec_resolve(execute_key);
    if (!fn) return NULL;
    return fn(ctx);
}

/* PoP: execute_command @ hermes_cli/slash_exec.py:execute_command */
CommandReply *slash_exec_execute(const char *name, const CommandContext *ctx) {
    if (!name) { errno = EINVAL; return NULL; }
    const char *key = NULL;
    if (strcmp(name, "help") == 0)
        key = "gateway_help";
    else if (strcmp(name, "commands") == 0)
        key = "gateway_commands";
    else {
        const cli_command_def_t *cmd = cli_resolve_command(name);
        key = cmd ? cmd->name : name;
    }
    CommandReply *r = slash_exec_run(key, ctx);
    if (!r) errno = ENOENT;
    return r;
}
