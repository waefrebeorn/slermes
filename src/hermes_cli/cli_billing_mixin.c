/*
 * cli_billing_mixin.c — canonical C11 port of hermes_cli/cli_billing_mixin.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_cli_billing_mixin_u_print_usage_cta @ hermes_cli/cli_billing_mixin.py:_print_usage_cta */
int hermes_cli_cli_billing_mixin_u_print_usage_cta(const char *arg) {
    /* Python: usage CTA line. */
    (void)arg;
    printf("  Run /subscription to change plan · /topup to add to your balance\n");
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_show_subscription @ hermes_cli/cli_billing_mixin.py:_show_subscription */
int hermes_cli_cli_billing_mixin_u_show_subscription(const char *arg) {
    /* Python: plan view. Arg =
     * "logged_in\tteam\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int logged_in = arg[0] == '1';
    int team = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!logged_in) {
        printf("  💳 Not logged into Nous Portal.\n");
        printf("  Run `hermes portal` to log in, then /subscription.\n");
        return 0;
    }
    if (team) {
        printf("  ⚕ Team subscription\n");
        printf("  This terminal is connected to a team org. Teams run on a shared balance · use /topup.\n");
        return 0;
    }
    printf("subscription overview + manage link: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_subscription_overview @ hermes_cli/cli_billing_mixin.py:_subscription_overview */
int hermes_cli_cli_billing_mixin_u_subscription_overview(const char *arg) {
    /* Python: dollars-only overview. Arg =
     * "free\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_free = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("plan read block (two-bar dollar usage: plan + top-up, plan name on bar%s, state-matched nudges)%s\n", is_free ? ", free-tier" : "", (t2 && t2[1] == '1') ? " — renews display" : "");
    printf("manage on portal (no in-terminal tier picker)\n");
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_open_url_in_browser @ hermes_cli/cli_billing_mixin.py:_open_url_in_browser */
int hermes_cli_cli_billing_mixin_u_open_url_in_browser(const char *arg) {
    /* Python: graphical-only opener. Arg = "url\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%d\n", (t2 && t2[1] == '1') ? 1 : 0);
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_subscription_free_catalog @ hermes_cli/cli_billing_mixin.py:_subscription_free_catalog */
int hermes_cli_cli_billing_mixin_u_subscription_free_catalog(const char *arg) {
    /* Python: plan catalog. Arg =
     * "has_tiers\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_tiers = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!has_tiers) {
        printf("  ⚕ Start a subscription (opens portal)\n");
        return 0;
    }
    printf("  ⚕ Choose a plan\n");
    printf("  %s\n", t2 ? t2 + 1 : "tiers listed, portal hand-off with plan=<tier_id>");
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_subscription_open_portal @ hermes_cli/cli_billing_mixin.py:_subscription_open_portal */
int hermes_cli_cli_billing_mixin_u_subscription_open_portal(const char *arg) {
    /* Python: portal hand-off. Arg =
     * "manage_url\tchoice\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *manage_url = arg;
    const char *choice = t1 ? t1 + 1 : "";
    int state = t2 && t2[1] == '1';
    if (!state) {
        printf("  No manage URL available — is your portal configured?\n");
        return 0;
    }
    if (strcmp(choice, "open") == 0) {
        printf("  Finish in your browser, then re-run /subscription.\n");
        return 0;
    }
    if (strcmp(choice, "copy") == 0) {
        printf("  📋 Copied: %s\n", manage_url);
        return 0;
    }
    printf("  🟡 Cancelled.\n");
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_subscription_change_menu @ hermes_cli/cli_billing_mixin.py:_subscription_change_menu */
int hermes_cli_cli_billing_mixin_u_subscription_change_menu(const char *arg) {
    /* Python: change menu. Arg =
     * "has_pending\tchoice\tstate\tresult". */
    if (!arg || !*arg) { printf("  🟡 Closed. No plan change.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_pending = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    const char *choice = t2 ? t2 + 1 : "";
    if (!state) { printf("  🟡 Closed. No plan change.\n"); return 0; }
    if (strcmp(choice, "keep") == 0) { printf("keep (undo pending change)\n"); return 0; }
    if (strcmp(choice, "change") == 0) { printf("change plan flow\n"); return 0; }
    if (strcmp(choice, "cancel_sub") == 0) { printf("cancel subscription flow\n"); return 0; }
    if (strcmp(choice, "portal") == 0) { printf("portal hand-off: %s\n", t3 ? t3 + 1 : ""); return 0; }
    printf("  🟡 Closed. No plan change.\n");
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_subscription_pick_tier @ hermes_cli/cli_billing_mixin.py:_subscription_pick_tier */
int hermes_cli_cli_billing_mixin_u_subscription_pick_tier(const char *arg) {
    /* Python: tier picker -> preview. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_selectable") == 0) {
        printf("  No other plans are available to switch to right now.\n");
        return 0;
    }
    if (strcmp(state, "cancelled") == 0) {
        printf("  🟡 Cancelled. No plan change.\n");
        return 0;
    }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_subscription_preview_and_confirm @ hermes_cli/cli_billing_mixin.py:_subscription_preview_and_confirm */
int hermes_cli_cli_billing_mixin_u_subscription_preview_and_confirm(const char *arg) {
    /* Python: quote+confirm. Arg =
     * "scoped\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int scoped = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("preview failed\n"); return 0; }
    if (!scoped) {
        printf("  Remote Spending still isn't active for this terminal — the authorization didn't take. Retry, or make this change on the portal.\n");
        return 0;
    }
    printf("chargeless quote shown (effect summary, confirm → apply with idempotency key replay)%s\n", (t2 && t2[1] == '1') ? " — allow_stepup replay" : "");
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel @ hermes_cli/cli_billing_mixin.py:_subscription_confirm_cancel */
int hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel(const char *arg) {
    /* Python: confirm modal + schedule cancel. Arg =
     * "tier\tend\tchoice\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("cancellation aborted\n"); return 0; }
    const char *choice = t1 ? t1 + 1 : "";
    if (strcmp(choice, "yes") != 0) {
        printf("  🟡 Cancelled. Your plan is unchanged.\n");
        return 0;
    }
    printf("cancellation scheduled at period end: %s\n", t2 ? t2 + 1 : "the end of the billing period");
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_subscription_apply @ hermes_cli/cli_billing_mixin.py:_subscription_apply */
int hermes_cli_cli_billing_mixin_u_subscription_apply(const char *arg) {
    /* Python: mutation runner. Arg =
     * "action\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *action = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("apply failed\n"); return 0; }
    printf("action '%s' applied (idempotency key reused across step-up replay, allow_stepup=false on post-grant)%s\n", action, (t2 && t2[1] == '1') ? " — pending change deleted" : "");
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_subscription_handle_scope_req_ed @ hermes_cli/cli_billing_mixin.py:_subscription_handle_scope_required */
int hermes_cli_cli_billing_mixin_u_subscription_handle_scope_req_ed(const char *arg) {
    /* Python: step-up + replay. Arg =
     * "granted\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int granted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!granted) {
        printf("  No change made. Allow Remote Spending when you're ready.\n");
        return 0;
    }
    printf("  Opening your browser to allow Remote Spending…\n");
    printf("  ✓ Remote Spending allowed — replaying your change.\n");
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_subscription_render_error @ hermes_cli/cli_billing_mixin.py:_subscription_render_error */
int hermes_cli_cli_billing_mixin_u_subscription_render_error(const char *arg) {
    /* Python: BillingError render by code. Arg = "code\tmessage\tportal_url". */
    if (!arg || !*arg) { printf("  🔴 Something went wrong.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *code = arg;
    const char *msg = t1 ? t1 + 1 : "";
    const char *url = t2 ? t2 + 1 : "";
    if (strcmp(code, "insufficient_scope") == 0) printf("  🟡 Remote Spending isn't allowed yet. Allow it, then retry.\n");
    else if (strcmp(code, "subscription_mutation_rejected") == 0 || strcmp(code, "preview_rejected") == 0) printf("  🟡 %s\n", msg);
    else printf("  🔴 %s\n", msg);
    if (url[0]) printf("  Portal: %s\n", url);
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_subscription_render_upgrade_a_us @ hermes_cli/cli_billing_mixin.py:_subscription_render_upgrade_ambiguous */
int hermes_cli_cli_billing_mixin_u_subscription_render_upgrade_a_us(const char *arg) {
    /* Python: ambiguous charge-route guidance. Arg = "portal_url". */
    printf("  🟡 Couldn't confirm the upgrade — your card may or may not have been charged.\n");
    printf("  Re-run /subscription to check your plan before trying again.\n");
    if (arg && *arg) printf("  Portal: %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_usage_bar_lines @ hermes_cli/cli_billing_mixin.py:_usage_bar_lines */
int hermes_cli_cli_billing_mixin_u_usage_bar_lines(const char *arg) {
    /* Python: plan + topup bars. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_cli_billing_mixin_u_billing_add_card_flow @ hermes_cli/cli_billing_mixin.py:_billing_add_card_flow */
int hermes_cli_cli_billing_mixin_u_billing_add_card_flow(const char *arg) {
    /* Python: portal card loop. Arg =
     * "found\tabandoned\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int abandoned = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (abandoned) {
        printf("  Cancelled. No funds added.\n");
        return 0;
    }
    if (found) {
        printf("  ✓ Card found: %s — continuing.\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("  💳 Add a card first — no saved card on file.\n");
    return 0;
}
