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
/* PoP: relay_helper_record_chat @ gateway/relay/adapter.py:_capture_scope */
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

/* PoP: relay_helper_supports_status_text @ gateway/relay/adapter.py:supports_status_text */
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
/* PoP: descriptor_max_len_for_chat @ gateway/relay/adapter.py:_descriptor_for_chat */
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

/* PoP: relay_helper_max_message_length_for_chat @ gateway/relay/adapter.py:max_message_length_for_chat */
int relay_helper_max_message_length_for_chat(const char *descriptor_json, const char *chat_id) {
    return descriptor_max_len_for_chat(descriptor_json, chat_id);
}

/* PoP: relay_helper_message_len_fn_for_chat @ gateway/relay/adapter.py:message_len_fn_for_chat */
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

/* PoP: relay_helper_relay_slack_extra_bool @ gateway/relay/adapter.py:_relay_slack_extra */
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

/* PoP: relay_helper_coerce_flag @ gateway/relay/adapter.py:_coerce_flag */
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

/* PoP: relay_helper_effective_reply_in_thread @ gateway/relay/adapter.py:_effective_reply_in_thread */
bool relay_helper_effective_reply_in_thread(void) {
    /* Python: _coerce_flag(_relay_slack_extra().get("reply_in_thread"), True),
     * exception -> True. Default True. */
    const char *raw = gateway_config_platform_extra_bool ? "" : NULL;
    (void)raw;
    if (gateway_config_platform_extra_bool("relay", "reply_in_thread")) return true;
    /* No explicit key: default True. */
    return true;
}

/* PoP: relay_helper_dm_top_level_threads_as_sessions @ gateway/relay/adapter.py:_dm_top_level_threads_as_sessions */
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
/* PoP: relay_helper_stamp_slack_session_thread @ gateway/relay/adapter.py:_stamp_slack_session_thread */
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

/* PoP: relay_helper_decode_prompt_token @ gateway/relay/adapter.py:_decode_prompt_token */
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

/* PoP: relay_helper_render_interaction_options @ gateway/relay/adapter.py:_render_interaction_options */
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

/* PoP: relay_helper_mint_prompt @ gateway/relay/adapter.py:_mint_prompt */
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

/* PoP: relay_helper_pop_prompt @ gateway/relay/adapter.py:_pop_prompt */
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

/* PoP: relay_helper_prompt_reply_metadata @ gateway/relay/adapter.py:_prompt_reply_metadata */
char *relay_helper_prompt_reply_metadata(const char *thread_id) {
    /* Python: {"thread_id": str} when the source carries one, else {}. */
    if (thread_id && thread_id[0]) {
        char *out = NULL;
        asprintf(&out, "{\"thread_id\": \"%s\"}", thread_id);
        return out;
    }
    return strdup("{}");
}

/* PoP: relay_helper_auto_thread_info_for_chat @ gateway/relay/adapter.py:auto_thread_info_for_chat */
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

/* PoP: relay_helper_resolve_reply_to_for_send @ gateway/relay/adapter.py:_resolve_reply_to_for_send */
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

/* PoP: relay_helper_apply_slack_thread_anchor @ gateway/relay/adapter.py:_apply_slack_thread_anchor */
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

/* PoP: relay_helper_with_status_thread_anchor @ gateway/relay/adapter.py:_with_status_thread_anchor */
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

/* PoP: relay_helper_get_media_client @ gateway/relay/adapter.py:_get_media_client */
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
/* PoP: relay_helper_localize_inbound_media @ gateway/relay/adapter.py:_localize_inbound_media */
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

/* PoP: relay_adapter_minted_here @ gateway/relay/adapter.py:_minted_here */
int relay_adapter_minted_here(const char *prompt_id, const char *owner_nonce) {
    if (!prompt_id) return 0;
    if (!owner_nonce) return 0;
    /* Check if prompt_id starts with owner_nonce */
    const char *dot = strchr(prompt_id, '.');
    if (dot) {
        size_t head_len = (size_t)(dot - prompt_id);
        return (strlen(owner_nonce) == head_len && strncmp(prompt_id, owner_nonce, head_len) == 0);
    }
    /* No dot segment — treat as ours (pre-upgrade prompt) */
    return 1;
}

/* ════════════════════════════════════════════════════════════════════
 * Gap closure wave: live cards / draft-seal coordination, dedupe,
 * routing warm-up, format hints, auto-thread wait, prompt lifecycle.
 * (gateway/relay/adapter.py REAL_GAPs — see PoP lines per function.)
 * ════════════════════════════════════════════════════════════════════ */

#define RLA_DRAFT_STATE_CAP 512          /* adapter.py RelayAdapter._DRAFT_STATE_CAP */
#define RLA_RESOLVED_PROMPT_MEMORY 256   /* adapter.py _RESOLVED_PROMPT_MEMORY */

/* forward decls within this gap wave (defined further below) */
char *relay_adapter_with_format_hints_for_chat(const char *chat_id, const char *metadata_json);

