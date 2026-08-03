/*
 * cli_cmd_parity.c — Command-surface parity handlers
 *
 * Ports the Python COMMAND_REGISTRY commands that had no C entry in the
 * dispatch table, so `slermes /<cmd>` matches `hermes /<cmd>` 1:1.
 *
 * Where a faithful wrapper already exists in the codebase
 * (port_cli_commands_mixin_wrappers.c — ccm_handle_*), we delegate to it
 * (reuse, don't duplicate). Only commands WITHOUT an existing wrapper are
 * implemented here, mirroring their Python source in
 * hermes_cli/commands.py + cli_commands_mixin.py + cli_billing_mixin.py.
 *
 * Commands: battery, blueprint, codex-runtime, egress, hatch, journey,
 * learn, moa, prompt, start, subscription, suggestions, timestamps, topup.
 */

#include "cli_cmd_parity.h"
#include "hermes_cli.h"
#include "hermes_agent.h"
#include "hermes_billing.h"
#include "port_config_py_helpers.h"
#include "battery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── existing faithful wrappers (port_cli_commands_mixin_wrappers.c) ── */
extern int ccm_handle_journey_command(const char *args);
extern int ccm_handle_hatch_command(const char *args);
extern int ccm_handle_suggestions_command(const char *args);
extern int ccm_handle_blueprint_command(const char *args);
extern int ccm_handle_learn_command(const char *args);
extern int ccm_handle_timestamps_command(const char *args);
extern int ccm_handle_prompt_compose_command(const char *args);

/* ── shared helpers ─────────────────────────────────────────────────── */

/* Set a nested config key via the shared config save helper. */
static int set_config_key(const char *key, const char *value) {
    json_t *v = json_new_string(value);
    if (!v) return 0;
    int rc = config_py_save_value(key, v);
    json_free(v);
    return rc == 0 ? 1 : 0;
}

/* Read a nested config boolean (string or bool; default false). */
static int get_config_bool(const char *key) {
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return 0;
    json_t *v = config_py_get_config_value(cfg, key, NULL);
    int on = 0;
    if (v) {
        if (v->type == JSON_BOOL) on = v->bool_val ? 1 : 0;
        else if (v->type == JSON_STRING && v->str_val) {
            if (strcasecmp(v->str_val, "true") == 0) on = 1;
        }
    }
    json_free(cfg);
    return on;
}

/* ── /battery ───────────────────────────────────────────────────────── */
/* PoP: battery @ hermes_cli/commands.py:battery
 * PoP: cmd_battery @ hermes_cli/cli_commands_mixin.py:_handle_battery_command */
void cmd_battery(const char *args, agent_state_t *state) {
    (void)state;
    const char *arg = "";
    if (args) {
        while (*args == ' ') args++;
        arg = args;
    }
    int on = get_config_bool("display.battery");
    battery_status_t *st = battery_read(true);

    if (strcmp(arg, "status") == 0 || strcmp(arg, "show") == 0) {
        if (battery_status_available(st)) {
            int pct = 0;
            bool has_pct = battery_status_percent(st, &pct);
            if (has_pct) {
                char *fmt = battery_format(st);
                printf("Battery indicator %s — currently %s\n",
                       on ? "on" : "off", fmt ? fmt : "");
                free(fmt);
            } else {
                printf("Battery indicator %s — no battery detected on this machine\n",
                       on ? "on" : "off");
            }
        } else {
            printf("Battery indicator %s — no battery detected on this machine\n",
                   on ? "on" : "off");
        }
        printf("  Usage: /battery [on|off|status]\n");
        battery_status_free(st);
        return;
    }
    battery_status_free(st);
    if (strcmp(arg, "on") == 0 || strcmp(arg, "true") == 0 || strcmp(arg, "yes") == 0) {
        set_config_key("display.battery", "true");
        printf("Battery indicator on.\n");
    } else if (strcmp(arg, "off") == 0 || strcmp(arg, "false") == 0 || strcmp(arg, "no") == 0) {
        set_config_key("display.battery", "false");
        printf("Battery indicator off.\n");
    } else if (arg[0] == '\0' || strcmp(arg, "toggle") == 0) {
        set_config_key("display.battery", on ? "false" : "true");
        printf("Battery indicator %s.\n", on ? "off" : "on");
    } else {
        printf("Unknown argument: %s\n", arg);
        printf("  Usage: /battery [on|off|status]\n");
    }
}

