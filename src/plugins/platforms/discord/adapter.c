/*
 * adapter.c — C11 port of upstream Hermes plugins/platforms/discord/adapter.py
 *            (gate family w27_adapter_1f12c3_p1 .. _p14).
 *
 * The truth module is the 380 KB Discord platform adapter (391 defs).  This
 * file ports its PURE decision surface — every function/method whose result is
 * a function of its arguments (plus the *values* its seams would have read):
 *
 *   PORTED  markdown/link shaping (_format_discord_markdown_link,
 *           _image_ext_from_content_type), UTF-16 component truncation
 *           (_truncate_discord_component_text), snowflake decoding
 *           (_discord_snowflake_time), non-conversational classifiers
 *           (_metadata_marks_nonconversational,
 *           _looks_like_nonconversational_history_message), pasted-id and
 *           mention parsing (_clean_discord_id, _raw_mentioned_user_ids),
 *           timeout/backoff math (_discord_ready_timeout_seconds,
 *           _text_batch_flush_deadline_seconds, _bot_tag_window_seconds), the
 *           liveness-knob resolver (_config_value/_liveness_knob), the
 *           missed-message-backfill numbers, the slash-command payload
 *           canonicaliser, the allow/deny gate readers (_extra_or_env_flag,
 *           _gate_raw, _gate_csv_set, ...), channel/thread key builders
 *           (_discord_channel_keys_from_channel, _bot_tag_debounce_key,
 *           _message_channel_ids), thread-title derivation, the connect-error
 *           classifier, the retry-after/rate-limit predicates, the
 *           authorization predicate (_is_allowed_user) and the YAML→extra
 *           seeder (_apply_yaml_config).
 *
 *   SEAMED  discord.py objects and the gateway socket, aiohttp REST and the
 *           standalone sender, the SQLite recovery store, the on-disk state
 *           files, the localization catalog (t()) and config-file reads.  A
 *           ported function takes the seam's *value* as a parameter (env
 *           string, extra dict, epoch seconds, localized default) so its
 *           decision is reproducible without the seam.  The i18n-dependent
 *           defaults and the HTTP-date branch of parse_retry_after_seconds
 *           are the two documented behaviour boundaries; every fixture keeps
 *           to the domain where the parameter IS the live value.
 *
 * PoP (one per ported symbol, 1:1 with the definitions in this file):
 * PoP: plugins_platforms_discord_adapter_bot_tag_debounce_key @ plugins/platforms/discord/adapter.py:_bot_tag_debounce_key
 * PoP: plugins_platforms_discord_adapter_bot_tag_window_seconds @ plugins/platforms/discord/adapter.py:_bot_tag_window_seconds
 * PoP: plugins_platforms_discord_adapter_discord_allow_all_users @ plugins/platforms/discord/adapter.py:_discord_allow_all_users
 * PoP: plugins_platforms_discord_adapter_gateway_allow_all_users @ plugins/platforms/discord/adapter.py:_gateway_allow_all_users
 * PoP: plugins_platforms_discord_adapter_get_allow_bots @ plugins/platforms/discord/adapter.py:_get_allow_bots
 * PoP: plugins_platforms_discord_adapter_resolved_allowlist_user_ids @ plugins/platforms/discord/adapter.py:DiscordAdapter.resolved_allowlist_user_ids
 * PoP: plugins_platforms_discord_adapter_discord_free_response_channels @ plugins/platforms/discord/adapter.py:_discord_free_response_channels
 * PoP: plugins_platforms_discord_adapter_is_bot_tag_debounce_continuation @ plugins/platforms/discord/adapter.py:_is_bot_tag_debounce_continuation
 * PoP: plugins_platforms_discord_adapter_raw_mentioned_user_ids @ plugins/platforms/discord/adapter.py:_raw_mentioned_user_ids
 * PoP: plugins_platforms_discord_adapter_record_bot_tag_debounce @ plugins/platforms/discord/adapter.py:_record_bot_tag_debounce
 * PoP: plugins_platforms_discord_adapter_self_is_raw_mentioned @ plugins/platforms/discord/adapter.py:_self_is_raw_mentioned
 * PoP: plugins_platforms_discord_adapter_discord_bots_require_inline_mention @ plugins/platforms/discord/adapter.py:_discord_bots_require_inline_mention
 * PoP: plugins_platforms_discord_adapter_discord_channel_keys @ plugins/platforms/discord/adapter.py:_discord_channel_keys
 * PoP: plugins_platforms_discord_adapter_discord_channel_keys_from_channel @ plugins/platforms/discord/adapter.py:_discord_channel_keys_from_channel
 * PoP: plugins_platforms_discord_adapter_discord_history_backfill @ plugins/platforms/discord/adapter.py:_discord_history_backfill
 * PoP: plugins_platforms_discord_adapter_discord_history_backfill_limit @ plugins/platforms/discord/adapter.py:_discord_history_backfill_limit
 * PoP: plugins_platforms_discord_adapter_discord_thread_require_mention @ plugins/platforms/discord/adapter.py:_discord_thread_require_mention
 * PoP: plugins_platforms_discord_adapter_approval_mention_content @ plugins/platforms/discord/adapter.py:_approval_mention_content
 * PoP: plugins_platforms_discord_adapter_forum_thread_parts @ plugins/platforms/discord/adapter.py:_forum_thread_parts
 * PoP: plugins_platforms_discord_adapter_self_contained_prompt_content @ plugins/platforms/discord/adapter.py:_self_contained_prompt_content
 * PoP: plugins_platforms_discord_adapter_thread_created @ plugins/platforms/discord/adapter.py:_thread_created
 * PoP: plugins_platforms_discord_adapter_apply_yaml_config @ plugins/platforms/discord/adapter.py:_apply_yaml_config
 * PoP: plugins_platforms_discord_adapter_derive_forum_thread_name @ plugins/platforms/discord/adapter.py:_derive_forum_thread_name
 * PoP: plugins_platforms_discord_adapter_standalone_response_encoding @ plugins/platforms/discord/adapter.py:_standalone_response_encoding
 * PoP: plugins_platforms_discord_adapter_standalone_warn_missing_media @ plugins/platforms/discord/adapter.py:_standalone_warn_missing_media
 * PoP: plugins_platforms_discord_adapter_clean_discord_id @ plugins/platforms/discord/adapter.py:_clean_discord_id
 * PoP: plugins_platforms_discord_adapter_discord_ready_timeout_seconds @ plugins/platforms/discord/adapter.py:_discord_ready_timeout_seconds
 * PoP: plugins_platforms_discord_adapter_discord_snowflake_time @ plugins/platforms/discord/adapter.py:_discord_snowflake_time
 * PoP: plugins_platforms_discord_adapter_env_bool @ plugins/platforms/discord/adapter.py:_env_bool
 * PoP: plugins_platforms_discord_adapter_format_discord_markdown_link @ plugins/platforms/discord/adapter.py:_format_discord_markdown_link
 * PoP: plugins_platforms_discord_adapter_format_privileged_intents_guidance @ plugins/platforms/discord/adapter.py:_format_privileged_intents_guidance
 * PoP: plugins_platforms_discord_adapter_image_ext_from_content_type @ plugins/platforms/discord/adapter.py:_image_ext_from_content_type
 * PoP: plugins_platforms_discord_adapter_is_discord_transport_error @ plugins/platforms/discord/adapter.py:_is_discord_transport_error
 * PoP: plugins_platforms_discord_adapter_looks_like_nonconversational_history_message @ plugins/platforms/discord/adapter.py:_looks_like_nonconversational_history_message
 * PoP: plugins_platforms_discord_adapter_metadata_marks_nonconversational @ plugins/platforms/discord/adapter.py:_metadata_marks_nonconversational
 * PoP: plugins_platforms_discord_adapter_needs_server_members_intent @ plugins/platforms/discord/adapter.py:_needs_server_members_intent
 * PoP: plugins_platforms_discord_adapter_prompt_target_id @ plugins/platforms/discord/adapter.py:_prompt_target_id
 * PoP: plugins_platforms_discord_adapter_truncate_discord_component_text @ plugins/platforms/discord/adapter.py:_truncate_discord_component_text
 * PoP: plugins_platforms_discord_adapter_config_int @ plugins/platforms/discord/adapter.py:_config_int
 * PoP: plugins_platforms_discord_adapter_config_value @ plugins/platforms/discord/adapter.py:_config_value
 * PoP: plugins_platforms_discord_adapter_finite_positive_config_float @ plugins/platforms/discord/adapter.py:_finite_positive_config_float
 * PoP: plugins_platforms_discord_adapter_format_tool_preview @ plugins/platforms/discord/adapter.py:DiscordAdapter.format_tool_preview
 * PoP: plugins_platforms_discord_adapter_tracker_contains @ plugins/platforms/discord/adapter.py:_DiscordNonConversationalMessageTracker.__contains__
 * PoP: plugins_platforms_discord_adapter_voice_receiver_dave_downgraded @ plugins/platforms/discord/adapter.py:VoiceReceiver._dave_downgraded
 * PoP: plugins_platforms_discord_adapter_classify_connect_exception @ plugins/platforms/discord/adapter.py:_classify_connect_exception
 * PoP: plugins_platforms_discord_adapter_command_sync_mutation_interval_seconds @ plugins/platforms/discord/adapter.py:_command_sync_mutation_interval_seconds
 * PoP: plugins_platforms_discord_adapter_command_sync_skip_reason @ plugins/platforms/discord/adapter.py:_command_sync_skip_reason
 * PoP: plugins_platforms_discord_adapter_command_sync_state_key @ plugins/platforms/discord/adapter.py:_command_sync_state_key
 * PoP: plugins_platforms_discord_adapter_extract_discord_retry_after @ plugins/platforms/discord/adapter.py:_extract_discord_retry_after
 * PoP: plugins_platforms_discord_adapter_is_discord_rate_limit @ plugins/platforms/discord/adapter.py:_is_discord_rate_limit
 * PoP: plugins_platforms_discord_adapter_is_discord_unknown_interaction @ plugins/platforms/discord/adapter.py:_is_discord_unknown_interaction
 * PoP: plugins_platforms_discord_adapter_text_batch_flush_deadline_seconds @ plugins/platforms/discord/adapter.py:_text_batch_flush_deadline_seconds
 * PoP: plugins_platforms_discord_adapter_is_down_notice_content @ plugins/platforms/discord/adapter.py:_is_down_notice_content
 * PoP: plugins_platforms_discord_adapter_message_channel_ids @ plugins/platforms/discord/adapter.py:_message_channel_ids
 * PoP: plugins_platforms_discord_adapter_missed_message_backfill_enabled @ plugins/platforms/discord/adapter.py:_missed_message_backfill_enabled
 * PoP: plugins_platforms_discord_adapter_missed_message_backfill_number @ plugins/platforms/discord/adapter.py:_missed_message_backfill_number
 * PoP: plugins_platforms_discord_adapter_canonicalize_app_command_option @ plugins/platforms/discord/adapter.py:_canonicalize_app_command_option
 * PoP: plugins_platforms_discord_adapter_canonicalize_app_command_payload @ plugins/platforms/discord/adapter.py:_canonicalize_app_command_payload
 * PoP: plugins_platforms_discord_adapter_existing_command_to_payload @ plugins/platforms/discord/adapter.py:_existing_command_to_payload
 * PoP: plugins_platforms_discord_adapter_get_discord_command_sync_policy @ plugins/platforms/discord/adapter.py:_get_discord_command_sync_policy
 * PoP: plugins_platforms_discord_adapter_is_length_overflow_error @ plugins/platforms/discord/adapter.py:_is_length_overflow_error
 * PoP: plugins_platforms_discord_adapter_is_reply_reference_rejected @ plugins/platforms/discord/adapter.py:_is_reply_reference_rejected
 * PoP: plugins_platforms_discord_adapter_normalize_permissions @ plugins/platforms/discord/adapter.py:_normalize_permissions
 * PoP: plugins_platforms_discord_adapter_patchable_app_command_payload @ plugins/platforms/discord/adapter.py:_patchable_app_command_payload
 * PoP: plugins_platforms_discord_adapter_discord_channel_ids_allowed @ plugins/platforms/discord/adapter.py:_discord_channel_ids_allowed
 * PoP: plugins_platforms_discord_adapter_load_discord_int_config @ plugins/platforms/discord/adapter.py:_load_discord_int_config
 * PoP: plugins_platforms_discord_adapter_load_playback_timeout @ plugins/platforms/discord/adapter.py:_load_playback_timeout
 * PoP: plugins_platforms_discord_adapter_load_voice_fx_config @ plugins/platforms/discord/adapter.py:_load_voice_fx_config
 * PoP: plugins_platforms_discord_adapter_load_voice_timeout @ plugins/platforms/discord/adapter.py:_load_voice_timeout
 * PoP: plugins_platforms_discord_adapter_playback_timeout_limit @ plugins/platforms/discord/adapter.py:_playback_timeout_limit
 * PoP: plugins_platforms_discord_adapter_voice_mixer_active @ plugins/platforms/discord/adapter.py:DiscordAdapter.voice_mixer_active
 * PoP: plugins_platforms_discord_adapter_voice_timeout_limit @ plugins/platforms/discord/adapter.py:_voice_timeout_limit
 * PoP: plugins_platforms_discord_adapter_warn_if_fail_closed_default @ plugins/platforms/discord/adapter.py:_warn_if_fail_closed_default
 * PoP: plugins_platforms_discord_adapter_attachment_message_type @ plugins/platforms/discord/adapter.py:_attachment_message_type
 * PoP: plugins_platforms_discord_adapter_derive_auto_thread_name @ plugins/platforms/discord/adapter.py:_derive_auto_thread_name
 * PoP: plugins_platforms_discord_adapter_get_effective_topic @ plugins/platforms/discord/adapter.py:_get_effective_topic
 * PoP: plugins_platforms_discord_adapter_get_parent_channel_id @ plugins/platforms/discord/adapter.py:_get_parent_channel_id
 * PoP: plugins_platforms_discord_adapter_guild_member_has_role @ plugins/platforms/discord/adapter.py:_guild_member_has_role
 * PoP: plugins_platforms_discord_adapter_interaction_guild_id @ plugins/platforms/discord/adapter.py:_interaction_guild_id
 * PoP: plugins_platforms_discord_adapter_is_allowed_user @ plugins/platforms/discord/adapter.py:_is_allowed_user
 * PoP: plugins_platforms_discord_adapter_is_forum_parent @ plugins/platforms/discord/adapter.py:_is_forum_parent
 * PoP: plugins_platforms_discord_adapter_discord_free_response_auto_thread @ plugins/platforms/discord/adapter.py:_discord_free_response_auto_thread
 * PoP: plugins_platforms_discord_adapter_discord_max_attachment_bytes @ plugins/platforms/discord/adapter.py:_discord_max_attachment_bytes
 * PoP: plugins_platforms_discord_adapter_discord_require_mention @ plugins/platforms/discord/adapter.py:_discord_require_mention
 * PoP: plugins_platforms_discord_adapter_extra_or_env_flag @ plugins/platforms/discord/adapter.py:_extra_or_env_flag
 * PoP: plugins_platforms_discord_adapter_is_discord_voice_message_attachment @ plugins/platforms/discord/adapter.py:_is_discord_voice_message_attachment
 * PoP: plugins_platforms_discord_adapter_missed_message_backfill_channels @ plugins/platforms/discord/adapter.py:_missed_message_backfill_channels
 * PoP: plugins_platforms_discord_adapter_gate_csv_set @ plugins/platforms/discord/adapter.py:_gate_csv_set
 * PoP: plugins_platforms_discord_adapter_gate_env @ plugins/platforms/discord/adapter.py:_gate_env
 * PoP: plugins_platforms_discord_adapter_gate_raw @ plugins/platforms/discord/adapter.py:_gate_raw
 * PoP: plugins_platforms_discord_adapter_get_allowed_channels @ plugins/platforms/discord/adapter.py:_get_allowed_channels
 * PoP: plugins_platforms_discord_adapter_get_ignored_channels @ plugins/platforms/discord/adapter.py:_get_ignored_channels
 * PoP: plugins_platforms_discord_adapter_get_no_thread_channels @ plugins/platforms/discord/adapter.py:_get_no_thread_channels
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "hermes_json.h"

/* ── small utilities ─────────────────────────────────────────────────── */

typedef struct { char *buf; size_t len, cap; } w27a1f_sb;

