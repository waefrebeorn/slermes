/*
 * platform_actions.c — Port of hermes_cli/platform_actions.py.
 * See include/platform_actions.h. The gate chain, error-code contract,
 * per-platform dispatch and audit rule are faithful; live gateway/consent
 * systems enter through injectable hooks.
 */
#define _POSIX_C_SOURCE 200809L

#include "platform_actions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── PoP: _err / _ok ───────────────────────────────────────────────────── */

json_t *platform_actions_err(const char *code, const char *detail)
{
    json_t *r = json_new_object();
    json_object_set(r, "ok", json_bool(0));
    json_object_set(r, "error", json_string(code ? code : ""));
    if (detail && *detail)
        json_object_set(r, "detail", json_string(detail));
    return r;
}

json_t *platform_actions_ok_verb(const char *verb)
{
    json_t *r = json_new_object();
    json_object_set(r, "ok", json_bool(1));
    json_object_set(r, "action", json_string(verb ? verb : ""));
    return r;
}

platform_actions_t *platform_actions_new(const char *plugin_id)
{
    platform_actions_t *pa = calloc(1, sizeof *pa);
    pa->plugin_id = plugin_id;
    return pa;
}

void platform_actions_free(platform_actions_t *pa) { free(pa); }

/* ── PoP: _capability_granted ──────────────────────────────────────────── */

int platform_actions_capability_granted(const platform_actions_t *pa)
{
    /* Ground rule: failure to read consent state = not granted. */
    if (!pa || !pa->capability_granted)
        return 0;
    return pa->capability_granted(pa->plugin_id,
                                  PLATFORM_ACTIONS_CAPABILITY_ID,
                                  pa->cap_ctx)
               ? 1 : 0;
}

/* ── PoP: _resolve_adapter — (adapter, error_dict), exactly one non-None ─ */

