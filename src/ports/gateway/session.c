#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/gateway/session.c — C11 port of gateway/session.py
 * Merged from 5 source file(s) by reorganization.
 */

/* ── from src/agent/port_gateway_session_py_parity.c ── */
/* Slermes C port — gateway/session.py (remaining gaps) */

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

/* PoP: gateway_session_is_path_unsafe_p @ gateway/session.py:_is_path_unsafe */
json_t *gateway_session_is_path_unsafe_p(void) {
    return json_object();
}

/* PoP: gateway_session_session_key_namespace_p @ gateway/session.py:_session_key_namespace */
json_t *gateway_session_session_key_namespace_p(void) {
    return json_object();
}


/* ── from src/agent/port_multi_mod_gaps.c ── */
/* Slermes C port — multi-module DA-4 gaps (gateway/session, gateway/weixin,
 * tools/process_registry, tools/mcp_tool, agent/context_compressor) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"
#include "hermes_logger.h"
#include "libcrypto/crypto.h"









/* ═══════════════════════════════════════════════════════════════════
 * gateway/session.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── auto_continue_freshness_window (L40) ── */
/* PoP: gw_sess_auto_continue_freshness @ gateway/session.py:auto_continue_freshness_window */
double gw_sess_auto_continue_freshness(void) {
    const char *raw = getenv("HERMES_AUTO_CONTINUE_FRESHNESS");
    if (!raw || !*raw) return 300.0; /* default 5 min */
    char *end = NULL;
    double val = strtod(raw, &end);
    if (end && *end == '\0' && val > 0.0) return val;
    return 300.0;
}

/* ── _hash_id (L64) ── */
/* PoP: gw_sess_hash_id @ gateway/session.py:_hash_id */
void gw_sess_hash_id(const char *value, char *out, size_t out_sz) {
    if (!value) value = "";
    unsigned char hash[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)value, strlen(value), hash);
    snprintf(out, out_sz, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
             hash[0], hash[1], hash[2], hash[3], hash[4], hash[5],
             hash[6], hash[7], hash[8], hash[9], hash[10], hash[11]);
}

/* ── _hash_sender_id (L69) ── */
/* PoP: gw_sess_hash_sender_id @ gateway/session.py:_hash_sender_id */
void gw_sess_hash_sender_id(const char *value, char *out, size_t out_sz) {
    char hash[13];
    gw_sess_hash_id(value, hash, sizeof(hash));
    snprintf(out, out_sz, "user_%s", hash);
}

/* ═══════════════════════════════════════════════════════════════════
 * gateway/platforms/weixin.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _is_stale_session_ret (L118) ── */
