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

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

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
/* PoP: mcp_jittered @ tools/mcp_tool.py:_jittered */
double mcp_jittered(double seconds) {
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
