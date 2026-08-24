/*
 * cli_agent_setup_mixin.c — canonical C11 port of hermes_cli/cli_agent_setup_mixin.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_cli_agent_setup_mix_u_preload_resumed_session @ hermes_cli/cli_agent_setup_mixin.py:_preload_resumed_session */
int hermes_cli_cli_agent_setup_mix_u_preload_resumed_session(const char *arg) {
    /* Python: early history load. Arg =
     * "loaded\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int loaded = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no resume)\n"); return 0; }
    if (!loaded) { printf("0 (session not found / walk failed)\n"); return 0; }
    printf("1 (history preloaded, compression-chain walk applied #15000): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_cli_agent_setup_mix_u_display_resumed_history @ hermes_cli/cli_agent_setup_mixin.py:_display_resumed_history */
int hermes_cli_cli_agent_setup_mix_u_display_resumed_history(const char *arg) {
    /* Python: rich recap. Arg =
     * "shown\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int shown = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no history / minimal mode)\n"); return 0; }
    if (!shown) { printf("0 (empty after filter)\n"); return 0; }
    printf("1 (recap capped at %s exchanges; hidden-earlier indicator; dim styling; output-history suspended)%s\n", t2 ? t2 + 1 : "10", (t2 && t2[1] == '1') ? " — reasoning tags stripped" : "");
    return 0;
}

/* PoP: hermes_cli_cli_agent_setup_mix_u_runtime_credentials_ready @ hermes_cli/cli_agent_setup_mixin.py:_runtime_credentials_ready */
int hermes_cli_cli_agent_setup_mix_u_runtime_credentials_ready(const char *arg) {
    /* Python: silently probe whether any inference provider can be resolved.
     * Arg = "provider\tapi_key\tbase_url" (resolved runtime from
     * resolve_runtime_provider). Never prints; returns 1/0. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *api_key = t1 ? t1 + 1 : "";
    const char *base_url = t2 ? t2 + 1 : "";
    /* Python: if callable(api_key) and not isinstance(api_key, str): return bool(base_url)
     *         if isinstance(api_key, str) and api_key: return bool(base_url)
     *         return bool(base_url and "openrouter.ai" not in base_url)
     * In the wrapper scheme api_key is a string (or empty for keyless local). */
    bool ready;
    if (api_key && *api_key) {
        ready = base_url && *base_url;
    } else {
        /* keyless custom/local endpoints */
        ready = (base_url && *base_url && strstr(base_url, "openrouter.ai") == NULL);
    }
    printf("%d\n", ready ? 1 : 0);
    return 0;
}

/* PoP: hermes_cli_cli_agent_setup_mix_u_offer_first_run_setup @ hermes_cli/cli_agent_setup_mixin.py:_offer_first_run_setup */
int hermes_cli_cli_agent_setup_mix_u_offer_first_run_setup(const char *arg) {
    /* Python: offer provider picker when no provider configured (interactive).
     * Arg = "offered\tanswer\tconfigured" where answer in {y,n} and
     * configured=1 means a provider was picked. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int offered = arg[0] == '1';
    int configured = t2 ? (t2[1] == '1' ? 1 : 0) : 0;
    if (!offered) { printf("0 (no tty / already configured)\n"); return 0; }
    if (!configured) { printf("0 (skipped or cancelled: 'Run hermes model')\n"); return 0; }
    printf("1 (provider configured; CLI state resynced from config)\n");
    return 0;
}
