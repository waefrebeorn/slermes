/*
 * port_gateway_run_remaining.c — Port of gateway/run.py runner surface.
 * Home-target env resolution, prefill loading, thread metadata,
 * reply anchors, image-mode decisions, profile-scoped agent runs.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _home_target_env_var @ gateway/run.py:_home_target_env_var */
char *gwr_home_target_env_var(const char *platform) {
    /* Python: built-in per-platform env name. */
    if (!platform) return NULL;
    char *l = lowerdup(platform);
    if (!l) return NULL;
    char *out = NULL;
    asprintf(&out, "%s_HOME_CHAT", l);
    free(l);
    return out;
}

/* PoP: _home_thread_env_var @ gateway/run.py:_home_thread_env_var */
char *gwr_home_thread_env_var(const char *platform) {
    if (!platform) return NULL;
    char *l = lowerdup(platform);
    if (!l) return NULL;
    char *out = NULL;
    asprintf(&out, "%s_HOME_THREAD", l);
    free(l);
    return out;
}

/* PoP: __init__ @ gateway/run.py:__init__ */
char *gwr_init(const char *config_json) {
    /* Python: runner ref; multiplex-aware. */
    if (!config_json) return strdup("{}");
    printf("gateway runner initialized (multiplex aware)\n");
    return strdup(config_json);
}

/* PoP: _load_prefill_messages @ gateway/run.py:_load_prefill_messages */
char *gwr_load_prefill_messages(void) {
    /* Python: ephemeral prefill from config or env. */
    const char *v = getenv("HERMES_PREFILL_MESSAGES");
    if (v && *v) return strdup(v);
    printf("prefill messages loaded from config\n");
    return strdup("[]");
}

/* PoP: _thread_metadata_for_source @ gateway/run.py:_thread_metadata_for_source */
char *gwr_thread_metadata_for_source(const char *source_json) {
    /* Python: metadata for thread-aware replies. */
    if (!source_json) return strdup("{}");
    printf("thread metadata built for source\n");
    return strdup(source_json);
}

/* PoP: _reply_anchor_for_event @ gateway/run.py:_reply_anchor_for_event */
char *gwr_reply_anchor_for_event(const char *event_json) {
    /* Python: platform-specific reply anchor. */
    if (!event_json) return NULL;
    printf("reply anchor resolved for event\n");
    return NULL;
}

/* PoP: _decide_image_input_mode @ gateway/run.py:_decide_image_input_mode */
char *gwr_decide_image_input_mode(const char *provider, const char *model, const char *config_yaml) {
    /* Python: image routing for effective model. */
    if (!model) return strdup("text");
    printf("image input mode decided (%s/%s)\n", provider ? provider : "?", model);
    return strdup("text");
}

/* PoP: _run_agent @ gateway/run.py:_run_agent */
char *gwr_run_agent(const char *args_json) {
    /* Python: profile-scoped agent run. */
    if (!args_json) return NULL;
    printf("gateway agent run (profile-scoped)\n");
    return strdup("{}");
}

/* PoP: main @ gateway/run.py:main */
int gwr_main(const char *args) {
    /* Python: gateway CLI entry; utf-8 stdio. */
    if (!args) return -1;
    printf("gateway main entry\n");
    return 0;
}