static char *w27a1f_strdup(const char *s)
{
    size_t n = s ? strlen(s) : 0;
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    if (n) memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static void w27a1f_sb_init(w27a1f_sb *b)
{
    b->cap = 128; b->len = 0; b->buf = (char *)malloc(b->cap);
    if (b->buf) b->buf[0] = '\0';
}

static void w27a1f_sb_putn(w27a1f_sb *b, const char *s, size_t n)
{
    if (!b->buf || !s) return;
    if (b->len + n + 1 > b->cap) {
        size_t c = b->cap ? b->cap : 128;
        char *nb;
        while (c < b->len + n + 1) c *= 2;
        nb = (char *)realloc(b->buf, c);
        if (!nb) return;
        b->buf = nb; b->cap = c;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void w27a1f_sb_puts(w27a1f_sb *b, const char *s)
{
    if (s) w27a1f_sb_putn(b, s, strlen(s));
}

static void w27a1f_sb_putc(w27a1f_sb *b, char c) { w27a1f_sb_putn(b, &c, 1); }

static char *w27a1f_sb_take(w27a1f_sb *b)
{
    char *r = b->buf;
    b->buf = NULL; b->len = b->cap = 0;
    return r ? r : w27a1f_strdup("");
}

static void w27a1f_sb_free(w27a1f_sb *b) { free(b->buf); b->buf = NULL; b->len = b->cap = 0; }


/* ── json helpers (Python-equality flavoured) ────────────────────────── */

static int w27a1f_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int w27a1f_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int w27a1f_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int w27a1f_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int w27a1f_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int w27a1f_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static const char *w27a1f_str_or(const json_t *v, const char *dflt)
{
    if (w27a1f_is_str(v) && v->str_val) return v->str_val;
    return dflt;
}

/* Python truthiness of a JSON value (absent == null == None). */
static int w27a1f_truthy(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count > 0;
    default:          return 0;
    }
}

/* Python str(value) for the scalar domain this module stringifies. */
static char *w27a1f_pystr(const json_t *v)
{
    char tmp[64];
    if (w27a1f_is_null(v)) return w27a1f_strdup("None");
    if (w27a1f_is_bool(v)) return w27a1f_strdup(v->bool_val ? "True" : "False");
    if (w27a1f_is_num(v)) {
        double d = v->num_val;
        long long ll = (long long)d;
        if (d > -1e15 && d < 1e15 && (double)ll == d) snprintf(tmp, sizeof tmp, "%lld", ll);
        else snprintf(tmp, sizeof tmp, "%.17g", d);
        return w27a1f_strdup(tmp);
    }
    if (w27a1f_is_str(v)) return w27a1f_strdup(v->str_val ? v->str_val : "");
    return w27a1f_strdup("");
}

/* json string from a malloc'd C string (takes ownership). */
static json_t *w27a1f_json_own(char *s)
{
    json_t *r;
    if (!s) return json_null();
    r = json_string(s);
    free(s);
    return r;
}

static json_t *w27a1f_json_strn(const char *s, size_t n)
{
    char *t = (char *)malloc(n + 1);
    json_t *r;
    if (!t) return json_null();
    if (n) memcpy(t, s, n);
    t[n] = '\0';
    r = json_string(t);
    free(t);
    return r;
}

static long long w27a1f_int_or(const json_t *v, long long dflt)
{
    if (w27a1f_is_num(v)) return (long long)v->num_val;
    if (w27a1f_is_bool(v)) return v->bool_val ? 1 : 0;
    return dflt;
}

static double w27a1f_num_or(const json_t *v, double dflt)
{
    if (w27a1f_is_num(v)) return v->num_val;
    if (w27a1f_is_bool(v)) return v->bool_val ? 1.0 : 0.0;
    return dflt;
}

/* str.strip() over ASCII whitespace, in place from the left. */
static const char *w27a1f_lstrip(const char *s)
{
    while (s && *s && isspace((unsigned char)*s)) s++;
    return s ? s : "";
}

static char *w27a1f_strip_dup(const char *s)
{
    size_t n;
    char *r;
    s = w27a1f_lstrip(s);
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    r = (char *)malloc(n + 1);
    if (!r) return NULL;
    if (n) memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static int w27a1f_ci_prefix(const char *s, const char *lit)
{
    while (*lit) {
        if (!*s) return 0;
        if (tolower((unsigned char)*s) != tolower((unsigned char)*lit)) return 0;
        s++; lit++;
    }
    return 1;
}

static int w27a1f_all_digits(const char *s)
{
    if (!s || !*s) return 0;
    while (*s) { if (!isdigit((unsigned char)*s)) return 0; s++; }
    return 1;
}

static int w27a1f_in_list_ci(const char *s, const char *const *items, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (!strcmp(s, items[i])) return 1;
    return 0;
}

/* UTF-8 next code point. */
static unsigned w27a1f_cp_at(const char *s, size_t i, size_t *lenout)
{
    const unsigned char *p = (const unsigned char *)s + i;
    unsigned cp; size_t n, k;
    if (p[0] < 0x80) { cp = p[0]; n = 1; }
    else if ((p[0] & 0xE0) == 0xC0) { cp = p[0] & 0x1Fu; n = 2; }
    else if ((p[0] & 0xF0) == 0xE0) { cp = p[0] & 0x0Fu; n = 3; }
    else if ((p[0] & 0xF8) == 0xF0) { cp = p[0] & 0x07u; n = 4; }
    else { cp = p[0]; n = 1; }
    for (k = 1; k < n; k++) {
        if ((p[k] & 0xC0) != 0x80) { n = k; break; }
        cp = (cp << 6) | (unsigned)(p[k] & 0x3F);
    }
    *lenout = n;
    return cp;
}

static int w27a1f_dbl_finite(double v)
{
    return v == v && v != INFINITY && v != -INFINITY;
}

/* ── gate p1: module-level pure helpers (adapter.py:44-660) ──────────── */

json_t *w27_adapter_1f12c3_p1_image_ext_from_content_type(const json_t *content_type)
{
    const char *s = w27a1f_str_or(content_type, "");
    if (strstr(s, "jpeg") || strstr(s, "jpg")) return json_string("jpg");
    if (strstr(s, "gif")) return json_string("gif");
    if (strstr(s, "webp")) return json_string("webp");
    return json_string("png");
}

static int w27a1f_quote_safe(unsigned char c)
{
    if (c < 0x80 && (isalnum(c) || strchr("_.-~", c))) return 1;
    return strchr(":/?#[]@!$&'*+,;=%", c) != NULL && c != '\0';
}

json_t *w27_adapter_1f12c3_p1_format_discord_markdown_link(const json_t *label, const json_t *url)
{
    const char *lab = w27a1f_str_or(label, "");
    const char *u = w27a1f_str_or(url, "");
    w27a1f_sb out;
    size_t i;

    if (w27a1f_ci_prefix(lab, "http://")) lab += 7;
    else if (w27a1f_ci_prefix(lab, "https://")) lab += 8;

    w27a1f_sb_init(&out);
    w27a1f_sb_putc(&out, '[');
    for (i = 0; lab[i]; i++) {
        if (lab[i] == '\\' || lab[i] == '[' || lab[i] == ']') w27a1f_sb_putc(&out, '\\');
        w27a1f_sb_putc(&out, lab[i]);
    }
    w27a1f_sb_puts(&out, "](<");
    for (i = 0; u[i]; i++) {
        unsigned char c = (unsigned char)u[i];
        if (w27a1f_quote_safe(c)) {
            w27a1f_sb_putc(&out, (char)c);
        } else {
            char enc[4];
            snprintf(enc, sizeof enc, "%%%02X", c);
            w27a1f_sb_puts(&out, enc);
        }
    }
    w27a1f_sb_puts(&out, ">)");
    return w27a1f_json_own(w27a1f_sb_take(&out));
}

json_t *w27_adapter_1f12c3_p1_is_discord_transport_error(const json_t *type_name, const json_t *text)
{
    static const char *const conn[] = {
        "ConnectionError", "ConnectionResetError", "ConnectionAbortedError",
        "ConnectionRefusedError", "BrokenPipeError", "OSError", "IOError"
    };
    static const char *const markers[] = {
        "websocket closed", "connection reset", "connection closed", "session is closed",
        "cannot write to closing transport", "not connected"
    };
    const char *tn = w27a1f_str_or(type_name, "");
    const char *msg = w27a1f_str_or(text, "");
    size_t i;
    char *low;
    int hit = 0;

    if (!strcmp(tn, "TimeoutError")) return json_bool(false);
    if (w27a1f_in_list_ci(tn, conn, sizeof conn / sizeof conn[0])) return json_bool(true);
    low = w27a1f_strdup(msg);
    if (!low) return json_bool(false);
    for (i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
    for (i = 0; i < sizeof markers / sizeof markers[0]; i++)
        if (strstr(low, markers[i])) { hit = 1; break; }
    free(low);
    return json_bool(hit ? true : false);
}

json_t *w27_adapter_1f12c3_p1_truncate_discord_component_text(const json_t *text, const json_t *limit)
{
    char *s;
    long long lim = w27a1f_int_or(limit, 0);
    size_t i = 0, units = 0;

    if (w27a1f_is_str(text)) s = w27a1f_strdup(text->str_val ? text->str_val : "");
    else if (w27a1f_truthy(text)) s = w27a1f_pystr(text);
    else s = w27a1f_strdup("");
    if (!s) return json_null();
    if (lim < 0) lim = 0;
    while (s[i]) {
        size_t n = 1;
        unsigned cp = w27a1f_cp_at(s, i, &n);
        size_t u = cp > 0xFFFF ? 2u : 1u;
        if ((long long)(units + u) > lim) break;
        units += u;
        i += n;
    }
    {
        json_t *r = w27a1f_json_strn(s, i);
        free(s);
        return r;
    }
}

json_t *w27_adapter_1f12c3_p1_needs_server_members_intent(const json_t *allowed_user_ids,
                                                         const json_t *allowed_role_ids)
{
    size_t i;
    if (w27a1f_is_arr(allowed_user_ids)) {
        for (i = 0; i < json_len(allowed_user_ids); i++) {
            json_t *e = json_get(allowed_user_ids, i);
            if (!e) continue;
            if (!(w27a1f_is_str(e) && !strcmp(e->str_val ? e->str_val : "", "*"))) {
                char *t = w27a1f_pystr(e);
                int digits = t ? w27a1f_all_digits(t) : 0;
                free(t);
                if (!digits) return json_bool(true);
            }
        }
    }
    return json_bool(w27a1f_truthy(allowed_role_ids) ? true : false);
}

json_t *w27_adapter_1f12c3_p1_format_privileged_intents_guidance(const json_t *needs_members)
{
    w27a1f_sb b;
    w27a1f_sb_init(&b);
    w27a1f_sb_puts(&b, "Discord rejected the connection because privileged Gateway Intents "
                       "are not enabled for this bot in the Developer Portal.");
    w27a1f_sb_putc(&b, '\n');
    w27a1f_sb_puts(&b, "Hermes is requesting:");
    w27a1f_sb_putc(&b, '\n');
    w27a1f_sb_puts(&b, "  - Message Content Intent (required to read message text)");
    if (w27a1f_truthy(needs_members)) {
        w27a1f_sb_putc(&b, '\n');
        w27a1f_sb_puts(&b, "  - Server Members Intent (required for username allowlists "
                           "and/or DISCORD_ALLOWED_ROLES)");
    }
    w27a1f_sb_putc(&b, '\n');
    w27a1f_sb_puts(&b, "Fix: https://discord.com/developers/applications \xE2\x86\x92 your application "
                       "\xE2\x86\x92 Bot \xE2\x86\x92 Privileged Gateway Intents \xE2\x86\x92 enable "
                       "the intent(s) listed above \xE2\x86\x92 Save Changes, then restart the gateway.");
    w27a1f_sb_putc(&b, '\n');
    w27a1f_sb_puts(&b, "Docs: https://hermes-agent.nousresearch.com/docs/user-guide/messaging/discord");
    return w27a1f_json_own(w27a1f_sb_take(&b));
}

static void w27a1f_civil_from_days(long long z, long long *y, int *m, int *d)
{
    long long era, yy;
    unsigned long long doe, yoe, doy, mp, dd, mm;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (unsigned long long)(z - era * 146097);
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    yy = (long long)yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    dd = doy - (153 * mp + 2) / 5 + 1;
    mm = mp + (mp < 10 ? 3 : -9);
    *y = yy + (mm <= 2);
    *m = (int)mm;
    *d = (int)dd;
}

json_t *w27_adapter_1f12c3_p1_discord_snowflake_time(const json_t *snowflake)
{
    long long sf = 0;
    long long ms, days, rem, hh, mm, ss, milli;
    long long y; int mo, dd;
    char buf[80];

    if (w27a1f_is_str(snowflake) && snowflake->str_val) sf = strtoll(snowflake->str_val, NULL, 10);
    else if (w27a1f_is_num(snowflake)) sf = (long long)snowflake->num_val;
    else return json_null();

    ms = (sf >> 22) + 1420070400000LL;
    days = ms / 86400000LL;
    rem = ms - days * 86400000LL;
    if (rem < 0) { days -= 1; rem += 86400000LL; }
    w27a1f_civil_from_days(days, &y, &mo, &dd);
    hh = rem / 3600000LL;
    rem -= hh * 3600000LL;
    mm = rem / 60000LL;
    rem -= mm * 60000LL;
    ss = rem / 1000LL;
    milli = rem - ss * 1000LL;
    if (milli) snprintf(buf, sizeof buf, "%04lld-%02d-%02dT%02lld:%02lld:%02lld.%06lld+00:00",
                        y, mo, dd, hh, mm, ss, milli * 1000LL);
    else snprintf(buf, sizeof buf, "%04lld-%02d-%02dT%02lld:%02lld:%02lld+00:00",
                  y, mo, dd, hh, mm, ss);
    return json_string(buf);
}

json_t *w27_adapter_1f12c3_p1_metadata_marks_nonconversational(const json_t *metadata)
{
    static const char *const keys[] = { "non_conversational", "non_conversational_history" };
    size_t i;
    if (!w27a1f_is_obj(metadata)) return json_bool(false);
    for (i = 0; i < sizeof keys / sizeof keys[0]; i++)
        if (w27a1f_truthy(json_obj_get(metadata, keys[i]))) return json_bool(true);
    return json_bool(false);
}

json_t *w27_adapter_1f12c3_p1_prompt_target_id(const json_t *chat_id, const json_t *metadata)
{
    if (w27a1f_truthy(metadata) && w27a1f_is_obj(metadata)) {
        json_t *tid = json_obj_get(metadata, "thread_id");
        if (w27a1f_truthy(tid)) return json_copy(tid);
    }
    return json_copy(chat_id);
}

/* \s*💾\s*Self-improvement review:\s+\S[\s\S]*$ */
static int w27a1f_m_self_review(const char *s)
{
    s = w27a1f_lstrip(s);
    if (strncmp(s, "\xF0\x9F\x92\xBE", 4)) return 0;
    s = w27a1f_lstrip(s + 4);
    if (!w27a1f_ci_prefix(s, "self-improvement review:")) return 0;
    s += strlen("self-improvement review:");
    if (!*s || !isspace((unsigned char)*s)) return 0;
    s = w27a1f_lstrip(s);
    return *s != '\0';
}

/* \s*💾\s+Skill\s+['"].+?['"]\s+(?:created|updated|improved|patched)\.?\s*$ */
static int w27a1f_m_skill(const char *s)
{
    static const char *const verbs[] = { "created", "updated", "improved", "patched" };
    s = w27a1f_lstrip(s);
    if (strncmp(s, "\xF0\x9F\x92\xBE", 4)) return 0;
    s += 4;
    if (!*s || !isspace((unsigned char)*s)) return 0;
    s = w27a1f_lstrip(s);
    if (!w27a1f_ci_prefix(s, "skill")) return 0;
    s += 5;
    if (!*s || !isspace((unsigned char)*s)) return 0;
    s = w27a1f_lstrip(s);
    if (*s != '\'' && *s != '"') return 0;
    {
        char q = *s++;
        const char *p;
        for (p = s; *p; p++) {
            const char *r;
            size_t i;
            if (*p != q) continue;
            if (p == s) continue;                      /* .+? needs one char */
            r = p + 1;
            if (!*r || !isspace((unsigned char)*r)) continue;
            r = w27a1f_lstrip(r);
            for (i = 0; i < sizeof verbs / sizeof verbs[0]; i++) {
                size_t ln = strlen(verbs[i]);
                if (!w27a1f_ci_prefix(r, verbs[i])) continue;
                r += ln;
                if (*r == '.') r++;
                r = w27a1f_lstrip(r);
                if (!*r) return 1;
            }
        }
    }
    return 0;
}

/* \s*⏳\s+Working\s+—\s+\d+\s+min(?:\s|$) */
static int w27a1f_m_working(const char *s)
{
    s = w27a1f_lstrip(s);
    if (strncmp(s, "\xE2\x8F\xB3", 3)) return 0;
    s += 3;
    if (!*s || !isspace((unsigned char)*s)) return 0;
    s = w27a1f_lstrip(s);
    if (!w27a1f_ci_prefix(s, "working")) return 0;
    s += 7;
    if (!*s || !isspace((unsigned char)*s)) return 0;
    s = w27a1f_lstrip(s);
    if (strncmp(s, "\xE2\x80\x94", 3)) return 0;
    s += 3;
    if (!*s || !isspace((unsigned char)*s)) return 0;
    s = w27a1f_lstrip(s);
    if (!isdigit((unsigned char)*s)) return 0;
    while (isdigit((unsigned char)*s)) s++;
    if (!*s || !isspace((unsigned char)*s)) return 0;
    s = w27a1f_lstrip(s);
    if (!w27a1f_ci_prefix(s, "min")) return 0;
    s += 3;
    return !*s || isspace((unsigned char)*s);
}

/* \s*\[Background process\s+\S+\s+(?:finished with exit code|is still running~)[\s\S]*\]\s*$ */
static int w27a1f_m_background(const char *s)
{
    char *tmp;
    size_t n;
    int ok = 0;
    const char *p;
    s = w27a1f_lstrip(s);
    if (*s != '[') return 0;
    s++;
    if (!w27a1f_ci_prefix(s, "background process")) return 0;
    s += strlen("background process");
    if (!*s || !isspace((unsigned char)*s)) return 0;
    s = w27a1f_lstrip(s);
    if (!*s) return 0;
    while (*s && !isspace((unsigned char)*s)) s++;
    if (!*s) return 0;
    s = w27a1f_lstrip(s);
    tmp = w27a1f_strip_dup(s);
    if (!tmp) return 0;
    n = strlen(tmp);
    if (n == 0 || tmp[n - 1] != ']') { free(tmp); return 0; }
    tmp[n - 1] = '\0';
    p = tmp;
    if (w27a1f_ci_prefix(p, "finished with exit code")) ok = 1;
    else if (w27a1f_ci_prefix(p, "is still running~")) ok = 1;
    free(tmp);
    return ok;
}

/* \s*(?:✅|❌)\s+Hermes update\s+(?:finished|failed|timed out)[\s\S]*$ */
static int w27a1f_m_update(const char *s)
{
    s = w27a1f_lstrip(s);
    if (!strncmp(s, "\xE2\x9C\x85", 3)) s += 3;
    else if (!strncmp(s, "\xE2\x9D\x8C", 3)) s += 3;
    else return 0;
    if (!*s || !isspace((unsigned char)*s)) return 0;
    s = w27a1f_lstrip(s);
    if (!w27a1f_ci_prefix(s, "hermes update")) return 0;
    s += strlen("hermes update");
    if (!*s || !isspace((unsigned char)*s)) return 0;
    s = w27a1f_lstrip(s);
    return w27a1f_ci_prefix(s, "finished") || w27a1f_ci_prefix(s, "failed")
        || w27a1f_ci_prefix(s, "timed out");
}

/* \s*♻️?\s+Gateway\s+(?:restarted successfully|online\b)[\s\S]*$ */
static int w27a1f_m_gateway(const char *s)
{
    s = w27a1f_lstrip(s);
    if (strncmp(s, "\xE2\x99\xBB", 3)) return 0;
    s += 3;
    if (!strncmp(s, "\xEF\xB8\x8F", 3)) s += 3;   /* optional VS16 */
    if (!*s || !isspace((unsigned char)*s)) return 0;
    s = w27a1f_lstrip(s);
    if (!w27a1f_ci_prefix(s, "gateway")) return 0;
    s += 7;
    if (!*s || !isspace((unsigned char)*s)) return 0;
    s = w27a1f_lstrip(s);
    if (w27a1f_ci_prefix(s, "restarted successfully")) return 1;
    if (w27a1f_ci_prefix(s, "online")) {
        s += 6;
        return !*s || !(isalnum((unsigned char)*s) || *s == '_');
    }
    return 0;
}

json_t *w27_adapter_1f12c3_p1_looks_like_nonconversational_history_message(const json_t *content)
{
    const char *s = w27a1f_str_or(content, "");
    int hit = w27a1f_m_self_review(s) || w27a1f_m_skill(s) || w27a1f_m_working(s)
           || w27a1f_m_background(s) || w27a1f_m_update(s) || w27a1f_m_gateway(s);
    return json_bool(hit ? true : false);
}

json_t *w27_adapter_1f12c3_p1_clean_discord_id(const json_t *entry)
{
    char *s = w27a1f_strip_dup(w27a1f_str_or(entry, ""));
    size_t n;
    if (!s) return json_null();
    n = strlen(s);
    if (n >= 3 && s[0] == '<' && s[1] == '@' && s[n - 1] == '>') {
        size_t i = 0, j;
        while (s[i] && (s[i] == '<' || s[i] == '@' || s[i] == '!')) i++;
        memmove(s, s + i, strlen(s + i) + 1);
        j = strlen(s);
        while (j && s[j - 1] == '>') s[--j] = '\0';
    }
    if (strlen(s) >= 5 && tolower((unsigned char)s[0]) == 'u' && tolower((unsigned char)s[1]) == 's'
        && tolower((unsigned char)s[2]) == 'e' && tolower((unsigned char)s[3]) == 'r' && s[4] == ':') {
        memmove(s, s + 5, strlen(s + 5) + 1);
    }
    {
        char *r = w27a1f_strip_dup(s);
        free(s);
        return w27a1f_json_own(r);
    }
}

json_t *w27_adapter_1f12c3_p1_discord_ready_timeout_seconds(const json_t *env_raw)
{
    char *raw = w27a1f_strip_dup(w27a1f_str_or(env_raw, ""));
    if (raw && raw[0]) {
        char *end = NULL;
        double v = strtod(raw, &end);
        if (end && end != raw && *end == '\0') {
            double r = v > 0.0 ? v : 0.0;
            free(raw);
            return json_number(r);
        }
    }
    free(raw);
    return json_number(30.0);
}

json_t *w27_adapter_1f12c3_p1_env_bool(const json_t *env_raw, const json_t *dflt)
{
    static const char *const yes[] = { "true", "1", "yes", "on" };
    char *raw = w27a1f_strip_dup(w27a1f_str_or(env_raw, ""));
    size_t i;
    int hit;
    if (!raw) return json_bool(false);
    for (i = 0; raw[i]; i++) raw[i] = (char)tolower((unsigned char)raw[i]);
    if (!raw[0]) {
        int d = w27a1f_truthy(dflt);
        free(raw);
        return json_bool(d ? true : false);
    }
    hit = w27a1f_in_list_ci(raw, yes, sizeof yes / sizeof yes[0]);
    free(raw);
    return json_bool(hit ? true : false);
}

/* ── gate p2: config/liveness knobs and preview shaping (adapter.py:440-1400) ── */

json_t *w27_adapter_1f12c3_p2_tracker_contains(const json_t *ids, const json_t *message_id)
{
    char *key;
    size_t i;
    if (!w27a1f_truthy(message_id)) key = w27a1f_strdup("");
    else key = w27a1f_pystr(message_id);
    if (!key) return json_bool(false);
    if (w27a1f_is_arr(ids)) {
        for (i = 0; i < json_len(ids); i++) {
            json_t *e = json_get(ids, i);
            if (w27a1f_is_str(e) && e->str_val && !strcmp(e->str_val, key)) {
                free(key);
                return json_bool(true);
            }
        }
    }
    free(key);
    return json_bool(false);
}

json_t *w27_adapter_1f12c3_p2_voice_receiver_dave_downgraded(const json_t *creds)
{
    if (!w27a1f_is_arr(creds) || json_len(creds) < 4) return json_bool(false);
    return json_bool(w27a1f_truthy(json_get(creds, 3)) ? true : false);
}

/* _config_value: extra[key] -> env -> default; None and "" both fall through. */
static json_t *w27a1f_config_value(const json_t *extra, const json_t *key, const json_t *dflt,
                                   const json_t *env_raw)
{
    const char *k = w27a1f_str_or(key, "");
    json_t *value = NULL;
    int owned = 0;

    if (w27a1f_is_obj(extra)) value = json_obj_get(extra, k);
    if (w27a1f_is_null(value) && env_raw) {
        char *env = w27a1f_strip_dup(w27a1f_str_or(env_raw, ""));
        if (env && env[0]) { value = json_string(env); owned = 1; }
        free(env);
    }
    if (w27a1f_is_null(value)) {
        if (owned) json_free(value);
        return json_copy(dflt);
    }
    if (w27a1f_is_str(value) && (!value->str_val || !value->str_val[0])) {
        if (owned) json_free(value);
        return json_copy(dflt);
    }
    {
        json_t *r = json_copy(value);
        if (owned) json_free(value);
        return r;
    }
}

json_t *w27_adapter_1f12c3_p2_config_value(const json_t *extra, const json_t *key,
                                           const json_t *dflt, const json_t *env_raw)
{
    return w27a1f_config_value(extra, key, dflt, env_raw);
}

/* float(raw) with bool rejected, exactly like _liveness_knob. */
static int w27a1f_to_float(const json_t *raw, double *out)
{
    if (!raw || w27a1f_is_null(raw) || w27a1f_is_bool(raw)) return 0;
    if (w27a1f_is_num(raw)) { *out = raw->num_val; return 1; }
    if (w27a1f_is_str(raw) && raw->str_val) {
        char *end = NULL;
        const char *s = w27a1f_lstrip(raw->str_val);
        double v;
        if (!*s) return 0;
        v = strtod(s, &end);
        if (!end || end == s) return 0;
        while (end && *end && isspace((unsigned char)*end)) end++;
        if (!end || *end) return 0;
        *out = v;
        return 1;
    }
    return 0;
}

static json_t *w27a1f_liveness_knob(const json_t *extra, const json_t *key, const json_t *dflt,
                                    const json_t *env_raw, int as_int)
{
    json_t *raw = w27a1f_config_value(extra, key, dflt, env_raw);
    double value;
    int have = w27a1f_to_float(raw, &value);
    json_free(raw);
    if (have && w27a1f_dbl_finite(value) && value >= 0.0
        && (as_int ? ((double)(long long)value == value) : 1)) {
        if (as_int) return json_number((double)(long long)value);
        return json_number(value);
    }
    return json_number(0.0);
}

json_t *w27_adapter_1f12c3_p2_finite_positive_config_float(const json_t *extra, const json_t *key,
                                                           const json_t *dflt, const json_t *env_raw)
{
    return w27a1f_liveness_knob(extra, key, dflt, env_raw, 0);
}

json_t *w27_adapter_1f12c3_p2_config_int(const json_t *extra, const json_t *key,
                                         const json_t *dflt, const json_t *env_raw)
{
    return w27a1f_liveness_knob(extra, key, dflt, env_raw, 1);
}

json_t *w27_adapter_1f12c3_p2_format_tool_preview(const json_t *preview)
{
    json_t *url = w27a1f_is_obj(preview) ? json_obj_get(preview, "url") : NULL;
    json_t *text = w27a1f_is_obj(preview) ? json_obj_get(preview, "text") : NULL;
    if (!w27a1f_truthy(url)) return json_copy(text);
    return w27_adapter_1f12c3_p1_format_discord_markdown_link(text, url);
}

/* ── gate p3: connect classification and command-sync math (adapter.py:1595-2300) ── */

json_t *w27_adapter_1f12c3_p3_classify_connect_exception(const json_t *type_name, const json_t *msg,
                                                         const json_t *shown,
                                                         const json_t *allowed_user_ids,
                                                         const json_t *allowed_role_ids)
{
    const char *tn = w27a1f_str_or(type_name, "");
    const char *text = w27a1f_str_or(msg, "");
    const char *shown_s = w27a1f_truthy(shown) ? w27a1f_str_or(shown, "") : NULL;
    const char *use = shown_s ? shown_s : text;
    json_t *out = json_array();

    if (!strcmp(tn, "LoginFailure")) {
        w27a1f_sb b;
        w27a1f_sb_init(&b);
        w27a1f_sb_puts(&b, "Discord bot token rejected: ");
        w27a1f_sb_puts(&b, use);
        w27a1f_sb_puts(&b, ". The token is invalid or was revoked \xE2\x80\x94 regenerate it in the "
                           "Discord Developer Portal and update DISCORD_BOT_TOKEN.");
        json_append(out, json_string("discord_auth_error"));
        json_append(out, w27a1f_json_own(w27a1f_sb_take(&b)));
        json_append(out, json_bool(false));
        return out;
    }
    if (!strcmp(tn, "PrivilegedIntentsRequired")) {
        json_t *needs = w27_adapter_1f12c3_p1_needs_server_members_intent(allowed_user_ids, allowed_role_ids);
        json_t *guidance = w27_adapter_1f12c3_p1_format_privileged_intents_guidance(needs);
        json_append(out, json_string("discord_intents_required"));
        json_append(out, guidance);
        json_append(out, json_bool(false));
        json_free(needs);
        return out;
    }
    {
        w27a1f_sb b;
        w27a1f_sb_init(&b);
        w27a1f_sb_puts(&b, "Discord startup failed: ");
        w27a1f_sb_puts(&b, use);
        json_append(out, json_string("discord_connect_error"));
        json_append(out, w27a1f_json_own(w27a1f_sb_take(&b)));
        json_append(out, json_bool(true));
    }
    return out;
}

json_t *w27_adapter_1f12c3_p3_text_batch_flush_deadline_seconds(const json_t *env_raw)
{
    double budget = 5.0, headroom, deadline;
    char *raw = w27a1f_strip_dup(w27a1f_str_or(env_raw, ""));
    if (raw && raw[0]) {
        char *end = NULL;
        double v = strtod(raw, &end);
        if (end && end != raw && *end == '\0' && v > 0.0) budget = v;
    }
    free(raw);
    headroom = budget * 0.2;
    if (headroom < 0.5) headroom = 0.5;
    deadline = budget - headroom;
    if (deadline < 1.0) deadline = 1.0;
    return json_number(deadline < budget * 0.9 ? deadline : budget * 0.9);
}

json_t *w27_adapter_1f12c3_p3_command_sync_state_key(const json_t *app_id)
{
    if (!w27a1f_truthy(app_id)) return json_string("unknown");
    return w27a1f_json_own(w27a1f_pystr(app_id));
}

static int w27a1f_to_float2(const json_t *raw, double *out, int allow_bool)
{
    if (!raw || w27a1f_is_null(raw)) return 0;
    if (w27a1f_is_bool(raw)) {
        if (!allow_bool) return 0;
        *out = raw->bool_val ? 1.0 : 0.0;
        return 1;
    }
    if (w27a1f_is_num(raw)) { *out = raw->num_val; return 1; }
    if (w27a1f_is_str(raw) && raw->str_val) {
        char *end = NULL;
        const char *s = w27a1f_lstrip(raw->str_val);
        double v;
        if (!*s) return 0;
        v = strtod(s, &end);
        if (!end || end == s) return 0;
        while (end && *end && isspace((unsigned char)*end)) end++;
        if (!end || *end) return 0;
        *out = v;
        return 1;
    }
    return 0;
}

/* float(entry.get(key) or 0) */
static double w27a1f_entry_float(const json_t *entry, const char *key)
{
    json_t *v = w27a1f_is_obj(entry) ? json_obj_get(entry, key) : NULL;
    double d = 0.0;
    if (!w27a1f_truthy(v)) return 0.0;
    if (w27a1f_to_float2(v, &d, 1)) return d;
    return 0.0;
}

json_t *w27_adapter_1f12c3_p3_command_sync_skip_reason(const json_t *entry, const json_t *fingerprint,
                                                       const json_t *now_v)
{
    double now = w27a1f_num_or(now_v, 0.0);
    double retry_after_until, last_success_at, last_attempt_at;
    if (!w27a1f_is_obj(entry)) return json_null();
    retry_after_until = w27a1f_entry_float(entry, "retry_after_until");
    if (retry_after_until > now) {
        long long remaining = (long long)(retry_after_until - now);
        char buf[128];
        if (remaining < 1) remaining = 1;
        snprintf(buf, sizeof buf,
                 "Discord asked us to wait before syncing slash commands; retry in %llds", remaining);
        return json_string(buf);
    }
    last_success_at = w27a1f_entry_float(entry, "last_success_at");
    last_attempt_at = w27a1f_entry_float(entry, "last_attempt_at");
    {
        json_t *fp = json_obj_get(entry, "fingerprint");
        int same = 0;
        if (w27a1f_is_str(fp) && w27a1f_is_str(fingerprint))
            same = !strcmp(fp->str_val ? fp->str_val : "", fingerprint->str_val ? fingerprint->str_val : "");
        else if (w27a1f_is_num(fp) && w27a1f_is_num(fingerprint))
            same = fp->num_val == fingerprint->num_val;
        else if (w27a1f_is_null(fp) && w27a1f_is_null(fingerprint))
            same = 1;
        if (same && last_success_at != 0.0 && last_success_at >= last_attempt_at)
            return json_string("same slash-command fingerprint already synced");
    }
    return json_null();
}

json_t *w27_adapter_1f12c3_p3_command_sync_mutation_interval_seconds(void)
{
    return json_number(4.5);
}

/* parse_retry_after_seconds over the numeric / numeric-string domain. */
static int w27a1f_retry_seconds(const json_t *v, double *out)
{
    double d;
    if (w27a1f_is_bool(v)) return 0;
    if (w27a1f_is_num(v)) { *out = v->num_val > 0.0 ? v->num_val : 0.0; return 1; }
    if (w27a1f_is_str(v) && v->str_val) {
        char *end = NULL;
        const char *s = w27a1f_lstrip(v->str_val);
        if (!*s) return 0;
        d = strtod(s, &end);
        if (!end || end == s) return 0;
        while (end && *end && isspace((unsigned char)*end)) end++;
        if (!end || *end) return 0;
        *out = d > 0.0 ? d : 0.0;
        return 1;
    }
    return 0;
}

json_t *w27_adapter_1f12c3_p3_extract_discord_retry_after(const json_t *retry_after, const json_t *headers)
{
    double parsed;
    if (w27a1f_retry_seconds(retry_after, &parsed))
        return json_number(parsed > 1.0 ? parsed : 1.0);
    if (w27a1f_is_obj(headers)) {
        json_t *ra = json_obj_get(headers, "Retry-After");
        if (!ra) ra = json_obj_get(headers, "retry-after");
        if (w27a1f_retry_seconds(ra, &parsed)) return json_number(parsed > 1.0 ? parsed : 1.0);
        ra = json_obj_get(headers, "X-RateLimit-Reset-After");
        if (w27a1f_retry_seconds(ra, &parsed)) return json_number(parsed > 1.0 ? parsed : 1.0);
    }
    return json_null();
}

json_t *w27_adapter_1f12c3_p3_is_discord_rate_limit(const json_t *type_name, const json_t *retry_after)
{
    char *name = w27a1f_strdup(w27a1f_str_or(type_name, ""));
    size_t i;
    int hit = 0;
    if (!name) return json_bool(false);
    for (i = 0; name[i]; i++) name[i] = (char)tolower((unsigned char)name[i]);
    if ((strstr(name, "ratelimit") || strstr(name, "rate_limit")) && !w27a1f_is_null(retry_after))
        hit = 1;
    free(name);
    return json_bool(hit ? true : false);
}

json_t *w27_adapter_1f12c3_p3_is_discord_unknown_interaction(const json_t *code_v, const json_t *data_code,
                                                             const json_t *status_v, const json_t *msg)
{
    long long code = 0, status = 0;
    int have_code = 0, have_status = 0;
    char *text;

    if (w27a1f_is_num(code_v)) { code = (long long)code_v->num_val; have_code = 1; }
    else if (w27a1f_is_str(code_v) && code_v->str_val && w27a1f_all_digits(code_v->str_val)) {
        code = strtoll(code_v->str_val, NULL, 10);
        have_code = 1;
    } else if (w27a1f_is_null(code_v)) {
        if (w27a1f_is_num(data_code)) { code = (long long)data_code->num_val; have_code = 1; }
        else if (w27a1f_is_str(data_code) && data_code->str_val && w27a1f_all_digits(data_code->str_val)) {
            code = strtoll(data_code->str_val, NULL, 10);
            have_code = 1;
        }
    }
    if (w27a1f_is_num(status_v)) { status = (long long)status_v->num_val; have_status = 1; }
    else if (w27a1f_is_str(status_v) && status_v->str_val && w27a1f_all_digits(status_v->str_val)) {
        status = strtoll(status_v->str_val, NULL, 10);
        have_status = 1;
    }
    if (have_code && code == 10062) return json_bool(true);
    text = w27a1f_strdup(w27a1f_str_or(msg, ""));
    if (!text) return json_bool(false);
    {
        size_t i;
        int hit = 0;
        for (i = 0; text[i]; i++) text[i] = (char)tolower((unsigned char)text[i]);
        hit = strstr(text, "unknown interaction") != NULL;
        free(text);
        return json_bool((have_status && status == 404 && hit) ? true : false);
    }
}

/* ── gate p4: missed-message backfill policy (adapter.py:2370-2730) ───── */

static json_t *w27a1f_backfill_cfg(const json_t *extra)
{
    return w27a1f_is_obj(extra) ? json_obj_get(extra, "missed_message_backfill") : NULL;
}

static int w27a1f_is_yes_word(const char *raw_in)
{
    static const char *const yes[] = { "true", "1", "yes", "on" };
    char *raw = w27a1f_strip_dup(raw_in ? raw_in : "");
    size_t i;
    int hit;
    if (!raw) return 0;
    for (i = 0; raw[i]; i++) raw[i] = (char)tolower((unsigned char)raw[i]);
    hit = w27a1f_in_list_ci(raw, yes, sizeof yes / sizeof yes[0]);
    free(raw);
    return hit;
}

json_t *w27_adapter_1f12c3_p4_missed_message_backfill_enabled(const json_t *extra, const json_t *env_raw)
{
    json_t *cfg = w27a1f_backfill_cfg(extra);
    if (w27a1f_is_obj(cfg) && json_has(cfg, "enabled")) {
        json_t *value = json_obj_get(cfg, "enabled");
        if (w27a1f_is_str(value)) return json_bool(w27a1f_is_yes_word(value->str_val) ? true : false);
        return json_bool(w27a1f_truthy(value) ? true : false);
    }
    return json_bool(w27a1f_is_yes_word(w27a1f_str_or(env_raw, "false")) ? true : false);
}

/* cast(raw) with Python's int()/float() domain; 0 when it would raise. */
static int w27a1f_cast_num(const json_t *raw, int as_int, double *out)
{
    double d;
    if (!raw || w27a1f_is_null(raw)) return 0;
    if (w27a1f_is_bool(raw)) { *out = raw->bool_val ? 1.0 : 0.0; return 1; }
    if (w27a1f_is_num(raw)) d = raw->num_val;
    else if (w27a1f_is_str(raw) && raw->str_val) {
        char *end = NULL;
        const char *s = w27a1f_lstrip(raw->str_val);
        if (!*s) return 0;
        d = strtod(s, &end);
        if (!end || end == s) return 0;
        while (end && *end && isspace((unsigned char)*end)) end++;
        if (!end || *end) return 0;
    } else return 0;
    *out = as_int ? (double)(long long)d : d;
    return 1;
}

json_t *w27_adapter_1f12c3_p4_missed_message_backfill_number(const json_t *extra, const json_t *key,
                                                             const json_t *dflt, const json_t *cast,
                                                             const json_t *lo, const json_t *hi,
                                                             const json_t *env_raw)
{
    const char *k = w27a1f_str_or(key, "");
    int as_int = !strcmp(w27a1f_str_or(cast, "float"), "int");
    json_t *cfg = w27a1f_backfill_cfg(extra);
    const json_t *raw = NULL;
    int owned = 0;
    double value = 0.0, lov = w27a1f_num_or(lo, 0.0), hiv;

    if (w27a1f_is_obj(cfg) && json_has(cfg, k)) raw = json_obj_get(cfg, k);
    else if (w27a1f_is_obj(cfg)) raw = dflt;
    else if (env_raw) {
        char *env = w27a1f_strip_dup(w27a1f_str_or(env_raw, ""));
        if (env && env[0]) { raw = json_string(env); owned = 1; }
        else raw = dflt;
        free(env);
    } else raw = dflt;

    if (!w27a1f_cast_num(raw, as_int, &value))
        if (!w27a1f_cast_num(dflt, as_int, &value)) value = 0.0;
    if (owned) json_free((json_t *)raw);

    if (value < lov) value = lov;
    if (hi && !w27a1f_is_null(hi)) {
        hiv = w27a1f_num_or(hi, 0.0);
        if (value > hiv) value = hiv;
    }
    return json_number(value);
}

#define W27A1F_BACKFILL_WRAPPER(fname, keylit, dflt_v, castlit, lo_v, hi_expr)                     \
    json_t *fname(const json_t *extra, const json_t *env_raw)                                     \
    {                                                                                             \
        json_t *d = json_number(dflt_v), *lo = json_number(lo_v), *r;                              \
        json_t *hi = (hi_expr);                                                                   \
        r = w27_adapter_1f12c3_p4_missed_message_backfill_number(                                 \
            extra, json_string(keylit), d, json_string(castlit), lo, hi, env_raw);                 \
        json_free(d); json_free(lo); if (hi) json_free(hi);                                        \
        return r;                                                                                 \
    }

W27A1F_BACKFILL_WRAPPER(w27_adapter_1f12c3_p4_missed_message_backfill_window_seconds,
                        "window_seconds", 21600.0, "float", 60.0, json_null())
W27A1F_BACKFILL_WRAPPER(w27_adapter_1f12c3_p4_missed_message_backfill_limit,
                        "limit", 100.0, "int", 1.0, json_number(500.0))
W27A1F_BACKFILL_WRAPPER(w27_adapter_1f12c3_p4_missed_message_backfill_max_dispatches,
                        "max_dispatches", 10.0, "int", 1.0, json_number(100.0))
W27A1F_BACKFILL_WRAPPER(w27_adapter_1f12c3_p4_missed_message_backfill_max_attempts,
                        "max_attempts", 3.0, "int", 1.0, json_number(100.0))

/* \b(?:hermes|the agent|agent|the gateway|gateway|bmo)\s+(?:is|was|appears to
 * be|is currently|was currently)\s+(?:down|offline|unavailable|not running)\b */
static int w27a1f_word_char(unsigned char c) { return isalnum(c) || c == '_'; }

json_t *w27_adapter_1f12c3_p4_is_down_notice_content(const json_t *content)
{
    static const char *const subjects[] = { "hermes", "the agent", "agent", "the gateway", "gateway", "bmo" };
    static const char *const states[] = { "appears to be", "is currently", "was currently", "is", "was" };
    static const char *const conds[] = { "down", "offline", "unavailable", "not running" };
    char *t = w27a1f_strdup(w27a1f_str_or(content, ""));
    size_t len, i, j, k, m;
    int hit = 0;

    if (!t) return json_bool(false);
    len = strlen(t);
    for (i = 0; t[i]; i++) t[i] = (char)tolower((unsigned char)t[i]);
    for (i = 0; i < len && !hit; i++) {
        if (i > 0 && w27a1f_word_char((unsigned char)t[i - 1])) continue;
        for (j = 0; j < sizeof subjects / sizeof subjects[0] && !hit; j++) {
            size_t sn = strlen(subjects[j]), pos, n;
            if (i + sn > len || strncmp(t + i, subjects[j], sn)) continue;
            pos = i + sn;
            if (pos < len && w27a1f_word_char((unsigned char)t[pos])) continue;
            if (pos >= len || !isspace((unsigned char)t[pos])) continue;
            while (pos < len && isspace((unsigned char)t[pos])) pos++;
            for (k = 0; k < sizeof states / sizeof states[0] && !hit; k++) {
                size_t stn = strlen(states[k]);
                if (pos + stn > len || strncmp(t + pos, states[k], stn)) continue;
                n = pos + stn;
                if (n < len && w27a1f_word_char((unsigned char)t[n])) continue;
                if (n >= len || !isspace((unsigned char)t[n])) continue;
                while (n < len && isspace((unsigned char)t[n])) n++;
                for (m = 0; m < sizeof conds / sizeof conds[0]; m++) {
                    size_t cn = strlen(conds[m]);
                    if (n + cn > len || strncmp(t + n, conds[m], cn)) continue;
                    if (n + cn < len && w27a1f_word_char((unsigned char)t[n + cn])) continue;
                    hit = 1;
                    break;
                }
            }
        }
    }
    free(t);
    return json_bool(hit ? true : false);
}

json_t *w27_adapter_1f12c3_p4_message_channel_ids(const json_t *channel)
{
    json_t *cid_v = w27a1f_is_obj(channel) ? json_obj_get(channel, "id") : NULL;
    json_t *parent_v = w27a1f_is_obj(channel) ? json_obj_get(channel, "parent_id") : NULL;
    char *channel_id = w27a1f_truthy(cid_v) ? w27a1f_pystr(cid_v) : w27a1f_strdup("");
    char *parent_id = w27a1f_truthy(parent_v) ? w27a1f_pystr(parent_v) : NULL;
    json_t *out = json_array();
    json_append(out, json_string(channel_id ? channel_id : ""));
    json_append(out, parent_id ? json_string(channel_id ? channel_id : "") : json_null());
    json_append(out, parent_id ? json_string(parent_id) : json_null());
    free(channel_id);
    free(parent_id);
    return out;
}

/* ── gate p5: slash-command payload canonicalisation (adapter.py:2956-3400) ── */

static json_t *w27a1f_copy_or_null(const json_t *v)
{
    return v ? json_copy(v) : json_null();
}

/* int(v), or dflt where Python would raise (fixtures stay in domain). */
static long long w27a1f_pyint(const json_t *v, long long dflt)
{
    if (!v || w27a1f_is_null(v)) return dflt;
    if (w27a1f_is_bool(v)) return v->bool_val ? 1 : 0;
    if (w27a1f_is_num(v)) return (long long)v->num_val;
    if (w27a1f_is_str(v) && v->str_val) {
        char *end = NULL;
        const char *s = w27a1f_lstrip(v->str_val);
        long long r;
        if (!*s) return dflt;
        r = strtoll(s, &end, 10);
        if (!end || end == s) return dflt;
        while (end && *end && isspace((unsigned char)*end)) end++;
        if (!end || *end) return dflt;
        return r;
    }
    return dflt;
}

/* str(v or "") for a payload field. */
static json_t *w27a1f_str_or_empty(const json_t *v)
{
    if (!w27a1f_truthy(v)) return json_string("");
    return w27a1f_json_own(w27a1f_pystr(v));
}

static json_t *w27a1f_sorted_ints(const json_t *arr)
{
    json_t *out = json_array();
    size_t i;
    long long *vals;
    size_t n = w27a1f_is_arr(arr) ? json_len(arr) : 0;
    if (!n) return out;
    vals = (long long *)malloc(sizeof(long long) * n);
    if (!vals) return out;
    for (i = 0; i < n; i++) vals[i] = w27a1f_pyint(json_get(arr, i), 0);
    for (i = 0; i < n; i++) {
        size_t j = i;
        long long tmp;
        while (j > 0 && vals[j - 1] > vals[j]) { tmp = vals[j - 1]; vals[j - 1] = vals[j]; vals[j] = tmp; j--; }
    }
    for (i = 0; i < n; i++) json_append(out, json_number((double)vals[i]));
    free(vals);
    return out;
}

json_t *w27_adapter_1f12c3_p5_normalize_permissions(const json_t *value)
{
    if (w27a1f_is_null(value)) return json_null();
    return w27a1f_json_own(w27a1f_pystr(value));
}

json_t *w27_adapter_1f12c3_p5_canonicalize_app_command_option(const json_t *payload)
{
    json_t *o = json_object();
    json_t *choices = json_array();
    json_t *options = json_array();
    json_t *channel_types;
    size_t i;

    json_set(o, "type", json_number((double)w27a1f_pyint(json_obj_get(payload, "type"), 0)));
    json_set(o, "name", w27a1f_str_or_empty(json_obj_get(payload, "name")));
    json_set(o, "description", w27a1f_str_or_empty(json_obj_get(payload, "description")));
    json_set(o, "required", json_bool(w27a1f_truthy(json_obj_get(payload, "required")) ? true : false));
    json_set(o, "autocomplete", json_bool(w27a1f_truthy(json_obj_get(payload, "autocomplete")) ? true : false));

    {
        json_t *cl = json_obj_get(payload, "choices");
        if (w27a1f_is_arr(cl)) {
            for (i = 0; i < json_len(cl); i++) {
                json_t *c = json_get(cl, i);
                json_t *entry;
                if (!w27a1f_is_obj(c)) continue;
                entry = json_object();
                json_set(entry, "name", w27a1f_str_or_empty(json_obj_get(c, "name")));
                json_set(entry, "value", w27a1f_copy_or_null(json_obj_get(c, "value")));
                json_append(choices, entry);
            }
        }
    }
    json_set(o, "choices", choices);

    channel_types = json_obj_get(payload, "channel_types");
    if (w27a1f_is_arr(channel_types) && json_len(channel_types)) json_set(o, "channel_types", json_copy(channel_types));
    else json_set(o, "channel_types", json_array());

    json_set(o, "min_value", w27a1f_copy_or_null(json_obj_get(payload, "min_value")));
    json_set(o, "max_value", w27a1f_copy_or_null(json_obj_get(payload, "max_value")));
    json_set(o, "min_length", w27a1f_copy_or_null(json_obj_get(payload, "min_length")));
    json_set(o, "max_length", w27a1f_copy_or_null(json_obj_get(payload, "max_length")));

    {
        json_t *ol = json_obj_get(payload, "options");
        if (w27a1f_is_arr(ol)) {
            for (i = 0; i < json_len(ol); i++) {
                json_t *item = json_get(ol, i);
                if (!w27a1f_is_obj(item)) continue;
                json_append(options, w27_adapter_1f12c3_p5_canonicalize_app_command_option(item));
            }
        }
    }
    json_set(o, "options", options);
    return o;
}

json_t *w27_adapter_1f12c3_p5_canonicalize_app_command_payload(const json_t *payload)
{
    json_t *o = json_object();
    json_t *options = json_array();
    json_t *contexts = json_obj_get(payload, "contexts");
    json_t *integration = json_obj_get(payload, "integration_types");
    size_t i;

    json_set(o, "type", json_number((double)w27a1f_pyint(json_obj_get(payload, "type"), 1)));
    json_set(o, "name", w27a1f_str_or_empty(json_obj_get(payload, "name")));
    json_set(o, "description", w27a1f_str_or_empty(json_obj_get(payload, "description")));
    json_set(o, "default_member_permissions",
             w27_adapter_1f12c3_p5_normalize_permissions(json_obj_get(payload, "default_member_permissions")));
    {
        json_t *dm = json_obj_get(payload, "dm_permission");
        json_set(o, "dm_permission", json_bool(!json_has(payload, "dm_permission") ? true : (w27a1f_truthy(dm) ? true : false)));
        json_set(o, "nsfw", json_bool(w27a1f_truthy(json_obj_get(payload, "nsfw")) ? true : false));
    }
    json_set(o, "contexts", w27a1f_truthy(contexts) ? w27a1f_sorted_ints(contexts) : json_null());
    json_set(o, "integration_types", w27a1f_truthy(integration) ? w27a1f_sorted_ints(integration) : json_null());
    {
        json_t *ol = json_obj_get(payload, "options");
        if (w27a1f_is_arr(ol)) {
            for (i = 0; i < json_len(ol); i++) {
                json_t *item = json_get(ol, i);
                if (!w27a1f_is_obj(item)) continue;
                json_append(options, w27_adapter_1f12c3_p5_canonicalize_app_command_option(item));
            }
        }
    }
    json_set(o, "options", options);
    return o;
}

json_t *w27_adapter_1f12c3_p5_patchable_app_command_payload(const json_t *payload)
{
    json_t *canon = w27_adapter_1f12c3_p5_canonicalize_app_command_payload(payload);
    json_t *o = json_object();
    json_set(o, "name", json_copy(json_obj_get(canon, "name")));
    json_set(o, "description", json_copy(json_obj_get(canon, "description")));
    json_set(o, "options", json_copy(json_obj_get(canon, "options")));
    json_free(canon);
    return o;
}

json_t *w27_adapter_1f12c3_p5_existing_command_to_payload(const json_t *command)
{
    json_t *payload;
    json_t *out;
    if (!w27a1f_is_obj(command)) return json_object();
    payload = json_copy(json_obj_get(command, "to_dict"));
    if (!w27a1f_is_obj(payload)) {
        json_free(payload);
        payload = json_object();
    }
    {
        json_t *nsfw = json_obj_get(command, "nsfw");
        if (!w27a1f_is_null(nsfw))
            json_set(payload, "nsfw", json_bool(w27a1f_truthy(nsfw) ? true : false));
    }
    {
        json_t *guild_only = json_obj_get(command, "guild_only");
        if (!w27a1f_is_null(guild_only))
            json_set(payload, "dm_permission", json_bool(w27a1f_truthy(guild_only) ? false : true));
    }
    {
        json_t *perms = json_obj_get(command, "default_member_permissions");
        if (!w27a1f_is_null(perms)) {
            json_t *value = w27a1f_is_obj(perms) ? json_obj_get(perms, "value") : NULL;
            json_set(payload, "default_member_permissions",
                     w27a1f_copy_or_null(value ? value : perms));
        }
    }
    out = payload;
    return out;
}

json_t *w27_adapter_1f12c3_p5_is_reply_reference_rejected(const json_t *text)
{
    const char *t = w27a1f_str_or(text, "");
    int hit = 0;
    if (strstr(t, "error code: 50035") && strstr(t, "Cannot reply to a system message")) hit = 1;
    if (strstr(t, "error code: 10008")) hit = 1;
    return json_bool(hit ? true : false);
}

json_t *w27_adapter_1f12c3_p5_is_length_overflow_error(const json_t *text)
{
    char *t = w27a1f_strdup(w27a1f_str_or(text, ""));
    int hit = 0;
    size_t i;
    if (!t) return json_bool(false);
    for (i = 0; t[i]; i++) t[i] = (char)tolower((unsigned char)t[i]);
    if (strstr(t, "error code: 50035")
        && (strstr(t, "2000 or fewer") || strstr(t, "fewer in length"))) hit = 1;
    free(t);
    return json_bool(hit ? true : false);
}

json_t *w27_adapter_1f12c3_p5_get_discord_command_sync_policy(const json_t *env_raw)
{
    static const char *const ok[] = { "safe", "bulk", "off" };
    char *raw = w27a1f_strip_dup(w27a1f_str_or(env_raw, "safe"));
    size_t i;
    if (!raw) return json_string("safe");
    for (i = 0; raw[i]; i++) raw[i] = (char)tolower((unsigned char)raw[i]);
    if (w27a1f_in_list_ci(raw, ok, sizeof ok / sizeof ok[0])) {
        json_t *r = json_string(raw);
        free(raw);
        return r;
    }
    free(raw);
    return json_string("safe");
}

/* ── gate p6: voice/config readers and the fail-closed warning (adapter.py:3480-4100) ── */

/* int(v) where Python raises -> 0 */
static int w27a1f_pyint_try(const json_t *v, long long *out)
{
    if (!v || w27a1f_is_null(v)) return 0;
    if (w27a1f_is_bool(v)) { *out = v->bool_val ? 1 : 0; return 1; }
    if (w27a1f_is_num(v)) { *out = (long long)v->num_val; return 1; }
    if (w27a1f_is_str(v) && v->str_val) {
        char *end = NULL;
        const char *s = w27a1f_lstrip(v->str_val);
        long long r;
        if (!*s) return 0;
        r = strtoll(s, &end, 10);
        if (!end || end == s) return 0;
        while (end && *end && isspace((unsigned char)*end)) end++;
        if (!end || *end) return 0;
        *out = r;
        return 1;
    }
    return 0;
}

json_t *w27_adapter_1f12c3_p6_load_voice_fx_config(const json_t *fx)
{
    static const char *const keys[] = {
        "enabled", "ambient_enabled", "ambient_path", "ambient_gain", "duck_gain",
        "speech_gain", "lead_silence_ms", "ack_enabled", "ack_phrases"
    };
    json_t *out = json_object();
    size_t i;
    json_set(out, "enabled", json_bool(false));
    json_set(out, "ambient_enabled", json_bool(true));
    json_set(out, "ambient_path", json_string(""));
    json_set(out, "ambient_gain", json_number(0.18));
    json_set(out, "duck_gain", json_number(0.06));
    json_set(out, "speech_gain", json_number(1.0));
    json_set(out, "lead_silence_ms", json_number(200));
    json_set(out, "ack_enabled", json_bool(true));
    json_set(out, "ack_phrases", json_null());
    if (w27a1f_is_obj(fx)) {
        for (i = 0; i < sizeof keys / sizeof keys[0]; i++) {
            json_t *v = json_obj_get(fx, keys[i]);
            if (v && !w27a1f_is_null(v)) json_set(out, keys[i], json_copy(v));
        }
    }
    return out;
}

json_t *w27_adapter_1f12c3_p6_load_discord_int_config(const json_t *discord_cfg, const json_t *key,
                                                      const json_t *dflt, const json_t *minimum)
{
    const char *k = w27a1f_str_or(key, "");
    json_t *raw = (w27a1f_is_obj(discord_cfg) && json_has(discord_cfg, k))
                      ? w27a1f_copy_or_null(json_obj_get(discord_cfg, k))
                      : json_copy(dflt);
    long long value, minv = w27a1f_pyint(dflt, 0);
    json_t *r;
    if (!w27a1f_pyint_try(raw, &value)) {
        json_free(raw);
        return json_copy(dflt);
    }
    json_free(raw);
    minv = w27a1f_pyint(minimum, 0);
    r = json_number((double)(value < minv ? minv : value));
    return r;
}

json_t *w27_adapter_1f12c3_p6_load_voice_timeout(const json_t *discord_cfg)
{
    return w27_adapter_1f12c3_p6_load_discord_int_config(
        discord_cfg, json_string("voice_channel_inactivity_timeout_seconds"),
        json_number(300), json_number(0));
}

json_t *w27_adapter_1f12c3_p6_load_playback_timeout(const json_t *discord_cfg)
{
    return w27_adapter_1f12c3_p6_load_discord_int_config(
        discord_cfg, json_string("voice_playback_timeout_seconds"),
        json_number(120), json_number(1));
}

json_t *w27_adapter_1f12c3_p6_voice_timeout_limit(const json_t *stored)
{
    long long v;
    if (!w27a1f_pyint_try(stored, &v)) return json_number(300);
    return json_number((double)v);
}

json_t *w27_adapter_1f12c3_p6_playback_timeout_limit(const json_t *stored)
{
    long long v;
    if (!w27a1f_pyint_try(stored, &v)) return json_number(120);
    return json_number((double)v);
}

json_t *w27_adapter_1f12c3_p6_voice_mixer_active(const json_t *mixers, const json_t *guild_id)
{
    long long want;
    size_t i;
    if (!w27a1f_is_obj(mixers) || json_object_size(mixers) == 0) return json_bool(false);
    if (!w27a1f_pyint_try(guild_id, &want)) return json_bool(false);
    for (i = 0; i < json_object_size(mixers); i++) {
        const char *k = json_object_get_key_at(mixers, i);
        char *end = NULL;
        long long kv;
        if (!k) continue;
        kv = strtoll(k, &end, 10);
        if (!end || *end) continue;
        if (kv != want) continue;
        return json_bool(w27a1f_is_null(json_object_get_at(mixers, i)) ? false : true);
    }
    return json_bool(false);
}

json_t *w27_adapter_1f12c3_p6_discord_channel_ids_allowed(const json_t *channel_ids, const json_t *allowed)
{
    size_t i, j;
    if (!w27a1f_is_arr(channel_ids) || json_len(channel_ids) == 0) return json_bool(false);
    if (!w27a1f_is_arr(allowed) || json_len(allowed) == 0) return json_bool(false);
    for (i = 0; i < json_len(allowed); i++) {
        json_t *a = json_get(allowed, i);
        if (w27a1f_is_str(a) && a->str_val && !strcmp(a->str_val, "*")) return json_bool(true);
    }
    for (i = 0; i < json_len(channel_ids); i++) {
        json_t *c = json_get(channel_ids, i);
        char *cs = w27a1f_pystr(c);
        if (!cs) continue;
        for (j = 0; j < json_len(allowed); j++) {
            char *as = w27a1f_pystr(json_get(allowed, j));
            int same = as && !strcmp(cs, as);
            free(as);
            if (same) { free(cs); return json_bool(true); }
        }
        free(cs);
    }
    return json_bool(false);
}

json_t *w27_adapter_1f12c3_p6_warn_if_fail_closed_default(const json_t *state)
{
    if (w27a1f_truthy(json_obj_get(state, "warned"))) return json_bool(true);
    if (w27a1f_truthy(json_obj_get(state, "allowed_users"))) return json_bool(false);
    if (w27a1f_truthy(json_obj_get(state, "allowed_roles"))) return json_bool(false);
    if (w27a1f_truthy(json_obj_get(state, "allowed_channels"))) return json_bool(false);
    if (w27a1f_truthy(json_obj_get(state, "discord_allow_all"))) return json_bool(false);
    if (w27a1f_truthy(json_obj_get(state, "gateway_allow_all"))) return json_bool(false);
    return json_bool(true);
}

/* ── gate p7: authorization predicate and channel shape readers (adapter.py:3995-5345) ── */

/* Python `v in seq` over the scalar domain. */
static int w27a1f_val_in_list(const json_t *v, const json_t *list)
{
    size_t i;
    if (!w27a1f_is_arr(list)) return 0;
    for (i = 0; i < json_len(list); i++) {
        json_t *e = json_get(list, i);
        if (!e) continue;
        if (w27a1f_is_num(e) && w27a1f_is_num(v)) { if (e->num_val == v->num_val) return 1; continue; }
        if (w27a1f_is_str(e) && w27a1f_is_str(v)) {
            const char *a = e->str_val ? e->str_val : "";
            const char *b = v->str_val ? v->str_val : "";
            if (!strcmp(a, b)) return 1;
            continue;
        }
        if (w27a1f_is_null(e) && w27a1f_is_null(v)) return 1;
        if (w27a1f_is_bool(e) && w27a1f_is_bool(v) && e->bool_val == v->bool_val) return 1;
    }
    return 0;
}

static const json_t *w27a1f_member_entry(const json_t *guild, const char *user_id)
{
    const json_t *members;
    char *end = NULL;
    long long uid;
    size_t i;
    if (!user_id || !*user_id) return NULL;
    uid = strtoll(user_id, &end, 10);
    if (!end || end == user_id) return NULL;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end) return NULL;
    members = json_obj_get(guild, "members");
    if (!w27a1f_is_obj(members)) return NULL;
    for (i = 0; i < json_object_size(members); i++) {
        const char *k = json_object_get_key_at(members, i);
        char *e2 = NULL;
        long long kv;
        if (!k) continue;
        kv = strtoll(k, &e2, 10);
        if (!e2 || *e2) continue;
        if (kv == uid) return json_object_get_at(members, i);
    }
    return NULL;
}

json_t *w27_adapter_1f12c3_p7_guild_member_has_role(const json_t *guild, const json_t *user_id,
                                                    const json_t *allowed_roles)
{
    const json_t *roles;
    char *uid;
    size_t i;
    if (!w27a1f_is_obj(guild)) return json_bool(false);
    uid = w27a1f_truthy(user_id) ? w27a1f_pystr(user_id) : w27a1f_strdup("");
    if (!uid) return json_bool(false);
    roles = w27a1f_member_entry(guild, uid);
    free(uid);
    if (!roles || !w27a1f_is_arr(roles)) return json_bool(false);
    for (i = 0; i < json_len(roles); i++)
        if (w27a1f_val_in_list(json_get(roles, i), allowed_roles)) return json_bool(true);
    return json_bool(false);
}

json_t *w27_adapter_1f12c3_p7_interaction_guild_id(const json_t *interaction)
{
    json_t *gid = w27a1f_is_obj(interaction) ? json_obj_get(interaction, "guild_id") : NULL;
    if (w27a1f_is_null(gid)) {
        json_t *channel = w27a1f_is_obj(interaction) ? json_obj_get(interaction, "channel") : NULL;
        json_t *guild = w27a1f_is_obj(channel) ? json_obj_get(channel, "guild") : NULL;
        gid = w27a1f_is_obj(guild) ? json_obj_get(guild, "id") : NULL;
    }
    if (!w27a1f_truthy(gid)) return json_null();
    return w27a1f_json_own(w27a1f_pystr(gid));
}

json_t *w27_adapter_1f12c3_p7_is_forum_parent(const json_t *channel)
{
    json_t *type;
    if (!w27a1f_is_obj(channel)) return json_bool(false);
    type = json_obj_get(channel, "type");
    if (w27a1f_is_null(type)) return json_bool(false);
    if (w27a1f_is_obj(type)) type = json_obj_get(type, "value");
    if (w27a1f_is_num(type) && type->num_val == 15.0) return json_bool(true);
    return json_bool(false);
}

json_t *w27_adapter_1f12c3_p7_get_parent_channel_id(const json_t *channel)
{
    json_t *parent, *pid;
    if (!w27a1f_is_obj(channel)) return json_null();
    parent = json_obj_get(channel, "parent");
    if (w27a1f_is_obj(parent)) {
        json_t *id = json_obj_get(parent, "id");
        if (!w27a1f_is_null(id)) return w27a1f_json_own(w27a1f_pystr(id));
    }
    pid = json_obj_get(channel, "parent_id");
    if (!w27a1f_is_null(pid)) return w27a1f_json_own(w27a1f_pystr(pid));
    return json_null();
}

json_t *w27_adapter_1f12c3_p7_get_effective_topic(const json_t *channel, const json_t *is_thread)
{
    json_t *topic, *parent;
    if (!w27a1f_is_obj(channel)) return json_null();
    topic = json_obj_get(channel, "topic");
    if (!w27a1f_truthy(topic) && w27a1f_truthy(is_thread)) {
        parent = json_obj_get(channel, "parent");
        if (w27a1f_is_obj(parent) && w27a1f_truthy(w27_adapter_1f12c3_p7_is_forum_parent(parent)))
            topic = json_obj_get(parent, "topic");
    }
    if (w27a1f_is_null(topic)) return json_null();
    return w27a1f_copy_or_null(topic);
}

json_t *w27_adapter_1f12c3_p7_attachment_message_type(const json_t *att)
{
    const char *ct = "";
    json_t *v;
    if (!w27a1f_is_obj(att)) return json_string("DOCUMENT");
    v = json_obj_get(att, "content_type");
    ct = w27a1f_truthy(v) ? w27a1f_str_or(v, "") : "";
    if (!strncmp(ct, "image/", 6)) return json_string("PHOTO");
    if (!strncmp(ct, "video/", 6)) return json_string("VIDEO");
    if (!strncmp(ct, "audio/", 6)) {
        json_t *marker = json_obj_get(att, "is_voice_message");
        int voice = 0;
        if (!w27a1f_is_null(marker)) voice = w27a1f_truthy(marker);
        else voice = !w27a1f_is_null(json_obj_get(att, "duration"))
                  && !w27a1f_is_null(json_obj_get(att, "waveform"));
        return json_string(voice ? "VOICE" : "AUDIO");
    }
    return json_string("DOCUMENT");
}

/* code-point aware prefix of n code points */
static size_t w27a1f_cp_prefix(const char *s, size_t n)
{
    size_t i = 0, k = 0;
    while (s[i] && k < n) {
        size_t adv = 1;
        w27a1f_cp_at(s, i, &adv);
        i += adv;
        k++;
    }
    return i;
}

json_t *w27_adapter_1f12c3_p7_derive_auto_thread_name(const json_t *content, const json_t *default_name)
{
    char *s = w27a1f_strip_dup(w27a1f_str_or(content, ""));
    w27a1f_sb b;
    size_t i;
    if (!s) return json_copy(default_name);
    /* strip <@[!&]?\d+> and <#\d+> */
    w27a1f_sb_init(&b);
    i = 0;
    while (s[i]) {
        if (s[i] == '<' && (s[i + 1] == '@' || s[i + 1] == '#')) {
            size_t j = i + 2;
            if (s[i + 1] == '@' && (s[j] == '!' || s[j] == '&')) j++;
            {
                size_t digits = j;
                while (isdigit((unsigned char)s[j])) j++;
                if (j > digits && s[j] == '>') { i = j + 1; continue; }
            }
        }
        w27a1f_sb_putc(&b, s[i]);
        i++;
    }
    free(s);
    /* collapse \s+ to one space, then strip */
    {
        char *collapsed = w27a1f_sb_take(&b);
        w27a1f_sb out;
        size_t k = 0;
        int prev_ws = 0;
        w27a1f_sb_init(&out);
        for (k = 0; collapsed[k]; k++) {
            if (isspace((unsigned char)collapsed[k])) {
                if (!prev_ws) w27a1f_sb_putc(&out, ' ');
                prev_ws = 1;
            } else {
                w27a1f_sb_putc(&out, collapsed[k]);
                prev_ws = 0;
            }
        }
        free(collapsed);
        {
            char *trimmed = w27a1f_strip_dup(w27a1f_sb_take(&out));
            size_t total, cut;
            json_t *r;
            if (!trimmed) return json_copy(default_name);
            if (!trimmed[0]) { free(trimmed); return json_copy(default_name); }
            total = 0;
            for (k = 0; trimmed[k]; ) { size_t adv = 1; w27a1f_cp_at(trimmed, k, &adv); k += adv; total++; }
            cut = w27a1f_cp_prefix(trimmed, 80);
            if (total > 80) {
                w27a1f_sb t;
                size_t head = w27a1f_cp_prefix(trimmed, 77);
                w27a1f_sb_init(&t);
                w27a1f_sb_putn(&t, trimmed, head);
                w27a1f_sb_puts(&t, "...");
                r = w27a1f_json_own(w27a1f_sb_take(&t));
            } else {
                r = w27a1f_json_strn(trimmed, cut);
            }
            free(trimmed);
            return r;
        }
    }
}

json_t *w27_adapter_1f12c3_p7_is_allowed_user(const json_t *req)
{
    json_t *users, *roles, *channel_ids, *guild, *uid_v, *direct, *agid;
    int has_users, has_roles, is_dm;
    char *uid;

    const json_t *st = req;
    if (!w27a1f_is_obj(st)) return json_bool(false);
    users = json_obj_get(st, "allowed_users");
    roles = json_obj_get(st, "allowed_roles");
    has_users = w27a1f_truthy(users);
    has_roles = w27a1f_truthy(roles);
    is_dm = w27a1f_truthy(json_obj_get(st, "is_dm"));
    channel_ids = json_obj_get(st, "channel_ids");
    guild = json_obj_get(st, "guild");
    uid_v = json_obj_get(st, "user_id");
    uid = w27a1f_truthy(uid_v) ? w27a1f_pystr(uid_v) : w27a1f_strdup("");
    if (!uid) return json_bool(false);

    if (w27a1f_truthy(json_obj_get(st, "pairing"))) { free(uid); return json_bool(true); }

    if (!has_users && !has_roles) {
        int ok = 0;
        if (w27a1f_truthy(json_obj_get(st, "discord_allow_all"))
            || w27a1f_truthy(json_obj_get(st, "gateway_allow_all"))) ok = 1;
        else if (!is_dm && !w27a1f_is_null(channel_ids)) {
            json_t *r = w27_adapter_1f12c3_p6_discord_channel_ids_allowed(
                channel_ids, json_obj_get(st, "allowed_channels"));
            ok = w27a1f_truthy(r);
            json_free(r);
        }
        free(uid);
        return json_bool(ok ? true : false);
    }
    if (has_users) {
        json_t *self = json_string(uid);
        int hit = w27a1f_val_in_list(json_string("*"), users) || w27a1f_val_in_list(self, users);
        json_free(self);
        if (hit) { free(uid); return json_bool(true); }
    }
    if (!has_roles) { free(uid); return json_bool(false); }

    if (is_dm || !w27a1f_is_obj(guild)) {
        json_t *dm_gid = json_obj_get(st, "dm_role_auth_guild");
        json_t *client_guilds = json_obj_get(st, "client_guilds");
        json_t *dm_guild = NULL;
        size_t i;
        if (w27a1f_is_null(dm_gid) || !w27a1f_is_obj(client_guilds)) { free(uid); return json_bool(false); }
        {
            char *want = w27a1f_pystr(dm_gid);
            for (i = 0; i < json_object_size(client_guilds) && want; i++) {
                const char *k = json_object_get_key_at(client_guilds, i);
                if (k && !strcmp(k, want)) { dm_guild = json_object_get_at(client_guilds, i); break; }
            }
            free(want);
        }
        if (!w27a1f_is_obj(dm_guild)) { free(uid); return json_bool(false); }
        {
            json_t *r = w27_adapter_1f12c3_p7_guild_member_has_role(dm_guild, json_string(uid), roles);
            int ok = w27a1f_truthy(r);
            json_free(r);
            free(uid);
            return json_bool(ok ? true : false);
        }
    }

    direct = json_obj_get(st, "author_roles");
    agid = json_obj_get(st, "author_guild_id");
    if (w27a1f_truthy(direct)) {
        json_t *gid = json_obj_get(guild, "id");
        int same_guild = w27a1f_is_null(agid) ? 1 : 0;
        size_t i;
        if (!w27a1f_is_null(agid)) {
            char *a = w27a1f_pystr(agid), *b = gid ? w27a1f_pystr(gid) : NULL;
            same_guild = a && b && !strcmp(a, b);
            free(a); free(b);
        }
        if (same_guild) {
            for (i = 0; i < json_len(direct); i++)
                if (w27a1f_val_in_list(json_get(direct, i), roles)) { free(uid); return json_bool(true); }
        }
    }
    {
        json_t *r = w27_adapter_1f12c3_p7_guild_member_has_role(guild, json_string(uid), roles);
        int ok = w27a1f_truthy(r);
        json_free(r);
        free(uid);
        return json_bool(ok ? true : false);
    }
}

/* ── gate p8: flag readers (adapter.py:2381, 4810-4870) ───────────────── */

static int w27a1f_cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w27_adapter_1f12c3_p9_gate_csv_set(const json_t *raw);
json_t *w27a1f_try_decode_list(const json_t *raw);

static int w27a1f_blanks(const char *s)
{
    while (s && *s) { if (!isspace((unsigned char)*s)) return 0; s++; }
    return 1;
}

/* sorted, de-duplicated string array from an owned list of malloc'd strings */
static json_t *w27a1f_sorted_unique(char **items, size_t n)
{
    json_t *out = json_array();
    size_t i;
    if (!n) return out;
    qsort(items, n, sizeof(char *), (int (*)(const void *, const void *))w27a1f_cmp_str);
    for (i = 0; i < n; i++) {
        if (i && !strcmp(items[i], items[i - 1])) { free(items[i]); continue; }
        json_append(out, json_string(items[i]));
    }
    for (; i < n; i++) free(items[i]);
    return out;
}

json_t *w27_adapter_1f12c3_p8_missed_message_backfill_channels(const json_t *extra, const json_t *env_raw,
                                                               const json_t *allowed_channels,
                                                               const json_t *free_response)
{
    json_t *cfg = w27a1f_backfill_cfg(extra);
    if (w27a1f_is_obj(cfg) && json_has(cfg, "channels")) {
        json_t *raw = json_obj_get(cfg, "channels");
        json_t *channels = w27_adapter_1f12c3_p9_gate_csv_set(raw);
        json_t *decoded = w27a1f_try_decode_list(raw);
        int is_list = decoded != NULL;
        if (decoded) json_free(decoded);
        if ((w27a1f_is_arr(channels) && json_len(channels) > 0) || is_list) return channels;
        json_free(channels);
    }
    {
        char *raw = w27a1f_strip_dup(w27a1f_str_or(env_raw, ""));
        int blank = !raw || !raw[0];
        free(raw);
        if (blank) {
            char **items;
            size_t n = 0, i, cap;
            json_t *out;
            cap = (allowed_channels ? json_len(allowed_channels) : 0)
                + (free_response ? json_len(free_response) : 0) + 8;
            items = (char **)malloc(sizeof(char *) * cap);
            if (!items) return json_array();
            for (i = 0; allowed_channels && i < json_len(allowed_channels); i++) {
                char *v = w27a1f_pystr(json_get(allowed_channels, i));
                if (v && v[0]) items[n++] = v; else free(v);
            }
            for (i = 0; free_response && i < json_len(free_response); i++) {
                char *v = w27a1f_pystr(json_get(free_response, i));
                if (v && v[0]) items[n++] = v; else free(v);
            }
            out = w27a1f_sorted_unique(items, n);
            free(items);
            return out;
        }
    }
    return w27_adapter_1f12c3_p9_gate_csv_set(env_raw);
}

/* _extra_or_secret(extra, key, env, None) */
static json_t *w27a1f_extra_or_secret(const json_t *extra, const char *key, const json_t *env_raw)
{
    if (env_raw) {
        char *env = w27a1f_strip_dup(w27a1f_str_or(env_raw, ""));
        if (env && env[0]) { json_t *r = json_string(env); free(env); return r; }
        free(env);
    }
    if (w27a1f_is_obj(extra) && json_has(extra, key)) {
        json_t *v = json_obj_get(extra, key);
        if (w27a1f_is_str(v) && w27a1f_blanks(v->str_val)) return NULL;
        if (w27a1f_is_null(v)) return NULL;
        return json_copy(v);
    }
    return NULL;
}

json_t *w27_adapter_1f12c3_p8_extra_or_env_flag(const json_t *extra, const json_t *key,
                                                const json_t *env_default, const json_t *truthy,
                                                const json_t *env_raw)
{
    static const char *const yes[] = { "true", "1", "yes", "on" };
    static const char *const no[] = { "false", "0", "no", "off" };
    const char *k = w27a1f_str_or(key, "");
    json_t *configured = w27a1f_extra_or_secret(extra, k, env_raw);
    int want_truthy = w27a1f_truthy(truthy);
    char *text;
    size_t i;
    int hit;

    if (!configured) {
        configured = json_copy(env_default);
        if (!configured) return json_bool(false);
    }
    if (w27a1f_is_bool(configured)) {
        int b = configured->bool_val;
        json_free(configured);
        return json_bool(b ? true : false);
    }
    text = w27a1f_strip_dup(w27a1f_pystr(configured));
    json_free(configured);
    if (!text) return json_bool(false);
    for (i = 0; text[i]; i++) text[i] = (char)tolower((unsigned char)text[i]);
    if (want_truthy) hit = w27a1f_in_list_ci(text, yes, sizeof yes / sizeof yes[0]);
    else hit = !w27a1f_in_list_ci(text, no, sizeof no / sizeof no[0]);
    free(text);
    return json_bool(hit ? true : false);
}

json_t *w27_adapter_1f12c3_p8_discord_require_mention(const json_t *extra, const json_t *env_raw)
{
    return w27_adapter_1f12c3_p8_extra_or_env_flag(
        extra, json_string("require_mention"), json_string("true"), json_bool(false), env_raw);
}

json_t *w27_adapter_1f12c3_p8_discord_free_response_auto_thread(const json_t *extra, const json_t *env_raw)
{
    return w27_adapter_1f12c3_p8_extra_or_env_flag(
        extra, json_string("free_response_auto_thread"), json_string("false"), json_bool(true), env_raw);
}

json_t *w27_adapter_1f12c3_p8_discord_max_attachment_bytes(const json_t *extra, const json_t *env_raw)
{
    const double dflt = 32.0 * 1024.0 * 1024.0;
    json_t *configured = NULL;
    long long v;
    if (w27a1f_is_obj(extra) && json_has(extra, "max_attachment_bytes"))
        configured = json_copy(json_obj_get(extra, "max_attachment_bytes"));
    if (w27a1f_is_null(configured)) {
        json_free(configured);
        configured = NULL;
        if (env_raw) {
            char *env = w27a1f_strip_dup(w27a1f_str_or(env_raw, ""));
            if (env && env[0]) configured = json_string(env);
            free(env);
        }
    }
    if (!configured || (w27a1f_is_str(configured) && (!configured->str_val || !configured->str_val[0]))) {
        json_free(configured);
        return json_number(dflt);
    }
    if (!w27a1f_pyint_try(configured, &v)) {
        json_free(configured);
        return json_number(dflt);
    }
    json_free(configured);
    return json_number((double)(v < 0 ? 0 : v));
}

json_t *w27_adapter_1f12c3_p8_is_discord_voice_message_attachment(const json_t *marker, const json_t *duration,
                                                                  const json_t *waveform)
{
    if (!w27a1f_is_null(marker)) return json_bool(w27a1f_truthy(marker) ? true : false);
    if (!w27a1f_is_null(duration) && !w27a1f_is_null(waveform)) return json_bool(true);
    return json_bool(false);
}

/* ── gate p9: gate env / csv lists (adapter.py:4883-4920) ─────────────── */

json_t *w27_adapter_1f12c3_p9_gate_env(const json_t *snapshot, const json_t *name, const json_t *dflt,
                                       const json_t *env_raw)
{
    const char *n = w27a1f_str_or(name, "");
    if (w27a1f_is_obj(snapshot) && json_has(snapshot, n)) {
        json_t *v = json_obj_get(snapshot, n);
        if (w27a1f_truthy(v)) return w27a1f_json_own(w27a1f_pystr(v));
        return w27a1f_json_own(w27a1f_pystr(dflt));
    }
    if (env_raw) {
        char *env = w27a1f_strip_dup(w27a1f_str_or(env_raw, ""));
        json_t *r;
        if (!env || !env[0]) { free(env); return w27a1f_json_own(w27a1f_pystr(dflt)); }
        r = json_string(env);
        free(env);
        return r;
    }
    return w27a1f_json_own(w27a1f_pystr(dflt));
}

json_t *w27_adapter_1f12c3_p9_gate_raw(const json_t *env_val, const json_t *extra, const json_t *extra_key)
{
    if (w27a1f_truthy(env_val)) return w27a1f_json_own(w27a1f_pystr(env_val));
    if (w27a1f_is_obj(extra) && json_has(extra, w27a1f_str_or(extra_key, "")))
        return json_copy(json_obj_get(extra, w27a1f_str_or(extra_key, "")));
    return json_null();
}

/* _decode_json_list_literal: '["a","b"]' -> array, else NULL (owned when != raw) */
json_t *w27a1f_try_decode_list(const json_t *raw)
{
    const char *s;
    if (!w27a1f_is_str(raw) || !raw->str_val) return NULL;
    s = raw->str_val;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s != '[') return NULL;
    {
        json_t *parsed = json_parse(s, NULL);
        if (parsed && parsed->type == JSON_ARRAY) return parsed;
        if (parsed) json_free(parsed);
    }
    return NULL;
}

json_t *w27_adapter_1f12c3_p9_gate_csv_set(const json_t *raw)
{
    char **items;
    size_t n = 0, cap = 8, i;

    if (w27a1f_is_null(raw)) return json_array();
    items = (char **)malloc(sizeof(char *) * cap);
    if (!items) return json_array();

    if (w27a1f_is_arr(raw)) {
        for (i = 0; i < json_len(raw); i++) {
            char *s = w27a1f_strip_dup(w27a1f_pystr(json_get(raw, i)));
            if (!s || !s[0]) { free(s); continue; }
            if (n + 1 >= cap) { cap *= 2; items = (char **)realloc(items, sizeof(char *) * cap); }
            items[n++] = s;
        }
    } else {
        json_t *decoded = w27a1f_try_decode_list(raw);
        if (decoded) {
            json_t *r = w27_adapter_1f12c3_p9_gate_csv_set(decoded);
            json_free(decoded);
            free(items);
            return r;
        }
        {
            char *text = w27a1f_pystr(raw);
            char *p = text;
            if (!text) { free(items); return json_array(); }
            for (;;) {
                char *comma = strchr(p, ',');
                char *part;
                if (comma) *comma = '\0';
                part = w27a1f_strip_dup(p);
                if (part && part[0]) {
                    if (n + 1 >= cap) { cap *= 2; items = (char **)realloc(items, sizeof(char *) * cap); }
                    items[n++] = part;
                } else free(part);
                if (!comma) break;
                p = comma + 1;
            }
            free(text);
        }
    }
    {
        json_t *out = w27a1f_sorted_unique(items, n);
        free(items);
        return out;
    }
}

json_t *w27_adapter_1f12c3_p9_get_allowed_channels(const json_t *env_val, const json_t *extra)
{
    json_t *raw = w27_adapter_1f12c3_p9_gate_raw(env_val, extra, json_string("allowed_channels"));
    json_t *r = w27_adapter_1f12c3_p9_gate_csv_set(raw);
    json_free(raw);
    return r;
}

json_t *w27_adapter_1f12c3_p9_get_ignored_channels(const json_t *env_val, const json_t *extra)
{
    json_t *raw = w27_adapter_1f12c3_p9_gate_raw(env_val, extra, json_string("ignored_channels"));
    json_t *r = w27_adapter_1f12c3_p9_gate_csv_set(raw);
    json_free(raw);
    return r;
}

json_t *w27_adapter_1f12c3_p9_get_no_thread_channels(const json_t *env_val, const json_t *extra)
{
    json_t *raw = w27_adapter_1f12c3_p9_gate_raw(env_val, extra, json_string("no_thread_channels"));
    json_t *r = w27_adapter_1f12c3_p9_gate_csv_set(raw);
    json_free(raw);
    return r;
}

/* ── gate p10: identity sets and bot-tag timing (adapter.py:4929-4960) ── */

json_t *w27_adapter_1f12c3_p10_resolved_allowlist_user_ids(const json_t *ids)
{
    char **items;
    size_t n = 0, i;
    json_t *out;
    if (!w27a1f_is_arr(ids)) return json_array();
    items = (char **)malloc(sizeof(char *) * (json_len(ids) + 1));
    if (!items) return json_array();
    for (i = 0; i < json_len(ids); i++) {
        char *s = w27a1f_pystr(json_get(ids, i));
        if (s) items[n++] = s;
    }
    out = w27a1f_sorted_unique(items, n);
    free(items);
    return out;
}

json_t *w27_adapter_1f12c3_p10_discord_allow_all_users(const json_t *env_val, const json_t *extra)
{
    static const char *const yes[] = { "true", "1", "yes" };
    json_t *raw = w27_adapter_1f12c3_p9_gate_raw(env_val, extra, json_string("allow_all_users"));
    char *text = w27a1f_truthy(raw) ? w27a1f_pystr(raw) : w27a1f_strdup("");
    char *stripped;
    size_t i;
    int hit;
    json_free(raw);
    stripped = w27a1f_strip_dup(text);
    free(text);
    if (!stripped) return json_bool(false);
    for (i = 0; stripped[i]; i++) stripped[i] = (char)tolower((unsigned char)stripped[i]);
    hit = w27a1f_in_list_ci(stripped, yes, sizeof yes / sizeof yes[0]);
    free(stripped);
    return json_bool(hit ? true : false);
}

json_t *w27_adapter_1f12c3_p10_gateway_allow_all_users(const json_t *env_val)
{
    static const char *const yes[] = { "true", "1", "yes" };
    char *text = w27a1f_strip_dup(w27a1f_str_or(env_val, ""));
    size_t i;
    int hit;
    if (!text) return json_bool(false);
    for (i = 0; text[i]; i++) text[i] = (char)tolower((unsigned char)text[i]);
    hit = w27a1f_in_list_ci(text, yes, sizeof yes / sizeof yes[0]);
    free(text);
    return json_bool(hit ? true : false);
}

json_t *w27_adapter_1f12c3_p10_get_allow_bots(const json_t *env_val, const json_t *extra)
{
    json_t *raw = w27_adapter_1f12c3_p9_gate_raw(env_val, extra, json_string("allow_bots"));
    char *text = w27a1f_truthy(raw) ? w27a1f_pystr(raw) : w27a1f_strdup("none");
    char *lower, *stripped;
    size_t i;
    json_free(raw);
    if (!text) return json_string("none");
    lower = text;
    for (i = 0; lower[i]; i++) lower[i] = (char)tolower((unsigned char)lower[i]);
    stripped = w27a1f_strip_dup(lower);
    free(text);
    if (!stripped || !stripped[0]) { free(stripped); return json_string("none"); }
    return w27a1f_json_own(stripped);
}

json_t *w27_adapter_1f12c3_p10_bot_tag_debounce_key(const json_t *channel_id, const json_t *author_id)
{
    w27a1f_sb b;
    char *cid = channel_id ? w27a1f_pystr(channel_id) : w27a1f_strdup("");
    char *aid = author_id ? w27a1f_pystr(author_id) : w27a1f_strdup("");
    w27a1f_sb_init(&b);
    w27a1f_sb_puts(&b, cid ? cid : "");
    w27a1f_sb_putc(&b, ':');
    w27a1f_sb_puts(&b, aid ? aid : "");
    free(cid); free(aid);
    return w27a1f_json_own(w27a1f_sb_take(&b));
}

json_t *w27_adapter_1f12c3_p10_bot_tag_window_seconds(const json_t *delay, const json_t *split_delay)
{
    double a = w27a1f_num_or(delay, 0.0), b = w27a1f_num_or(split_delay, 0.0);
    return json_number(a > b ? a : b);
}

/* ── gate p11: bot-tag debounce and mention parsing (adapter.py:4961-5040) ── */

json_t *w27_adapter_1f12c3_p11_record_bot_tag_debounce(const json_t *delay, const json_t *automated,
                                                       const json_t *mentioned, const json_t *now,
                                                       const json_t *split_delay)
{
    double d = w27a1f_num_or(delay, 0.0), n = w27a1f_num_or(now, 0.0), s = w27a1f_num_or(split_delay, 0.0);
    if (d <= 0.0 || !w27a1f_truthy(automated) || !w27a1f_truthy(mentioned)) return json_null();
    return json_number(n + (d > s ? d : s));
}

json_t *w27_adapter_1f12c3_p11_is_bot_tag_debounce_continuation(const json_t *delay, const json_t *automated,
                                                                const json_t *until, const json_t *now,
                                                                const json_t *window)
{
    double d = w27a1f_num_or(delay, 0.0), u = w27a1f_num_or(until, 0.0);
    double n = w27a1f_num_or(now, 0.0);
    if (d <= 0.0 || !w27a1f_truthy(automated)) return json_bool(false);
    if (u <= n) return json_bool(false);
    return json_bool(true);
}

json_t *w27_adapter_1f12c3_p11_discord_free_response_channels(const json_t *extra, const json_t *env_raw)
{
    json_t *raw = NULL;
    json_t *out;
    if (w27a1f_is_obj(extra) && json_has(extra, "free_response_channels"))
        raw = json_copy(json_obj_get(extra, "free_response_channels"));
    if (w27a1f_is_null(raw)) {
        json_free(raw);
        raw = env_raw ? json_copy(env_raw) : json_null();
    }
    out = w27_adapter_1f12c3_p9_gate_csv_set(raw);
    json_free(raw);
    return out;
}

json_t *w27_adapter_1f12c3_p11_raw_mentioned_user_ids(const json_t *content)
{
    const char *s = w27a1f_str_or(content, "");
    char **items;
    size_t n = 0, i = 0;
    json_t *out;
    items = (char **)malloc(sizeof(char *) * (strlen(s) / 3 + 2));
    if (!items) return json_array();
    while (s[i]) {
        if (s[i] == '<' && s[i + 1] == '@') {
            size_t j = i + 2;
            if (s[j] == '!') j++;
            {
                size_t start = j;
                while (isdigit((unsigned char)s[j])) j++;
                if (j > start && s[j] == '>') {
                    items[n] = (char *)malloc(j - start + 1);
                    if (items[n]) {
                        memcpy(items[n], s + start, j - start);
                        items[n][j - start] = '\0';
                        n++;
                    }
                    i = j + 1;
                    continue;
                }
            }
        }
        i++;
    }
    out = w27a1f_sorted_unique(items, n);
    free(items);
    return out;
}

json_t *w27_adapter_1f12c3_p11_self_is_raw_mentioned(const json_t *client_user_id, const json_t *content)
{
    json_t *ids, *self;
    char *selfs;
    int hit;
    if (!w27a1f_truthy(client_user_id)) return json_bool(false);
    ids = w27_adapter_1f12c3_p11_raw_mentioned_user_ids(content);
    selfs = w27a1f_pystr(client_user_id);
    self = json_string(selfs ? selfs : "");
    free(selfs);
    hit = w27a1f_val_in_list(self, ids);
    json_free(self);
    json_free(ids);
    return json_bool(hit ? true : false);
}

json_t *w27_adapter_1f12c3_p12_discord_bots_require_inline_mention(const json_t *extra, const json_t *env_raw)
{
    static const char *const yes[] = { "true", "1", "yes", "on" };
    json_t *configured = w27a1f_is_obj(extra) ? json_obj_get(extra, "bots_require_inline_mention") : NULL;
    if (w27a1f_is_str(configured)) {
        char *text = w27a1f_strdup(configured->str_val ? configured->str_val : "");
        size_t i;
        int hit;
        if (!text) return json_bool(false);
        for (i = 0; text[i]; i++) text[i] = (char)tolower((unsigned char)text[i]);
        hit = w27a1f_in_list_ci(text, yes, sizeof yes / sizeof yes[0]);
        free(text);
        return json_bool(hit ? true : false);
    }
    return w27_adapter_1f12c3_p8_extra_or_env_flag(
        extra, json_string("bots_require_inline_mention"), json_string("true"), json_bool(true), env_raw);
}

/* ── gate p12: channel keys and history backfill (adapter.py:5041-5100) ── */

json_t *w27_adapter_1f12c3_p12_discord_channel_keys_from_channel(const json_t *channel,
                                                                 const json_t *parent_channel_id)
{
    char **items;
    size_t n = 0, cap = 8, i;
    json_t *out;
    json_t *cid = w27a1f_is_obj(channel) ? json_obj_get(channel, "id") : NULL;
    json_t *cname = w27a1f_is_obj(channel) ? json_obj_get(channel, "name") : NULL;
    json_t *pid = w27a1f_is_obj(channel) ? json_obj_get(channel, "parent_id") : NULL;
    json_t *parent = w27a1f_is_obj(channel) ? json_obj_get(channel, "parent") : NULL;
    json_t *pname = w27a1f_is_obj(parent) ? json_obj_get(parent, "name") : NULL;

    items = (char **)malloc(sizeof(char *) * cap);
    if (!items) return json_array();
#define W27A1F_PUSH(sv) do {                                                                 \
        char *tmp_ = w27a1f_strip_dup(sv);                                                   \
        if (tmp_ && tmp_[0]) {                                                               \
            if (n + 1 >= cap) { cap *= 2; items = (char **)realloc(items, sizeof(char *) * cap); } \
            items[n++] = tmp_;                                                               \
        } else free(tmp_);                                                                   \
    } while (0)

    if (!w27a1f_is_null(cid)) {
        char *s = w27a1f_pystr(cid);
        if (s) { if (n + 1 >= cap) { cap *= 2; items = (char **)realloc(items, sizeof(char *) * cap); } items[n++] = s; }
    }
    {
        char *cnames = (cname && json_has(channel, "name")) ? w27a1f_pystr(cname) : w27a1f_strdup("");
        char *cname_trim = w27a1f_strip_dup(cnames ? cnames : "");
        if (cname_trim && cname_trim[0]) {
            w27a1f_sb b;
            W27A1F_PUSH(cname_trim);
            w27a1f_sb_init(&b);
            w27a1f_sb_putc(&b, '#');
            w27a1f_sb_puts(&b, cname_trim);
            W27A1F_PUSH(b.buf ? b.buf : "");
            w27a1f_sb_free(&b);
        }
        free(cname_trim);
        free(cnames);
    }
    if (w27a1f_truthy(parent_channel_id)) {
        char *s = w27a1f_pystr(parent_channel_id);
        if (s) { if (n + 1 >= cap) { cap *= 2; items = (char **)realloc(items, sizeof(char *) * cap); } items[n++] = s; }
    } else if (w27a1f_truthy(pid)) {
        char *s = w27a1f_pystr(pid);
        if (s) { if (n + 1 >= cap) { cap *= 2; items = (char **)realloc(items, sizeof(char *) * cap); } items[n++] = s; }
    }
    {
        char *pnames = (pname && json_has(parent, "name")) ? w27a1f_pystr(pname) : w27a1f_strdup("");
        char *pname_trim = w27a1f_strip_dup(pnames ? pnames : "");
        if (pname_trim && pname_trim[0]) {
            w27a1f_sb b;
            W27A1F_PUSH(pname_trim);
            w27a1f_sb_init(&b);
            w27a1f_sb_putc(&b, '#');
            w27a1f_sb_puts(&b, pname_trim);
            W27A1F_PUSH(b.buf ? b.buf : "");
            w27a1f_sb_free(&b);
        }
        free(pname_trim);
        free(pnames);
    }
#undef W27A1F_PUSH
    out = w27a1f_sorted_unique(items, n);
    free(items);
    (void)i;
    return out;
}

json_t *w27_adapter_1f12c3_p12_discord_channel_keys(const json_t *channel, const json_t *parent_channel_id)
{
    return w27_adapter_1f12c3_p12_discord_channel_keys_from_channel(channel, parent_channel_id);
}

json_t *w27_adapter_1f12c3_p12_discord_thread_require_mention(const json_t *extra, const json_t *env_raw)
{
    return w27_adapter_1f12c3_p8_extra_or_env_flag(
        extra, json_string("thread_require_mention"), json_string("false"), json_bool(true), env_raw);
}

json_t *w27_adapter_1f12c3_p12_discord_history_backfill(const json_t *extra, const json_t *env_raw)
{
    return w27_adapter_1f12c3_p8_extra_or_env_flag(
        extra, json_string("history_backfill"), json_string("true"), json_bool(true), env_raw);
}

json_t *w27_adapter_1f12c3_p12_discord_history_backfill_limit(const json_t *extra, const json_t *env_raw)
{
    json_t *configured = w27a1f_is_obj(extra) ? json_obj_get(extra, "history_backfill_limit") : NULL;
    long long v;
    if (!w27a1f_is_null(configured) && w27a1f_pyint_try(configured, &v)) return json_number((double)v);
    {
        char *raw = w27a1f_strip_dup(w27a1f_str_or(env_raw, "50"));
        long long e;
        int ok = 0;
        if (raw) {
            char *end = NULL;
            const char *s = w27a1f_lstrip(raw);
            if (*s) {
                e = strtoll(s, &end, 10);
                if (end && end != s) {
                    while (end && *end && isspace((unsigned char)*end)) end++;
                    if (!*end) { v = e; ok = 1; }
                }
            }
        }
        free(raw);
        if (ok) return json_number((double)v);
    }
    return json_number(50);
}

/* ── gate p13: prompt/approval shaping and thread parts (adapter.py:3224, 5317-5620) ── */

json_t *w27_adapter_1f12c3_p13_thread_created(const json_t *thread, const json_t *name)
{
    json_t *o = json_object();
    json_t *tid = w27a1f_is_obj(thread) ? json_obj_get(thread, "id") : NULL;
    json_t *tname = w27a1f_is_obj(thread) ? json_obj_get(thread, "name") : NULL;
    json_set(o, "success", json_bool(true));
    json_set(o, "thread_id", w27a1f_json_own(tid ? w27a1f_pystr(tid) : w27a1f_strdup("")));
    if (w27a1f_truthy(tname)) json_set(o, "thread_name", json_copy(tname));
    else json_set(o, "thread_name", w27a1f_copy_or_null(name));
    return o;
}

json_t *w27_adapter_1f12c3_p13_self_contained_prompt_content(const json_t *header, const json_t *body,
                                                             const json_t *code_block, const json_t *tail,
                                                             const json_t *max_len, const json_t *marker)
{
    char *bodys = w27a1f_truthy(body) ? w27a1f_pystr(body) : w27a1f_strdup("");
    w27a1f_sb prefix, suffix, trunc_marker, out;
    long long budget;
    size_t body_cps, prefix_cps, suffix_cps, marker_cps;

    if (!bodys) return json_null();
    w27a1f_sb_init(&prefix);
    w27a1f_sb_init(&suffix);
    w27a1f_sb_init(&trunc_marker);
    w27a1f_sb_init(&out);
    w27a1f_sb_puts(&prefix, w27a1f_str_or(header, ""));
    if (w27a1f_truthy(code_block)) {
        w27a1f_sb_puts(&prefix, "\n```bash\n");
        w27a1f_sb_puts(&suffix, "\n```");
        w27a1f_sb_puts(&suffix, w27a1f_str_or(tail, ""));
    } else {
        w27a1f_sb_puts(&prefix, "\n\n");
        w27a1f_sb_puts(&suffix, w27a1f_str_or(tail, ""));
    }
    w27a1f_sb_putc(&trunc_marker, '\n');
    w27a1f_sb_puts(&trunc_marker, w27a1f_str_or(marker, ""));

    body_cps = 0;
    { size_t k = 0; while (bodys[k]) { size_t adv = 1; w27a1f_cp_at(bodys, k, &adv); k += adv; body_cps++; } }
    prefix_cps = 0;
    { size_t k = 0; while (prefix.buf && prefix.buf[k]) { size_t adv = 1; w27a1f_cp_at(prefix.buf, k, &adv); k += adv; prefix_cps++; } }
    suffix_cps = 0;
    { size_t k = 0; while (suffix.buf && suffix.buf[k]) { size_t adv = 1; w27a1f_cp_at(suffix.buf, k, &adv); k += adv; suffix_cps++; } }
    marker_cps = 0;
    { size_t k = 0; while (trunc_marker.buf && trunc_marker.buf[k]) { size_t adv = 1; w27a1f_cp_at(trunc_marker.buf, k, &adv); k += adv; marker_cps++; } }

    budget = w27a1f_int_or(max_len, 2000) - (long long)prefix_cps - (long long)suffix_cps;
    if (budget < 0) budget = 0;
    if ((long long)body_cps > budget) {
        long long keep = budget - (long long)marker_cps;
        size_t cut;
        if (keep < 0) keep = 0;
        cut = w27a1f_cp_prefix(bodys, (size_t)keep);
        w27a1f_sb_putn(&out, bodys, cut);
        w27a1f_sb_puts(&out, trunc_marker.buf ? trunc_marker.buf : "");
    } else {
        w27a1f_sb_puts(&out, bodys);
    }
    {
        w27a1f_sb final;
        w27a1f_sb_init(&final);
        w27a1f_sb_puts(&final, prefix.buf ? prefix.buf : "");
        w27a1f_sb_puts(&final, out.buf ? out.buf : "");
        w27a1f_sb_puts(&final, suffix.buf ? suffix.buf : "");
        w27a1f_sb_free(&prefix);
        w27a1f_sb_free(&suffix);
        w27a1f_sb_free(&trunc_marker);
        w27a1f_sb_free(&out);
        free(bodys);
        return w27a1f_json_own(w27a1f_sb_take(&final));
    }
}

json_t *w27_adapter_1f12c3_p13_approval_mention_content(const json_t *flag, const json_t *allowed_ids)
{
    char **items;
    size_t n = 0, i;
    w27a1f_sb b;
    json_t *out;
    if (!w27a1f_truthy(flag)) return json_null();
    items = (char **)malloc(sizeof(char *) * ((w27a1f_is_arr(allowed_ids) ? json_len(allowed_ids) : 0) + 1));
    if (!items) return json_null();
    for (i = 0; w27a1f_is_arr(allowed_ids) && i < json_len(allowed_ids); i++) {
        char *s = w27a1f_pystr(json_get(allowed_ids, i));
        if (s && w27a1f_all_digits(s)) items[n++] = s; else free(s);
    }
    if (!n) { free(items); return json_null(); }
    qsort(items, n, sizeof(char *), w27a1f_cmp_str);
    w27a1f_sb_init(&b);
    for (i = 0; i < n; i++) {
        if (i) w27a1f_sb_putc(&b, ' ');
        w27a1f_sb_puts(&b, "<@");
        w27a1f_sb_puts(&b, items[i]);
        w27a1f_sb_putc(&b, '>');
        free(items[i]);
    }
    free(items);
    out = w27a1f_json_own(w27a1f_sb_take(&b));
    return out;
}

json_t *w27_adapter_1f12c3_p13_forum_thread_parts(const json_t *thread)
{
    json_t *out = json_array();
    json_t *tid = w27a1f_is_obj(thread) ? json_obj_get(thread, "id") : NULL;
    json_t *msg = w27a1f_is_obj(thread) ? json_obj_get(thread, "message") : NULL;
    char *thread_id;
    char *message_id;
    if (!w27a1f_is_obj(thread)) return out;
    if (!w27a1f_truthy(json_obj_get(thread, "has_send"))) {
        json_t *inner = json_obj_get(thread, "thread");
        if (w27a1f_is_obj(inner)) {
            json_t *inner_id = json_obj_get(inner, "id");
            if (!w27a1f_is_null(inner_id)) tid = inner_id;
        }
    }
    thread_id = w27a1f_pystr(tid);
    if (!msg) message_id = w27a1f_strdup(thread_id ? thread_id : "");
    else {
        json_t *mid = json_obj_get(msg, "id");
        message_id = mid ? w27a1f_pystr(mid) : w27a1f_strdup(thread_id ? thread_id : "");
    }
    json_append(out, w27a1f_json_own(thread_id));
    json_append(out, w27a1f_json_own(message_id));
    return out;
}

/* ── gate p14: forum titles, REST response seams, YAML seeding (adapter.py:6811-7188) ── */

json_t *w27_adapter_1f12c3_p14_derive_forum_thread_name(const json_t *message, const json_t *default_title)
{
    char *s = w27a1f_strip_dup(w27a1f_str_or(message, ""));
    char *first;
    size_t i;
    if (!s) return json_copy(default_title);
    for (i = 0; s[i]; i++) if (s[i] == '\n') { s[i] = '\0'; break; }
    first = w27a1f_strip_dup(s);
    free(s);
    if (!first) return json_copy(default_title);
    {
        size_t k = 0;
        while (first[k] == '#') k++;
        if (k) {
            char *trimmed = w27a1f_strip_dup(first + k);
            free(first);
            first = trimmed;
            if (!first) return json_copy(default_title);
        }
    }
    if (!first[0]) { free(first); return json_copy(default_title); }
    {
        size_t cut = w27a1f_cp_prefix(first, 100);
        json_t *r = w27a1f_json_strn(first, cut);
        free(first);
        return r;
    }
}

json_t *w27_adapter_1f12c3_p14_standalone_response_encoding(const json_t *encoding)
{
    if (w27a1f_truthy(encoding)) return w27a1f_json_own(w27a1f_pystr(encoding));
    return json_string("utf-8");
}

json_t *w27_adapter_1f12c3_p14_standalone_warn_missing_media(const json_t *path)
{
    w27a1f_sb b;
    char *p = w27a1f_pystr(w27a1f_is_null(path) ? json_string("") : path);
    w27a1f_sb_init(&b);
    w27a1f_sb_puts(&b, "Media file not found, skipping: ");
    w27a1f_sb_puts(&b, p ? p : "");
    free(p);
    return w27a1f_json_own(w27a1f_sb_take(&b));
}

/* _csv(value): list -> comma-joined, else str(value) */
static char *w27a1f_csv_value(const json_t *v)
{
    w27a1f_sb b;
    size_t i;
    if (w27a1f_is_arr(v)) {
        w27a1f_sb_init(&b);
        for (i = 0; i < json_len(v); i++) {
            char *s = w27a1f_pystr(json_get(v, i));
            if (i) w27a1f_sb_putc(&b, ',');
            w27a1f_sb_puts(&b, s ? s : "");
            free(s);
        }
        return w27a1f_sb_take(&b);
    }
    return w27a1f_pystr(v);
}

static void w27a1f_seed(json_t *seeded, const char *key, json_t *value)
{
    if (key && value) json_set(seeded, key, value);
}

static void w27a1f_seed_str_lower(json_t *seeded, const char *key, const json_t *value)
{
    char *s = w27a1f_pystr(value);
    size_t i;
    if (!s) return;
    for (i = 0; s[i]; i++) s[i] = (char)tolower((unsigned char)s[i]);
    w27a1f_seed(seeded, key, w27a1f_json_own(s));
}

json_t *w27_adapter_1f12c3_p14_apply_yaml_config(const json_t *yaml_cfg, const json_t *discord_cfg)
{
    static const char *const bool_keys[][2] = {
        { "require_mention", "DISCORD_REQUIRE_MENTION" },
        { "thread_require_mention", "DISCORD_THREAD_REQUIRE_MENTION" },
        { "bots_require_inline_mention", "DISCORD_BOTS_REQUIRE_INLINE_MENTION" }
    };
    static const char *const ws_keys[][2] = {
        { "websocket_liveness_interval_seconds", "liveness_interval_seconds" },
        { "websocket_liveness_failure_threshold", "liveness_failure_threshold" },
        { "websocket_heartbeat_ack_max_age_seconds", NULL },
        { "websocket_max_latency_seconds", NULL },
        { "websocket_event_max_silence_seconds", NULL }
    };
    static const char *const gate_keys[][3] = {
        { "allow_from", "platform", NULL },
        { "allowed_roles", "platform", NULL },
        { "allow_all_users", "platform", "lower" },
        { "allow_bots", "platform", "lower" },
        { "free_response_channels", "", NULL },
        { "ignored_channels", "", NULL },
        { "allowed_channels", "", NULL },
        { "no_thread_channels", "", NULL }
    };
    json_t *seeded = json_object();
    json_t *platform_extra = NULL;
    const json_t *platforms, *dp, *cand;
    size_t i;

    if (!w27a1f_is_obj(discord_cfg)) discord_cfg = NULL;
    if (w27a1f_is_obj(yaml_cfg)) {
        platforms = json_obj_get(yaml_cfg, "platforms");
        if (w27a1f_is_obj(platforms)) {
            dp = json_obj_get(platforms, "discord");
            if (w27a1f_is_obj(dp)) {
                cand = json_obj_get(dp, "extra");
                if (w27a1f_is_obj(cand)) platform_extra = (json_t *)cand;
            }
        }
    }
    for (i = 0; i < sizeof bool_keys / sizeof bool_keys[0]; i++) {
        if (discord_cfg && json_has(discord_cfg, bool_keys[i][0])) {
            json_t *v = json_obj_get(discord_cfg, bool_keys[i][0]);
            w27a1f_seed(seeded, bool_keys[i][0], json_copy(v));
        }
    }
    for (i = 0; i < sizeof gate_keys / sizeof gate_keys[0]; i++) {
        const char *key = gate_keys[i][0];
        int from_platform = !strcmp(gate_keys[i][1], "platform");
        int lower = gate_keys[i][2] != NULL;
        json_t *value = NULL;
        char *text;
        if (discord_cfg && json_has(discord_cfg, key)) value = json_obj_get(discord_cfg, key);
        else if (from_platform && platform_extra && json_has(platform_extra, key))
            value = json_obj_get(platform_extra, key);
        if (w27a1f_is_null(value)) continue;
        text = lower ? w27a1f_pystr(value) : w27a1f_csv_value(value);
        if (!text) continue;
        if (lower) { size_t k; for (k = 0; text[k]; k++) text[k] = (char)tolower((unsigned char)text[k]); }
        w27a1f_seed(seeded, key, w27a1f_json_own(text));
    }
    {
        json_t *am = NULL;
        if (discord_cfg && json_has(discord_cfg, "approval_mentions")) am = json_obj_get(discord_cfg, "approval_mentions");
        else if (platform_extra) am = json_obj_get(platform_extra, "approval_mentions");
        if (!w27a1f_is_null(am)) w27a1f_seed(seeded, "approval_mentions", json_copy(am));
    }
    {
        static const char *const plain[] = { "auto_thread", "free_response_auto_thread", "reactions" };
        for (i = 0; i < sizeof plain / sizeof plain[0]; i++) {
            if (discord_cfg && json_has(discord_cfg, plain[i]))
                w27a1f_seed(seeded, plain[i], json_copy(json_obj_get(discord_cfg, plain[i])));
        }
    }
    if (discord_cfg) {
        json_t *bf = json_obj_get(discord_cfg, "missed_message_backfill");
        if (w27a1f_is_obj(bf)) w27a1f_seed(seeded, "missed_message_backfill", json_copy(bf));
    }
    if (discord_cfg && json_has(discord_cfg, "history_backfill"))
        w27a1f_seed(seeded, "history_backfill", json_copy(json_obj_get(discord_cfg, "history_backfill")));
    {
        json_t *hbl = discord_cfg ? json_obj_get(discord_cfg, "history_backfill_limit") : NULL;
        if (!w27a1f_is_null(hbl)) w27a1f_seed(seeded, "history_backfill_limit", json_copy(hbl));
    }
    {
        json_t *am = discord_cfg ? json_obj_get(discord_cfg, "allow_mentions") : NULL;
        if (w27a1f_is_obj(am)) w27a1f_seed(seeded, "allow_mentions", json_copy(am));
    }
    {
        json_t *dx = discord_cfg ? json_obj_get(discord_cfg, "extra") : NULL;
        json_t *rtm = NULL;
        if (discord_cfg && json_has(discord_cfg, "reply_to_mode")) rtm = json_obj_get(discord_cfg, "reply_to_mode");
        else if (w27a1f_is_obj(dx)) rtm = json_obj_get(dx, "reply_to_mode");
        (void)rtm;   /* env-bridge only: never seeded into extra */
    }
    for (i = 0; i < sizeof ws_keys / sizeof ws_keys[0]; i++) {
        json_t *value = NULL;
        if (discord_cfg && json_has(discord_cfg, ws_keys[i][0])) value = json_obj_get(discord_cfg, ws_keys[i][0]);
        else if (discord_cfg) {
            json_t *dx = json_obj_get(discord_cfg, "extra");
            if (w27a1f_is_obj(dx) && json_has(dx, ws_keys[i][0])) value = json_obj_get(dx, ws_keys[i][0]);
        }
        if (w27a1f_is_null(value) && ws_keys[i][1]) {
            if (discord_cfg && json_has(discord_cfg, ws_keys[i][1])) value = json_obj_get(discord_cfg, ws_keys[i][1]);
        }
        if (!w27a1f_is_null(value)) w27a1f_seed(seeded, ws_keys[i][0], json_copy(value));
    }
    if (json_object_size(seeded) == 0) { json_free(seeded); return json_null(); }
    return seeded;
}