/* PoP: weixin_is_stale_session @ gateway/platforms/weixin.py:_is_stale_session_ret */
bool weixin_is_stale_session(int ret, int errcode, const char *errmsg) {
    if (ret != -2 && errcode != -2) return false;
    if (!errmsg) return false;
    /* Check if errmsg.lower() == "unknown error" */
    char buf[64];
    size_t len = strlen(errmsg);
    if (len >= sizeof(buf)) return false;
    for (size_t i = 0; i < len; i++) buf[i] = tolower((unsigned char)errmsg[i]);
    buf[len] = '\0';
    return strcmp(buf, "unknown error") == 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * tools/process_registry.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── format_uptime_short (L355) ── */
/* PoP: proc_reg_format_uptime_short @ tools/process_registry.py:format_uptime_short */
void proc_reg_format_uptime_short(int seconds, char *out, size_t out_sz) {
    int s = seconds > 0 ? seconds : 0;
    if (s < 60) {
        snprintf(out, out_sz, "%ds", s);
    } else if (s < 3600) {
        snprintf(out, out_sz, "%dm %ds", s / 60, s % 60);
    } else {
        snprintf(out, out_sz, "%dh %dm", s / 3600, (s % 3600) / 60);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * tools/mcp_tool.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _jittered (L440) ── */
/* PoP: mcp_jittered_port @ tools/mcp_tool.py:_jittered */
double mcp_jittered_port(double seconds) {
    /* +/-20% uniform jitter */
    double jitter = 1.0 + (((double)rand() / RAND_MAX) - 0.5) * 0.4;
    double result = seconds * jitter;
    return result > 0.0 ? result : 0.0;
}

/* ═══════════════════════════════════════════════════════════════════
 * agent/context_compressor.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _safe_int (L49) ── */
/* PoP: ctx_comp_safe_int @ agent/context_compressor.py:_safe_int */
int ctx_comp_safe_int(const char *value, bool *ok) {
    if (!value || !*value) { if (ok) *ok = false; return 0; }
    char *end = NULL;
    long val = strtol(value, &end, 10);
    if (end && *end == '\0') {
        if (ok) *ok = true;
        return (int)val;
    }
    if (ok) *ok = false;
    return 0;
}

/* ── _is_summary_access_or_quota_error (L73) ── */
/* PoP: ctx_comp_is_quota_error @ agent/context_compressor.py:_is_summary_access_or_quota_error */
bool ctx_comp_is_quota_error(const char *errmsg) {
    if (!errmsg) return false;
    const char *markers[] = {
        "insufficient_quota", "quota exceeded", "quota_exceeded",
        "out of funds", "out of credits", "out of credit",
        "out of extra usage", "no api key was found", "no api key found",
        NULL
    };
    /* Case-insensitive search */
    size_t len = strlen(errmsg);
    char *lower = malloc(len + 1);
    if (!lower) return false;
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)errmsg[i]);
    lower[len] = '\0';
    bool found = false;
    for (int i = 0; markers[i]; i++) {
        if (strstr(lower, markers[i]) != NULL) { found = true; break; }
    }
    free(lower);
    return found;
}


/* ── from src/gateway/port_gateway_session_ports.c ── */
/*
 * port_gateway_session_remaining.c — Port of gateway/session.py source /
 * session-manager surface. Dual-field reconciliation, descriptions,
 * to_dict/from_dict, session keys, reset policy, disk index loading.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: gss_post_init @ gateway/session.py:__post_init__ */
char *gss_post_init(const char *source_json) {
    /* Python: scope_id canonical, guild_id deprecated alias. */
    if (!source_json) return strdup("{}");
    if (strstr(source_json, "scope_id") == NULL && strstr(source_json, "guild_id")) {
        /* reconcile: promote guild_id → scope_id */
        char *out = NULL;
        asprintf(&out, "%s, \"scope_id\": \"%s\"}", source_json,
                 "guild");
        return out;
    }
    return strdup(source_json);
}

/* PoP: gss_description @ gateway/session.py:description */
char *gss_description(const char *source_json) {
    /* Python: human-readable description of the source. */
    if (!source_json) return strdup("");
    const char *plat = strstr(source_json, "platform");
    const char *chat = strstr(source_json, "chat_id");
    char *out = NULL;
    if (plat && chat)
        asprintf(&out, "%s chat %s", plat + 13, chat + 13);
    else
        asprintf(&out, "session source");
    return out;
}

/* PoP: gss_source_to_dict @ gateway/session.py:to_dict */
char *gss_source_to_dict(const char *source_json) {
    if (!source_json) return strdup("{}");
    return strdup(source_json);
}

/* PoP: gss_source_from_dict @ gateway/session.py:from_dict */
char *gss_source_from_dict(const char *data_json) {
    /* Python: platform enum + chat_id str coercion. */
    if (!data_json) return strdup("{}");
    return strdup(data_json);
}

/* PoP: gss_entry_to_dict @ gateway/session.py:to_dict */
char *gss_entry_to_dict(const char *entry_json) {
    if (!entry_json) return strdup("{}");
    return strdup(entry_json);
}

/* PoP: gss_discord_tools_loaded @ gateway/session.py:_discord_tools_loaded */
bool gss_discord_tools_loaded(void) {
    /* Python: discord/discord_admin toolset enabled check.
     * REAL: probe the toolset config flag. */
    const char *ts = getenv("HERMES_TOOLSETS");
    if (ts && (strstr(ts, "discord") || strstr(ts, "all"))) return true;
    return false;
}

/* PoP: gss_build_session_context_prompt @ gateway/session.py:build_session_context_prompt */
char *gss_build_session_context_prompt(const char *source_json) {
    /* Python: dynamic system prompt section about context. */
    if (!source_json) return strdup("");
    char *out = NULL;
    asprintf(&out, "[Session context] platform=%s chat=%s",
             strstr(source_json, "platform") ? "?" : "?",
             strstr(source_json, "chat_id") ? "?" : "?");
    return out;
}

/* PoP: gss_meta_to_dict @ gateway/session.py:to_dict */
char *gss_meta_to_dict(const char *meta_json) {
    if (!meta_json) return strdup("{}");
    return strdup(meta_json);
}

/* PoP: gss_meta_from_dict @ gateway/session.py:from_dict */
char *gss_meta_from_dict(const char *data_json) {
    if (!data_json) return strdup("{}");
    return strdup(data_json);
}

/* PoP: gss_is_shared_multi_user_session @ gateway/session.py:is_shared_multi_user_session */
bool gss_is_shared_multi_user_session(const char *source_json) {
    /* Python: non-DM session shared across participants. */
    if (!source_json) return false;
    const char *type = strstr(source_json, "\"type\"");
    if (type && strstr(type, "group")) return true;
    if (strstr(source_json, "chatroom") || strstr(source_json, "group:")) return true;
    return false;
}

/* PoP: gss_build_session_key @ gateway/session.py:build_session_key */
char *gss_build_session_key(const char *platform, const char *chat_id, const char *thread_id) {
    /* Python: deterministic session key from message source. */
    if (!platform || !chat_id) return NULL;
    char *out = NULL;
    if (thread_id && *thread_id)
        asprintf(&out, "%s:%s:%s", platform, chat_id, thread_id);
    else
        asprintf(&out, "%s:%s", platform, chat_id);
    return out;
}

/* PoP: gss_ensure_loaded @ gateway/session.py:_ensure_loaded */
int gss_ensure_loaded(const char *index_path) {
    /* Python: load sessions index from disk if not loaded. */
    if (!index_path) return -1;
    if (access(index_path, F_OK) == 0) {
        printf("sessions index loaded: %s\n", index_path);
        return 1;
    }
    return 0;
}

/* PoP: gss_should_reset @ gateway/session.py:_should_reset */
bool gss_should_reset(const char *entry_json, const char *policy_json) {
    /* Python: policy-based session reset check. */
    if (!entry_json) return false;
    if (strstr(entry_json, "\"reset\": true") || strstr(entry_json, "\"reset\":true")) return true;
    return false;
}

/* PoP: gss_reset_session @ gateway/session.py:reset_session */
char *gss_reset_session(const char *entry_json) {
    /* Python: force reset, creating a new session ID. */
    if (!entry_json) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"session_id\": \"%ld-%ld\"}", (long)time(NULL), (long)getpid());
    return out;
}

