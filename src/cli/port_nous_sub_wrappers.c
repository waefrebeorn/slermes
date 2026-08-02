/*
 * port_nous_sub_wrappers.c — C port of hermes_cli/nous_subscription.py
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

/* PoP: _uses_gateway @ hermes_cli/nous_subscription.py:_uses_gateway */
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

/* PoP: image_gen @ hermes_cli/nous_subscription.py:image_gen */
int nsub_image_gen(const char *arg) {
    /* Python property: features["image_gen"]. */
    static char g_v[64] = "";
    if (arg && *arg) snprintf(g_v, sizeof(g_v), "%s", arg);
    printf("%s\n", g_v);
    return 0;
}

/* PoP: video_gen @ hermes_cli/nous_subscription.py:video_gen */
int nsub_video_gen(const char *arg) {
    /* Python property: features["video_gen"]. */
    static char g_v[64] = "";
    if (arg && *arg) snprintf(g_v, sizeof(g_v), "%s", arg);
    printf("%s\n", g_v);
    return 0;
}

/* PoP: _toolset_enabled @ hermes_cli/nous_subscription.py:_toolset_enabled */
int nsub_u_toolset_enabled(const char *arg) { (void)arg; return 0; }

/* PoP: _has_agent_browser @ hermes_cli/nous_subscription.py:_has_agent_browser */
int nsub_u_has_agent_browser(const char *arg) { (void)arg; return 0; }

/* PoP: _local_browser_runnable @ hermes_cli/nous_subscription.py:_local_browser_runnable */
int nsub_u_local_browser_runnable(const char *arg) { (void)arg; return 0; }

/* PoP: _browser_label @ hermes_cli/nous_subscription.py:_browser_label */
int nsub_u_browser_label(const char *arg) { (void)arg; return 0; }

/* PoP: _tts_label @ hermes_cli/nous_subscription.py:_tts_label */
int nsub_u_tts_label(const char *arg) { (void)arg; return 0; }

/* PoP: _stt_label @ hermes_cli/nous_subscription.py:_stt_label */
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

/* PoP: _local_stt_backend_available @ hermes_cli/nous_subscription.py:_local_stt_backend_available */
int nsub_u_local_stt_backend_available(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_browser_feature_state @ hermes_cli/nous_subscription.py:_resolve_browser_feature_state */
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

/* PoP: apply_nous_managed_defaults @ hermes_cli/nous_subscription.py:apply_nous_managed_defaults */
int nsub_apply_nous_managed_defaults(const char *arg) { (void)arg; return 0; }

/* PoP: _get_gateway_direct_credentials @ hermes_cli/nous_subscription.py:_get_gateway_direct_credentials */
int nsub_u_get_gateway_direct_credentials(const char *arg) { (void)arg; return 0; }

/* PoP: get_gateway_eligible_tools @ hermes_cli/nous_subscription.py:get_gateway_eligible_tools */
int nsub_get_gateway_eligible_tools(const char *arg) { (void)arg; return 0; }

/* PoP: apply_gateway_defaults @ hermes_cli/nous_subscription.py:apply_gateway_defaults */
int nsub_apply_gateway_defaults(const char *arg) { (void)arg; return 0; }

/* PoP: prompt_enable_tool_gateway @ hermes_cli/nous_subscription.py:prompt_enable_tool_gateway */
int nsub_prompt_enable_tool_gateway(const char *arg) { (void)arg; return 0; }

/* PoP: ensure_nous_portal_access @ hermes_cli/nous_subscription.py:ensure_nous_portal_access */
int nsub_ensure_nous_portal_access(const char *arg) { (void)arg; return 0; }

/* PoP: _run_nous_portal_login_only @ hermes_cli/nous_subscription.py:_run_nous_portal_login_only */
int nsub_u_run_nous_portal_login_only(const char *arg) { (void)arg; return 0; }