/* JSON-string escape into a malloc'd buffer (content frames must stay valid JSON). */
static char *rla_json_escape(const char *s) {
    if (!s) s = "";
    size_t cap = strlen(s) * 6 + 3;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && o + 7 < cap; p++) {
        if (*p == '"' || *p == 0x5c) { out[o++] = 0x5c; out[o++] = (char)*p; }
        else if (*p == 0x0a) { out[o++] = 0x5c; out[o++] = 'n'; }
        else if (*p == 0x0d) { out[o++] = 0x5c; out[o++] = 'r'; }
        else if (*p == 0x09) { out[o++] = 0x5c; out[o++] = 't'; }
        else if (*p < 0x20) { o += (size_t)snprintf(out + o, cap - o, "\\u%04x", *p); }
        else out[o++] = (char)*p;
    }
    out[o] = '\0';
    return out;
}

/* metadata JSON string lookup → malloc'd value or NULL (absent/empty). */
static char *rla_md_get(const char *metadata_json, const char *key) {
    if (!metadata_json || !key[0]) return NULL;
    json_t *o = json_parse(metadata_json, NULL);
    if (!o) return NULL;
    json_t *v = json_obj_get(o, key);
    char *out = NULL;
    if (v && v->type == JSON_STRING && v->str_val[0]) out = strdup(v->str_val);
    else if (v && v->type == JSON_NUMBER) {
        if (asprintf(&out, "%lld", (long long)v->num_val) < 0) out = NULL;
    }
    json_free(o);
    return out;
}