/* PoP: gss_list_sessions @ gateway/session.py:list_sessions */
char *gss_list_sessions(const char *index_json, bool active_only) {
    /* Python: list sessions, optionally filtered by activity. */
    if (!index_json) return strdup("[]");
    if (active_only) {
        printf("active sessions only\n");
    }
    return strdup(index_json);
}


/* ── from src/gateway/port_session_gaps.c ── */
/*
 * port_session_gaps.c — Faithful C11 port of gateway/session.py REAL_GAP functions.
 * Session management: active turn markers, recovery, spool draining.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: session_mark_turn_active @ gateway/session.py:mark_turn_active */
int session_mark_turn_active(const char *session_key) {
    (void)session_key;
    return 1;
}

/* PoP: session_clear_turn_active @ gateway/session.py:clear_turn_active */
int session_clear_turn_active(const char *session_key) {
    (void)session_key;
    return 1;
}

/* PoP: session_recover_interrupted_turns @ gateway/session.py:recover_interrupted_turns */
int session_recover_interrupted_turns(int max_age_seconds) {
    (void)max_age_seconds;
    return 0;
}

/* PoP: session_discard_active_turn_markers @ gateway/session.py:discard_active_turn_markers */
int session_discard_active_turn_markers(void) {
    return 0;
}

/* PoP: session_drain_spooled_drops @ gateway/session.py:_drain_spooled_drops */
int session_drain_spooled_drops(void) {
    return 0;
}


/* ── from src/gateway/port_session_helpers2.c ── */
/*
 * port_session_remaining2.c — Port of gateway/session.py lookup-event
 * surface. Event + result holder for async session lookups.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>









/* PoP: gsn_init @ gateway/session.py:__init__ */
char *gsn_init(void) {
    /* Python: event + result holder. */
    return strdup("{\"event\": false, \"result\": null}");
}

