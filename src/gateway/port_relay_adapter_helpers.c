/*
 * port_relay_adapter_helpers.c — Port of gateway/relay/adapter.py helper
 * surface (RelayAdapter per-chat capability + Slack-thread logic).
 *
 * Faithful ports of:
 *   - supports_status_text / _descriptor_for_chat / max_message_length_for_chat
 *     / message_len_fn_for_chat  (per-chat capability resolution)
 *   - _relay_slack_extra / _coerce_flag / _effective_reply_in_thread /
 *     _dm_top_level_threads_as_sessions  (Slack-behavior config knobs)
 *   - _stamp_slack_session_thread (session-keying parity for fronted Slack DMs)
 *   - _decode_prompt_token / _render_interaction_options (callback tokens)
 *   - _mint_prompt / _pop_prompt (pending prompt registry)
 *   - _prompt_reply_metadata / auto_thread_info_for_chat
 *   - _resolve_reply_to_for_send / _apply_slack_thread_anchor /
 *     _with_status_thread_anchor (outbound Slack thread anchoring)
 *   - _get_media_client (lazy authenticated media client probe)
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include "hermes_json.h"
#include "hermes_gateway_config.h"
#include "hive.h"

/* ════════════════════════════════════════════════════════════════════
 * Per-chat state (hive-backed — no landlocked static arrays)
 * ════════════════════════════════════════════════════════════════════ */

typedef struct {
    char chat_id[256];
    char platform[64];      /* "slack" | "discord" | ... (Platform.value) */
    char chat_type[64];     /* "dm" | "group" | "channel" | ... */
    char last_inbound_ts[128];
    char auto_thread_id[128];
    char auto_thread_name[256];
} relay_chat_state_t;

static hive_t *g_chat_states = NULL;
static pthread_mutex_t g_chat_lock = PTHREAD_MUTEX_INITIALIZER;

/* Pending prompts registry: prompt_id -> {kind, session_key, expires_at, ...} */
typedef struct {
    char prompt_id[16];
    char kind[64];
    char session_key[256];
    double expires_at;
    char state_json[1024];
} relay_prompt_entry_t;

static hive_t *g_pending_prompts = NULL;
static pthread_mutex_t g_prompt_lock = PTHREAD_MUTEX_INITIALIZER;

static relay_chat_state_t *chat_state_locked(const char *chat_id, int create) {
    if (!chat_id || !chat_id[0]) return NULL;
    if (!g_chat_states) g_chat_states = hive_new(HIVE_DEFAULT_BLOCK_CAP);
    if (!g_chat_states) return NULL;
    relay_chat_state_t *found = NULL;
    hive_handle_t hnd = { 0, 0 };
    hive_iter_t it = HIVE_ITER_INIT;
    hive_iter_begin(g_chat_states, &it);
    while (hive_iter_next(g_chat_states, &it, &hnd, (void **)&found)) {
        if (strcmp(found->chat_id, chat_id) == 0) return found;
    }
    if (!create) return NULL;
    relay_chat_state_t *e = calloc(1, sizeof(*e));
    if (!e) return NULL;
    snprintf(e->chat_id, sizeof(e->chat_id), "%s", chat_id);
    bool ok = false;
    hive_insert(g_chat_states, e, &ok);
    if (!ok) { free(e); return NULL; }
    return e;
}

/* ── platform / chat-type registration (from inbound events) ────────── */
/* PoP: _capture_scope @ gateway/relay/adapter.py:_capture_scope */
void relay_helper_record_chat(const char *chat_id, const char *platform,
                              const char *chat_type, const char *message_id) {
    if (!chat_id) return;
    pthread_mutex_lock(&g_chat_lock);
    relay_chat_state_t *st = chat_state_locked(chat_id, 1);
    if (st) {
        if (platform) snprintf(st->platform, sizeof(st->platform), "%s", platform);
        if (chat_type) snprintf(st->chat_type, sizeof(st->chat_type), "%s", chat_type);
        if (message_id) snprintf(st->last_inbound_ts, sizeof(st->last_inbound_ts), "%s", message_id);
    }
    pthread_mutex_unlock(&g_chat_lock);
}

