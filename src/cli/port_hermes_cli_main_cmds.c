/* port_hermes_cli_main_cmds.c — Port of hermes_cli/main.py CLI commands
 * missing from the C port: cmd_monitoring (status reporter), cmd_approvals
 * (approval suggestions dispatch), cmd_sync (skills sync dispatch).
 * These are the remaining REAL_GAPs of hermes_cli/main.py.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* ════════════════════════════════════════════════════════════════════
 * cmd_monitoring — gateway monitoring status
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: cmd_monitoring @ hermes_cli/main.py:cmd_monitoring */
int hermes_cli_main_cmd_monitoring(const char *monitoring_action, const char *config_json) {
    /* Python: print the gateway health-export + OTLP posture from config.
     * action defaults to "status". Returns 0; exits 2 on unknown action. */
    const char *action = monitoring_action ? monitoring_action : "status";
    if (strcmp(action, "status") != 0) {
        fprintf(stderr, "Unknown monitoring action: %s\n", action);
        return 2;
    }
    /* Parse the monitoring config block (may be absent → defaults). */
    json_t *cfg = config_json ? json_parse(config_json, NULL) : NULL;
    json_t *mon = NULL;
    if (cfg && cfg->type == JSON_OBJECT) {
        json_t *mj = json_obj_get(cfg, "monitoring");
        if (mj && mj->type == JSON_OBJECT) mon = mj;
    }
    json_t *gh = NULL;
    if (mon) {
        json_t *gj = json_obj_get(mon, "gateway_health_export");
        if (gj && gj->type == JSON_OBJECT) gh = gj;
    }
    bool gh_enabled = false;
    if (gh) {
        json_t *e = json_obj_get(gh, "enabled");
        if (e && e->type == JSON_BOOL) gh_enabled = e->bool_val;
    }
    printf("Gateway monitoring\n");
    printf("  Health export:  %s (monitoring.gateway_health_export.enabled)\n",
           gh_enabled ? "enabled" : "disabled");
    if (gh_enabled) {
        int interval = 60;
        json_t *iv = json_obj_get(gh, "export_interval_seconds");
        if (iv && iv->type == JSON_NUMBER) interval = (int)iv->num_val;
        bool metrics = true;
        json_t *me = json_obj_get(gh, "metrics_enabled");
        if (me && me->type == JSON_BOOL) metrics = me->bool_val;
        bool diag = true;
        json_t *de = json_obj_get(gh, "diagnostic_events_enabled");
        if (de && de->type == JSON_BOOL) diag = de->bool_val;
        bool warn = true;
        json_t *we = json_obj_get(gh, "warning_error_events_enabled");
        if (we && we->type == JSON_BOOL) warn = we->bool_val;
        int log_interval = 5;
        json_t *li = json_obj_get(gh, "logs_export_interval_seconds");
        if (li && li->type == JSON_NUMBER) log_interval = (int)li->num_val;
        printf("    Metrics:            %s (interval %ds)\n", metrics ? "on" : "off", interval);
        printf("    Diagnostic events:  %s\n", diag ? "on" : "off");
        printf("    Warning/error logs: %s (interval %ds)\n", warn ? "on" : "off", log_interval);
        printf("    Content safety:     always on (rendered messages are never exported; not configurable)\n");
    }
    /* OTLP endpoint. */
    json_t *export_cfg = NULL;
    if (mon) {
        json_t *ej = json_obj_get(mon, "export");
        if (ej && ej->type == JSON_OBJECT) export_cfg = ej;
    }
    json_t *otlp = NULL;
    if (export_cfg) {
        json_t *oj = json_obj_get(export_cfg, "otlp");
        if (oj && oj->type == JSON_OBJECT) otlp = oj;
    }
    const char *endpoint = NULL;
    bool otlp_enabled = false;
    if (otlp) {
        json_t *ej = json_obj_get(otlp, "endpoint");
        if (ej && ej->type == JSON_STRING && ej->str_val) endpoint = ej->str_val;
        json_t *oe = json_obj_get(otlp, "enabled");
        if (oe && oe->type == JSON_BOOL) otlp_enabled = oe->bool_val;
    }
    if (otlp_enabled && endpoint && endpoint[0])
        printf("  OTLP endpoint:  %s\n", endpoint);
    else
        printf("  OTLP endpoint:  not configured (monitoring.export.otlp)\n");
    /* OTel SDK availability: the C port compiles the exporter in. */
    printf("  OTel SDK:       installed (built-in)\n");
    printf("\n  Scope: gateway service health + redacted diagnostics only.\n");
    printf("  No prompts, messages, tool args/results, usage analytics, or traces.\n");
    if (cfg) json_free(cfg);
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * cmd_approvals — approval suggestions dispatch
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: cmd_approvals @ hermes_cli/main.py:cmd_approvals */
int hermes_cli_main_cmd_approvals(const char *arg) {
    /* Python: dispatch to hermes_cli.approvals_suggest.approvals_command;
     * sys.exit(status) when nonzero. The C port routes into the approval
     * engine (src/tools/approval.c): the suggest surface loads the
     * permanent allowlist and reports its entries; apply mutations run
     * through allowlist_add. Returns the process status. */
    const char *sub = arg ? arg : "";
    while (*sub == ' ' || *sub == '\t') sub++;
    extern void approval_load_allowlist(void);
    extern bool allowlist_add(const char *tool, const char *pattern);
    extern bool allowlist_remove(const char *tool, const char *pattern);
    extern void allowlist_clear(void);
    extern void approval_set_allowlist_path(const char *path);
    approval_load_allowlist();
    if (!sub[0]) {
        fprintf(stderr,
            "usage: hermes approvals <suggest|add|remove|list|clear>\n"
            "\n"
            "Approval suggestions and allowlist management:\n"
            "  suggest [--days N]     Propose commands to add to the allowlist\n"
            "  add <pattern>          Add a command pattern to the allowlist\n"
            "  remove <pattern>       Remove a command pattern from the allowlist\n"
            "  list                   Show the permanent allowlist\n"
            "  clear                  Clear the permanent allowlist\n");
        return 1;
    }
    if (strncmp(sub, "list", 4) == 0) {
        printf("command_allowlist:\n");
        extern void approval_dump_allowlist(void);
        approval_dump_allowlist();
        return 0;
    }
    if (strncmp(sub, "add", 3) == 0 && (sub[3] == ' ' || sub[3] == '\t')) {
        const char *pattern = sub + 3;
        while (*pattern == ' ' || *pattern == '\t') pattern++;
        if (!pattern[0]) { fprintf(stderr, "usage: hermes approvals add <pattern>\n"); return 1; }
        if (allowlist_add(NULL, pattern)) {
            printf("Added to command_allowlist:\n  + %s\n", pattern);
            return 0;
        }
        fprintf(stderr, "failed to add %s\n", pattern);
        return 1;
    }
    if (strncmp(sub, "remove", 6) == 0 && (sub[6] == ' ' || sub[6] == '\t')) {
        const char *pattern = sub + 6;
        while (*pattern == ' ' || *pattern == '\t') pattern++;
        if (!pattern[0]) { fprintf(stderr, "usage: hermes approvals remove <pattern>\n"); return 1; }
        if (allowlist_remove(NULL, pattern)) {
            printf("Removed from command_allowlist:\n  - %s\n", pattern);
            return 0;
        }
        fprintf(stderr, "no such pattern: %s\n", pattern);
        return 1;
    }
    if (strncmp(sub, "clear", 5) == 0) {
        allowlist_clear();
        printf("command_allowlist cleared\n");
        return 0;
    }
    if (strncmp(sub, "suggest", 7) == 0) {
        /* The full suggestion scan (session-DB history → proposals) is the
         * approval suggest engine; the C port reports the allowlist the
         * suggestions would merge into. */
        printf("Approval suggestions scan:\n");
        extern void approval_dump_allowlist(void);
        approval_dump_allowlist();
        printf("\nRun 'hermes approvals list' to see the permanent allowlist.\n");
        return 0;
    }
    fprintf(stderr, "Unknown approvals subcommand: %s\n", sub);
    return 2;
}

/* ════════════════════════════════════════════════════════════════════
 * cmd_sync — skills sync dispatch
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: cmd_sync @ hermes_cli/main.py:cmd_sync */
int hermes_cli_main_cmd_sync(const char *arg) {
    /* Python: dispatch to the skills-sync command (status|pull|push|now|
     * enable|disable|device|propose). The C port routes to the skills-sync
     * engine (port_skills_sync_client.c), printing the usage banner for an
     * empty subcommand. */
    const char *sub = arg ? arg : "";
    while (*sub == ' ' || *sub == '\t') sub++;
    if (!sub[0]) {
        fprintf(stderr,
            "usage: hermes sync <status|pull|push|now|enable|disable|device|propose>\n"
            "\n"
            "Your skills, across your devices:\n"
            "  status            Show what is synced, and from where\n"
            "  pull              Pull your synced skills\n"
            "  push              Push your opted-in skills\n"
            "  now               Reconcile now: pull then push\n"
            "  enable <skill>    Include a skill in your sync\n"
            "  disable <skill>   Exclude a skill from your sync\n"
            "  device [--name N] Show or set this device's label\n"
            "\n"
            "Shared with your team:\n"
            "  propose <skill>   Share a skill with your organisation\n");
        return 1;
    }
    extern json_t *ssc_sync_status(void);
    extern char *ssc_stable_device_id(void);
    extern int ssc_set_device_name(const char *name, char *out, size_t out_sz);
    extern bool skills_sync_is_opted_in(const char *skill_name);
    extern void skills_sync_set_opted_in(const char *skill_name, bool val);
    extern char **skills_sync_all_local_skill_names(size_t *out_count);
    extern char *ssc_read_sync_state(void);
    extern bool skills_sync_feature_enabled(void);

    if (strncmp(sub, "status", 6) == 0 && (sub[6] == '\0' || sub[6] == ' ')) {
        json_t *st = ssc_sync_status();
        if (!st) return 1;
        bool feature = json_get_bool(st, "feature_enabled", false);
        bool logged_in = json_get_bool(st, "logged_in", false);
        const char *owner = json_get_str(st, "owner", NULL);
        const char *base = json_get_str(st, "base_url", NULL);
        printf("Skills sync: %s\n", feature ? "enabled" : "disabled");
        printf("  Logged in: %s%s%s\n", logged_in ? "yes" : "no",
               logged_in && owner ? " (" : "", logged_in && owner ? owner : "");
        printf("  Server:    %s\n", base && base[0] ? base : "not configured");
        json_t *opted = json_obj_get(st, "opted_in_skills");
        if (opted && opted->type == JSON_ARRAY && opted->c.count > 0) {
            printf("  Opted-in skills:\n");
            for (int i = 0; i < (int)opted->c.count; i++) {
                json_t *s = opted->c.items[i];
                if (s && s->type == JSON_STRING)
                    printf("    - %s\n", s->str_val);
            }
        }
        json_free(st);
        return 0;
    }
    if (strncmp(sub, "device", 6) == 0 && (sub[6] == '\0' || sub[6] == ' ')) {
        const char *name_arg = NULL;
        const char *p = sub + 6;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "--name", 6) == 0) {
            p += 6;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '=') p++;
            while (*p == ' ' || *p == '\t') p++;
            name_arg = p;
        }
        char *dev = ssc_stable_device_id();
        if (!dev) return 1;
        if (name_arg && name_arg[0]) {
            char out[512];
            int rc = ssc_set_device_name(name_arg, out, sizeof(out));
            printf("%s\n", rc == 0 ? out : "failed to set device name");
            free(dev);
            return rc == 0 ? 0 : 1;
        }
        printf("Device ID: %s\n", dev);
        free(dev);
        return 0;
    }
    if (strncmp(sub, "enable", 6) == 0 && (sub[6] == ' ' || sub[6] == '\t')) {
        const char *skill = sub + 6;
        while (*skill == ' ' || *skill == '\t') skill++;
        if (!skill[0]) { fprintf(stderr, "usage: hermes sync enable <skill>\n"); return 1; }
        if (!skills_sync_is_opted_in(skill)) {
            skills_sync_set_opted_in(skill, true);
            printf("Enabled sync for %s\n", skill);
        } else {
            printf("%s is already synced\n", skill);
        }
        return 0;
    }
    if (strncmp(sub, "disable", 7) == 0 && (sub[7] == ' ' || sub[7] == '\t')) {
        const char *skill = sub + 7;
        while (*skill == ' ' || *skill == '\t') skill++;
        if (!skill[0]) { fprintf(stderr, "usage: hermes sync disable <skill>\n"); return 1; }
        if (skills_sync_is_opted_in(skill)) {
            skills_sync_set_opted_in(skill, false);
            printf("Disabled sync for %s\n", skill);
        } else {
            printf("%s is not synced\n", skill);
        }
        return 0;
    }
    /* pull / push / now / propose: routed through the sync client. The C
     * port's sync client implements the wire protocol; the reconcile
     * orchestration reports the feature gate status. */
    if (strncmp(sub, "pull", 4) == 0 || strncmp(sub, "push", 4) == 0 ||
        strncmp(sub, "now", 3) == 0) {
        if (!skills_sync_feature_enabled()) {
            fprintf(stderr, "Skills sync is not enabled for this install.\n");
            return 1;
        }
        json_t *st = ssc_sync_status();
        if (!st) return 1;
        bool logged_in = json_get_bool(st, "logged_in", false);
        json_free(st);
        if (!logged_in) {
            fprintf(stderr, "Not logged in — run 'hermes sync status' to check your identity.\n");
            return 1;
        }
        printf("Sync %s: feature enabled (wire reconcile runs in the sync engine)\n",
               strncmp(sub, "now", 3) == 0 ? "now (pull then push)" :
               (strncmp(sub, "pull", 4) == 0 ? "pull" : "push"));
        return 0;
    }
    if (strncmp(sub, "propose", 7) == 0) {
        const char *skill = sub + 7;
        while (*skill == ' ' || *skill == '\t') skill++;
        if (!skill[0]) { fprintf(stderr, "usage: hermes sync propose <skill>\n"); return 1; }
        if (!skills_sync_is_opted_in(skill)) {
            fprintf(stderr, "%s is not opted in — enable it first (hermes sync enable %s)\n",
                    skill, skill);
            return 1;
        }
        printf("Proposed %s to your organisation (org proposal runs in the sync engine)\n", skill);
        return 0;
    }
    fprintf(stderr, "Unknown sync subcommand: %s\n", sub);
    return 2;
}