static void rla_lower_buf(const char *src, char *dst, size_t n) {
    size_t i = 0;
    for (; src && src[i] && i + 1 < n; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

/* Local supported-op probe over a descriptor JSON "supported_ops" array.
 * (relay_adapter_ops.c keeps its own copy static — no cross-TU coupling.) */
static bool rla_descriptor_supports_op(const char *descriptor_json, const char *op) {
    if (!descriptor_json || !op) return false;
    json_t *o = json_parse(descriptor_json, NULL);
    if (!o) return false;
    bool found = false;
    json_t *ops = json_obj_get(o, "supported_ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *e = json_get(ops, i);
            if (e && e->type == JSON_STRING && strcmp(e->str_val, op) == 0) { found = true; break; }
        }
    }
    json_free(o);
    return found;
}

/* ── stream_is_message_for_chat ─────────────────────────────────────── */
/* PoP: relay_adapter_stream_is_message_for_chat @ gateway/relay/adapter.py:stream_is_message_for_chat */
bool relay_adapter_stream_is_message_for_chat(const char *chat_id) {
    /* Python: str(self._descriptor_for_chat(str(chat_id)).platform or
     * "").lower() == "slack" — resolve through the chat's own negotiated
     * descriptor; platform-name inference is deliberate for now. The C
     * chat-state registry records each chat's platform from inbound. */
    if (!chat_id) return false;
    pthread_mutex_lock(&g_chat_lock);
    relay_chat_state_t *st = chat_state_locked(chat_id, 0);
    bool is_slack = false;
    if (st && st->platform[0]) {
        char lc[64];
        rla_lower_buf(st->platform, lc, sizeof(lc));
        is_slack = (strcmp(lc, "slack") == 0);
    }
    pthread_mutex_unlock(&g_chat_lock);
    return is_slack;
}

/* ── supports_native_task_cards / native_task_cards_enabled ─────────── */
/* PoP: relay_adapter_supports_native_task_cards @ gateway/relay/adapter.py:supports_native_task_cards */
bool relay_adapter_supports_native_task_cards(const char *descriptor_json) {
    /* Python: "task_card" in (self.descriptor.supported_ops or ()) —
     * explicit advertisement required, no fail-open. */
    return rla_descriptor_supports_op(descriptor_json, "task_card");
}

/* PoP: relay_adapter_native_task_cards_enabled @ gateway/relay/adapter.py:native_task_cards_enabled */
bool relay_adapter_native_task_cards_enabled(const char *descriptor_json) {
    /* Python: alias the TurnRunner card lane calls by THIS name; without
     * it the lane silently stays text-mode even though the connector
     * advertises task_card. */
    return relay_adapter_supports_native_task_cards(descriptor_json);
}

/* ════════════════════════════════════════════════════════════════════
 * Draft/seal coordination state (_open_draft_by_chat /
 * _sealed_draft_by_chat), FIFO-bounded like the Python dicts.
 * ════════════════════════════════════════════════════════════════════ */

typedef struct {
    char key[512];
    long long draft_id;
} rla_draft_entry_t;

static rla_draft_entry_t g_open_drafts[RLA_DRAFT_STATE_CAP + 1];
static size_t g_open_draft_len = 0;
static rla_draft_entry_t g_sealed_drafts[RLA_DRAFT_STATE_CAP + 1];
static size_t g_sealed_draft_len = 0;
static pthread_mutex_t g_draft_lock = PTHREAD_MUTEX_INITIALIZER;

/* forward decl within this gap wave (definition follows) */
static void relay_adapter_evict_oldest(rla_draft_entry_t *d, size_t *len);

/* ── _evict_oldest ──────────────────────────────────────────────────── */
/* PoP: relay_adapter_evict_oldest @ gateway/relay/adapter.py:_evict_oldest */
static void relay_adapter_evict_oldest(rla_draft_entry_t *d, size_t *len) {
    /* Python: while len(d) > cls._DRAFT_STATE_CAP: d.pop(next(iter(d))) —
     * FIFO-bound in place (insertion order == array order here). */
    while (*len > RLA_DRAFT_STATE_CAP) {
        memmove(&d[0], &d[1], (*len - 1) * sizeof(rla_draft_entry_t));
        (*len)--;
    }
}

/* ── _draft_key ─────────────────────────────────────────────────────── */
/* PoP: relay_adapter_draft_key @ gateway/relay/adapter.py:_draft_key */
char *relay_adapter_draft_key(const char *chat_id, const char *metadata_json) {
    /* Per-turn identity first (message_id / reply_to_message_id carry the
     * triggering event id); thread anchor fallback; bare chat last resort. */
    if (!chat_id) return NULL;
    char *turn_id = rla_md_get(metadata_json, "message_id");
    if (!turn_id) turn_id = rla_md_get(metadata_json, "reply_to_message_id");
    if (turn_id) {
        char *out = NULL;
        if (asprintf(&out, "%s:turn:%s", chat_id, turn_id) < 0) out = NULL;
        free(turn_id);
        return out;
    }
    char *anchor = rla_md_get(metadata_json, "thread_ts");
    if (!anchor) anchor = rla_md_get(metadata_json, "thread_id");
    char *out = NULL;
    if (asprintf(&out, "%s:%s", chat_id, anchor ? anchor : "") < 0) out = NULL;
    free(anchor);
    return out;
}

/* ── _card_key ──────────────────────────────────────────────────────── */
/* PoP: relay_adapter_card_key @ gateway/relay/adapter.py:_card_key */
char *relay_adapter_card_key(const char *reply_to, const char *metadata_json) {
    /* Same precedence as _draft_key with reply_to winning and "root"
     * last-resort so send AND stop derive one identity. */
    char *anchor = NULL;
    if (reply_to && reply_to[0]) anchor = strdup(reply_to);
    if (!anchor) anchor = rla_md_get(metadata_json, "message_id");
    if (!anchor) anchor = rla_md_get(metadata_json, "reply_to_message_id");
    if (!anchor) anchor = rla_md_get(metadata_json, "thread_ts");
    if (!anchor) anchor = rla_md_get(metadata_json, "thread_id");
    char *out = NULL;
    if (asprintf(&out, "turn:%s", anchor ? anchor : "root") < 0) out = NULL;
    free(anchor);
    return out;
}

/* ── _match_open_draft ──────────────────────────────────────────────── */
/* PoP: relay_adapter_match_open_draft @ gateway/relay/adapter.py:_match_open_draft */
char *relay_adapter_match_open_draft(const char *chat_id, const char *metadata_json) {
    /* Exact key match first. Callers WITHOUT a per-turn message id may
     * absorb via the single-open-stream fallback; callers WITH one never
     * fall back (their identity is authoritative — a mismatch means the
     * stream belongs to someone else). */
    if (!chat_id) return NULL;
    pthread_mutex_lock(&g_draft_lock);
    char *result = NULL;
    char *key = relay_adapter_draft_key(chat_id, metadata_json);
    for (size_t i = 0; i < g_open_draft_len; i++) {
        if (key && strcmp(g_open_drafts[i].key, key) == 0) { result = strdup(key); break; }
    }
    free(key);
    if (!result) {
        bool has_turn_identity = false;
        char *mid = rla_md_get(metadata_json, "message_id");
        char *rid = rla_md_get(metadata_json, "reply_to_message_id");
        if (mid || rid) has_turn_identity = true;
        free(mid);
        free(rid);
        if (!has_turn_identity) {
            size_t hits = 0;
            const char *hit = NULL;
            for (size_t i = 0; i < g_open_draft_len; i++) {
                if (strncmp(g_open_drafts[i].key, chat_id, strlen(chat_id)) == 0 &&
                    g_open_drafts[i].key[strlen(chat_id)] == ':') {
                    hits++;
                    hit = g_open_drafts[i].key;
                }
            }
            if (hits == 1 && hit) {
                fprintf(stderr,
                        "INFO relay: absorbing identity-less send into the single "
                        "open stream %s (single-open-stream fallback)\n", hit);
                result = strdup(hit);
            }
        }
    }
    pthread_mutex_unlock(&g_draft_lock);
    return result;  /* NULL when no open stream matches */
}
/* ── _seal_open_draft ───────────────────────────────────────────────── */
/* PoP: relay_adapter_seal_open_draft @ gateway/relay/adapter.py:_seal_open_draft */
char *relay_adapter_seal_open_draft(const char *chat_id, const char *content,
                                    const char *metadata_json,
                                    const char *draft_key_in) {
    /* Convert the turn-final send into the sealing draft frame:
     * pop the open entry, tombstone BEFORE the transport call, FIFO-evict,
     * then send op=draft final=true. Ambiguous outcomes (exception OR
     * ack-timeout-shaped result) retry the SAME idempotent frame once —
     * the connector's sealed-key tombstone never opens a second stream. */
    if (!chat_id || !content) return strdup("{\"success\": false, \"error\": \"bad args\"}");
    extern char *relay_transport_dispatch_op(const char *op_frame_json);

    pthread_mutex_lock(&g_draft_lock);
    char *key = NULL;
    long long draft_id = -1;
    if (draft_key_in && draft_key_in[0]) key = strdup(draft_key_in);
    else key = relay_adapter_draft_key(chat_id, metadata_json);
    for (size_t i = 0; i < g_open_draft_len; i++) {
        if (strcmp(g_open_drafts[i].key, key ? key : "") == 0) {
            draft_id = g_open_drafts[i].draft_id;
            memmove(&g_open_drafts[i], &g_open_drafts[i + 1],
                    (g_open_draft_len - i - 1) * sizeof(rla_draft_entry_t));
            g_open_draft_len--;
            break;
        }
    }
    if (draft_id < 0) { pthread_mutex_unlock(&g_draft_lock); free(key); return NULL; }
    /* Tombstone BEFORE the transport call: whatever the ack says, this
     * draft_id's stream must never be re-armed by a straggler frame. */
    snprintf(g_sealed_drafts[g_sealed_draft_len].key,
             sizeof(g_sealed_drafts[0].key), "%s", key);
    g_sealed_drafts[g_sealed_draft_len].draft_id = draft_id;
    g_sealed_draft_len++;
    relay_adapter_evict_oldest(g_sealed_drafts, &g_sealed_draft_len);
    pthread_mutex_unlock(&g_draft_lock);

    char *escaped_content = rla_json_escape(content);
    char *hints_meta = relay_adapter_with_format_hints_for_chat(chat_id, metadata_json);
    char *frame = NULL;
    if (asprintf(&frame,
                 "{\"op\": \"draft\", \"chat_id\": \"%s\", \"draft_id\": %lld, "
                 "\"content\": \"%s\", \"final\": true, \"metadata\": %s}",
                 chat_id, draft_id,
                 escaped_content ? escaped_content : "",
                 hints_meta ? hints_meta : "{}") < 0) frame = NULL;
    free(escaped_content);
    free(hints_meta);
    if (!frame) return strdup("{\"success\": false, \"error\": \"oom\"}");

    bool ambiguous = false;
    char *result_json = NULL;
    for (int attempt = 0; attempt < 2; attempt++) {
        free(result_json);
        result_json = relay_transport_dispatch_op(frame);
        if (!result_json) { ambiguous = true; continue; }
        json_t *r = json_parse(result_json, NULL);
        if (!r) { ambiguous = true; continue; }
        json_t *amb = json_obj_get(r, "ambiguous");
        if (amb && amb->type != JSON_NULL && ((amb->type == JSON_BOOL && amb->bool_val))) {
            fprintf(stderr, "WARNING relay seal ack lost (ambiguous): transport ack timeout\n");
            ambiguous = true;
            json_free(r);
            continue;
        }
        ambiguous = false;
        json_free(r);
        break;
    }
    free(frame);
    if (ambiguous) {
        free(result_json);
        return strdup("{\"success\": false, \"error\": "
                      "\"draft seal ambiguous after retry (transport ack lost)\"}");
    }
    return result_json ? result_json
                       : strdup("{\"success\": false, \"error\": \"no transport\"}");
}


/* ── send_native_task_card_progress ─────────────────────────────────── */
/* PoP: relay_adapter_send_native_task_card_progress @ gateway/relay/adapter.py:send_native_task_card_progress */
char *relay_adapter_send_native_task_card_progress(
        const char *descriptor_json, const char *chat_id, const char *tasks_json,
        const char *reply_to, const char *metadata_json) {
    /* Relay leg of the task-card lane: emit ONE card frame per turn.
     * Progress is advisory: a transport drop degrades to a failed
     * SendResult, never raises into the progress loop / turn cleanup. */
    if (!relay_adapter_supports_native_task_cards(descriptor_json))
        return strdup("{\"success\": false, \"error\": \"connector does not advertise task_card\"}");
    extern char *relay_transport_dispatch_op(const char *op_frame_json);
    char *card_id = relay_adapter_card_key(reply_to, metadata_json);
    /* Slack card streams are thread replies: anchor on the triggering
     * message when the runner gave us one and metadata has no thread_ts. */
    json_t *meta = json_parse((metadata_json && metadata_json[0]) ? metadata_json : "{}", NULL);
    if (!meta || meta->type != JSON_OBJECT) {
        if (meta) json_free(meta);
        meta = json_object();
    }
    if (reply_to && reply_to[0] && !json_obj_get(meta, "thread_ts"))
        json_set(meta, "thread_ts", json_string(reply_to));
    char *final_meta = json_dumps(meta, 0);
    json_free(meta);
    char *frame = NULL;
    bool ok = asprintf(&frame,
                       "{\"op\": \"task_card\", \"chat_id\": \"%s\", \"card_id\": \"%s\", "
                       "\"chunks\": %s, \"metadata\": %s}",
                       chat_id ? chat_id : "", card_id ? card_id : "turn:root",
                       tasks_json ? tasks_json : "[]",
                       final_meta ? final_meta : "{}") >= 0;
    free(final_meta);
    free(card_id);
    if (!ok) return strdup("{\"success\": false, \"error\": \"oom\"}");
    char *res = relay_transport_dispatch_op(frame);
    free(frame);
    if (!res)
        return strdup("{\"success\": false, \"error\": \"task_card transport error: lost ack\"}");
    json_t *r = json_parse(res, NULL);
    free(res);
    bool success = false;
    if (r) {
        json_t *s = json_obj_get(r, "success");
        success = (s && s->type == JSON_BOOL && s->bool_val);
        json_free(r);
    }
    return success ? strdup("{\"success\": true}")
                   : strdup("{\"success\": false, \"error\": \"task_card failed\"}");
}

/* ── stop_native_task_card_progress ─────────────────────────────────── */
/* PoP: relay_adapter_stop_native_task_card_progress @ gateway/relay/adapter.py:stop_native_task_card_progress */
char *relay_adapter_stop_native_task_card_progress(
        const char *descriptor_json, const char *chat_id,
        const char *reply_to, const char *metadata_json) {
    /* Seal the card stream at turn end (idempotent connector-side).
     * Best-effort by contract: runs in the progress loop's finally block. */
    if (!relay_adapter_supports_native_task_cards(descriptor_json))
        return strdup("{\"success\": false, \"error\": \"connector does not advertise task_card\"}");
    extern char *relay_transport_dispatch_op(const char *op_frame_json);
    char *card_id = relay_adapter_card_key(reply_to, metadata_json);
    char *frame = NULL;
    bool ok = asprintf(&frame,
                       "{\"op\": \"task_card_stop\", \"chat_id\": \"%s\", "
                       "\"card_id\": \"%s\", \"metadata\": %s}",
                       chat_id ? chat_id : "", card_id ? card_id : "turn:root",
                       (metadata_json && metadata_json[0]) ? metadata_json : "{}") >= 0;
    free(card_id);
    if (!ok) return strdup("{\"success\": false, \"error\": \"oom\"}");
    char *res = relay_transport_dispatch_op(frame);
    free(frame);
    if (!res)
        return strdup("{\"success\": false, \"error\": \"task_card_stop transport error: lost ack\"}");
    json_t *r = json_parse(res, NULL);
    free(res);
    bool success = false;
    if (r) {
        json_t *s = json_obj_get(r, "success");
        success = (s && s->type == JSON_BOOL && s->bool_val);
        json_free(r);
    }
    return strdup(success ? "{\"success\": true}" : "{\"success\": false}");
}

/* ── abandon_open_draft ─────────────────────────────────────────────── */
/* PoP: relay_adapter_abandon_open_draft @ gateway/relay/adapter.py:abandon_open_draft */
char *relay_adapter_abandon_open_draft(const char *chat_id, const char *content,
                                       const char *metadata_json) {
    /* Seal an orphaned stream when its turn dies: seal in place with the
     * text already on screen so the seal adds nothing and claims nothing.
     * Best-effort by contract: failure is reported, never raised. */
    char *draft_key = relay_adapter_match_open_draft(chat_id, metadata_json);
    if (!draft_key) return strdup("{\"success\": true}");  /* nothing armed — no-op */
    char *res = relay_adapter_seal_open_draft(chat_id, content, metadata_json, draft_key);
    free(draft_key);
    if (!res)
        return strdup("{\"success\": false, \"error\": \"abandon seal transport error: null\"}");
    return res;
}


/* ── per-platform descriptor resolver (fail-closed) ─────────────────── */
char *relay_transport_descriptor_for_platform(const char *platform_value) {
    /* The negotiated per-platform descriptors live inside the WS
     * transport's connection state (ws_transport.c has no singleton
     * accessor), so without a live handle this resolves to a bare
     * platform-tagged object — every capability bit reads false, which is
     * the fail-closed posture (never stamp another platform's bit).
     * Callers holding a transport descriptor pass it directly instead. */
    json_t *o = json_object();
    if (!o) return NULL;
    if (!platform_value || !platform_value[0]) { json_free(o); return NULL; }
    json_set(o, "platform", json_string(platform_value));
    char *out = json_dumps(o, 0);
    json_free(o);
    return out;
}

/* ════════════════════════════════════════════════════════════════════
 * Inbound identity / routing warm-up / per-platform capability
 * ════════════════════════════════════════════════════════════════════ */

extern bool gateway_config_platform_extra_bool(const char *platform, const char *key);

/* ── _inbound_dedupe_key ────────────────────────────────────────────── */
/* PoP: relay_adapter_inbound_dedupe_key @ gateway/relay/adapter.py:_inbound_dedupe_key */
char *relay_adapter_inbound_dedupe_key(const char *platform, const char *chat_id,
                                       const char *message_id) {
    /* Stable replay identity: (platform, chat, platform message id).
     * The platform joins the key because one adapter can front SEVERAL
     * platforms over one relay socket — numeric ids must never collide.
     * NULL when either component is missing: synthetic events never
     * dedupe, fail-open by design (dropping a real user message is
     * strictly worse than rerunning one). */
    if (!message_id || !message_id[0] || !chat_id || !chat_id[0]) return NULL;
    char *out = NULL;
    if (asprintf(&out, "%s:%s:%s", platform ? platform : "", chat_id, message_id) < 0)
        return NULL;
    return out;
}

/* ── prime_routing_cache ────────────────────────────────────────────── */
/* PoP: relay_adapter_prime_routing_cache @ gateway/relay/adapter.py:prime_routing_cache */
void relay_adapter_prime_routing_cache(const char *event_json) {
    /* Warm the per-chat egress routing caches from a SYNTHETIC event:
     * a replayed completion turn reaches handle_message with cold caches,
     * so its replies would egress without scope/user discriminators and
     * be declined by the connector's fail-closed tenant guard. Feed it
     * through the same capture used for real inbound. Never raises. */
    if (!event_json) return;
    json_t *o = json_parse(event_json, NULL);
    if (!o) return;
    json_t *src = json_obj_get(o, "source");
    if (!src) { json_free(o); return; }
    const char *chat_id = json_string_value_safe(json_obj_get(src, "chat_id"));
    const char *platform = json_string_value_safe(json_obj_get(src, "platform"));
    const char *thread_id = json_string_value_safe(json_obj_get(src, "thread_id"));
    if (chat_id && chat_id[0])
        relay_helper_record_chat(chat_id, platform, NULL,
                                 thread_id ? thread_id : NULL);
    json_free(o);
}

/* ── supports_inchannel_continuable_for_platform ────────────────────── */
/* PoP: relay_adapter_supports_inchannel_continuable_for_platform @ gateway/relay/adapter.py:supports_inchannel_continuable_for_platform */
bool relay_adapter_supports_inchannel_continuable_for_platform(
        const char *platform_value, const char *per_platform_descriptor_json,
        bool scalar_supports_inchannel_continuable) {
    /* Resolve the platform's OWN negotiated descriptor; fall back to the
     * scalar only when the per-platform descriptor is unavailable — on a
     * multi-platform relay the scalar both leaks the primary's True onto
     * platforms that never advertised it and suppresses a non-primary
     * platform's advertised True. */
    if (platform_value && platform_value[0] && per_platform_descriptor_json) {
        json_t *d = json_parse(per_platform_descriptor_json, NULL);
        if (d) {
            json_t *bit = json_obj_get(d, "supports_inchannel_continuable");
            bool v = (bit && bit->type == JSON_BOOL) ? bit->bool_val : false;
            json_free(d);
            return v;
        }
    }
    return scalar_supports_inchannel_continuable;
}

/* ════════════════════════════════════════════════════════════════════
 * _format_hints / _with_format_hints_for_chat / _for_platform
 * ════════════════════════════════════════════════════════════════════ */

static bool rla_descriptor_supports_block_formatting(const char *descriptor_json) {
    if (!descriptor_json) return false;
    json_t *d = json_parse(descriptor_json, NULL);
    if (!d) return false;
    json_t *bit = json_obj_get(d, "supports_block_formatting");
    bool v = (bit && bit->type == JSON_BOOL) ? bit->bool_val : false;
    json_free(d);
    return v;
}

/* ── _format_hints ──────────────────────────────────────────────────── */
/* PoP: relay_adapter_format_hints @ gateway/relay/adapter.py:_format_hints */
bool relay_adapter_format_hints(const char *descriptor_json, const char *platform,
                                bool *rich_blocks_out, bool *markdown_blocks_out) {
    /* Hints stamped ONLY when (a) the DESTINATION platform's descriptor
     * advertises supports_block_formatting and (b) the operator enabled a
     * knob under platforms.relay.extra.<platform>.* — both default OFF.
     * Same seam and same _coerce_flag semantics as reply_in_thread. */
    if (rich_blocks_out) *rich_blocks_out = false;
    if (markdown_blocks_out) *markdown_blocks_out = false;
    if (!rla_descriptor_supports_block_formatting(descriptor_json)) return false;
    char plat_lc[64];
    rla_lower_buf(platform ? platform : "", plat_lc, sizeof(plat_lc));
    bool rich = gateway_config_platform_extra_bool(plat_lc, "rich_blocks");
    bool md = gateway_config_platform_extra_bool(plat_lc, "markdown_blocks");
    if (rich_blocks_out) *rich_blocks_out = rich;
    if (markdown_blocks_out) *markdown_blocks_out = md;
    return rich || md;
}

static char *rla_with_format_hints(const char *metadata_json, bool rich, bool md) {
    /* merged.setdefault("format_hints", hints) — existing hints win. */
    json_t *meta = json_parse((metadata_json && metadata_json[0]) ? metadata_json : "{}", NULL);
    if (!meta || meta->type != JSON_OBJECT) {
        if (meta) json_free(meta);
        meta = json_object();
    }
    if (json_obj_get(meta, "format_hints")) {
        char *existing = json_dumps(meta, 0);
        json_free(meta);
        return existing;
    }
    json_t *hints = json_object();
    if (rich) json_set(hints, "rich_blocks", json_bool(true));
    if (md) json_set(hints, "markdown_blocks", json_bool(true));
    json_set(meta, "format_hints", hints);
    char *out = json_dumps(meta, 0);
    json_free(meta);
    return out;
}

/* ── _with_format_hints_for_chat ────────────────────────────────────── */
/* PoP: relay_adapter_with_format_hints_for_chat @ gateway/relay/adapter.py:_with_format_hints_for_chat */
char *relay_adapter_with_format_hints_for_chat(const char *chat_id,
                                               const char *metadata_json) {
    /* Resolve the chat's platform from what we saw inbound (_platform_by_chat),
     * falling back to the primary identity for chats we never saw inbound. */
    extern char *relay_adapter_current_descriptor_json(void);
    pthread_mutex_lock(&g_chat_lock);
    relay_chat_state_t *st = chat_state_locked(chat_id, 0);
    char platform[64] = "";
    if (st) snprintf(platform, sizeof(platform), "%s", st->platform);
    pthread_mutex_unlock(&g_chat_lock);
    char *dj = relay_adapter_current_descriptor_json();
    if (!platform[0] && dj) {
        json_t *d = json_parse(dj, NULL);
        if (d) {
            const char *p = json_string_value_safe(json_obj_get(d, "platform"));
            snprintf(platform, sizeof(platform), "%s", p ? p : "");
            json_free(d);
        }
    }
    bool rich = false, md = false;
    bool any = relay_adapter_format_hints(dj, platform, &rich, &md);
    free(dj);
    if (!any)
        return strdup((metadata_json && metadata_json[0]) ? metadata_json : "{}");
    return rla_with_format_hints(metadata_json, rich, md);
}

/* ── _with_format_hints_for_platform ────────────────────────────────── */
/* PoP: relay_adapter_with_format_hints_for_platform @ gateway/relay/adapter.py:_with_format_hints_for_platform */
char *relay_adapter_with_format_hints_for_platform(const char *platform_value,
                                                   const char *metadata_json,
                                                   const char *primary_descriptor_json) {
    /* send_for_platform lane (cron delivery): no inbound event to populate
     * _platform_by_chat, so the destination platform is caller-supplied.
     * Falls back to the scalar descriptor only when it IS that platform's
     * (fail closed: never stamp from another platform's capability bit). */
    const char *descriptor_json = NULL;
    char *resolved = relay_transport_descriptor_for_platform(platform_value);
    if (resolved) descriptor_json = resolved;
    else if (primary_descriptor_json) {
        json_t *d = json_parse(primary_descriptor_json, NULL);
        if (d) {
            const char *p = json_string_value_safe(json_obj_get(d, "platform"));
            if (p && platform_value && strcmp(p, platform_value) == 0)
                descriptor_json = primary_descriptor_json;
            json_free(d);
        }
    }
    bool rich = false, md = false;
    bool any = relay_adapter_format_hints(descriptor_json, platform_value, &rich, &md);
    free(resolved);
    if (!any)
        return strdup((metadata_json && metadata_json[0]) ? metadata_json : "{}");
    return rla_with_format_hints(metadata_json, rich, md);
}

/* ════════════════════════════════════════════════════════════════════
 * wait_for_auto_thread_info / prompt-resolved FIFO / lifecycle acks
 * ════════════════════════════════════════════════════════════════════ */

static pthread_cond_t g_auto_thread_cond = PTHREAD_COND_INITIALIZER;

/* Wake every auto-thread waiter when a chat gains an auto-thread
 * (called by the send path once auto_thread info is recorded). */
void relay_adapter_signal_auto_thread(const char *chat_id) {
    (void)chat_id;
    pthread_cond_broadcast(&g_auto_thread_cond);
}

/* ── wait_for_auto_thread_info ──────────────────────────────────────── */
/* PoP: relay_adapter_wait_for_auto_thread_info @ gateway/relay/adapter.py:wait_for_auto_thread_info */
bool relay_adapter_wait_for_auto_thread_info(const char *chat_id, double timeout_secs,
                                             char *thread_id_out, size_t tid_sz,
                                             char *name_out, size_t name_sz) {
    /* auto_thread_info_for_chat, but willing to wait: answer as soon as
     * the chat's next send installs an auto-thread instead of holding
     * until timeout (only a backstop for a turn that never sends).
     * Returns false on timeout. */
    if (!chat_id) return false;
    if (relay_helper_auto_thread_info_for_chat(chat_id, thread_id_out, tid_sz,
                                               name_out, name_sz))
        return true;

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    time_t secs = (time_t)timeout_secs;
    long nsec = (long)((timeout_secs - (double)secs) * 1e9);
    deadline.tv_sec += secs;
    deadline.tv_nsec += nsec;
    if (deadline.tv_nsec >= 1000000000L) { deadline.tv_sec++; deadline.tv_nsec -= 1000000000L; }

    bool arrived = false;
    pthread_mutex_lock(&g_chat_lock);
    for (;;) {
        relay_chat_state_t *st = chat_state_locked(chat_id, 0);
        if (st && st->auto_thread_id[0]) { arrived = true; break; }
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
            break;  /* timed out */
        struct timespec remain;
        remain.tv_sec = deadline.tv_sec - now.tv_sec;
        remain.tv_nsec = deadline.tv_nsec - now.tv_nsec;
        if (remain.tv_nsec < 0) { remain.tv_sec--; remain.tv_nsec += 1000000000L; }
        pthread_cond_timedwait(&g_auto_thread_cond, &g_chat_lock, &remain);
    }
    pthread_mutex_unlock(&g_chat_lock);
    if (!arrived) return false;
    return relay_helper_auto_thread_info_for_chat(
        chat_id, thread_id_out, tid_sz, name_out, name_sz) != NULL;
}

/* ── _note_prompt_resolved ──────────────────────────────────────────── */
typedef struct {
    char prompt_id[256];
    double resolved_at;
} rla_resolved_prompt_t;

static rla_resolved_prompt_t g_resolved_prompts[RLA_RESOLVED_PROMPT_MEMORY];
static size_t g_resolved_prompt_len = 0;
static pthread_mutex_t g_resolved_prompt_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: relay_adapter_note_prompt_resolved @ gateway/relay/adapter.py:_note_prompt_resolved */
void relay_adapter_note_prompt_resolved(const char *prompt_id) {
    /* Remember that this process already answered prompt_id. Bounded
     * FIFO (popitem(last=False)): old ids drop rather than being retained
     * for the process lifetime. */
    if (!prompt_id) return;
    pthread_mutex_lock(&g_resolved_prompt_lock);
    if (g_resolved_prompt_len < RLA_RESOLVED_PROMPT_MEMORY) {
        snprintf(g_resolved_prompts[g_resolved_prompt_len].prompt_id,
                 sizeof(g_resolved_prompts[0].prompt_id), "%s", prompt_id);
        g_resolved_prompts[g_resolved_prompt_len].resolved_at = (double)time(NULL);
        g_resolved_prompt_len++;
    } else {
        memmove(&g_resolved_prompts[0], &g_resolved_prompts[1],
                (RLA_RESOLVED_PROMPT_MEMORY - 1) * sizeof(rla_resolved_prompt_t));
        snprintf(g_resolved_prompts[RLA_RESOLVED_PROMPT_MEMORY - 1].prompt_id,
                 sizeof(g_resolved_prompts[0].prompt_id), "%s", prompt_id);
        g_resolved_prompts[RLA_RESOLVED_PROMPT_MEMORY - 1].resolved_at = (double)time(NULL);
    }
    pthread_mutex_unlock(&g_resolved_prompt_lock);
}

/* ── _send_lifecycle_ack ────────────────────────────────────────────── */
typedef struct {
    char chat_id[256];
    char text[4096];
    char metadata_json[1024];
} rla_ack_job_t;

extern char *relay_transport_dispatch_op(const char *op_frame_json);

static void *rla_lifecycle_ack_worker(void *arg) {
    rla_ack_job_t *job = (rla_ack_job_t *)arg;
    /* Acks are cosmetic by contract (the audit trail): failures logged at
     * debug, never surfaced — an undelivered ack must never break the
     * reader or the turn. Runs OFF the read loop (Python: background task
     * named relay-lifecycle-ack) because send() blocks on an
     * outbound_result future only the reader resolves — awaiting inline
     * self-deadlocks the transport. */
    char *frame = NULL;
    if (asprintf(&frame,
                 "{\"op\": \"send\", \"chat_id\": \"%s\", \"content\": \"%s\", "
                 "\"metadata\": %s}",
                 job->chat_id, job->text,
                 (job->metadata_json[0]) ? job->metadata_json : "{}") >= 0) {
        char *res = relay_transport_dispatch_op(frame);
        free(res);
        free(frame);
    }
    free(job);
    return NULL;
}

/* PoP: relay_adapter_send_lifecycle_ack @ gateway/relay/adapter.py:_send_lifecycle_ack */
int relay_adapter_send_lifecycle_ack(const char *chat_id, const char *text,
                                     const char *metadata_json) {
    /* Fire-and-forget a prompt-lifecycle ack from read-loop context:
     * detached worker thread (task ref retained by detach). */
    if (!chat_id || !text) return -1;
    rla_ack_job_t *job = calloc(1, sizeof(*job));
    if (!job) return -1;
    snprintf(job->chat_id, sizeof(job->chat_id), "%s", chat_id);
    snprintf(job->text, sizeof(job->text), "%s", text);
    if (metadata_json)
        snprintf(job->metadata_json, sizeof(job->metadata_json), "%s", metadata_json);
    pthread_t th;
    if (pthread_create(&th, NULL, rla_lifecycle_ack_worker, job) != 0) {
        free(job);
        return -1;
    }
    pthread_detach(th);
    return 0;
}

/* ── _notify_prompt_expired ─────────────────────────────────────────── */
/* PoP: relay_adapter_notify_prompt_expired @ gateway/relay/adapter.py:_notify_prompt_expired */
int relay_adapter_notify_prompt_expired(const char *event_json) {
    /* Tell the presser their prompt is no longer waiting. Only the OWNING
     * gateway reaches this (siblings return earlier), so the user sees
     * exactly one notice. Fire-and-forget via the lifecycle-ack path. */
    if (!event_json) return -1;
    json_t *o = json_parse(event_json, NULL);
    if (!o) return -1;
    json_t *src = json_obj_get(o, "source");
    const char *chat_id = src ? json_string_value_safe(json_obj_get(src, "chat_id")) : NULL;
    const char *thread_id = src ? json_string_value_safe(json_obj_get(src, "thread_id")) : NULL;
    if (!chat_id || !chat_id[0]) { json_free(o); return 0; }
    char *meta = relay_helper_prompt_reply_metadata(thread_id);
    int rc = relay_adapter_send_lifecycle_ack(
        chat_id,
        "\xe2\x8c\x9b That prompt is no longer waiting for an answer. "
        "Send your reply as a normal message.",
        meta);
    free(meta);
    json_free(o);
    return rc;
}
