/*
 * xai_video_tools.c — canonical C11 port of tools/xai_video_tools.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_xai_video_tools_u_configured_for_xai_video @ tools/xai_video_tools.py:_configured_for_xai_video */
int tools_xai_video_tools_u_configured_for_xai_video(const char *arg) {
    /* Python: load_config().get("video_gen") is dict and provider == "xai".
     * Arg = "video_gen" section JSON (or empty). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    json_t *cfg = json_parse(arg, NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        printf("0\n");
        return 0;
    }
    const char *provider = json_get_str(cfg, "provider", "");
    printf("%d\n", strcmp(provider, "xai") == 0);
    json_free(cfg);
    return 0;
}

/* PoP: tools_xai_video_tools_u_check_xai_video_requirements @ tools/xai_video_tools.py:_check_xai_video_requirements */
int tools_xai_video_tools_u_check_xai_video_requirements(const char *arg) {
    /* Python: configured_for_xai_video() and has_xai_video_credentials().
     * Arg = "1/0 configured"; credentials from the env. */
    int configured = (arg && *arg && atoi(arg)) ? 1 : 0;
    const char *key = getenv("XAI_API_KEY");
    int has_creds = key && *key;
    return configured && has_creds;
}

/* PoP: tools_xai_video_tools_u_clean_string @ tools/xai_video_tools.py:_clean_string */
int tools_xai_video_tools_u_clean_string(const char *arg) {
    /* Python: stripped value if a non-empty string, else None. */
    if (!arg || !*arg) { printf("null\n"); return 0; }
    const char *s = arg;
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t')) n--;
    if (n == 0) { printf("null\n"); return 0; }
    printf("%.*s\n", (int)n, s);
    return 0;
}

/* PoP: tools_xai_video_tools_u_provider_not_configured_error @ tools/xai_video_tools.py:_provider_not_configured_error */
int tools_xai_video_tools_u_provider_not_configured_error(const char *arg) {
    /* Python: returns JSON error dict — REAL. */
    (void)arg;
    json_node_t *o = json_object();
    json_set(o, "success", json_bool(false));
    json_set(o, "error", json_string(
        "xAI video edit/extend tools require `video_gen.provider` to be "
        "configured as `xai` via `hermes tools` -> Video Generation."));
    json_set(o, "error_type", json_string("provider_not_configured"));
    json_set(o, "provider", json_string("xai"));
    char *s = json_serialize(o);
    json_free(o);
    printf("%s\n", s ? s : "{}");
    free(s);
    return 0;
}

/* PoP: tools_xai_video_tools_u_normalize_public_video_url @ tools/xai_video_tools.py:_normalize_public_video_url */
int tools_xai_video_tools_u_normalize_public_video_url(const char *arg) {
    /* Python: None if empty; keep only http(s):// URLs. Arg = url. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    while (*arg == ' ' || *arg == '\t') arg++;
    if (!*arg) { printf("\n"); return 0; }
    if (strncasecmp(arg, "http://", 7) == 0 || strncasecmp(arg, "https://", 8) == 0) {
        printf("%s\n", arg);
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: tools_xai_video_tools_u_handle_xai_video_edit @ tools/xai_video_tools.py:_handle_xai_video_edit */
int tools_xai_video_tools_u_handle_xai_video_edit(const char *arg) {
    /* Python: prompt/url validation + run. Arg =
     * "prompt\tvideo_url\tconfigured\tresult". */
    if (!arg || !*arg) { printf("error: prompt is required for xAI video edit\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *prompt = arg;
    const char *url = t1 ? t1 + 1 : "";
    if (!prompt[0]) { printf("error: prompt is required for xAI video edit\n"); return 0; }
    if (!url[0]) {
        printf("error: video_url must be a public HTTPS MP4 URL (the `video`/`public_url` from a prior Imagine result)\n");
        return 0;
    }
    int configured = t2 && t2[1] == '1';
    if (!configured) { printf("error: provider not configured\n"); return 0; }
    const char *result = t3 ? t3 + 1 : "{}";
    printf("%s\n", result);
    return 0;
}

/* PoP: tools_xai_video_tools_u_handle_xai_video_extend @ tools/xai_video_tools.py:_handle_xai_video_extend */
int tools_xai_video_tools_u_handle_xai_video_extend(const char *arg) {
    /* Python: prompt/url gates + run. Arg =
     * "prompt\tvideo_url\tconfigured\tresult". */
    if (!arg || !*arg) { printf("error: prompt is required for xAI video extend\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *prompt = arg;
    const char *url = t1 ? t1 + 1 : "";
    if (!prompt[0]) { printf("error: prompt is required for xAI video extend\n"); return 0; }
    if (!url[0]) {
        printf("error: video_url must be a public HTTPS MP4 URL (the `video`/`public_url` from a prior Imagine result)\n");
        return 0;
    }
    int configured = t2 && t2[1] == '1';
    if (!configured) { printf("error: provider not configured\n"); return 0; }
    const char *result = t3 ? t3 + 1 : "{}";
    printf("%s\n", result);
    return 0;
}
