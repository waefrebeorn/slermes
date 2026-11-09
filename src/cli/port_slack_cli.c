/*
 * port_slack_cli.c
 *
 * Pure helper ported from hermes_cli/slack_cli.py.
 *
 *   sl_build_full_manifest <- slack_cli.py:_build_full_manifest
 *
 * The manifest's slash-command list normally comes from slack_native_slashes(),
 * which walks the command registry and the plugin entries - registry
 * introspection, not portable. It is supplied here as a flat
 * [name, description, usage_hint, ...] array instead, which is exactly the
 * shape slack_native_slashes() produces.
 */

#include "port_slack_cli.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int cmp_str(const void *a, const void *b);

/* ===========================================================================
 *  _build_full_manifest  (slack_cli.py:_build_full_manifest)
 *
 *      if messaging_experience is None:
 *          messaging_experience = "assistant" if include_assistant else "none"
 *      messaging_experience = str(messaging_experience).strip().lower()
 *      if messaging_experience not in {"assistant","agent","none"}:
 *          raise ValueError(...)
 *
 *      features = {app_home..., bot_user{display_name: bot_name[:80], ...},
 *                  slash_commands: <the supplied list>}
 *      bot_scopes = [...16 scopes...]
 *      bot_events = [...6 events...]
 *      if messaging_experience == "assistant": +assistant_view, +assistant:write,
 *                                              +2 events
 *      elif messaging_experience == "agent":   +agent_view,   +assistant:write,
 *                                              +2 other events
 *      bot_scopes.sort(); bot_events.sort()
 *      display_information = {name: bot_name[:35],
 *                             description: (bot_description or "...")[:140],
 *                             background_color: "#1a1a2e"}
 *      if long_description is not None: +long_description
 *
 *  `slashes` is a flat NULL-terminated [name, desc, usage, ...] array.
 *  Returns malloc'd JSON; caller frees. `err` receives 1 when the
 *  messaging_experience value is invalid (the Python raises ValueError).
 * =========================================================================== */