/* PoP: _resolve_adapter @ hermes_cli/platform_actions.py:_resolve_adapter */
const pa_adapter_t *platform_actions_resolve_adapter(
    const platform_actions_t *pa, const char *platform, json_t **out_err)
{
    *out_err = NULL;
    void *runner = NULL;
    if (pa->get_runner)
        runner = pa->get_runner(pa->runner_ctx);
    if (!runner) {
        *out_err = platform_actions_err(
            "gateway_unavailable", "no gateway runner is active in this process");
        return NULL;
    }

    /* Platform(str(platform).strip().lower()) — unknown enum → error */
    size_t n = strlen(platform);
    char *norm = malloc(n + 1);
    size_t b = 0, e = n;
    while (b < e && (platform[b] == ' ' || platform[b] == '\t')) b++;
    while (e > b && (platform[e - 1] == ' ' || platform[e - 1] == '\t')) e--;
    for (size_t i = b; i < e; i++) {
        char c = platform[i];
        norm[i - b] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    norm[e - b] = '\0';

    int known = 0;
    for (size_t i = 0; i < pa->n_known_platforms; i++)
        if (strcmp(pa->known_platforms[i], norm) == 0) { known = 1; break; }
    if (!known) {
        size_t need = snprintf(NULL, 0, "unknown platform '%s'", platform) + 1;
        char *detail = malloc(need);
        snprintf(detail, need, "unknown platform '%s'", platform);
        *out_err = platform_actions_err("unknown_platform", detail);
        free(detail);
        free(norm);
        return NULL;
    }

    const pa_adapter_t *adapter =
        pa->runner_adapter ? pa->runner_adapter(runner, norm) : NULL;
    if (!adapter) {
        size_t need = snprintf(NULL, 0, "no %s adapter is registered", norm) + 1;
        char *detail = malloc(need);
        snprintf(detail, need, "no %s adapter is registered", norm);
        *out_err = platform_actions_err("adapter_not_registered", detail);
        free(detail);
        free(norm);
        return NULL;
    }
    if (!adapter->is_connected) {
        size_t need = snprintf(NULL, 0,
            "the %s adapter is not connected", norm) + 1;
        char *detail = malloc(need);
        snprintf(detail, need, "the %s adapter is not connected", norm);
        *out_err = platform_actions_err("adapter_disconnected", detail);
        free(detail);
        free(norm);
        return NULL;
    }
    free(norm);
    return adapter;
}

/* ── PoP: _gate — capability + required strings + adapter resolution ───── */

static int nonempty_str(const char *s)
{
    if (!s) return 0;
    for (const char *p = s; *p; p++)
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') return 1;
    return 0;
}

const pa_adapter_t *platform_actions_gate(const platform_actions_t *pa,
                                          const char *platform,
                                          const char *const *required_names,
                                          const char *const *required_values,
                                          size_t n_required, json_t **out_err)
{
    if (!platform_actions_capability_granted(pa)) {
        size_t need = snprintf(NULL, 0,
            "plugin '%s' lacks the 'gateway.platform_actions' capability "
            "(grant via consent flow or plugins.entries.%s.allow_platform_actions)",
            pa->plugin_id, pa->plugin_id) + 1;
        char *detail = malloc(need);
        snprintf(detail, need,
            "plugin '%s' lacks the 'gateway.platform_actions' capability "
            "(grant via consent flow or plugins.entries.%s.allow_platform_actions)",
            pa->plugin_id, pa->plugin_id);
        *out_err = platform_actions_err("capability_not_granted", detail);
        free(detail);
        return NULL;
    }
    for (size_t i = 0; i < n_required; i++) {
        if (!nonempty_str(required_values[i])) {
            size_t need = snprintf(NULL, 0,
                "%s must be a non-empty string", required_names[i]) + 1;
            char *detail = malloc(need);
            snprintf(detail, need, "%s must be a non-empty string",
                     required_names[i]);
            *out_err = platform_actions_err("invalid_argument", detail);
            free(detail);
            return NULL;
        }
    }
    return platform_actions_resolve_adapter(pa, platform, out_err);
}

/* ── PoP: add_reaction / set_thread_title v1 verbs ─────────────────────── */

/* PoP: add_reaction @ hermes_cli/platform_actions.py:add_reaction */
json_t *platform_actions_add_reaction(const platform_actions_t *pa,
                                      const char *platform,
                                      const char *chat_id,
                                      const char *message_id,
                                      const char *emoji)
{
    const char *req_names[] = {"chat_id", "message_id", "emoji"};
    const char *req_vals[] = {chat_id, message_id, emoji};
    json_t *error = NULL;
    const pa_adapter_t *adapter = platform_actions_gate(
        pa, platform, req_names, req_vals, 3, &error);
    if (error || !adapter) {
        if (!error)
            error = platform_actions_err("gateway_unavailable", "");
        return error;
    }

    json_t *result;
    const char *pv = adapter->platform_value;
    if (strcmp(pv, "telegram") == 0) {
        int done = adapter->set_reaction
            ? adapter->set_reaction(adapter->impl, chat_id, message_id, emoji)
            : 0;
        result = done > 0
            ? platform_actions_ok_verb("add_reaction")
            : platform_actions_err("action_failed",
                                   "telegram set_message_reaction failed");
    } else if (strcmp(pv, "discord") == 0) {
        result = platform_actions_discord_add_reaction(adapter, chat_id,
                                                       message_id, emoji);
    } else {
        size_t need = snprintf(NULL, 0,
            "add_reaction is not implemented for %s", platform) + 1;
        char *detail = malloc(need);
        snprintf(detail, need, "add_reaction is not implemented for %s", platform);
        result = platform_actions_err("unsupported_platform_action", detail);
        free(detail);
    }
    return result;
}

/* PoP: set_thread_title @ hermes_cli/platform_actions.py:set_thread_title */
json_t *platform_actions_set_thread_title(const platform_actions_t *pa,
                                          const char *platform,
                                          const char *chat_id,
                                          const char *thread_id,
                                          const char *title)
{
    const char *req_names[] = {"chat_id", "thread_id", "title"};
    const char *req_vals[] = {chat_id, thread_id, title};
    json_t *error = NULL;
    const pa_adapter_t *adapter = platform_actions_gate(
        pa, platform, req_names, req_vals, 3, &error);
    if (error || !adapter) {
        if (!error)
            error = platform_actions_err("gateway_unavailable", "");
        return error;
    }

    json_t *result;
    const char *pv = adapter->platform_value;
    if (strcmp(pv, "telegram") == 0) {
        long tid = strtol(thread_id, NULL, 10);
        int rc = adapter->rename_dm_topic
            ? adapter->rename_dm_topic(adapter->impl, chat_id, tid, title)
            : -1;
        result = rc >= 0 ? platform_actions_ok_verb("set_thread_title")
                         : platform_actions_err("action_failed", "edit_forum_topic raised");
    } else if (strcmp(pv, "discord") == 0) {
        int done = adapter->rename_thread
            ? adapter->rename_thread(adapter->impl, thread_id, title)
            : -1;
        result = done > 0
            ? platform_actions_ok_verb("set_thread_title")
            : (done == 0
               ? platform_actions_err("action_failed", "discord thread rename failed")
               : platform_actions_err("action_failed", "discord api raised"));
    } else {
        size_t need = snprintf(NULL, 0,
            "set_thread_title is not implemented for %s", platform) + 1;
        char *detail = malloc(need);
        snprintf(detail, need, "set_thread_title is not implemented for %s", platform);
        result = platform_actions_err("unsupported_platform_action", detail);
        free(detail);
    }
    return result;
}

/* ── PoP: _discord_add_reaction ────────────────────────────────────────── */

/* PoP: _discord_add_reaction @ hermes_cli/platform_actions.py:_discord_add_reaction */
json_t *platform_actions_discord_add_reaction(const pa_adapter_t *adapter,
                                              const char *chat_id,
                                              const char *message_id,
                                              const char *emoji)
{
    void *client = adapter->client;
    if (!client)
        return platform_actions_err("adapter_disconnected",
                                    "discord client unavailable");
    /* channel_id = int(str(chat_id)); msg_id = int(str(message_id)) */
    char *end = NULL;
    long channel_id = strtol(chat_id, &end, 10);
    if (!end || *end != '\0' || end == chat_id)
        return platform_actions_err("invalid_argument",
                                    "discord ids must be numeric");
    end = NULL;
    long msg_id = strtol(message_id, &end, 10);
    if (!end || *end != '\0' || end == message_id)
        return platform_actions_err("invalid_argument",
                                    "discord ids must be numeric");

    if (!adapter->fetch_message || !adapter->add_reaction)
        return platform_actions_err("action_failed", "discord api raised");

    void *channel = adapter->get_channel
        ? adapter->get_channel(client, channel_id) : NULL;
    if (!channel) {
        channel = adapter->fetch_channel
            ? adapter->fetch_channel(client, channel_id) : NULL;
        if (!channel)
            return platform_actions_err("action_failed", "discord api raised");
    }
    void *message = adapter->fetch_message(client, channel, msg_id);
    if (!message)
        return platform_actions_err("action_failed", "discord api raised");
    if (adapter->add_reaction(channel, message, emoji) <= 0)
        return platform_actions_err("action_failed", "discord api raised");
    return platform_actions_ok_verb("add_reaction");
}

/* ── PoP: _audit ───────────────────────────────────────────────────────── */

char *platform_actions_audit(const platform_actions_t *pa, const char *verb,
                             const char *platform, json_t *result)
{
    int ok = json_get_bool(result, "ok", 0) ? 1 : 0;
    char line[1024];
    if (ok) {
        snprintf(line, sizeof line,
                 "platform_action plugin=%s verb=%s platform=%s ok=True",
                 pa->plugin_id, verb, platform);
    } else {
        const char *errv = json_get_str(result, "error", "");
        snprintf(line, sizeof line,
                 "platform_action plugin=%s verb=%s platform=%s ok=False error=%s",
                 pa->plugin_id, verb, platform, errv);
    }
    return strdup(line);
}
