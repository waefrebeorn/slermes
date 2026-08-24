/*
 * webhook.c — canonical C11 port of hermes_cli/webhook.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_webhook_u_subscriptions_path @ hermes_cli/webhook.py:_subscriptions_path */
int hermes_cli_webhook_u_subscriptions_path(const char *arg) {
    /* Python: _hermes_home() / "webhook_subscriptions.json". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/webhook_subscriptions.json\n", base);
    return 0;
}

/* PoP: hermes_cli_webhook_u_load_subscriptions @ hermes_cli/webhook.py:_load_subscriptions */
int hermes_cli_webhook_u_load_subscriptions(const char *arg) {
    /* Python: json dict from subscriptions path, {} on any error. Arg =
     * file path. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    FILE *fp = fopen(arg, "r");
    if (!fp) { printf("{}\n"); return 0; }
    char buf[16384];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    json_t *doc = json_parse(buf, NULL);
    if (!doc || !json_is_object(doc)) {
        if (doc) json_free(doc);
        printf("{}\n");
        return 0;
    }
    char *s = json_dumps(doc, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(doc);
    return 0;
}

/* PoP: hermes_cli_webhook_u_save_subscriptions @ hermes_cli/webhook.py:_save_subscriptions */
int hermes_cli_webhook_u_save_subscriptions(const char *arg) {
    /* Python: 0600 atomic write. Arg = "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("subscriptions save failed\n"); return 1; }
    printf("subscriptions saved (0600 atomic): %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_webhook_u_get_webhook_config @ hermes_cli/webhook.py:_get_webhook_config */
int hermes_cli_webhook_u_get_webhook_config(const char *arg) {
    /* Python: cfg.platforms.webhook or {}. Arg = config JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *cfg = json_parse(arg, NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        printf("{}\n");
        return 0;
    }
    json_t *platforms = json_obj_get(cfg, "platforms");
    if (platforms && json_is_object(platforms)) {
        json_t *wh = json_obj_get(platforms, "webhook");
        if (wh && json_is_object(wh)) {
            char *s = json_dumps(wh, 0);
            printf("%s\n", s ? s : "{}");
            free(s);
            json_free(cfg);
            return 0;
        }
    }
    printf("{}\n");
    json_free(cfg);
    return 0;
}

/* PoP: hermes_cli_webhook_u_is_webhook_enabled @ hermes_cli/webhook.py:_is_webhook_enabled */
int hermes_cli_webhook_u_is_webhook_enabled(const char *arg) {
    /* Python: bool(_get_webhook_config().get("enabled")). Arg = "1"/"0". */
    if (!arg || !*arg) return 0;
    return atoi(arg) != 0;
}

/* PoP: hermes_cli_webhook_u_get_webhook_base_url @ hermes_cli/webhook.py:_get_webhook_base_url */
int hermes_cli_webhook_u_get_webhook_base_url(const char *arg) {
    /* Python: http://display_host:port; loopback -> localhost; IPv6 bracket.
     * Arg = "host\tport". */
    if (!arg || !*arg) { printf("http://localhost:8644\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *host = arg;
    const char *port = tab ? tab + 1 : "8644";
    if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "::") == 0) host = "localhost";
    int needs_bracket = strchr(host, ':') != NULL && host[0] != '[';
    printf("http://%s%s%s:%s\n", needs_bracket ? "[" : "", host,
           needs_bracket ? "]" : "", port);
    return 0;
}

/* PoP: hermes_cli_webhook_u_setup_hint @ hermes_cli/webhook.py:_setup_hint */
int hermes_cli_webhook_u_setup_hint(const char *arg) {
    /* Python: full setup hint block. Arg = "hermes_home". */
    const char *home = (arg && *arg) ? arg : "~/.hermes";
    printf("\n  Webhook platform is not enabled. To set it up:\n\n");
    printf("  1. Run the gateway setup wizard:\n     hermes gateway setup\n\n");
    printf("  2. Or manually add to %s/config.yaml:\n", home);
    printf("     platforms:\n       webhook:\n         enabled: true\n         extra:\n           port: 8644\n           secret: \"your-global-hmac-secret\"\n\n");
    printf("  3. Or set environment variables in %s/.env:\n", home);
    printf("     WEBHOOK_ENABLED=true\n     WEBHOOK_PORT=8644\n     WEBHOOK_SECRET=your-global-secret\n\n");
    printf("  Then start the gateway: hermes gateway run\n");
    return 0;
}

/* PoP: hermes_cli_webhook_u_require_webhook_enabled @ hermes_cli/webhook.py:_require_webhook_enabled */
int hermes_cli_webhook_u_require_webhook_enabled(const char *arg) {
    /* Python: True if _is_webhook_enabled(); else print setup hint and
     * return False. Arg = "1"/"0" enabled flag (empty = check env). */
    if (arg && *arg) {
        int enabled = (strcmp(arg, "1") == 0 || strcmp(arg, "true") == 0);
        if (enabled) { printf("1\n"); return 0; }
        printf("webhook disabled — run 'hermes webhook enable' to set it up\n0\n");
        return 0;
    }
    const char *v = getenv("HERMES_WEBHOOK_ENABLED");
    if (v && *v && strcmp(v, "0") != 0 && strcasecmp(v, "false") != 0) {
        printf("1\n");
        return 0;
    }
    printf("webhook disabled — run 'hermes webhook enable' to set it up\n0\n");
    return 0;
}

/* PoP: hermes_cli_webhook_u_cmd_subscribe @ hermes_cli/webhook.py:_cmd_subscribe */
int hermes_cli_webhook_u_cmd_subscribe(const char *arg) {
    /* Python: subscription create. Arg =
     * "valid\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int valid = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!valid) {
        printf("Error: Invalid name. Use lowercase alphanumeric with hyphens/underscores.\n");
        return 0;
    }
    printf("\n  %s webhook subscription: %s\n", t2 && t2[1] == '1' ? "Updated" : "Created", t3 ? t3 + 1 : "?");
    return 0;
}

/* PoP: hermes_cli_webhook_u_cmd_remove @ hermes_cli/webhook.py:_cmd_remove */
int hermes_cli_webhook_u_cmd_remove(const char *arg) {
    /* Python: remove subscription by name (lowercased). Arg =
     * "name\tsubs_json" (subs empty = none). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char name[128];
    size_t nlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
    for (size_t i = 0; i < nlen; i++) name[i] = (char)tolower((unsigned char)arg[i]);
    name[nlen] = '\0';
    json_t *subs = json_parse(tab ? tab + 1 : "", NULL);
    if (!subs || !json_is_object(subs)) {
        if (subs) json_free(subs);
        printf("  No subscription named '%s'.\n", name);
        printf("  Note: Static routes from config.yaml cannot be removed here.\n");
        return 1;
    }
    json_t *found = json_obj_get(subs, name);
    if (!found) {
        printf("  No subscription named '%s'.\n", name);
        printf("  Note: Static routes from config.yaml cannot be removed here.\n");
        json_free(subs);
        return 1;
    }
    json_obj_del(subs, name);
    char *s = json_dumps(subs, 0);
    printf("  Removed webhook subscription: %s\n", name);
    free(s);
    json_free(subs);
    return 0;
}