/* PoP: _build_full_manifest @ hermes_cli/slack_cli.py:_build_full_manifest */
char *sl_build_full_manifest(const char *bot_name, const char *bot_description,
                             int include_assistant, const char *messaging_experience,
                             const char *long_description,
                             const char *const *slashes, int *err)
{
    if (err) *err = 0;
    const char *name = bot_name ? bot_name : "";
    const char *desc  = bot_description ? bot_description : "";

    /* messaging_experience: None -> derived; then str().strip().lower() */
    char mode[32];
    if (!messaging_experience || !messaging_experience[0]) {
        strcpy(mode, include_assistant ? "assistant" : "none");
    } else {
        size_t n = strlen(messaging_experience);
        if (n >= sizeof(mode)) n = sizeof(mode) - 1;
        for (size_t i = 0; i < n; i++) mode[i] = (char)tolower((unsigned char)messaging_experience[i]);
        mode[n] = '\0';
        /* strip leading/trailing whitespace (Python's str.strip()) */
        char *s = mode, *e = mode + strlen(mode);
        while (*s && isspace((unsigned char)*s)) s++;
        while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
        memmove(mode, s, (size_t)(e - s) + 1);
    }
    if (strcmp(mode, "assistant") != 0 && strcmp(mode, "agent") != 0 && strcmp(mode, "none") != 0) {
        if (err) *err = 1;
        return NULL;
    }

    /* bot_scopes / bot_events, then the mode-specific additions */
    const char *scopes[] = {
        "app_mentions:read", "channels:history", "channels:read", "chat:write", "commands",
        "files:read", "files:write", "groups:history", "groups:read", "im:history",
        "im:read", "im:write", "mpim:history", "mpim:read", "reactions:read", "users:read"};
    const char *events[] = {
        "app_mention", "message.channels", "message.groups", "message.im",
        "message.mpim", "reaction_added", "reaction_removed"};

    const char *extra_scopes[2]  = {NULL, NULL};
    const char *extra_events[3]  = {NULL, NULL, NULL};
    int n_extra_scopes = 0, n_extra_events = 0;
    const char *view_key = NULL, *view_desc = NULL, *view_field = "description";

    if (strcmp(mode, "assistant") == 0) {
        view_key = "assistant_view";
        view_desc = "Chat with Hermes in threads and DMs.";
        view_field = "assistant_description";
        extra_scopes[n_extra_scopes++] = "assistant:write";
        extra_events[n_extra_events++] = "assistant_thread_context_changed";
        extra_events[n_extra_events++] = "assistant_thread_started";
    } else if (strcmp(mode, "agent") == 0) {
        view_key = "agent_view";
        view_desc = "Chat with Hermes in Slack Messages.";
        view_field = "agent_description";
        extra_scopes[n_extra_scopes++] = "assistant:write";
        extra_events[n_extra_events++] = "app_context_changed";
        extra_events[n_extra_events++] = "app_home_opened";
    }

    /* sort the scope/event lists (Python's list.sort() - ASCII) */
    const char **sc = (const char **)malloc(sizeof(char *) * (16 + 2));
    const char **ev = (const char **)malloc(sizeof(char *) * (6 + 2));
    if (!sc || !ev) { free(sc); free(ev); return NULL; }
    int nsc = 0, nev = 0;
    for (int i = 0; i < 16; i++) sc[nsc++] = scopes[i];
    for (int i = 0; i < n_extra_scopes; i++) sc[nsc++] = extra_scopes[i];
    for (int i = 0; i < 7; i++) ev[nev++] = events[i];
    for (int i = 0; i < n_extra_events; i++) ev[nev++] = extra_events[i];
    qsort(sc, (size_t)nsc, sizeof(char *), cmp_str);
    qsort(ev, (size_t)nev, sizeof(char *), cmp_str);

    /* slash_commands JSON */
    size_t cap = 4096;
    for (int i = 0; slashes && slashes[i]; i++) cap += strlen(slashes[i]) * 2 + 64;
    cap += strlen(name) * 2 + strlen(desc) * 2 + (long_description ? strlen(long_description) * 2 : 0);
    char *out = (char *)malloc(cap);
    if (!out) { free(sc); free(ev); return NULL; }
    char *p = out;

    p += sprintf(p, "{\"_metadata\":{\"major_version\":1,\"minor_version\":1},");
    p += sprintf(p, "\"display_information\":{\"name\":\"%.*s\",\"description\":\"%.*s\","
                    "\"background_color\":\"#1a1a2e\"",
                 35, name, 140, (desc[0] ? desc : "Your Hermes agent on Slack"));
    if (long_description) {
        p += sprintf(p, ",\"long_description\":\"");
        for (const char *q = long_description; *q; q++) {
            if (*q == '"' || *q == '\\') *p++ = '\\';
            *p++ = *q;
        }
        *p++ = '"';
    }
    p += sprintf(p, "},");
    p += sprintf(p, "\"features\":{\"app_home\":{\"home_tab_enabled\":false,"
                    "\"messages_tab_enabled\":true,\"messages_tab_read_only_enabled\":false},");
    p += sprintf(p, "\"bot_user\":{\"display_name\":\"%.*s\",\"always_online\":true},",
                 80, name);
    if (view_key) p += sprintf(p, "\"%s\":{\"%s\":\"%s\"},", view_key, view_field, view_desc);
    p += sprintf(p, "\"slash_commands\":[");
    for (int i = 0; slashes && slashes[i]; i += 3) {
        const char *sn = slashes[i], *sd = slashes[i + 1], *su = slashes[i + 2];
        if (i) *p++ = ',';
        p += sprintf(p, "{\"command\":\"/%s\",\"description\":\"%s\",\"should_escape\":false,"
                        "\"url\":\"https://hermes-agent.local/slack/commands\"",
                     sn, sd ? sd : "");
        if (su && su[0]) p += sprintf(p, ",\"usage_hint\":\"%s\"", su);
        *p++ = '}';
    }
    p += sprintf(p, "]},");
    p += sprintf(p, "\"oauth_config\":{\"scopes\":{\"bot\":[");
    for (int i = 0; i < nsc; i++) p += sprintf(p, "%s\"%s\"", i ? "," : "", sc[i]);
    p += sprintf(p, "]}},");
    p += sprintf(p, "\"settings\":{\"event_subscriptions\":{\"bot_events\":[");
    for (int i = 0; i < nev; i++) p += sprintf(p, "%s\"%s\"", i ? "," : "", ev[i]);
    p += sprintf(p, "]},\"interactivity\":{\"is_enabled\":true},\"org_deploy_enabled\":false,"
                    "\"socket_mode_enabled\":true,\"token_rotation_enabled\":false}}");

    free(sc); free(ev);
    return out;
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