/* ── /timestamps ────────────────────────────────────────────────────── */
/* PoP: timestamps @ hermes_cli/commands.py:timestamps
 * PoP: cmd_timestamps @ hermes_cli/cli_commands_mixin.py:_handle_timestamps_command */
void cmd_timestamps(const char *args, agent_state_t *state) {
    (void)state;
    /* Delegate to the faithful wrapper (string coercion + config save). */
    ccm_handle_timestamps_command(args ? args : "");
}

/* ── /start ─────────────────────────────────────────────────────────── */
/* PoP: start @ hermes_cli/commands.py:start */
void cmd_start(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    /* Python: acknowledge platform start pings without replying. */
    printf("Acknowledged (start ping consumed — no reply sent).\n");
}

/* ── /egress ────────────────────────────────────────────────────────── */
/* PoP: egress @ hermes_cli/commands.py:egress */
void cmd_egress(const char *args, agent_state_t *state) {
    (void)state;
    const char *arg = "";
    if (args) {
        while (*args == ' ') args++;
        arg = args;
    }
    if (arg[0] != '\0' && strcmp(arg, "status") != 0) {
        printf("Unknown argument: %s\n", arg);
        printf("  Usage: /egress [status]\n");
        return;
    }
    /* Python reads Docker egress-proxy state from the environment. */
    const char *proxy = getenv("HERMES_EGRESS_PROXY");
    const char *enabled = getenv("HERMES_EGRESS_ENABLED");
    int active = enabled && strcmp(enabled, "1") == 0 && proxy && *proxy;
    if (active) {
        printf("Egress proxy: ACTIVE (%s)\n", proxy);
    } else {
        printf("Egress proxy: inactive\n");
        printf("  Set HERMES_EGRESS_PROXY to enable Docker egress routing.\n");
    }
}

/* ── /codex-runtime ─────────────────────────────────────────────────── */
/* PoP: codex-runtime @ hermes_cli/commands.py:codex-runtime */
void cmd_codex_runtime(const char *args, agent_state_t *state) {
    (void)state;
    const char *arg = "";
    if (args) {
        while (*args == ' ') args++;
        arg = args;
    }
    /* Python toggles codex app-server runtime (auto | codex_app_server). */
    if (arg[0] == '\0' || strcmp(arg, "status") == 0) {
        const char *mode = getenv("CODEX_RUNTIME");
        printf("Codex runtime: %s\n", mode && *mode ? mode : "auto");
        printf("  Usage: /codex-runtime [auto|codex_app_server|status]\n");
        return;
    }
    if (strcmp(arg, "auto") == 0 || strcmp(arg, "codex_app_server") == 0) {
        set_config_key("codex.runtime", arg);
        printf("Codex runtime set to: %s\n", arg);
    } else {
        printf("Unknown argument: %s\n", arg);
        printf("  Usage: /codex-runtime [auto|codex_app_server|status]\n");
    }
}

/* ── /subscription ──────────────────────────────────────────────────── */
/* PoP: subscription @ hermes_cli/commands.py:subscription
 * PoP: cmd_subscription @ hermes_cli/cli_billing_mixin.py:_show_subscription */
void cmd_subscription(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    /* Real billing-state read (build_billing_state fetches the portal's
     * /api/billing/state). Fail-open like Python: a missing key or portal
     * hiccup degrades to a clear message, never a crash. */
    extern billing_state_t build_billing_state(void);
    extern void billing_format_money(double amount, char *out, size_t out_sz);
    billing_state_t b = build_billing_state();

    if (!b.logged_in) {
        printf("  Nous plan: not logged in (%s)\n",
               b.error[0] ? b.error : "no billing key");
        printf("  Set NOUS_BILLING_KEY (or NOUS_API_KEY) to view your plan.\n");
        printf("  Run `slermes nous-account` or open the Nous portal to\n");
        printf("  manage your subscription in the browser.\n");
        return;
    }
    printf("  Nous plan:\n");
    if (b.org_name[0])
        printf("    org:       %s\n", b.org_name);
    if (b.role[0])
        printf("    role:      %s\n", b.role);
    if (b.balance_usd > 0 || b.logged_in) {
        char amt[64];
        billing_format_money(b.balance_usd, amt, sizeof(amt));
        printf("    balance:   %s\n", amt);
    }
    if (b.portal_url[0])
        printf("    portal:    %s\n", b.portal_url);
    printf("  Manage your subscription in the browser on the portal.\n");
}

/* ── /topup ─────────────────────────────────────────────────────────── */
/* PoP: topup @ hermes_cli/commands.py:topup
 * PoP: cmd_topup @ hermes_cli/cli_billing_mixin.py:_show_billing */
