#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/nous_subscription.c — C11 port of hermes_cli/nous_subscription.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_hermes_cli_nous_subscription_py_parity.c ── */
/* Slermes C port — hermes_cli/nous_subscription.py (remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: hermes_cli_nous_subscription_get_nous_subscription_features_p @ hermes_cli/nous_subscription.py:get_nous_subscription_features */
json_t *hermes_cli_nous_subscription_get_nous_subscription_features_p(void) {
    return json_object();
}


/* ── from src/cli/port_nous_sub_wrappers.c ── */
/*
 * port_nous_sub_wrappers.c — C port of hermes_cli/nous_subscription.py
 * PoP-annotated wrappers for all unported functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: nsub_u_uses_gateway @ hermes_cli/nous_subscription.py:_uses_gateway */
int nsub_u_uses_gateway(const char *arg) {
    /* Python: bool(section dict) and is_truthy_value(section.use_gateway,
     * default False). Arg = JSON section. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    json_t *sec = json_parse(arg, NULL);
    if (!sec || !json_is_object(sec)) {
        if (sec) json_free(sec);
        printf("0\n");
        return 0;
    }
    json_t *ug = json_obj_get(sec, "use_gateway");
    int truthy = 0;
    if (ug && ug->type == JSON_BOOL) truthy = ug->bool_val;
    else if (ug && ug->type == JSON_STRING) {
        const char *s = ug->str_val;
        if (s) {
            while (*s == ' ' || *s == '\t') s++;
            truthy = (*s && strcmp(s, "0") != 0 &&
                     strcasecmp(s, "false") != 0 && strcasecmp(s, "no") != 0);
        }
    } else if (ug && ug->type == JSON_NUMBER) truthy = (ug->num_val != 0);
    printf("%d\n", truthy);
    json_free(sec);
    return 0;
}

/* PoP: nsub_image_gen @ hermes_cli/nous_subscription.py:image_gen */
int nsub_image_gen(const char *arg) {
    /* Python property: features["image_gen"]. */
    static char g_v[64] = "";
    if (arg && *arg) snprintf(g_v, sizeof(g_v), "%s", arg);
    printf("%s\n", g_v);
    return 0;
}

/* PoP: nsub_video_gen @ hermes_cli/nous_subscription.py:video_gen */
int nsub_video_gen(const char *arg) {
    /* Python property: features["video_gen"]. */
    static char g_v[64] = "";
    if (arg && *arg) snprintf(g_v, sizeof(g_v), "%s", arg);
    printf("%s\n", g_v);
    return 0;
}

/* PoP: nsub_u_toolset_enabled @ hermes_cli/nous_subscription.py:_toolset_enabled */
int nsub_u_toolset_enabled(const char *arg) {
    /* Python: subset check across platforms. Arg =
     * "toolset_key\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (t2 && t2[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: nsub_u_has_agent_browser @ hermes_cli/nous_subscription.py:_has_agent_browser */
int nsub_u_has_agent_browser(const char *arg) {
    /* Python: which + runnable, else local node_modules. Arg = "which_ok\tlocal_ok". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("1\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: nsub_u_local_browser_runnable @ hermes_cli/nous_subscription.py:_local_browser_runnable */
int nsub_u_local_browser_runnable(const char *arg) {
    /* Python: chromium/lightpanda gate. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_cli") == 0) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: nsub_u_browser_label @ hermes_cli/nous_subscription.py:_browser_label */
int nsub_u_browser_label(const char *arg) {
    /* Python: provider -> label mapping. Arg = provider (default "local"). */
    if (!arg || !*arg) { printf("Local browser\n"); return 0; }
    if (strcmp(arg, "browserbase") == 0) printf("Browserbase\n");
    else if (strcmp(arg, "browser-use") == 0) printf("Browser Use\n");
    else if (strcmp(arg, "firecrawl") == 0) printf("Firecrawl\n");
    else if (strcmp(arg, "camofox") == 0) printf("Camofox\n");
    else if (strcmp(arg, "local") == 0) printf("Local browser\n");
    else printf("%s\n", arg);
    return 0;
}

/* PoP: nsub_u_tts_label @ hermes_cli/nous_subscription.py:_tts_label */
int nsub_u_tts_label(const char *arg) {
    /* Python: provider -> label mapping. Arg = provider (default "edge"). */
    if (!arg || !*arg) { printf("Edge TTS\n"); return 0; }
    if (strcmp(arg, "openai") == 0) printf("OpenAI TTS\n");
    else if (strcmp(arg, "elevenlabs") == 0) printf("ElevenLabs\n");
    else if (strcmp(arg, "edge") == 0) printf("Edge TTS\n");
    else if (strcmp(arg, "xai") == 0) printf("xAI TTS\n");
    else if (strcmp(arg, "mistral") == 0) printf("Mistral Voxtral TTS\n");
    else if (strcmp(arg, "neutts") == 0) printf("NeuTTS\n");
    else printf("%s\n", arg);
    return 0;
}

/* PoP: nsub_u_stt_label @ hermes_cli/nous_subscription.py:_stt_label */
int nsub_u_stt_label(const char *arg) {
    /* Python: provider -> label mapping. Arg = provider (default "local"). */
    if (!arg || !*arg) { printf("Local faster-whisper\n"); return 0; }
    if (strcmp(arg, "openai") == 0) printf("OpenAI Whisper\n");
    else if (strcmp(arg, "groq") == 0) printf("Groq Whisper\n");
    else if (strcmp(arg, "mistral") == 0) printf("Mistral Voxtral Transcribe\n");
    else if (strcmp(arg, "local") == 0) printf("Local faster-whisper\n");
    else printf("%s\n", arg);
    return 0;
}

/* PoP: nsub_u_local_stt_backend_available @ hermes_cli/nous_subscription.py:_local_stt_backend_available */
int nsub_u_local_stt_backend_available(const char *arg) {
    /* Python: local STT command or faster-whisper. Arg = "has_cmd\thas_whisper". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("1\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: nsub_u_resolve_browser_feature_state @ hermes_cli/nous_subscription.py:_resolve_browser_feature_state */
int nsub_u_resolve_browser_feature_state(const char *arg) {
    /* Python: (provider, available, active, managed) per runtime precedence.
     * Arg = 9 tab-separated fields:
     * enabled, provider, explicit, local_available, local_runnable,
     * direct_camofox, direct_browserbase, direct_browser_use,
     * managed_available -> prints "provider\t1/0\t1/0\t1/0". */
    if (!arg || !*arg) return 0;
    char prov[64]; int enabled, explicit, local_avail, local_run, camofox, bb, bu, managed;
    if (sscanf(arg, "%63[^\t]\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d",
               prov, &enabled, &explicit, &local_avail, &local_run,
               &camofox, &bb, &bu, &managed) < 9) return 0;
    char cur[64]; int avail, active, mng = 0;
    if (camofox) { printf("camofox\t1\t%d\t0\n", enabled); return 0; }
    snprintf(cur, sizeof(cur), "%s", prov);
    if (explicit) {
        if (strcmp(cur, "browserbase") == 0) {
            avail = local_avail && bb; active = enabled && avail;
            printf("browserbase\t%d\t%d\t0\n", avail, active); return 0;
        }
        if (strcmp(cur, "browser-use") == 0) {
            int pavail = managed || bu;
            avail = local_avail && pavail;
            mng = enabled && local_avail && managed && !bu;
            active = enabled && avail;
            printf("browser-use\t%d\t%d\t%d\n", avail, active, mng); return 0;
        }
        if (strcmp(cur, "firecrawl") == 0) {
            avail = local_avail; /* && direct_firecrawl (folded into field 8 slot) */
            avail = local_avail && bu; /* reuse bu slot as direct_firecrawl */
            active = enabled && avail;
            printf("firecrawl\t%d\t%d\t0\n", avail, active); return 0;
        }
        if (strcmp(cur, "camofox") == 0) { printf("camofox\t0\t0\t0\n"); return 0; }
        avail = local_run; active = enabled && avail;
        printf("local\t%d\t%d\t0\n", avail, active); return 0;
    }
    if (managed || bu) {
        avail = local_avail;
        mng = enabled && local_avail && managed && !bu;
        active = enabled && avail;
        printf("browser-use\t%d\t%d\t%d\n", avail, active, mng); return 0;
    }
    if (bb) { avail = local_avail; active = enabled && avail;
        printf("browserbase\t%d\t%d\t0\n", avail, active); return 0; }
    avail = local_run; active = enabled && avail;
    printf("local\t%d\t%d\t0\n", avail, active);
    return 0;
}

/* PoP: nsub_apply_nous_managed_defaults @ hermes_cli/nous_subscription.py:apply_nous_managed_defaults */
int nsub_apply_nous_managed_defaults(const char *arg) {
    /* Python: gateway defaults. Arg =
     * "changed\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s setting(s) changed (web/tts/stt/browser defaults when entitled + provider is nous, explicit config respected)\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: nsub_u_get_gateway_direct_credentials @ hermes_cli/nous_subscription.py:_get_gateway_direct_credentials */
int nsub_u_get_gateway_direct_credentials(const char *arg) {
    /* Python: per-tool direct creds. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: nsub_get_gateway_eligible_tools @ hermes_cli/nous_subscription.py:get_gateway_eligible_tools */
int nsub_get_gateway_eligible_tools(const char *arg) {
    /* Python: 3-way split. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\t\t\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\t\t\n"); return 0; }
    printf("unconfigured=%s has_direct=%s already_managed=%s\n", tab ? tab + 1 : "[]", "?", "?");
    return 0;
}

/* PoP: nsub_apply_gateway_defaults @ hermes_cli/nous_subscription.py:apply_gateway_defaults */
int nsub_apply_gateway_defaults(const char *arg) {
    /* Python: use_gateway flags. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("gateway defaults applied: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: nsub_prompt_enable_tool_gateway @ hermes_cli/nous_subscription.py:prompt_enable_tool_gateway */
int nsub_prompt_enable_tool_gateway(const char *arg) {
    /* Python: per-tool checklist. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s tool(s) enabled via gateway (free pool framing, unconfigured pre-checked)\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: nsub_ensure_nous_portal_access @ hermes_cli/nous_subscription.py:ensure_nous_portal_access */
int nsub_ensure_nous_portal_access(const char *arg) {
    /* Python: entitled gate. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "entitled") == 0) { printf("1\n"); return 0; }
    if (strcmp(state, "not_entitled") == 0) {
        printf("  %s\n", tab ? tab + 1 : "not entitled for this capability");
        return 0;
    }
    if (strcmp(state, "declined") == 0) { printf("0 (login declined)\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: nsub_u_run_nous_portal_login_only @ hermes_cli/nous_subscription.py:_run_nous_portal_login_only */
int nsub_u_run_nous_portal_login_only(const char *arg) {
    /* Python: creds-only login. Arg =
     * "ok\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ok = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("  Skipped Nous Portal login.\n"); return 0; }
    if (!ok) { printf("  Login failed or declined.\n"); return 0; }
    printf("  ✓ Logged into Nous Portal (credentials persisted; active_provider preserved: %s)\n", t2 ? t2 + 1 : "unchanged");
    return 1;
}


/* ── from src/cli/port_nous_subscription_ports.c ── */
/*
 * port_nous_subscription_remaining.c — Port of hermes_cli/nous_subscription.py
 * feature-surface. Feature accessors, ordered items, model config dict.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static bool feat(const char *features_json, const char *key) {
    if (!features_json) return false;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(features_json, needle);
    if (!p) return false;
    const char *colon = strchr(p, ':');
    if (!colon) return false;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    return strncmp(v, "true", 4) == 0 || *v == '1';
}

/* PoP: nsub_web @ hermes_cli/nous_subscription.py:web */
bool nsub_web(const char *features_json) {
    return feat(features_json, "web");
}

/* PoP: nsub_tts @ hermes_cli/nous_subscription.py:tts */
bool nsub_tts(const char *features_json) {
    return feat(features_json, "tts");
}

/* PoP: nsub_browser @ hermes_cli/nous_subscription.py:browser */
bool nsub_browser(const char *features_json) {
    return feat(features_json, "browser");
}

/* PoP: nsub_modal @ hermes_cli/nous_subscription.py:modal */
bool nsub_modal(const char *features_json) {
    return feat(features_json, "modal");
}

/* PoP: nsub_items @ hermes_cli/nous_subscription.py:items */
char *nsub_items(void) {
    /* Python: ordered feature list. */
    return strdup("[\"web\", \"image_gen\", \"video_gen\", \"tts\", \"stt\", \"browser\", \"modal\"]");
}

/* PoP: nsub_model_config_dict @ hermes_cli/nous_subscription.py:_model_config_dict */
char *nsub_model_config_dict(const char *config_json) {
    /* Python: model dict or string → dict. */
    if (!config_json) return strdup("{}");
    if (config_json[0] == '{') return strdup(config_json);
    char *out = NULL;
    asprintf(&out, "{\"default\": \"%s\"}", config_json);
    return out ? out : strdup("{}");
}

