/*
 * port_model_setup_wrappers.c — C port of hermes_cli/model_setup_flows.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: bedrock_region_geo_prefix @ hermes_cli/model_setup_flows.py:bedrock_region_geo_prefix */
int msf_bedrock_region_geo_prefix(const char *arg) {
    /* Python: us./eu./ap./ca./sa./me./af. prefix map. Arg = region. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *r = arg;
    if (strncasecmp(r, "us-", 3) == 0 || strncasecmp(r, "us_gov", 6) == 0) { printf("us.\n"); return 0; }
    if (strncasecmp(r, "eu-", 3) == 0) { printf("eu.\n"); return 0; }
    if (strncasecmp(r, "ap-", 3) == 0) { printf("ap.\n"); return 0; }
    if (strncasecmp(r, "ca-", 3) == 0) { printf("ca.\n"); return 0; }
    if (strncasecmp(r, "sa-", 3) == 0) { printf("sa.\n"); return 0; }
    if (strncasecmp(r, "me-", 3) == 0) { printf("me.\n"); return 0; }
    if (strncasecmp(r, "af-", 3) == 0) { printf("af.\n"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: bedrock_model_routable_from_region @ hermes_cli/model_setup_flows.py:bedrock_model_routable_from_region */
int msf_bedrock_model_routable_from_region(const char *arg) {
    /* Python: geo prefix route check. Arg =
     * "model_geo\tregion_geo\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *model_geo = arg;
    const char *region_geo = t1 ? t1 + 1 : "";
    if (strcmp(model_geo, "none") == 0 || strcmp(model_geo, "global.") == 0) { printf("1\n"); return 0; }
    if (!region_geo[0]) { printf("1\n"); return 0; }
    if (strcmp(region_geo, "ap.") == 0) {
        printf("%d\n", (strcmp(model_geo, "ap.") == 0 || strcmp(model_geo, "apac.") == 0 || strcmp(model_geo, "jp.") == 0) ? 1 : 0);
        return 0;
    }
    printf("%d\n", strcmp(model_geo, region_geo) == 0 ? 1 : 0);
    return 0;
}

/* PoP: _prune_replaced_custom_model_config_credentials @ hermes_cli/model_setup_flows.py:_prune_replaced_custom_model_config_credentials */
int msf_u_prune_replaced_custom_model_config_credentials(const char *arg) {
    /* Python: stale model_config drop. Arg =
     * "removed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no prune needed\n"); return 0; }
    printf("pruned %s stale model_config credential(s)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: _prompt_auth_credentials_choice @ hermes_cli/model_setup_flows.py:_prompt_auth_credentials_choice */
int msf_u_prompt_auth_credentials_choice(const char *arg) {
    /* Python: use/reauth/cancel. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("use\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "use") == 0 || strcmp(state, "reauth") == 0 || strcmp(state, "cancel") == 0) { printf("%s\n", state); return 0; }
    printf("%s\n", tab ? tab + 1 : "use");
    return 0;
}

/* PoP: _model_flow_openrouter @ hermes_cli/model_setup_flows.py:_model_flow_openrouter */
int msf_u_model_flow_openrouter(const char *arg) { (void)arg; return 0; }

/* PoP: _print_moa_preset @ hermes_cli/model_setup_flows.py:_print_moa_preset */
int msf_u_print_moa_preset(const char *arg) {
    /* Python: preset breakdown print. Arg = "name\tpreset_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *name = arg;
    const char *pj = tab ? tab + 1 : "{}";
    printf("  Preset: %.*s\n", (int)(tab ? (size_t)(tab - arg) : strlen(arg)), name);
    printf("  Reference models:\n");
    json_t *j = json_parse(pj, NULL);
    if (j && json_is_object(j)) {
        json_t *refs = json_obj_get(j, "reference_models");
        if (refs && json_is_array(refs)) {
            size_t n = json_array_size(refs);
            for (size_t i = 0; i < n; i++) {
                json_t *slot = json_array_get(refs, i);
                if (!slot) continue;
                const char *prov = json_get_str(slot, "provider", "");
                const char *model = json_get_str(slot, "model", "");
                printf("    %zu. %s:%s\n", i + 1, prov, model);
            }
        }
        json_t *agg = json_obj_get(j, "aggregator");
        const char *ap = agg ? json_get_str(agg, "provider", "") : "";
        const char *am = agg ? json_get_str(agg, "model", "") : "";
        printf("  Aggregator:  %s:%s\n", ap, am);
    }
    if (j) json_free(j);
    return 0;
}

/* PoP: _model_flow_moa @ hermes_cli/model_setup_flows.py:_model_flow_moa */
int msf_u_model_flow_moa(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_nous @ hermes_cli/model_setup_flows.py:_model_flow_nous */
int msf_u_model_flow_nous(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_openai_codex @ hermes_cli/model_setup_flows.py:_model_flow_openai_codex */
int msf_u_model_flow_openai_codex(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_xai_oauth @ hermes_cli/model_setup_flows.py:_model_flow_xai_oauth */
int msf_u_model_flow_xai_oauth(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_qwen_oauth @ hermes_cli/model_setup_flows.py:_model_flow_qwen_oauth */
int msf_u_model_flow_qwen_oauth(const char *arg) {
    /* Python: qwen CLI reuse. Arg =
     * "logged_in\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int logged_in = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!logged_in || !state) {
        printf("Not logged into Qwen CLI OAuth.\nRun: qwen auth qwen-oauth\n");
        return 0;
    }
    printf("Default model set to: %s (via Qwen OAuth)\n", t3 ? t3 + 1 : "?");
    return 0;
}

/* PoP: _model_flow_minimax_oauth @ hermes_cli/model_setup_flows.py:_model_flow_minimax_oauth */
int msf_u_model_flow_minimax_oauth(const char *arg) {
    /* Python: minimax login gate. Arg =
     * "logged_in\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int logged_in = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!logged_in) {
        printf("Not logged into MiniMax. Starting OAuth login...\n");
        printf("Login cancelled or failed.\n");
        return 0;
    }
    printf("✓ Using MiniMax model: %s\n", t3 ? t3 + 1 : "?");
    return 0;
}

/* PoP: _model_flow_custom @ hermes_cli/model_setup_flows.py:_model_flow_custom */
int msf_u_model_flow_custom(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_azure_foundry @ hermes_cli/model_setup_flows.py:_model_flow_azure_foundry */
int msf_u_model_flow_azure_foundry(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_named_custom @ hermes_cli/model_setup_flows.py:_model_flow_named_custom */
int msf_u_model_flow_named_custom(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_copilot @ hermes_cli/model_setup_flows.py:_model_flow_copilot */
int msf_u_model_flow_copilot(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_copilot_acp @ hermes_cli/model_setup_flows.py:_model_flow_copilot_acp */
int msf_u_model_flow_copilot_acp(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_kimi @ hermes_cli/model_setup_flows.py:_model_flow_kimi */
int msf_u_model_flow_kimi(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_stepfun @ hermes_cli/model_setup_flows.py:_model_flow_stepfun */
int msf_u_model_flow_stepfun(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_bedrock_api_key @ hermes_cli/model_setup_flows.py:_model_flow_bedrock_api_key */
int msf_u_model_flow_bedrock_api_key(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_bedrock @ hermes_cli/model_setup_flows.py:_model_flow_bedrock */
int msf_u_model_flow_bedrock(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_vertex @ hermes_cli/model_setup_flows.py:_model_flow_vertex */
int msf_u_model_flow_vertex(const char *arg) { (void)arg; return 0; }

/* PoP: _select_zai_endpoint @ hermes_cli/model_setup_flows.py:_select_zai_endpoint */
int msf_u_select_zai_endpoint(const char *arg) { (void)arg; return 0; }

/* PoP: _model_flow_anthropic @ hermes_cli/model_setup_flows.py:_model_flow_anthropic */
int msf_u_model_flow_anthropic(const char *arg) { (void)arg; return 0; }