void cmd_topup(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    extern billing_state_t build_billing_state(void);
    extern void billing_format_money(double amount, char *out, size_t out_sz);
    billing_state_t b = build_billing_state();

    if (!b.logged_in) {
        printf("  Nous balance: not logged in (%s)\n",
               b.error[0] ? b.error : "no billing key");
        printf("  Set NOUS_BILLING_KEY (or NOUS_API_KEY) to view your balance.\n");
        printf("  Open the Nous portal to manage billing.\n");
        return;
    }
    char amt[64];
    billing_format_money(b.balance_usd, amt, sizeof(amt));
    printf("  Nous balance: %s\n", amt);
    if (b.auto_reload.enabled) {
        char thresh[64], top[64];
        billing_format_money(b.auto_reload.threshold_usd, thresh, sizeof(thresh));
        billing_format_money(b.auto_reload.reload_to_usd, top, sizeof(top));
        printf("  Auto top-up:  on (reload to %s when balance < %s)\n", top, thresh);
    } else {
        printf("  Auto top-up:  off\n");
    }
    if (b.portal_url[0])
        printf("  portal:    %s\n", b.portal_url);
    printf("  Open the Nous portal to top up and manage billing.\n");
}

/* ── /journey ───────────────────────────────────────────────────────── */
/* PoP: journey @ hermes_cli/commands.py:journey
 * PoP: cmd_journey @ hermes_cli/cli_commands_mixin.py:_handle_journey_command */
void cmd_journey(const char *args, agent_state_t *state) {
    (void)state;
    /* Delegate to the faithful journey wrapper (hermes_cli/journey.py). */
    ccm_handle_journey_command(args ? args : "");
}

/* ── /learn ─────────────────────────────────────────────────────────── */
/* PoP: learn @ hermes_cli/commands.py:learn
 * PoP: cmd_learn @ hermes_cli/cli_commands_mixin.py:_handle_learn_command */
void cmd_learn(const char *args, agent_state_t *state) {
    (void)state;
    /* Delegate to the faithful learn wrapper. */
    ccm_handle_learn_command(args ? args : "");
}

/* ── /moa ───────────────────────────────────────────────────────────── */
/* PoP: moa @ hermes_cli/commands.py:moa
 * PoP: cmd_moa @ hermes_cli/moa_cmd.py:cmd_moa */
void cmd_moa(const char *args, agent_state_t *state) {
    (void)state;
    /* Delegate to the faithful MoA command wrapper (hermes_cli/moa_cmd.py). */
    extern int hermes_cli_moa_cmd_cmd_moa(const char *arg);
    hermes_cli_moa_cmd_cmd_moa(args ? args : "");
}

/* ── /prompt ────────────────────────────────────────────────────────── */
/* PoP: prompt @ hermes_cli/commands.py:prompt
 * PoP: cmd_prompt @ hermes_cli/cli_commands_mixin.py:_handle_prompt_command */
void cmd_prompt(const char *args, agent_state_t *state) {
    (void)state;
    /* Delegate to the faithful prompt-compose wrapper ($EDITOR flow). */
    ccm_handle_prompt_compose_command(args ? args : "");
}

/* ── /suggestions ───────────────────────────────────────────────────── */
/* PoP: suggestions @ hermes_cli/commands.py:suggestions
 * PoP: cmd_suggestions @ hermes_cli/suggestions_cmd.py:handle_suggestions_command */
void cmd_suggestions(const char *args, agent_state_t *state) {
    (void)state;
    /* Delegate to the faithful suggestions wrapper. */
    ccm_handle_suggestions_command(args ? args : "");
}

/* ── /blueprint ─────────────────────────────────────────────────────── */
/* PoP: blueprint @ hermes_cli/commands.py:blueprint */
void cmd_blueprint(const char *args, agent_state_t *state) {
    (void)state;
    /* Delegate to the faithful blueprint wrapper. */
    ccm_handle_blueprint_command(args ? args : "");
}

/* ── /hatch ─────────────────────────────────────────────────────────── */
/* PoP: hatch @ hermes_cli/commands.py:hatch
 * PoP: cmd_hatch @ hermes_cli/cli_commands_mixin.py:_handle_hatch_command */
void cmd_hatch(const char *args, agent_state_t *state) {
    (void)state;
    /* Delegate to the faithful hatch wrapper (pet generation pipeline). */
    ccm_handle_hatch_command(args ? args : "");
}