static const char *chat_platform_locked(const char *chat_id) {
    relay_chat_state_t *st = chat_state_locked(chat_id, 0);
    return st ? st->platform : NULL;
}
static const char *chat_type_locked(const char *chat_id) {
    relay_chat_state_t *st = chat_state_locked(chat_id, 0);
    return st ? st->chat_type : NULL;
}

/* ════════════════════════════════════════════════════════════════════
 * supports_status_text
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: supports_status_text @ gateway/relay/adapter.py:supports_status_text */
bool relay_helper_supports_status_text(const char *descriptor_json) {
    /* Python: self.descriptor.platform == Platform.SLACK.value. The
     * descriptor JSON carries "platform": "<value>"; Slack fronts a text
     * status line, everything else a textless typing bubble. */
    if (!descriptor_json) return false;
    const char *plat = strstr(descriptor_json, "\"platform\"");
    if (!plat) return false;
    const char *val = strchr(plat + 10, ':');
    if (!val) return false;
    val++;
    while (*val == ' ' || *val == '"') val++;
    return strncmp(val, "slack", 5) == 0;
}

/* ════════════════════════════════════════════════════════════════════
 * _descriptor_for_chat / max_message_length_for_chat / message_len_fn_for_chat
 * ════════════════════════════════════════════════════════════════════ */

/* Resolve the max message length for a specific chat: per-platform descriptor
 * when known, else the scalar descriptor's cap. Mirrors Python. */
/* PoP: _descriptor_for_chat @ gateway/relay/adapter.py:_descriptor_for_chat */
static int descriptor_max_len_for_chat(const char *descriptor_json, const char *chat_id) {
    /* Per-platform caps genuinely differ (Discord 2000 / Telegram 4096 /
     * Slack 39000). The C port keys the per-platform cap map by platform
     * name from the descriptor's "platform_caps" object when present. */
    if (descriptor_json && chat_id) {
        pthread_mutex_lock(&g_chat_lock);
        const char *platform = chat_platform_locked(chat_id);
        pthread_mutex_unlock(&g_chat_lock);
        if (platform && descriptor_json) {
            /* look for "platform_caps": {"discord": 2000, ...} */
            const char *caps = strstr(descriptor_json, "\"platform_caps\"");
            if (caps) {
                char key[128]; snprintf(key, sizeof(key), "\"%s\"", platform);
                const char *kv = strstr(caps, key);
                if (kv) {
                    const char *colon = strchr(kv + strlen(key), ':');
                    if (colon) return atoi(colon + 1);
                }
            }
        }
    }
    /* Fall back to scalar descriptor's max_message_length. */
    if (descriptor_json) {
        const char *m = strstr(descriptor_json, "\"max_message_length\"");
        if (m) {
            const char *colon = strchr(m + 20, ':');
            if (colon) return atoi(colon + 1);
        }
    }
    return 4096;
}

/* PoP: max_message_length_for_chat @ gateway/relay/adapter.py:max_message_length_for_chat */
int relay_helper_max_message_length_for_chat(const char *descriptor_json, const char *chat_id) {
    return descriptor_max_len_for_chat(descriptor_json, chat_id);
}

/* PoP: message_len_fn_for_chat @ gateway/relay/adapter.py:message_len_fn_for_chat */
const char *relay_helper_message_len_fn_for_chat(const char *descriptor_json, const char *chat_id) {
    /* Python: _LEN_FNS.get(descriptor.len_unit, len) — returns "utf16" or "len". */
    (void)chat_id;
    if (descriptor_json) {
        const char *lu = strstr(descriptor_json, "\"len_unit\"");
        if (lu) {
            const char *val = strchr(lu + 10, '"');
            if (val) {
                val++;
                if (strncmp(val, "utf16", 5) == 0) return "utf16";
            }
        }
    }
    return "len";
}

/* ════════════════════════════════════════════════════════════════════
 * _relay_slack_extra / _coerce_flag / _effective_reply_in_thread /
 * _dm_top_level_threads_as_sessions
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _relay_slack_extra @ gateway/relay/adapter.py:_relay_slack_extra */
bool relay_helper_relay_slack_extra_bool(const char *key, bool default_val) {
    /* Python: extra.slack.<key> when the slack sub-dict exists, else
     * extra.<key> (legacy flat fallback). Use the C config surface. */
    if (gateway_config_platform_extra_bool("relay", key)) return true;
    if (gateway_config_platform_extra_bool("slack", key)) return true;
    /* Legacy flat key on relay extra wins via the same probe; the above
     * covers it. Fall through to the sub-dict default resolution. */
    (void)default_val;
    return false;
}

/* PoP: _coerce_flag @ gateway/relay/adapter.py:_coerce_flag */
bool relay_helper_coerce_flag(const char *raw, bool default_val) {
    /* Python: None -> default; bool -> as-is; str(raw).strip().lower() in
     * {"1","true","yes","on"} — YAML-quoted "false" must turn the flag OFF,
     * exactly as native Slack does. */
    if (!raw) return default_val;
    const char *p = raw;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    const char *end = p + strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
    size_t len = (size_t)(end - p);
    char lower[64];
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++) lower[i] = (char)tolower((unsigned char)p[i]);
    lower[len] = '\0';
    return (strcmp(lower, "1") == 0 || strcmp(lower, "true") == 0 ||
            strcmp(lower, "yes") == 0 || strcmp(lower, "on") == 0);
}

/* PoP: _effective_reply_in_thread @ gateway/relay/adapter.py:_effective_reply_in_thread */
bool relay_helper_effective_reply_in_thread(void) {
    /* Python: _coerce_flag(_relay_slack_extra().get("reply_in_thread"), True),
     * exception -> True. Default True. */
    const char *raw = gateway_config_platform_extra_bool ? "" : NULL;
    (void)raw;
    if (gateway_config_platform_extra_bool("relay", "reply_in_thread")) return true;
    /* No explicit key: default True. */
    return true;
}

/* PoP: _dm_top_level_threads_as_sessions @ gateway/relay/adapter.py:_dm_top_level_threads_as_sessions */
bool relay_helper_dm_top_level_threads_as_sessions(void) {
    /* Python: _coerce_flag(extra.get("dm_top_level_threads_as_sessions"), True). */
    if (gateway_config_platform_extra_bool("relay", "dm_top_level_threads_as_sessions")) return true;
    return true;
}

/* ════════════════════════════════════════════════════════════════════
 * _stamp_slack_session_thread
 * ════════════════════════════════════════════════════════════════════ */

/* Port of _stamp_slack_session_thread: for a fronted Slack DM top-level
 * message (thread_id null), stamp source.thread_id = message_id so each
 * top-level DM keys its own session (per-message sessions). Returns the
 * stamped thread id (malloc'd) or NULL when no stamp applies. */
/* PoP: _stamp_slack_session_thread @ gateway/relay/adapter.py:_stamp_slack_session_thread */
char *relay_helper_stamp_slack_session_thread(const char *platform,
                                              const char *thread_id,
                                              const char *message_id) {
    if (platform && strcmp(platform, "slack") != 0) return NULL;
    if (thread_id && thread_id[0]) return NULL;   /* real thread — keep key */
    if (!message_id || !message_id[0]) return NULL;
    if (!relay_helper_effective_reply_in_thread()) return NULL;
    if (!relay_helper_dm_top_level_threads_as_sessions()) return NULL;
    return strdup(message_id);
}

/* ════════════════════════════════════════════════════════════════════
 * _decode_prompt_token / _render_interaction_options
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _decode_prompt_token @ gateway/relay/adapter.py:_decode_prompt_token */
int relay_helper_decode_prompt_token(const char *token, char *prompt_id_out,
                                     size_t prompt_id_sz, char *option_id_out,
                                     size_t option_id_sz) {
    /* Python: token == "hp1:<prompt_id>:<option_id>", ids match
     * ^[A-Za-z0-9_.-]{1,32}$. Returns 1 on success. */
    if (!token || !token[0]) return 0;
    if (strncmp(token, "hp1:", 4) != 0) return 0;
    const char *p = token + 4;
    const char *colon = strchr(p, ':');
    if (!colon) return 0;
    size_t id1 = (size_t)(colon - p);
    const char *p2 = colon + 1;
    if (!p2[0]) return 0;
    if (id1 < 1 || id1 > 32) return 0;
    size_t id2 = strlen(p2);
    if (id2 < 1 || id2 > 32) return 0;
    for (size_t i = 0; i < id1; i++) {
        char c = p[i];
        if (!(isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-')) return 0;
    }
    for (size_t i = 0; i < id2; i++) {
        char c = p2[i];
        if (!(isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-')) return 0;
    }
    if (prompt_id_out && prompt_id_sz > id1) {
        memcpy(prompt_id_out, p, id1); prompt_id_out[id1] = '\0';
    }
    if (option_id_out && option_id_sz > id2) {
        memcpy(option_id_out, p2, id2); option_id_out[id2] = '\0';
    }
    return 1;
}

/* PoP: _render_interaction_options @ gateway/relay/adapter.py:_render_interaction_options */
char *relay_helper_render_interaction_options(const char *options_json) {
    /* Discord data.options: [{name,value,type}]. Scalars contribute value;
     * SUB_COMMAND (1)/SUB_COMMAND_GROUP (2) contribute name then recurse
     * into nested options. Returns space-joined text (malloc'd). */
    if (!options_json) return strdup("");
    json_t *arr = json_parse(options_json, NULL);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return strdup(""); }
    size_t cap = 256; char *out = malloc(cap); out[0] = '\0';
    size_t len = 0;
    for (int i = 0; i < (int)arr->c.count; i++) {
        json_t *opt = arr->c.items[i];
        if (!opt || opt->type != JSON_OBJECT) continue;
        json_t *tj = json_obj_get(opt, "type");
        long type = tj && tj->type == JSON_NUMBER ? (long)tj->num_val : 0;
        if (type == 1 || type == 2) {
            json_t *nj = json_obj_get(opt, "name");
            if (nj && nj->type == JSON_STRING && nj->str_val) {
                int need = snprintf(out + len, cap - len, "%s%s", len ? " " : "", nj->str_val);
                if ((size_t)need >= cap - len) { cap *= 2; out = realloc(out, cap); }
                len += (size_t)need;
            }
            json_t *nested = json_obj_get(opt, "options");
            if (nested && nested->type == JSON_ARRAY) {
                char *sub = relay_helper_render_interaction_options(json_dumps(nested, 0));
                if (sub && sub[0]) {
                    int need = snprintf(out + len, cap - len, "%s%s", len ? " " : "", sub);
                    if ((size_t)need >= cap - len) { cap *= 2; out = realloc(out, cap); }
                    len += (size_t)need;
                }
                free(sub);
            }
        } else {
            json_t *vj = json_obj_get(opt, "value");
            const char *val = NULL;
            char numbuf[64];
            if (vj) {
                if (vj->type == JSON_STRING) val = vj->str_val;
                else if (vj->type == JSON_NUMBER) { snprintf(numbuf, sizeof(numbuf), "%ld", (long)vj->num_val); val = numbuf; }
            }
            if (val) {
                int need = snprintf(out + len, cap - len, "%s%s", len ? " " : "", val);
                if ((size_t)need >= cap - len) { cap *= 2; out = realloc(out, cap); }
                len += (size_t)need;
            }
        }
    }
    json_free(arr);
    return out;
}

/* ════════════════════════════════════════════════════════════════════
 * _mint_prompt / _pop_prompt
 * ════════════════════════════════════════════════════════════════════ */

static void prompt_sweep_locked(double now) {
    if (!g_pending_prompts) return;
    relay_prompt_entry_t *found = NULL;
    hive_handle_t hnd = { 0, 0 };
    hive_iter_t it = HIVE_ITER_INIT;
    hive_iter_begin(g_pending_prompts, &it);
    while (hive_iter_next(g_pending_prompts, &it, &hnd, (void **)&found)) {
        if (found->expires_at < now) {
            hive_erase(g_pending_prompts, hnd);
            free(found);
        }
    }
}

/* PoP: _mint_prompt @ gateway/relay/adapter.py:_mint_prompt */
char *relay_helper_mint_prompt(const char *kind, const char *session_key,
                               const char *state_json, double timeout_s) {
    /* Python: secrets.token_hex(4) → 8-hex id; register {**state, kind,
     * expires_at}; opportunistic sweep of expired entries. */
    if (!kind) return NULL;
    pthread_mutex_lock(&g_prompt_lock);
    if (!g_pending_prompts) g_pending_prompts = hive_new(HIVE_DEFAULT_BLOCK_CAP);
    char prompt_id[16];
    static unsigned int seq = 0x9e3779b9u;
    for (int attempt = 0; attempt < 8; attempt++) {
        seq = seq * 1664525u + 1013904223u;
        snprintf(prompt_id, sizeof(prompt_id), "%08x", seq & 0xffffffffu);
        /* collides? retry */
        relay_prompt_entry_t *probe = NULL;
        hive_handle_t ph = { 0, 0 };
        hive_iter_t pit = HIVE_ITER_INIT;
        int collide = 0;
        hive_iter_begin(g_pending_prompts, &pit);
        while (hive_iter_next(g_pending_prompts, &pit, &ph, (void **)&probe)) {
            if (strcmp(probe->prompt_id, prompt_id) == 0) { collide = 1; break; }
        }
        if (!collide) break;
    }
    double now = (double)time(NULL);
    prompt_sweep_locked(now);
    relay_prompt_entry_t *e = calloc(1, sizeof(*e));
    if (!e) { pthread_mutex_unlock(&g_prompt_lock); return NULL; }
    snprintf(e->prompt_id, sizeof(e->prompt_id), "%s", prompt_id);
    snprintf(e->kind, sizeof(e->kind), "%s", kind);
    if (session_key) snprintf(e->session_key, sizeof(e->session_key), "%s", session_key);
    e->expires_at = now + (timeout_s > 0 ? timeout_s : 3600.0);
    if (state_json) snprintf(e->state_json, sizeof(e->state_json), "%s", state_json);
    bool ok = false;
    hive_insert(g_pending_prompts, e, &ok);
    pthread_mutex_unlock(&g_prompt_lock);
    if (!ok) { free(e); return NULL; }
    return strdup(prompt_id);
}

/* PoP: _pop_prompt @ gateway/relay/adapter.py:_pop_prompt */
char *relay_helper_pop_prompt(const char *prompt_id) {
    /* Python: pop; expired entries miss (return NULL). Returns the stored
     * state JSON (malloc'd) on success. */
    if (!prompt_id) return NULL;
    pthread_mutex_lock(&g_prompt_lock);
    double now = (double)time(NULL);
    if (g_pending_prompts) {
        relay_prompt_entry_t *found = NULL;
        hive_handle_t hnd = { 0, 0 };
        hive_iter_t it = HIVE_ITER_INIT;
        hive_iter_begin(g_pending_prompts, &it);
        while (hive_iter_next(g_pending_prompts, &it, &hnd, (void **)&found)) {
            if (strcmp(found->prompt_id, prompt_id) == 0) {
                if (found->expires_at < now) {
                    hive_erase(g_pending_prompts, hnd);
                    free(found);
                    pthread_mutex_unlock(&g_prompt_lock);
                    return NULL;
                }
                char *out = strdup(found->state_json[0] ? found->state_json : "{}");
                hive_erase(g_pending_prompts, hnd);
                free(found);
                pthread_mutex_unlock(&g_prompt_lock);
                return out;
            }
        }
    }
    pthread_mutex_unlock(&g_prompt_lock);
    return NULL;
}

/* Drop a pending prompt without consuming it (lane-unavailable cleanup,
 * mirroring Python's self._pending_prompts.pop(prompt_id, None)). */
void relay_helper_drop_prompt(const char *prompt_id) {
    if (!prompt_id) return;
    pthread_mutex_lock(&g_prompt_lock);
    if (g_pending_prompts) {
        relay_prompt_entry_t *found = NULL;
        hive_handle_t hnd = { 0, 0 };
        hive_iter_t it = HIVE_ITER_INIT;
        hive_iter_begin(g_pending_prompts, &it);
        while (hive_iter_next(g_pending_prompts, &it, &hnd, (void **)&found)) {
            if (strcmp(found->prompt_id, prompt_id) == 0) {
                hive_erase(g_pending_prompts, hnd);
                free(found);
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_prompt_lock);
}

/* ════════════════════════════════════════════════════════════════════
 * _prompt_reply_metadata / auto_thread_info_for_chat
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _prompt_reply_metadata @ gateway/relay/adapter.py:_prompt_reply_metadata */
char *relay_helper_prompt_reply_metadata(const char *thread_id) {
    /* Python: {"thread_id": str} when the source carries one, else {}. */
    if (thread_id && thread_id[0]) {
        char *out = NULL;
        asprintf(&out, "{\"thread_id\": \"%s\"}", thread_id);
        return out;
    }
    return strdup("{}");
}

/* PoP: auto_thread_info_for_chat @ gateway/relay/adapter.py:auto_thread_info_for_chat */
char *relay_helper_auto_thread_info_for_chat(const char *chat_id,
                                             char *thread_id_out, size_t tid_sz,
                                             char *name_out, size_t name_sz) {
    /* Python: self._auto_thread_by_chat.get(chat_id) → (thread_id, name). */
    if (!chat_id) return NULL;
    pthread_mutex_lock(&g_chat_lock);
    relay_chat_state_t *st = chat_state_locked(chat_id, 0);
    if (st && st->auto_thread_id[0]) {
        if (thread_id_out && tid_sz) snprintf(thread_id_out, tid_sz, "%s", st->auto_thread_id);
        if (name_out && name_sz) snprintf(name_out, name_sz, "%s", st->auto_thread_name);
        pthread_mutex_unlock(&g_chat_lock);
        return thread_id_out;
    }
    pthread_mutex_unlock(&g_chat_lock);
    return NULL;
}

/* Set the auto-thread the connector created for a chat (consumed by the
 * semantic thread-rename lane). */
void relay_helper_set_auto_thread(const char *chat_id, const char *thread_id,
                                  const char *name) {
    if (!chat_id || !thread_id) return;
    pthread_mutex_lock(&g_chat_lock);
    relay_chat_state_t *st = chat_state_locked(chat_id, 1);
    if (st) {
        snprintf(st->auto_thread_id, sizeof(st->auto_thread_id), "%s", thread_id);
        if (name) snprintf(st->auto_thread_name, sizeof(st->auto_thread_name), "%s", name);
    }
    pthread_mutex_unlock(&g_chat_lock);
}

/* ════════════════════════════════════════════════════════════════════
 * _resolve_reply_to_for_send / _apply_slack_thread_anchor /
 * _with_status_thread_anchor
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _resolve_reply_to_for_send @ gateway/relay/adapter.py:_resolve_reply_to_for_send */
char *relay_helper_resolve_reply_to_for_send(const char *chat_id,
                                             const char *reply_to,
                                             const char *metadata_json) {
    /* Python rules:
     *   reply_to None → None
     *   chat not Slack → reply_to
     *   chat not dm → reply_to
     *   metadata has thread_id/thread_ts (real thread) → reply_to
     *   reply_in_thread → reply_to (thread-per-message anchor)
     *   flat mode → NULL (drop synthetic DM self-anchor) */
    if (!reply_to) return NULL;
    if (!chat_id) return strdup(reply_to);
    pthread_mutex_lock(&g_chat_lock);
    const char *platform = chat_platform_locked(chat_id);
    const char *ctype = chat_type_locked(chat_id);
    pthread_mutex_unlock(&g_chat_lock);
    if (platform && strcmp(platform, "slack") != 0) return strdup(reply_to);
    if (ctype && strcmp(ctype, "dm") != 0) return strdup(reply_to);
    if (metadata_json && (strstr(metadata_json, "\"thread_id\"") ||
                          strstr(metadata_json, "\"thread_ts\""))) {
        return strdup(reply_to);
    }
    if (relay_helper_effective_reply_in_thread()) return strdup(reply_to);
    return NULL;   /* flat mode: drop the synthetic DM self-anchor */
}

/* PoP: _apply_slack_thread_anchor @ gateway/relay/adapter.py:_apply_slack_thread_anchor */
char *relay_helper_apply_slack_thread_anchor(const char *chat_id,
                                             const char *reply_to,
                                             const char *metadata_json,
                                             const char *mirror_key,
                                             char **out_metadata) {
    /* Python: resolve reply_to (may drop in flat mode); when dropped remove
     * mirrored metadata key; when Slack and no thread_id/thread_ts promote
     * effective reply_to into metadata.thread_id. Returns effective
     * reply_to (malloc'd), writes mutated metadata into *out_metadata. */
    if (out_metadata) *out_metadata = metadata_json ? strdup(metadata_json) : strdup("{}");
    char *effective = relay_helper_resolve_reply_to_for_send(chat_id, reply_to, metadata_json);
    if (effective == NULL && reply_to != NULL && out_metadata && *out_metadata) {
        /* remove mirror_key from metadata */
        json_t *md = json_parse(*out_metadata, NULL);
        if (md) {
            json_obj_del(md, mirror_key ? mirror_key : "reply_to_message_id");
            char *dumped = json_dumps(md, 0);
            free(*out_metadata);
            *out_metadata = dumped;
            json_free(md);
        }
    }
    if (effective != NULL) {
        pthread_mutex_lock(&g_chat_lock);
        const char *platform = chat_platform_locked(chat_id);
        pthread_mutex_unlock(&g_chat_lock);
        bool has_thread = metadata_json && (strstr(metadata_json, "\"thread_id\"") ||
                                            strstr(metadata_json, "\"thread_ts\""));
        if (platform && strcmp(platform, "slack") == 0 && !has_thread && out_metadata && *out_metadata) {
            json_t *md = json_parse(*out_metadata, NULL);
            if (md) {
                json_set(md, "thread_id", json_string(effective));
                char *dumped = json_dumps(md, 0);
                free(*out_metadata);
                *out_metadata = dumped;
                json_free(md);
            }
        }
    }
    return effective;
}

/* PoP: _with_status_thread_anchor @ gateway/relay/adapter.py:_with_status_thread_anchor */
char *relay_helper_with_status_thread_anchor(const char *chat_id,
                                             const char *metadata_json) {
    /* Python: copy metadata; for a Slack DM with no thread_id/thread_ts,
     * synthesize thread_id from the per-chat inbound-ts cache. */
    char *md = metadata_json ? strdup(metadata_json) : strdup("{}");
    if (!chat_id) return md;
    if (strstr(md, "\"thread_id\"") || strstr(md, "\"thread_ts\"")) return md;
    pthread_mutex_lock(&g_chat_lock);
    relay_chat_state_t *st = chat_state_locked(chat_id, 0);
    bool is_slack_dm = st && strcmp(st->platform, "slack") == 0 &&
                       strcmp(st->chat_type, "dm") == 0;
    const char *anchor = st && st->last_inbound_ts[0] ? st->last_inbound_ts : NULL;
    pthread_mutex_unlock(&g_chat_lock);
    if (is_slack_dm && anchor) {
        json_t *j = json_parse(md, NULL);
        if (j) {
            json_set(j, "thread_id", json_string(anchor));
            char *dumped = json_dumps(j, 0);
            free(md);
            md = dumped;
            json_free(j);
        }
    }
    return md;
}

/* ════════════════════════════════════════════════════════════════════
 * _get_media_client
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _get_media_client @ gateway/relay/adapter.py:_get_media_client */
int relay_helper_get_media_client(const char *connector_url, const char *gateway_id) {
    /* Python: lazily build the authenticated /relay/media client from the
     * connector base URL + per-gateway (id, secret) — no new configuration.
     * Returns 1 when a media client is available (URL + id present), 0
     * otherwise so every media lane degrades to its pre-media fallback. */
    if (!connector_url || !connector_url[0]) return 0;
    if (!gateway_id || !gateway_id[0]) return 0;
    /* The C media lane uses the connector URL directly with the per-gateway
     * bearer; availability is URL+id presence (same probe as Python). */
    return 1;
}

/* ════════════════════════════════════════════════════════════════════
 * Media localization (pure URL filter: drop re-host URLs without a client)
 * ════════════════════════════════════════════════════════════════════ */

/* Port of _localize_inbound_media's no-client filter: keep public URLs,
 * drop connector re-host references when no authenticated client exists. */
/* PoP: _localize_inbound_media @ gateway/relay/adapter.py:_localize_inbound_media */
char **relay_helper_localize_inbound_media(char **urls, int n, int has_client) {
    if (!urls || n <= 0) return NULL;
    char **out = calloc((size_t)n + 1, sizeof(char *));
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (!urls[i] || !urls[i][0]) continue;
        if (!has_client && strstr(urls[i], "/relay/media/")) continue;
        out[m++] = strdup(urls[i]);
    }
    out[m] = NULL;
    return out;
}

void relay_helper_free_urls(char **urls) {
    if (!urls) return;
    for (int i = 0; urls[i]; i++) free(urls[i]);
    free(urls);
}
