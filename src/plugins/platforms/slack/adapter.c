/*
 * adapter.c - C11 port of upstream plugins/platforms/slack/adapter.py.
 *
 * 387 KB, 329 defs. What is ported here is exactly the part of the Slack
 * adapter that is a pure function of values: mrkdwn/block-kit conversion,
 * table fencing/alignment, text chunk descriptors, thread-key building,
 * retry/backoff arithmetic, JSON shaping, label/format helpers and the
 * routing/mention policy predicates. Seamed (never started here): every
 * Slack Web API call (chat.*, conversations.*, files.*, assistant.*,
 * reactions.*), the Socket Mode websocket and its watchdog, aiohttp/httpx
 * downloads, the credential vault/env reads, the session store, and the
 * plugin register() wiring.
 *
 * PoP: w27_p1_unfurl_kwargs             @ plugins/platforms/slack/adapter.py:_slack_unfurl_kwargs
 * PoP: w27_p1_is_table_row              @ plugins/platforms/slack/adapter.py:_is_table_row
 * PoP: w27_p1_disp_width                @ plugins/platforms/slack/adapter.py:_disp_width
 * PoP: w27_p1_pad                       @ plugins/platforms/slack/adapter.py:_pad
 * PoP: w27_p1_split_table_row           @ plugins/platforms/slack/adapter.py:_split_table_row
 * PoP: w27_p1_align_table               @ plugins/platforms/slack/adapter.py:_align_table
 * PoP: w27_p1_wrap_markdown_tables      @ plugins/platforms/slack/adapter.py:_wrap_markdown_tables
 * PoP: w27_p1_permalink_path            @ plugins/platforms/slack/adapter.py:_slack_permalink_path
 * PoP: w27_p1_str_or_empty              @ plugins/platforms/slack/adapter.py:_str_or_empty
 * PoP: w27_p1_int_or_zero               @ plugins/platforms/slack/adapter.py:_int_or_zero
 * PoP: w27_p1_first_truthy              @ plugins/platforms/slack/adapter.py:_first_truthy
 * PoP: w27_p2_collect_block_mentions    @ plugins/platforms/slack/adapter.py:_collect_slack_block_mentions
 * PoP: w27_p2_mention_detection_text    @ plugins/platforms/slack/adapter.py:_slack_mention_detection_text
 * PoP: w27_p2_slack_str_field           @ plugins/platforms/slack/adapter.py:_slack_str_field
 * PoP: w27_p2_render_inline_element     @ plugins/platforms/slack/adapter.py:_render_slack_inline_element
 * PoP: w27_p2_render_inline_elements    @ plugins/platforms/slack/adapter.py:_render_inline_elements
 * PoP: w27_p2_extract_text_blocks       @ plugins/platforms/slack/adapter.py:_extract_text_from_slack_blocks
 * PoP: w27_p2_table_cell_text           @ plugins/platforms/slack/adapter.py:_collect_slack_table_cell_text
 * PoP: w27_p2_render_table_block        @ plugins/platforms/slack/adapter.py:_render_slack_table_block
 * PoP: w27_p2_slack_error_is            @ plugins/platforms/slack/adapter.py:_slack_error_is
 * PoP: w27_p3_unescape_entities         @ plugins/platforms/slack/adapter.py:_unescape_slack_entities
 * PoP: w27_p3_normalize_dedupe          @ plugins/platforms/slack/adapter.py:_normalize_slack_text_for_dedupe
 * PoP: w27_p3_additional_text           @ plugins/platforms/slack/adapter.py:_extract_additional_text_from_slack_blocks
 * PoP: w27_p3_serialize_blocks          @ plugins/platforms/slack/adapter.py:_serialize_slack_blocks_for_agent
 * PoP: w27_p3_extract_urls              @ plugins/platforms/slack/adapter.py:_extract_urls_from_slack_blocks
 * PoP: w27_p4_resolve_audio_ext         @ plugins/platforms/slack/adapter.py:_resolve_slack_audio_ext
 * PoP: w27_p4_is_voice_clip             @ plugins/platforms/slack/adapter.py:_is_slack_voice_clip
 * PoP: w27_p4_attachment_label          @ plugins/platforms/slack/adapter.py:_attachment_label
 * PoP: w27_p4_file_kind                 @ plugins/platforms/slack/adapter.py:_slack_file_kind
 * PoP: w27_p4_describe_api_error        @ plugins/platforms/slack/adapter.py:SlackAdapter._describe_slack_api_error
 * PoP: w27_p4_describe_download_failure @ plugins/platforms/slack/adapter.py:SlackAdapter._describe_slack_download_failure
 * PoP: w27_p4_retryable_upload_error    @ plugins/platforms/slack/adapter.py:SlackAdapter._is_retryable_upload_error
 * PoP: w27_p5_timestamp_sort_key        @ plugins/platforms/slack/adapter.py:SlackAdapter._slack_timestamp_sort_key
 * PoP: w27_p5_discard_oldest            @ plugins/platforms/slack/adapter.py:SlackAdapter._discard_oldest_by_thread_ts
 * PoP: w27_p5_evict_oldest              @ plugins/platforms/slack/adapter.py:SlackAdapter._evict_oldest_by_ts
 * PoP: w27_p5_trim_dict_entries         @ plugins/platforms/slack/adapter.py:SlackAdapter._trim_oldest_dict_entries
 * PoP: w27_p5_metadata_team_id          @ plugins/platforms/slack/adapter.py:SlackAdapter._metadata_team_id
 * PoP: w27_p5_workspace_event_id        @ plugins/platforms/slack/adapter.py:SlackAdapter._workspace_event_id
 * PoP: w27_p5_workspace_message_marker  @ plugins/platforms/slack/adapter.py:SlackAdapter._workspace_message_marker
 * PoP: w27_p5_truthy_config             @ plugins/platforms/slack/adapter.py:SlackAdapter._truthy_config
 * PoP: w27_p5_is_ignored_channel        @ plugins/platforms/slack/adapter.py:SlackAdapter._is_ignored_channel
 * PoP: w27_p5_workspace_thread_key      @ plugins/platforms/slack/adapter.py:SlackAdapter._workspace_thread_key
 * PoP: w27_p5_agent_view_context_key    @ plugins/platforms/slack/adapter.py:SlackAdapter._agent_view_context_key
 * PoP: w27_p6_task_update_chunk         @ plugins/platforms/slack/adapter.py:SlackAdapter._task_update_chunk
 * PoP: w27_p6_strip_stream_cursor       @ plugins/platforms/slack/adapter.py:SlackAdapter._strip_stream_cursor
 * PoP: w27_p6_stream_key                @ plugins/platforms/slack/adapter.py:SlackAdapter._stream_key
 * PoP: w27_p6_mrkdwn_insensitive        @ plugins/platforms/slack/adapter.py:SlackAdapter._mrkdwn_insensitive
 * PoP: w27_p6_stream_relation           @ plugins/platforms/slack/adapter.py:SlackAdapter._stream_relation
 * PoP: w27_p6_default_status_text       @ plugins/platforms/slack/adapter.py:SlackAdapter._default_status_text
 * PoP: w27_p6_cron_continuable_surface  @ plugins/platforms/slack/adapter.py:SlackAdapter._cron_continuable_surface
 * PoP: w27_p6_slack_allow_bots          @ plugins/platforms/slack/adapter.py:SlackAdapter._slack_allow_bots
 * PoP: w27_p6_api_human_users           @ plugins/platforms/slack/adapter.py:SlackAdapter._slack_api_human_users
 * PoP: w27_p6_event_declares_bot_sender @ plugins/platforms/slack/adapter.py:SlackAdapter._event_declares_bot_sender
 * PoP: w27_p6_format_tool_preview       @ plugins/platforms/slack/adapter.py:SlackAdapter.format_tool_preview
 * PoP: w27_p6_markdown_block_payload    @ plugins/platforms/slack/adapter.py:SlackAdapter._markdown_block_payload
 * PoP: w27_p6_append_feedback_block     @ plugins/platforms/slack/adapter.py:SlackAdapter._append_feedback_block
 * PoP: w27_p7_resolve_thread_ts         @ plugins/platforms/slack/adapter.py:SlackAdapter._resolve_thread_ts
 * PoP: w27_p7_reactions_enabled         @ plugins/platforms/slack/adapter.py:SlackAdapter._reactions_enabled
 * PoP: w27_p7_extra_flag                @ plugins/platforms/slack/adapter.py:SlackAdapter._extra_flag
 * PoP: w27_p7_parse_users_info          @ plugins/platforms/slack/adapter.py:SlackAdapter._parse_users_info
 * PoP: w27_p7_reaction_triggers         @ plugins/platforms/slack/adapter.py:SlackAdapter._slack_reaction_triggers
 * PoP: w27_p7_reaction_trigger_target   @ plugins/platforms/slack/adapter.py:SlackAdapter._slack_reaction_trigger_target
 * PoP: w27_p7_first_file_share          @ plugins/platforms/slack/adapter.py:SlackAdapter._first_file_share
 * PoP: w27_p7_synthetic_reaction_event  @ plugins/platforms/slack/adapter.py:SlackAdapter._synthetic_reaction_event
 * PoP: w27_p7_build_identity_prompt     @ plugins/platforms/slack/adapter.py:SlackAdapter._build_identity_prompt
 * PoP: w27_p8_format_message            @ plugins/platforms/slack/adapter.py:SlackAdapter.format_message
 * PoP: w27_p8_append_block_text         @ plugins/platforms/slack/adapter.py:SlackAdapter._append_block_text
 * PoP: w27_p8_normalize_changed_message @ plugins/platforms/slack/adapter.py:SlackAdapter._normalize_changed_message
 * PoP: w27_p8_append_link_unfurls       @ plugins/platforms/slack/adapter.py:SlackAdapter._append_link_unfurls
 * PoP: w27_p8_media_message_type        @ plugins/platforms/slack/adapter.py:SlackAdapter._media_message_type
 * PoP: w27_p8_session_thread_ts         @ plugins/platforms/slack/adapter.py:SlackAdapter._session_thread_ts
 * PoP: w27_p8_button                    @ plugins/platforms/slack/adapter.py:SlackAdapter._button
 * PoP: w27_p8_interaction_fields        @ plugins/platforms/slack/adapter.py:SlackAdapter._interaction_fields
 * PoP: w27_p8_section_text              @ plugins/platforms/slack/adapter.py:SlackAdapter._section_text
 * PoP: w27_p8_thread_cache_key          @ plugins/platforms/slack/adapter.py:SlackAdapter._thread_cache_key
 * PoP: w27_p8_thread_root_message       @ plugins/platforms/slack/adapter.py:SlackAdapter._thread_root_message
 * PoP: w27_p8_render_message_text       @ plugins/platforms/slack/adapter.py:SlackAdapter._render_message_text
 * PoP: w27_p9_event_team_id             @ plugins/platforms/slack/adapter.py:SlackAdapter._event_team_id
 * PoP: w27_p9_team_in_install_set       @ plugins/platforms/slack/adapter.py:SlackAdapter._team_in_install_set
 * PoP: w27_p9_is_foreign_author         @ plugins/platforms/slack/adapter.py:SlackAdapter._is_foreign_author
 * PoP: w27_p9_context_channel_id        @ plugins/platforms/slack/adapter.py:SlackAdapter._context_channel_id
 * PoP: w27_p9_extract_assistant_meta    @ plugins/platforms/slack/adapter.py:SlackAdapter._extract_assistant_thread_metadata
 * PoP: w27_p9_agent_view_event_fields   @ plugins/platforms/slack/adapter.py:SlackAdapter._agent_view_event_fields
 * PoP: w27_p9_suggested_prompts         @ plugins/platforms/slack/adapter.py:SlackAdapter._assistant_suggested_prompts
 * PoP: w27_p10_require_mention          @ plugins/platforms/slack/adapter.py:SlackAdapter._slack_require_mention
 * PoP: w27_p10_extra_or_env_flag        @ plugins/platforms/slack/adapter.py:SlackAdapter._extra_or_env_flag
 * PoP: w27_p10_addressed_other_user     @ plugins/platforms/slack/adapter.py:SlackAdapter._slack_message_addressed_to_other_user
 * PoP: w27_p10_is_free_channel          @ plugins/platforms/slack/adapter.py:SlackAdapter._slack_is_free_channel
 * PoP: w27_p10_reply_expected           @ plugins/platforms/slack/adapter.py:SlackAdapter._slack_reply_expected
 * PoP: w27_p10_message_mentions_self    @ plugins/platforms/slack/adapter.py:SlackAdapter._slack_message_mentions_self
 * PoP: w27_p10_extra_or_env_channel_set @ plugins/platforms/slack/adapter.py:SlackAdapter._extra_or_env_channel_set
 * PoP: w27_p10_slash_channel_gated      @ plugins/platforms/slack/adapter.py:SlackAdapter._slash_channel_gated
 * PoP: w27_p10_is_slack_cdn_url         @ plugins/platforms/slack/adapter.py:SlackAdapter._is_slack_cdn_url
 * PoP: w27_p10_thread_rehydration_key   @ plugins/platforms/slack/adapter.py:SlackAdapter._thread_rehydration_key
 * PoP: w27_p11_post_kwargs              @ plugins/platforms/slack/adapter.py:_standalone_post_kwargs
 * PoP: w27_p11_bot_tokens               @ plugins/platforms/slack/adapter.py:_load_slack_bot_tokens
 * PoP: w27_p11_format_mrkdwn            @ plugins/platforms/slack/adapter.py:_standalone_format_mrkdwn
 * PoP: w27_p11_apply_yaml_config        @ plugins/platforms/slack/adapter.py:_apply_yaml_config
 * PoP: w27_p11_model_picker_notice      @ plugins/platforms/slack/adapter.py:_model_picker_expired_notice
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "noc_w27_adapter_74f618_p1.h"
#include "noc_w27_adapter_74f618_p2.h"
#include "noc_w27_adapter_74f618_p3.h"
#include "noc_w27_adapter_74f618_p4.h"
#include "noc_w27_adapter_74f618_p5.h"
#include "noc_w27_adapter_74f618_p6.h"
#include "noc_w27_adapter_74f618_p7.h"
#include "noc_w27_adapter_74f618_p8.h"
#include "noc_w27_adapter_74f618_p9.h"
#include "noc_w27_adapter_74f618_p10.h"
#include "noc_w27_adapter_74f618_p11.h"

/* ===================== shared value helpers ===================== */

typedef struct {
    char *p;
    size_t n, cap;
} sb_t;

static char *xstrdup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static void sb_init(sb_t *b) {
    b->cap = 256;
    b->n = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void sb_reserve(sb_t *b, size_t extra) {
    size_t c;
    if (!b->p) return;
    if (b->n + extra + 1 <= b->cap) return;
    c = b->cap ? b->cap : 256;
    while (c < b->n + extra + 1) c *= 2;
    b->p = (char *)realloc(b->p, c);
    b->cap = c;
}

static void sb_addn(sb_t *b, const char *s, size_t n) {
    if (!b->p || !s || !n) return;
    sb_reserve(b, n);
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void sb_adds(sb_t *b, const char *s) {
    if (!s) return;
    sb_addn(b, s, strlen(s));
}

static void sb_addc(sb_t *b, char c) { sb_addn(b, &c, 1); }

static void sb_add_int(sb_t *b, long long v) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%lld", v);
    sb_adds(b, tmp);
}

static char *sb_take(sb_t *b) {
    char *p = b->p;
    if (!p) return xstrdup("");
    b->p = NULL;
    b->n = b->cap = 0;
    return p;
}

static void sb_free(sb_t *b) {
    free(b->p);
    b->p = NULL;
    b->n = b->cap = 0;
}

/* Decode one UTF-8 code point at s[i]; *adv receives the byte width. */
static unsigned cp_at(const char *s, size_t i, size_t *adv) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) {
        *adv = 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && s[i + 1]) {
        *adv = 2;
        return ((unsigned)(c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F);
    }
    if ((c & 0xF0) == 0xE0 && s[i + 1] && s[i + 2]) {
        *adv = 3;
        return ((unsigned)(c & 0x0F) << 12) | (((unsigned char)s[i + 1] & 0x3F) << 6) |
               ((unsigned char)s[i + 2] & 0x3F);
    }
    if ((c & 0xF8) == 0xF0 && s[i + 1] && s[i + 2] && s[i + 3]) {
        *adv = 4;
        return ((unsigned)(c & 0x07) << 18) | (((unsigned char)s[i + 1] & 0x3F) << 12) |
               (((unsigned char)s[i + 2] & 0x3F) << 6) | ((unsigned char)s[i + 3] & 0x3F);
    }
    *adv = 1;
    return c;
}

static size_t cp_count(const char *s) {
    size_t n = 0, i = 0, a;
    while (s[i]) {
        cp_at(s, i, &a);
        i += a;
        n++;
    }
    return n;
}

/* malloc'd prefix of at most n code points. */
static char *cp_prefix(const char *s, size_t n) {
    size_t i = 0, a;
    while (s[i] && n) {
        cp_at(s, i, &a);
        i += a;
        n--;
    }
    {
        char *out = (char *)malloc(i + 1);
        memcpy(out, s, i);
        out[i] = '\0';
        return out;
    }
}

/* malloc'd slice: skip `start` code points, take `n` code points. */
static char *cp_slice(const char *s, size_t start, size_t n) {
    size_t i = 0, a, b;
    while (s[i] && start) {
        cp_at(s, i, &a);
        i += a;
        start--;
    }
    b = i;
    while (s[b] && n) {
        cp_at(s, b, &a);
        b += a;
        n--;
    }
    {
        char *out = (char *)malloc(b - i + 1);
        memcpy(out, s + i, b - i);
        out[b - i] = '\0';
        return out;
    }
}

static int cp_is_space(unsigned c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v' ||
           c == 0x85 || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 ||
           c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}

static char *py_repr(const json_t *v);

/* Python truthiness of a JSON value (NULL == None). */
static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

static json_t *jo(const json_t *o, const char *k) {
    if (!o || o->type != JSON_OBJECT) return NULL;
    return json_obj_get(o, k);
}

static int is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

/* Python str() of a JSON value; caller frees. */
static char *py_str(const json_t *v) {
    char tmp[64];
    if (!v || v->type == JSON_NULL) return xstrdup("None");
    switch (v->type) {
        case JSON_STRING: return xstrdup(v->str_val ? v->str_val : "");
        case JSON_BOOL: return xstrdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val) {
                snprintf(tmp, sizeof(tmp), "%lld", (long long)v->num_val);
            } else {
                snprintf(tmp, sizeof(tmp), "%g", v->num_val);
            }
            return xstrdup(tmp);
        case JSON_ARRAY: {
            sb_t b;
            size_t i;
            sb_init(&b);
            sb_addc(&b, '[');
            for (i = 0; i < v->c.count; i++) {
                char *s;
                if (i) sb_adds(&b, ", ");
                s = py_repr(json_get(v, i));
                sb_adds(&b, s);
                free(s);
            }
            sb_addc(&b, ']');
            return sb_take(&b);
        }
        case JSON_OBJECT: {
            sb_t b;
            size_t i;
            sb_init(&b);
            sb_addc(&b, '{');
            for (i = 0; i < v->c.count; i++) {
                char *s;
                if (i) sb_adds(&b, ", ");
                sb_addc(&b, '\'');
                sb_adds(&b, v->c.keys[i]);
                sb_adds(&b, "': ");
                s = py_repr(json_object_get_at(v, i));
                sb_adds(&b, s);
                free(s);
            }
            sb_addc(&b, '}');
            return sb_take(&b);
        }
        default: break;
    }
    return xstrdup("");
}

/* Python repr() - only scalar/list/dict shapes reach this in the ported paths. */
static char *py_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return xstrdup("None");
    if (v->type == JSON_STRING) {
        sb_t b;
        const char *s = v->str_val ? v->str_val : "";
        sb_init(&b);
        sb_addc(&b, '\'');
        for (; *s; s++) {
            if (*s == '\'' || *s == '\\') sb_addc(&b, '\\');
            sb_addc(&b, *s);
        }
        sb_addc(&b, '\'');
        return sb_take(&b);
    }
    return py_str(v);
}

static json_t *opt_str(const json_t *v) {
    if (v && v->type == JSON_STRING) return json_string(v->str_val);
    return json_null();
}

static const char *rq_str(const json_t *req, const char *k, const char *d) {
    json_t *v = jo(req, k);
    return is_str(v) ? v->str_val : d;
}

static double rq_num(const json_t *req, const char *k, double d) {
    json_t *v = jo(req, k);
    return is_num(v) ? v->num_val : d;
}

/* ===================== gate p1: module text helpers ===================== */

/* East-Asian Width W/F ranges (unicodedata.east_asian_width in "WF"). */
static const unsigned WIDE_RANGES[][2] = {
    {0x1100, 0x115F}, {0x231A, 0x231B}, {0x2329, 0x232A}, {0x23E9, 0x23EC}, {0x23F0, 0x23F0},
    {0x23F3, 0x23F3}, {0x25FD, 0x25FE}, {0x2614, 0x2615}, {0x2648, 0x2653}, {0x267F, 0x267F},
    {0x2693, 0x2693}, {0x26A1, 0x26A1}, {0x26AA, 0x26AB}, {0x26BD, 0x26BE}, {0x26C4, 0x26C5},
    {0x26CE, 0x26CE}, {0x26D4, 0x26D4}, {0x26EA, 0x26EA}, {0x26F2, 0x26F3}, {0x26F5, 0x26F5},
    {0x26FA, 0x26FA}, {0x26FD, 0x26FD}, {0x2705, 0x2705}, {0x270A, 0x270B}, {0x2728, 0x2728},
    {0x274C, 0x274C}, {0x274E, 0x274E}, {0x2753, 0x2755}, {0x2757, 0x2757}, {0x2795, 0x2797},
    {0x27B0, 0x27B0}, {0x27BF, 0x27BF}, {0x2B1B, 0x2B1C}, {0x2B50, 0x2B50}, {0x2B55, 0x2B55},
    {0x2E80, 0x2E99}, {0x2E9B, 0x2EF3}, {0x2F00, 0x2FD5}, {0x2FF0, 0x2FFB}, {0x3000, 0x303E},
    {0x3041, 0x3096}, {0x3099, 0x30FF}, {0x3105, 0x312F}, {0x3131, 0x318E}, {0x3190, 0x31E3},
    {0x31F0, 0x321E}, {0x3220, 0x3247}, {0x3250, 0x4DBF}, {0x4E00, 0xA48C}, {0xA490, 0xA4C6},
    {0xA960, 0xA97C}, {0xAC00, 0xD7A3}, {0xF900, 0xFAFF}, {0xFE10, 0xFE19}, {0xFE30, 0xFE52},
    {0xFE54, 0xFE66}, {0xFE68, 0xFE6B}, {0xFF01, 0xFF60}, {0xFFE0, 0xFFE6}, {0x16FE0, 0x16FE4},
    {0x17000, 0x187F7}, {0x18800, 0x18CD5}, {0x1B000, 0x1B152}, {0x1B164, 0x1B167},
    {0x1B170, 0x1B2FB}, {0x1F004, 0x1F004}, {0x1F0CF, 0x1F0CF}, {0x1F18E, 0x1F18E},
    {0x1F191, 0x1F19A}, {0x1F200, 0x1F202}, {0x1F210, 0x1F23B}, {0x1F240, 0x1F248},
    {0x1F250, 0x1F251}, {0x1F260, 0x1F265}, {0x1F300, 0x1F320}, {0x1F32D, 0x1F335},
    {0x1F337, 0x1F37C}, {0x1F37E, 0x1F393}, {0x1F3A0, 0x1F3CA}, {0x1F3CF, 0x1F3D3},
    {0x1F3E0, 0x1F3F0}, {0x1F3F4, 0x1F3F4}, {0x1F3F8, 0x1F43E}, {0x1F440, 0x1F440},
    {0x1F442, 0x1F4FC}, {0x1F4FF, 0x1F53D}, {0x1F54B, 0x1F54E}, {0x1F550, 0x1F567},
    {0x1F57A, 0x1F57A}, {0x1F595, 0x1F596}, {0x1F5A4, 0x1F5A4}, {0x1F5FB, 0x1F64F},
    {0x1F680, 0x1F6C5}, {0x1F6CC, 0x1F6CC}, {0x1F6D0, 0x1F6D2}, {0x1F6D5, 0x1F6D7},
    {0x1F6DC, 0x1F6DF}, {0x1F6EB, 0x1F6EC}, {0x1F6F4, 0x1F6FC}, {0x1F7E0, 0x1F7EB},
    {0x1F7F0, 0x1F7F0}, {0x1F90C, 0x1F93A}, {0x1F93C, 0x1F945}, {0x1F947, 0x1F9FF},
    {0x1FA70, 0x1FA7C}, {0x1FA80, 0x1FA88}, {0x1FA90, 0x1FABD}, {0x1FABF, 0x1FAC5},
    {0x1FACE, 0x1FADB}, {0x1FAE0, 0x1FAE8}, {0x1FAF0, 0x1FAF8}, {0x20000, 0x2FFFD},
    {0x30000, 0x3FFFD},
};

static int eaw_wide(unsigned cp) {
    size_t lo = 0, hi = sizeof(WIDE_RANGES) / sizeof(WIDE_RANGES[0]);
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (cp < WIDE_RANGES[mid][0]) hi = mid;
        else if (cp > WIDE_RANGES[mid][1]) lo = mid + 1;
        else return 1;
    }
    return 0;
}

static long disp_width(const char *s) {
    long w = 0;
    size_t i = 0, a;
    if (!s) return 0;
    while (s[i]) {
        unsigned cp = cp_at(s, i, &a);
        w += eaw_wide(cp) ? 2 : 1;
        i += a;
    }
    return w;
}

static void sb_pad_to(sb_t *b, const char *cell, long width) {
    long d = width - disp_width(cell);
    sb_adds(b, cell ? cell : "");
    while (d-- > 0) sb_addc(b, ' ');
}

static int has_pipe(const char *s) { return s && strchr(s, '|') != NULL; }

static char *strip_copy(const char *s) {
    const char *a;
    const char *e;
    size_t n;
    char *out;
    if (!s) return xstrdup("");
    a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    e = a + strlen(a);
    while (e > a && isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - a);
    out = (char *)malloc(n + 1);
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* _is_table_row */
static int is_table_row(const char *line) {
    char *st;
    int ok;
    if (!line) return 0;
    st = strip_copy(line);
    ok = st[0] != '\0' && strchr(st, '|') != NULL;
    free(st);
    return ok;
}

/* ^\s*\|?\s*:?-+:?\s*(?:\|\s*:?-+:?\s*){1,}\|?\s*$ */
static int is_table_sep(const char *line) {
    const char *p = line;
    int groups = 0;
    size_t dashes;
    if (!p) return 0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '|') {
        p++;
        while (*p == ' ' || *p == '\t') p++;
    }
    if (*p == ':') p++;
    dashes = 0;
    while (*p == '-') {
        p++;
        dashes++;
    }
    if (!dashes) return 0;
    if (*p == ':') p++;
    while (*p == ' ' || *p == '\t') p++;
    for (;;) {
        if (*p != '|') break;
        p++;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') return groups >= 1;
        if (*p == ':') p++;
        dashes = 0;
        while (*p == '-') {
            p++;
            dashes++;
        }
        if (!dashes) return 0;
        if (*p == ':') p++;
        while (*p == ' ' || *p == '\t') p++;
        groups++;
    }
    if (*p == '|') {
        p++;
        while (*p == ' ' || *p == '\t') p++;
    }
    return *p == '\0' && groups >= 1;
}

/* _split_table_row */
static json_t *split_table_row(const char *line) {
    char *s = strip_copy(line);
    char *cur;
    json_t *arr = json_array();
    size_t n = strlen(s);
    if (n && s[0] == '|') memmove(s, s + 1, n);
    n = strlen(s);
    if (n && s[n - 1] == '|') s[n - 1] = '\0';
    cur = s;
    for (;;) {
        char *bar = strchr(cur, '|');
        char saved;
        char *cell;
        if (!bar) {
            cell = strip_copy(cur);
            json_append(arr, json_string(cell));
            free(cell);
            break;
        }
        saved = *bar;
        *bar = '\0';
        cell = strip_copy(cur);
        json_append(arr, json_string(cell));
        free(cell);
        *bar = saved;
        cur = bar + 1;
    }
    free(s);
    return arr;
}

/* _align_table */
static json_t *align_table(const json_t *rows) {
    size_t n, i, c, n_cols = 0;
    json_t *out = json_array();
    json_t **parsed;
    long *widths;
    if (!is_arr(rows)) return rows ? json_copy(rows) : json_array();
    n = json_len(rows);
    if (n < 2) return json_copy(rows);
    parsed = (json_t **)calloc(n, sizeof(json_t *));
    for (i = 0; i < n; i++) {
        json_t *rv = json_get(rows, i);
        if (!is_str(rv)) parsed[i] = json_array();
        else parsed[i] = split_table_row(rv->str_val);
        if (json_len(parsed[i]) > n_cols) n_cols = json_len(parsed[i]);
    }
    for (i = 0; i < n; i++) {
        size_t have = json_len(parsed[i]);
        for (c = have; c < n_cols; c++) json_append(parsed[i], json_string(""));
    }
    {
        json_t *sep = json_array();
        for (c = 0; c < n_cols; c++) json_append(sep, json_string("---"));
        json_free(parsed[1]);
        parsed[1] = sep;
    }
    widths = (long *)calloc(n_cols ? n_cols : 1, sizeof(long));
    for (c = 0; c < n_cols; c++) {
        long w = 0;
        for (i = 0; i < n; i++) {
            json_t *cell = json_get(parsed[i], c);
            long cw = disp_width(is_str(cell) ? cell->str_val : "");
            if (cw > w) w = cw;
        }
        widths[c] = w;
    }
    for (i = 0; i < n; i++) {
        sb_t b;
        sb_init(&b);
        sb_adds(&b, "| ");
        for (c = 0; c < n_cols; c++) {
            json_t *cell = json_get(parsed[i], c);
            const char *cs = is_str(cell) ? cell->str_val : "";
            if (c) sb_adds(&b, " | ");
            if (i == 1) {
                long d = widths[c];
                while (d-- > 0) sb_addc(&b, '-');
            } else {
                sb_pad_to(&b, cs, widths[c]);
            }
        }
        sb_adds(&b, " |");
        json_append(out, json_string(b.p ? b.p : ""));
        sb_free(&b);
    }
    for (i = 0; i < n; i++) json_free(parsed[i]);
    free(parsed);
    free(widths);
    return out;
}

/* _wrap_markdown_tables */
static char *wrap_markdown_tables(const char *text) {
    char **lines = NULL;
    size_t nlines = 0, cap = 0, i;
    sb_t out;
    int in_fence = 0;
    char *copy;
    char *p;
    if (!text || !strchr(text, '|') || !strchr(text, '-')) return xstrdup(text ? text : "");
    copy = xstrdup(text);
    p = copy;
    for (;;) {
        char *nl = strchr(p, '\n');
        if (nlines == cap) {
            cap = cap ? cap * 2 : 16;
            lines = (char **)realloc(lines, cap * sizeof(char *));
        }
        if (nl) {
            *nl = '\0';
            lines[nlines++] = xstrdup(p);
            p = nl + 1;
        } else {
            lines[nlines++] = xstrdup(p);
            break;
        }
    }
    sb_init(&out);
    i = 0;
    while (i < nlines) {
        const char *line = lines[i];
        const char *l = line;
        while (*l == ' ' || *l == '\t') l++;
        if (strncmp(l, "```", 3) == 0) {
            in_fence = !in_fence;
        } else if (!in_fence && has_pipe(line) && i + 1 < nlines && is_table_sep(lines[i + 1])) {
            json_t *block = json_array();
            json_t *aligned;
            size_t j = i + 2, k;
            json_append(block, json_string(line));
            json_append(block, json_string(lines[i + 1]));
            while (j < nlines && is_table_row(lines[j])) {
                json_append(block, json_string(lines[j]));
                j++;
            }
            aligned = align_table(block);
            if (out.n) sb_addc(&out, '\n');
            sb_adds(&out, "```");
            for (k = 0; k < json_len(aligned); k++) {
                json_t *row = json_get(aligned, k);
                sb_addc(&out, '\n');
                sb_adds(&out, is_str(row) ? row->str_val : "");
            }
            sb_addc(&out, '\n');
            sb_adds(&out, "```");
            json_free(aligned);
            json_free(block);
            i = j;
            continue;
        }
        if (out.n) sb_addc(&out, '\n');
        sb_adds(&out, line);
        i++;
    }
    for (i = 0; i < nlines; i++) free(lines[i]);
    free(lines);
    free(copy);
    return sb_take(&out);
}

static char *permalink_path(const json_t *channel_id, const json_t *message_ts) {
    sb_t b;
    char *ts;
    char *cid;
    size_t i, j = 0;
    if (!py_truthy(channel_id) || !py_truthy(message_ts)) return xstrdup("");
    cid = py_str(channel_id);
    sb_init(&b);
    sb_adds(&b, "archives/");
    sb_adds(&b, cid);
    sb_adds(&b, "/p");
    free(cid);
    ts = py_str(message_ts);
    for (i = 0; ts[i]; i++) {
        if (ts[i] == '.') continue;
        ts[j++] = ts[i];
    }
    ts[j] = '\0';
    sb_adds(&b, ts);
    free(ts);
    return sb_take(&b);
}

static char *str_or_empty(const json_t *v) {
    if (!py_truthy(v)) return xstrdup("");
    return py_str(v);
}

static long long int_or_zero(const char *value) {
    char *tmp;
    char *end;
    long long v;
    if (!value) return 0;
    tmp = strip_copy(value);
    if (!tmp[0]) {
        free(tmp);
        return 0;
    }
    errno = 0;
    v = strtoll(tmp, &end, 10);
    if (*end != '\0') {
        free(tmp);
        return 0;
    }
    free(tmp);
    return v;
}

static const char *BOOL_WORDS[] = {"1", "0", "true", "false", "yes", "no", "on", "off"};

static int in_bool_words(const char *s) {
    size_t i;
    for (i = 0; i < sizeof(BOOL_WORDS) / sizeof(BOOL_WORDS[0]); i++) {
        if (strcmp(s, BOOL_WORDS[i]) == 0) return 1;
    }
    return 0;
}

static int is_truthy_word(const char *s) {
    return strcmp(s, "1") == 0 || strcmp(s, "true") == 0 || strcmp(s, "yes") == 0 ||
           strcmp(s, "on") == 0;
}

/* _slack_unfurl_kwargs */
static json_t *unfurl_kwargs(const json_t *extra) {
    static const char *KEYS[2] = {"unfurl_links", "unfurl_media"};
    json_t *out = json_object();
    size_t i;
    for (i = 0; i < 2; i++) {
        json_t *val = is_obj(extra) ? jo(extra, KEYS[i]) : NULL;
        if (val && val->type == JSON_BOOL) {
            json_set(out, KEYS[i], json_bool(val->bool_val));
        } else if (is_str(val)) {
            char *low = strip_copy(val->str_val);
            size_t k;
            for (k = 0; low[k]; k++) low[k] = (char)tolower((unsigned char)low[k]);
            if (in_bool_words(low)) json_set(out, KEYS[i], json_bool(is_truthy_word(low)));
            free(low);
        }
    }
    return out;
}

json_t *w27_p1_unfurl_kwargs(const json_t *req) { return unfurl_kwargs(jo(req, "extra")); }

json_t *w27_p1_is_table_row(const json_t *req) {
    return json_bool(is_table_row(rq_str(req, "line", "")));
}

json_t *w27_p1_disp_width(const json_t *req) {
    return json_number((double)disp_width(rq_str(req, "s", "")));
}

json_t *w27_p1_pad(const json_t *req) {
    sb_t b;
    const char *cell = rq_str(req, "cell", "");
    long width = (long)rq_num(req, "width", 0);
    sb_init(&b);
    sb_pad_to(&b, cell, width);
    {
        json_t *out = json_string(b.p ? b.p : "");
        sb_free(&b);
        return out;
    }
}

json_t *w27_p1_split_table_row(const json_t *req) {
    return split_table_row(rq_str(req, "line", ""));
}

json_t *w27_p1_align_table(const json_t *req) { return align_table(jo(req, "rows")); }

json_t *w27_p1_wrap_markdown_tables(const json_t *req) {
    json_t *v = jo(req, "text");
    char *out;
    json_t *res;
    if (!is_str(v)) return v ? json_copy(v) : json_null();
    out = wrap_markdown_tables(v->str_val);
    res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p1_permalink_path(const json_t *req) {
    char *out = permalink_path(jo(req, "channel_id"), jo(req, "message_ts"));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p1_str_or_empty(const json_t *req) {
    char *out = str_or_empty(jo(req, "value"));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p1_int_or_zero(const json_t *req) {
    json_t *v = jo(req, "value");
    if (v && v->type == JSON_NUMBER) return json_number((double)(long long)v->num_val);
    return json_number((double)int_or_zero(is_str(v) ? v->str_val : NULL));
}

json_t *w27_p1_first_truthy(const json_t *req) {
    json_t *mapping = jo(req, "mapping");
    json_t *keys = jo(req, "keys");
    size_t i;
    if (is_obj(mapping) && is_arr(keys)) {
        for (i = 0; i < json_len(keys); i++) {
            json_t *k = json_get(keys, i);
            json_t *v;
            if (!is_str(k)) continue;
            v = json_obj_get(mapping, k->str_val);
            if (py_truthy(v)) return json_copy(v);
        }
    }
    return json_null();
}

/* ===================== gate p2: block-kit rendering ===================== */

typedef struct {
    char **v;
    size_t n, cap;
} svec_t;

static void sv_init(svec_t *b) {
    b->v = NULL;
    b->n = 0;
    b->cap = 0;
}

static void sv_push(svec_t *b, char *s) {
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 8;
        b->v = (char **)realloc(b->v, b->cap * sizeof(char *));
    }
    b->v[b->n++] = s;
}

static char *sv_join(svec_t *b, const char *sep) {
    sb_t o;
    size_t i;
    sb_init(&o);
    for (i = 0; i < b->n; i++) {
        if (i) sb_adds(&o, sep);
        sb_adds(&o, b->v[i]);
    }
    for (i = 0; i < b->n; i++) free(b->v[i]);
    free(b->v);
    b->v = NULL;
    b->n = b->cap = 0;
    return sb_take(&o);
}

static char *rstrip_copy(const char *s) {
    size_t n;
    char *out;
    if (!s) return xstrdup("");
    n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' || s[n - 1] == '\r' ||
                 s[n - 1] == '\f' || s[n - 1] == '\v'))
        n--;
    out = (char *)malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static int is_blank(const char *s) {
    if (!s) return 1;
    for (; *s; s++) {
        if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r' && *s != '\f' && *s != '\v')
            return 0;
    }
    return 1;
}

/* _slack_str_field */
static char *slack_str_field(const json_t *el, const char *name) {
    json_t *v = jo(el, name);
    return is_str(v) ? xstrdup(v->str_val) : xstrdup("");
}

static char *render_inline_element(const json_t *el);

static char *render_inline_elements(const json_t *elements) {
    sb_t b;
    size_t i;
    sb_init(&b);
    if (is_arr(elements)) {
        for (i = 0; i < json_len(elements); i++) {
            char *s = render_inline_element(json_get(elements, i));
            sb_adds(&b, s);
            free(s);
        }
    }
    return sb_take(&b);
}

static void render_inline_into(sb_t *b, const json_t *elements) {
    char *s = render_inline_elements(elements);
    sb_adds(b, s);
    free(s);
}

/* _render_slack_inline_element */
static char *render_inline_element(const json_t *el) {
    json_t *tv = jo(el, "type");
    const char *type = is_str(tv) ? tv->str_val : "";
    if (strcmp(type, "text") == 0) return slack_str_field(el, "text");
    if (strcmp(type, "color") == 0) return slack_str_field(el, "value");
    {
        static const struct {
            const char *type;
            const char *fmt;
            const char *key;
            const char *def;
        } ENTITIES[] = {
            {"channel", "<#%s>", "channel_id", ""},   {"user", "<@%s>", "user_id", ""},
            {"usergroup", "<!subteam^%s>", "usergroup_id", ""},
            {"team", "<!team^%s>", "team_id", ""},    {"emoji", ":%s:", "name", ""},
            {"broadcast", "<!%s>", "range", "here"},
        };
        size_t i;
        for (i = 0; i < sizeof(ENTITIES) / sizeof(ENTITIES[0]); i++) {
            if (strcmp(type, ENTITIES[i].type) == 0) {
                json_t *v = jo(el, ENTITIES[i].key);
                char *raw = v ? py_str(v) : xstrdup(ENTITIES[i].def);
                sb_t b;
                sb_init(&b);
                {
                    const char *f = ENTITIES[i].fmt;
                    const char *pct = strstr(f, "%s");
                    if (pct) {
                        sb_addn(&b, f, (size_t)(pct - f));
                        sb_adds(&b, raw);
                        sb_adds(&b, pct + 2);
                    } else {
                        sb_adds(&b, f);
                    }
                }
                free(raw);
                return sb_take(&b);
            }
        }
    }
    if (strcmp(type, "date") == 0) {
        char *fallback = slack_str_field(el, "fallback");
        if (fallback[0]) return fallback;
        free(fallback);
    }
    {
        char *url = slack_str_field(el, "url");
        char *label = slack_str_field(el, "text");
        char *fb;
        if (!label[0]) {
            fb = slack_str_field(el, "fallback");
            free(label);
            label = fb;
        }
        if (!url[0] && strcmp(type, "message_mention") == 0) {
            free(url);
            url = permalink_path(jo(el, "channel_id"), jo(el, "message_ts"));
        }
        if (url[0]) {
            if (label[0] && strcmp(label, url) != 0) {
                sb_t b;
                sb_init(&b);
                sb_adds(&b, label);
                sb_adds(&b, " (");
                sb_adds(&b, url);
                sb_adds(&b, ")");
                free(url);
                free(label);
                return sb_take(&b);
            }
            free(label);
            return url;
        }
        free(url);
        return label;
    }
}

/* _collect_slack_table_cell_text */
static void cell_visit(const json_t *node, svec_t *parts, int depth) {
    if (depth > 64 || !node) return;
    if (node->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < json_len(node); i++) cell_visit(json_get(node, i), parts, depth + 1);
        return;
    }
    if (node->type == JSON_OBJECT) {
        size_t i;
        json_t *txt = jo(node, "text");
        if (is_str(txt)) sv_push(parts, xstrdup(txt->str_val));
        for (i = 0; i < node->c.count; i++) cell_visit(json_object_get_at(node, i), parts, depth + 1);
    }
}

static char *collect_table_cell_text(const json_t *value) {
    svec_t parts;
    size_t i;
    sb_t b;
    sv_init(&parts);
    cell_visit(value, &parts, 0);
    sb_init(&b);
    {
        int first = 1;
        for (i = 0; i < parts.n; i++) {
            if (!parts.v[i][0]) continue;
            if (!first) sb_addc(&b, ' ');
            sb_adds(&b, parts.v[i]);
            first = 0;
        }
    }
    for (i = 0; i < parts.n; i++) free(parts.v[i]);
    free(parts.v);
    {
        char *raw = sb_take(&b);
        char *out = strip_copy(raw);
        free(raw);
        return out;
    }
}

/* _render_slack_table_block */
static char *render_table_block(const json_t *block, long max_chars) {
    json_t *rows = is_obj(block) ? jo(block, "rows") : NULL;
    svec_t lines;
    size_t i;
    char *text;
    if (!is_arr(rows)) return xstrdup("");
    sv_init(&lines);
    for (i = 0; i < json_len(rows); i++) {
        json_t *row = json_get(rows, i);
        sb_t b;
        size_t c;
        if (!is_arr(row)) continue;
        sb_init(&b);
        for (c = 0; c < json_len(row); c++) {
            char *cell = collect_table_cell_text(json_get(row, c));
            if (c) sb_adds(&b, " | ");
            sb_adds(&b, cell);
            free(cell);
        }
        {
            char *rendered = sb_take(&b);
            char *check = strip_copy(rendered);
            size_t k, m = 0;
            for (k = 0; check[k]; k++) {
                if (check[k] != ' ' && check[k] != '|') m++;
            }
            if (m) sv_push(&lines, xstrdup(rendered));
            free(check);
            free(rendered);
        }
    }
    text = sv_join(&lines, "\n");
    if (!text[0]) {
        free(text);
        return xstrdup("");
    }
    if ((long)cp_count(text) > max_chars) {
        long keep = max_chars - 20;
        char *head = keep > 0 ? cp_prefix(text, (size_t)keep) : xstrdup("");
        char *rst = rstrip_copy(head);
        sb_t b;
        sb_init(&b);
        sb_adds(&b, rst);
        sb_adds(&b, "\n[table truncated]");
        free(head);
        free(rst);
        free(text);
        return sb_take(&b);
    }
    return text;
}

/* _append_line closure of _extract_text_from_slack_blocks */
static void append_line(svec_t *parts, const char *text, int depth, const char *bullet) {
    sb_t b;
    char *line;
    int i;
    if (!text || is_blank(text)) return;
    sb_init(&b);
    for (i = 0; i < depth; i++) sb_addc(&b, '>');
    if (depth) sb_addc(&b, ' ');
    sb_adds(&b, bullet ? bullet : "");
    sb_adds(&b, text);
    line = rstrip_copy(b.p ? b.p : "");
    sb_free(&b);
    sv_push(parts, line);
}

static void walk_elements(const json_t *elements, int depth, const char *bullet, svec_t *parts,
                          int level);

static void walk_one(const json_t *elem, int depth, const char *bullet, svec_t *parts, int level) {
    json_t *tv = jo(elem, "type");
    const char *type = is_str(tv) ? tv->str_val : "";
    json_t *children = jo(elem, "elements");
    if (strcmp(type, "rich_text_section") == 0) {
        char *rendered = render_inline_elements(is_arr(children) ? children : NULL);
        append_line(parts, rendered, depth, bullet);
        free(rendered);
    } else if (strcmp(type, "rich_text_quote") == 0) {
        walk_elements(is_arr(children) ? children : NULL, depth + 1, "", parts, level + 1);
    } else if (strcmp(type, "rich_text_list") == 0) {
        json_t *style = jo(elem, "style");
        int bullet_style = is_str(style) && strcmp(style->str_val, "bullet") == 0;
        size_t idx;
        if (!is_arr(children)) return;
        for (idx = 0; idx < json_len(children); idx++) {
            char bl[32];
            if (bullet_style) snprintf(bl, sizeof(bl), "\xe2\x80\xa2 ");
            else snprintf(bl, sizeof(bl), "%d. ", (int)(idx + 1));
            walk_one(json_get(children, idx), depth, bl, parts, level + 1);
        }
    } else if (strcmp(type, "rich_text_preformatted") == 0) {
        svec_t code;
        size_t idx;
        char *code_text;
        sv_init(&code);
        if (is_arr(children)) {
            for (idx = 0; idx < json_len(children); idx++) {
                json_t *child = json_get(children, idx);
                json_t *ctv = jo(child, "type");
                const char *ctype = is_str(ctv) ? ctv->str_val : "";
                char *rendered;
                if (strcmp(ctype, "rich_text_section") == 0)
                    rendered = render_inline_elements(jo(child, "elements"));
                else {
                    json_t *one = json_array();
                    json_append(one, child ? json_copy(child) : json_null());
                    rendered = render_inline_elements(one);
                    json_free(one);
                }
                sv_push(&code, rendered);
            }
        }
        {
            svec_t kept;
            size_t k;
            sv_init(&kept);
            for (k = 0; k < code.n; k++) {
                if (code.v[k][0]) sv_push(&kept, xstrdup(code.v[k]));
            }
            code_text = sv_join(&kept, "\n");
        }
        if (code_text[0]) {
            json_t *langv = jo(elem, "language");
            char *lang = langv ? py_str(langv) : xstrdup("");
            sb_t b;
            sb_init(&b);
            sb_adds(&b, "```");
            sb_adds(&b, lang);
            sb_addc(&b, '\n');
            sb_adds(&b, code_text);
            sb_adds(&b, "\n```");
            append_line(parts, b.p ? b.p : "", depth, bullet);
            sb_free(&b);
            free(lang);
        }
        free(code_text);
        sv_join(&code, ""); /* frees owned strings */
    } else {
        json_t *one = json_array();
        char *rendered;
        json_append(one, elem ? json_copy(elem) : json_null());
        rendered = render_inline_elements(one);
        json_free(one);
        append_line(parts, rendered, depth, bullet);
        free(rendered);
    }
}

static void walk_elements(const json_t *elements, int depth, const char *bullet, svec_t *parts,
                          int level) {
    size_t i;
    if (level > 64 || !is_arr(elements)) return;
    for (i = 0; i < json_len(elements); i++)
        walk_one(json_get(elements, i), depth, bullet, parts, level);
}

/* _extract_text_from_slack_blocks */
static char *extract_text_blocks(const json_t *blocks) {
    svec_t parts;
    size_t i;
    if (!is_arr(blocks) || json_len(blocks) == 0) return xstrdup("");
    sv_init(&parts);
    for (i = 0; i < json_len(blocks); i++) {
        json_t *block = json_get(blocks, i);
        json_t *bv = jo(block, "type");
        const char *btype = is_str(bv) ? bv->str_val : "";
        if (strcmp(btype, "rich_text") == 0) {
            walk_elements(jo(block, "elements"), 0, "", &parts, 0);
        } else if (strcmp(btype, "table") == 0) {
            char *table_text = render_table_block(block, 20000);
            if (table_text[0]) sv_push(&parts, xstrdup(table_text));
            free(table_text);
        }
    }
    return sv_join(&parts, "\n");
}

/* _collect_slack_block_mentions */
static void collect_mentions(const json_t *node, int in_quote, svec_t *out, int depth) {
    if (depth > 64 || !node) return;
    if (node->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < json_len(node); i++) collect_mentions(json_get(node, i), in_quote, out, depth + 1);
        return;
    }
    if (node->type != JSON_OBJECT) return;
    {
        json_t *tv = jo(node, "type");
        const char *ntype = is_str(tv) ? tv->str_val : "";
        int quoted = in_quote || strcmp(ntype, "rich_text_quote") == 0;
        json_t *uid = jo(node, "user_id");
        static const char *CHILD_KEYS[2] = {"elements", "element"};
        size_t i;
        if (strcmp(ntype, "user") == 0 && !quoted && py_truthy(uid)) {
            char *u = py_str(uid);
            sb_t b;
            sb_init(&b);
            sb_adds(&b, "<@");
            sb_adds(&b, u);
            sb_addc(&b, '>');
            sv_push(out, sb_take(&b));
            free(u);
        }
        for (i = 0; i < 2; i++) {
            json_t *child = jo(node, CHILD_KEYS[i]);
            if (child) collect_mentions(child, quoted, out, depth + 1);
        }
    }
}

static int str_contains(const char *hay, const char *needle) {
    if (!hay || !needle) return 0;
    return strstr(hay, needle) != NULL;
}

json_t *w27_p2_collect_block_mentions(const json_t *req) {
    svec_t out;
    size_t i;
    json_t *arr = json_array();
    sv_init(&out);
    collect_mentions(jo(req, "blocks"), 0, &out, 0);
    for (i = 0; i < out.n; i++) json_append(arr, json_string(out.v[i]));
    for (i = 0; i < out.n; i++) free(out.v[i]);
    free(out.v);
    return arr;
}

json_t *w27_p2_mention_detection_text(const json_t *req) {
    json_t *event = jo(req, "event");
    json_t *tv = jo(event, "text");
    char *flat = is_str(tv) ? xstrdup(tv->str_val) : xstrdup("");
    json_t *blocks = jo(event, "blocks");
    svec_t mentions, extra;
    size_t i;
    sb_t b;
    sv_init(&mentions);
    sv_init(&extra);
    if (py_truthy(blocks)) {
        collect_mentions(blocks, 0, &mentions, 0);
        for (i = 0; i < mentions.n; i++) {
            if (!str_contains(flat, mentions.v[i])) sv_push(&extra, xstrdup(mentions.v[i]));
        }
    }
    sb_init(&b);
    if (extra.n) {
        char *stripped = strip_copy(flat);
        sb_adds(&b, stripped);
        sb_addc(&b, '\n');
        for (i = 0; i < extra.n; i++) {
            if (i) sb_addc(&b, ' ');
            sb_adds(&b, extra.v[i]);
        }
        free(stripped);
        {
            char *joined = sb_take(&b);
            char *out = strip_copy(joined);
            free(joined);
            free(flat);
            for (i = 0; i < mentions.n; i++) free(mentions.v[i]);
            free(mentions.v);
            for (i = 0; i < extra.n; i++) free(extra.v[i]);
            free(extra.v);
            return json_string(out);
        }
    }
    sb_free(&b);
    for (i = 0; i < mentions.n; i++) free(mentions.v[i]);
    free(mentions.v);
    for (i = 0; i < extra.n; i++) free(extra.v[i]);
    free(extra.v);
    {
        json_t *res = json_string(flat);
        free(flat);
        return res;
    }
}

json_t *w27_p2_slack_str_field(const json_t *req) {
    char *out = slack_str_field(jo(req, "el"), rq_str(req, "name", ""));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p2_render_inline_element(const json_t *req) {
    char *out = render_inline_element(jo(req, "el"));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p2_render_inline_elements(const json_t *req) {
    char *out = render_inline_elements(jo(req, "elements"));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p2_extract_text_blocks(const json_t *req) {
    char *out = extract_text_blocks(jo(req, "blocks"));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p2_table_cell_text(const json_t *req) {
    char *out = collect_table_cell_text(jo(req, "value"));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p2_render_table_block(const json_t *req) {
    json_t *mv = jo(req, "max_chars");
    long maxc = is_num(mv) ? (long)mv->num_val : 20000;
    char *out = render_table_block(jo(req, "block"), maxc);
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p2_slack_error_is(const json_t *req) {
    json_t *resp = jo(req, "response");
    const char *code = rq_str(req, "code", "");
    json_t *ev;
    if (!py_truthy(resp) || !is_obj(resp)) return json_bool(0);
    ev = jo(resp, "error");
    return json_bool(is_str(ev) && strcmp(ev->str_val, code) == 0);
}

/* ===================== gate p3: dedupe + payload serialization ===================== */

/* _unescape_slack_entities: one regex pass over &(amp|lt|gt); */
static char *unescape_entities(const char *text) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    if (!text) return sb_take(&b);
    while (text[i]) {
        if (text[i] == '&') {
            if (strncmp(text + i, "&amp;", 5) == 0) {
                sb_addc(&b, '&');
                i += 5;
                continue;
            }
            if (strncmp(text + i, "&lt;", 4) == 0) {
                sb_addc(&b, '<');
                i += 4;
                continue;
            }
            if (strncmp(text + i, "&gt;", 4) == 0) {
                sb_addc(&b, '>');
                i += 4;
                continue;
            }
        }
        sb_addc(&b, text[i++]);
    }
    return sb_take(&b);
}

static int is_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int is_alnum(int c) { return is_alpha(c) || (c >= '0' && c <= '9'); }
static int is_digit_c(int c) { return c >= '0' && c <= '9'; }

static int scheme_delim(int c) { return is_alnum(c) || c == '+' || c == '.' || c == '-'; }

/* <scheme:target|label> -> "label (url)" / url */
static char *sub_links(const char *s) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (s[i] == '<' && is_alpha((unsigned char)s[i + 1])) {
            size_t p = i + 1;
            while (scheme_delim((unsigned char)s[p])) p++;
            if (s[p] == ':') {
                size_t target = ++p;
                while (s[p] && s[p] != '>' && s[p] != '|') p++;
                if (p > target) {
                    size_t tend = p;
                    size_t urlstart = i + 1;
                    size_t lstart = 0, lend = 0;
                    int has_label = 0;
                    if (s[p] == '|') {
                        lstart = ++p;
                        while (s[p] && s[p] != '>') p++;
                        if (p > lstart) {
                            lend = p;
                            has_label = 1;
                        }
                    }
                    if (s[p] == '>' && (!has_label || lend > lstart)) {
                        char *url = (char *)malloc(tend - urlstart + 1);
                        memcpy(url, s + urlstart, tend - urlstart);
                        url[tend - urlstart] = '\0';
                        if (has_label) {
                            char *label = (char *)malloc(lend - lstart + 1);
                            memcpy(label, s + lstart, lend - lstart);
                            label[lend - lstart] = '\0';
                            if (label[0] && strcmp(label, url) != 0) {
                                sb_adds(&b, label);
                                sb_adds(&b, " (");
                                sb_adds(&b, url);
                                sb_addc(&b, ')');
                            } else {
                                sb_adds(&b, url);
                            }
                            free(label);
                        } else {
                            sb_adds(&b, url);
                        }
                        free(url);
                        i = p + 1;
                        continue;
                    }
                }
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

/* <!date^ts^format^url|fallback> -> fallback, else third '^' part */
static char *sub_dates(const char *s) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (strncmp(s + i, "<!date^", 7) == 0) {
            size_t g1 = i + 7;
            size_t p = g1;
            while (s[p] && s[p] != '>' && s[p] != '|') p++;
            {
                size_t g1end = p;
                size_t lstart = 0, lend = 0;
                int has_label = 0;
                if (s[p] == '|') {
                    lstart = p + 1;
                    p = lstart;
                    while (s[p] && s[p] != '>') p++;
                    lend = p;
                    has_label = 1;
                }
                if (s[p] == '>') {
                    if (has_label && lend > lstart) {
                        sb_addn(&b, s + lstart, lend - lstart);
                    } else {
                        /* parts = group1.split("^"); parts[2] if len > 2 else "" */
                        size_t q = g1;
                        int idx = 0;
                        size_t seg = g1;
                        size_t out_s = 0, out_e = 0;
                        int found = 0;
                        for (;;) {
                            if (q == g1end || s[q] == '^') {
                                if (idx == 2) {
                                    out_s = seg;
                                    out_e = q;
                                    found = 1;
                                }
                                if (q == g1end) break;
                                idx++;
                                seg = q + 1;
                            }
                            q++;
                        }
                        if (found) sb_addn(&b, s + out_s, out_e - out_s);
                    }
                    i = p + 1;
                    continue;
                }
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

/* https?://host/(archives/ID/pNNN)(?query)? -> archives/ID/pNNN */
static char *sub_permalinks(const char *s) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (s[i] == 'h' && strncmp(s + i, "http", 4) == 0) {
            size_t p = i + 4;
            if (s[p] == 's') p++;
            if (s[p] == ':' && s[p + 1] == '/' && s[p + 2] == '/') {
                size_t host = p + 3;
                size_t h = host;
                while (s[h] && s[h] != '/' && s[h] != ' ' && s[h] != '\t' && s[h] != '\n' &&
                       s[h] != '\r' && s[h] != ')')
                    h++;
                if (h > host && s[h] == '/' && strncmp(s + h + 1, "archives/", 9) == 0) {
                    size_t id = h + 10;
                    size_t q = id;
                    while (is_alnum((unsigned char)s[q])) q++;
                    if (q > id && strncmp(s + q, "/p", 2) == 0) {
                        size_t d = q + 2;
                        size_t ds = d;
                        while (is_digit_c((unsigned char)s[d])) d++;
                        if (d > ds) {
                            size_t tail = d;
                            if (s[tail] == '?') {
                                size_t qq = tail + 1;
                                while (s[qq] && s[qq] != ' ' && s[qq] != '\t' && s[qq] != '\n' &&
                                       s[qq] != '\r' && s[qq] != ')')
                                    qq++;
                                tail = qq;
                            }
                            sb_addn(&b, s + h + 1, 9); /* archives/ */
                            sb_addn(&b, s + id, q - id);
                            sb_addn(&b, s + q, d - q);
                            i = tail;
                            continue;
                        }
                    }
                }
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

/* <([@#!][^>|]*)\|[^>]*> -> <\1> */
static char *sub_entity_labels(const char *s) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (s[i] == '<' && (s[i + 1] == '@' || s[i + 1] == '#' || s[i + 1] == '!')) {
            size_t p = i + 2;
            while (s[p] && s[p] != '>' && s[p] != '|') p++;
            if (s[p] == '|') {
                size_t bar = p;
                while (s[p] && s[p] != '>') p++;
                if (s[p] == '>' && p > bar + 1) {
                    sb_addn(&b, s + i, bar - i);
                    sb_addc(&b, '>');
                    i = p + 1;
                    continue;
                }
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

/* (?<!`)\n*```[ \t]*\n?(.*?)\n?[ \t]*```\n*(?!`) */
static int fence_next(const char *s, size_t from, size_t *ms, size_t *me, size_t *cs, size_t *ce) {
    size_t i;
    for (i = from; s[i]; i++) {
        size_t ms0, p, q, cend, mend, r;
        if (s[i] != '`' || s[i + 1] != '`' || s[i + 2] != '`') continue;
        if (i > 0 && s[i - 1] == '`') continue;
        ms0 = i;
        while (ms0 > 0 && s[ms0 - 1] == '\n') ms0--;
        p = i + 3;
        while (s[p] == ' ' || s[p] == '\t') p++;
        if (s[p] == '\n') p++;
        q = p;
        for (;;) {
            while (s[q] && !(s[q] == '`' && s[q + 1] == '`' && s[q + 2] == '`')) q++;
            if (!s[q]) break;
            if (s[q + 3] != '`') break;
            q++;
        }
        if (!s[q]) continue;
        if (s[q + 3] == '`') continue;
        cend = q;
        while (cend > p && (s[cend - 1] == ' ' || s[cend - 1] == '\t')) cend--;
        if (cend > p && s[cend - 1] == '\n') cend--;
        mend = q + 3;
        while (s[mend] == '\n') mend++;
        (void)r;
        *ms = ms0;
        *me = mend;
        *cs = p;
        *ce = cend;
        return 1;
    }
    return 0;
}

/* _SLACK_FENCED_CODE_RE.sub(r"\1", s) */
static char *strip_fences(const char *s) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    for (;;) {
        size_t ms, me, cs, ce;
        if (!fence_next(s, i, &ms, &me, &cs, &ce)) {
            sb_adds(&b, s + i);
            break;
        }
        sb_addn(&b, s + i, ms - i);
        sb_addn(&b, s + cs, ce > cs ? ce - cs : 0);
        i = me;
    }
    return sb_take(&b);
}

/* `([^`\n]+)` -> \1 */
static char *strip_inline_code(const char *s) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (s[i] == '`') {
            size_t p = i + 1;
            while (s[p] && s[p] != '`' && s[p] != '\n') p++;
            if (s[p] == '`' && p > i + 1) {
                sb_addn(&b, s + i + 1, p - i - 1);
                i = p + 1;
                continue;
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

/* ([*_~])([^\n]+?)\1 -> \2, repeated until stable */
static char *strip_inline_style(const char *s, int *changed) {
    sb_t b;
    size_t i = 0;
    int did = 0;
    sb_init(&b);
    while (s[i]) {
        char c = s[i];
        if (c == '*' || c == '_' || c == '~') {
            size_t p = i + 1;
            while (s[p] && s[p] != '\n' && s[p] != c) p++;
            if (s[p] == c && p > i + 1) {
                sb_addn(&b, s + i + 1, p - i - 1);
                i = p + 1;
                did = 1;
                continue;
            }
        }
        sb_addc(&b, s[i++]);
    }
    *changed = did;
    return sb_take(&b);
}

static int is_ws_byte(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* re.sub(r"\s+", " ", s).strip() over code points */
static char *collapse_ws(const char *s) {
    sb_t b;
    size_t i = 0;
    int pending_space = 0, wrote = 0;
    sb_init(&b);
    while (s[i]) {
        size_t adv;
        unsigned cp = cp_at(s, i, &adv);
        if (cp_is_space(cp)) {
            pending_space = 1;
            i += adv;
            continue;
        }
        if (pending_space && wrote) sb_addc(&b, ' ');
        pending_space = 0;
        sb_addn(&b, s + i, adv);
        wrote = 1;
        i += adv;
    }
    return sb_take(&b);
}

/* _normalize_slack_text_for_dedupe */
static char *normalize_dedupe(const char *text, const char *bot_uid) {
    char *cur = xstrdup(text ? text : "");
    char *tmp;
    tmp = unescape_entities(cur);
    free(cur);
    cur = sub_links(tmp);
    free(tmp);
    tmp = sub_dates(cur);
    free(cur);
    cur = sub_permalinks(tmp);
    free(tmp);
    tmp = sub_entity_labels(cur);
    free(cur);
    cur = tmp;
    if (bot_uid && bot_uid[0]) {
        sb_t b;
        const char *p = cur;
        sb_init(&b);
        for (;;) {
            const char *hit = strstr(p, "<@");
            if (!hit) {
                sb_adds(&b, p);
                break;
            }
            if (strncmp(hit + 2, bot_uid, strlen(bot_uid)) == 0 && hit[2 + strlen(bot_uid)] == '>') {
                sb_addn(&b, p, (size_t)(hit - p));
                p = hit + 3 + strlen(bot_uid);
            } else {
                sb_addn(&b, p, (size_t)(hit - p) + 2);
                p = hit + 2;
            }
        }
        free(cur);
        cur = sb_take(&b);
    }
    tmp = strip_fences(cur);
    free(cur);
    cur = strip_inline_code(tmp);
    free(tmp);
    {
        int round;
        for (round = 0; round < 64; round++) {
            int changed = 0;
            char *next = strip_inline_style(cur, &changed);
            free(cur);
            cur = next;
            if (!changed) break;
        }
    }
    tmp = collapse_ws(cur);
    free(cur);
    cur = strip_copy(tmp);
    free(tmp);
    return cur;
}

/* The fenced-code matches of a text, normalized (primary_fenced set). */
static json_t *fenced_set(const char *text, const char *bot_uid) {
    json_t *arr = json_array();
    size_t i = 0;
    for (;;) {
        size_t ms, me, cs, ce;
        char *raw;
        char *norm;
        if (!fence_next(text ? text : "", i, &ms, &me, &cs, &ce)) break;
        raw = (char *)malloc(me - ms + 1);
        memcpy(raw, text + ms, me - ms);
        raw[me - ms] = '\0';
        norm = normalize_dedupe(raw, bot_uid);
        json_append(arr, json_string(norm));
        free(norm);
        free(raw);
        i = me;
    }
    return arr;
}

static int arr_contains(const json_t *arr, const char *v) {
    size_t i;
    for (i = 0; i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        if (is_str(e) && strcmp(e->str_val, v) == 0) return 1;
    }
    return 0;
}

/* _extract_additional_text_from_slack_blocks */
static char *additional_text_blocks(const json_t *blocks, const char *primary_text,
                                   const char *bot_uid) {
    char *primary = normalize_dedupe(primary_text ? primary_text : "", bot_uid);
    json_t *pfen = fenced_set(primary_text ? primary_text : "", bot_uid);
    svec_t parts;
    size_t bi;
    sv_init(&parts);
    for (bi = 0; is_arr(blocks) && bi < json_len(blocks); bi++) {
        json_t *block = json_get(blocks, bi);
        json_t *bv = jo(block, "type");
        const char *btype = is_str(bv) ? bv->str_val : "";
        if (strcmp(btype, "table") == 0) {
            char *t = render_table_block(block, 20000);
            if (t[0]) sv_push(&parts, xstrdup(t));
            free(t);
            continue;
        }
        if (strcmp(btype, "rich_text") != 0) continue;
        {
            json_t *elements = jo(block, "elements");
            size_t ei;
            for (ei = 0; is_arr(elements) && ei < json_len(elements); ei++) {
                json_t *element = json_get(elements, ei);
                json_t *ev = jo(element, "type");
                const char *etype = is_str(ev) ? ev->str_val : "";
                json_t *wrap = json_array();
                json_t *blocks2 = json_array();
                json_t *one = json_object();
                char *rendered_raw;
                char *rendered;
                char *normalized;
                int dup;
                json_append(wrap, element ? json_copy(element) : json_null());
                json_set(one, "type", json_string("rich_text"));
                json_set(one, "elements", wrap);
                json_append(blocks2, one);
                rendered_raw = extract_text_blocks(blocks2);
                json_free(blocks2);
                rendered = strip_copy(rendered_raw);
                free(rendered_raw);
                if (!rendered[0]) {
                    free(rendered);
                    continue;
                }
                normalized = normalize_dedupe(rendered, bot_uid);
                if (strcmp(etype, "rich_text_preformatted") == 0) {
                    dup = arr_contains(pfen, normalized);
                } else {
                    dup = (strcmp(normalized, primary) == 0) || str_contains(primary, normalized);
                }
                if (normalized[0] && dup) {
                    free(normalized);
                    free(rendered);
                    continue;
                }
                free(normalized);
                sv_push(&parts, rendered);
            }
        }
    }
    free(primary);
    json_free(pfen);
    return sv_join(&parts, "\n");
}

/* ---- Block Kit payload dump (sanitize + indent-2 JSON) ---- */

static const char *SCALAR_KEYS[] = {"type",     "block_id",  "action_id", "style",
                                    "dispatch_action", "optional", "multiple", "emoji"};
static const char *RECURSIVE_KEYS[] = {"text",       "title",     "description", "label",
                                       "placeholder", "accessory", "fields",     "elements",
                                       "options",    "option_groups", "confirm", "submit",
                                       "close",      "hint"};

static int in_list(const char *needle, const char **list, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (strcmp(needle, list[i]) == 0) return 1;
    }
    return 0;
}

static int empty_like(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_STRING) return v->str_val == NULL || v->str_val[0] == '\0';
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count == 0;
    return 0;
}

static json_t *sanitize_block_value(const json_t *v);

static json_t *sanitize_block_value(const json_t *v) {
    if (is_arr(v)) {
        json_t *out = json_array();
        size_t i;
        for (i = 0; i < json_len(v); i++) {
            json_t *item = sanitize_block_value(json_get(v, i));
            if (!empty_like(item)) json_append(out, item);
            else json_free(item);
        }
        return out;
    }
    if (is_obj(v)) {
        json_t *out = json_object();
        size_t i;
        for (i = 0; i < v->c.count; i++) {
            const char *key = v->c.keys[i];
            json_t *item = v->c.items[i];
            if (in_list(key, SCALAR_KEYS, sizeof(SCALAR_KEYS) / sizeof(SCALAR_KEYS[0]))) {
                json_set(out, key, item ? json_copy(item) : json_null());
            } else if (in_list(key, RECURSIVE_KEYS,
                               sizeof(RECURSIVE_KEYS) / sizeof(RECURSIVE_KEYS[0]))) {
                json_t *cleaned = sanitize_block_value(item);
                if (!empty_like(cleaned)) json_set(out, key, cleaned);
                else json_free(cleaned);
            }
        }
        return out;
    }
    if (v && (v->type == JSON_STRING || v->type == JSON_NUMBER || v->type == JSON_BOOL ||
              v->type == JSON_NULL))
        return json_copy(v);
    if (!v) return json_null();
    {
        char *r = py_repr(v);
        json_t *out = json_string(r);
        free(r);
        return out;
    }
}

static void dump_json_string(sb_t *b, const char *s) {
    sb_addc(b, '"');
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
            case '"': sb_adds(b, "\\\""); break;
            case '\\': sb_adds(b, "\\\\"); break;
            case '\n': sb_adds(b, "\\n"); break;
            case '\r': sb_adds(b, "\\r"); break;
            case '\t': sb_adds(b, "\\t"); break;
            case '\b': sb_adds(b, "\\b"); break;
            case '\f': sb_adds(b, "\\f"); break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    sb_adds(b, esc);
                } else {
                    sb_addc(b, (char)c);
                }
        }
    }
    sb_addc(b, '"');
}

static void indent_to(sb_t *b, int level) {
    int i;
    for (i = 0; i < level; i++) sb_adds(b, "  ");
}

/* json.dumps(value, ensure_ascii=False, indent=2) */
static void dump_indent2(sb_t *b, const json_t *v, int level) {
    if (!v || v->type == JSON_NULL) {
        sb_adds(b, "null");
        return;
    }
    switch (v->type) {
        case JSON_BOOL: sb_adds(b, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val) {
                sb_add_int(b, (long long)v->num_val);
            } else {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
                sb_adds(b, tmp);
            }
            return;
        case JSON_STRING: dump_json_string(b, v->str_val ? v->str_val : ""); return;
        case JSON_ARRAY: {
            size_t i;
            if (v->c.count == 0) {
                sb_adds(b, "[]");
                return;
            }
            sb_adds(b, "[\n");
            for (i = 0; i < v->c.count; i++) {
                indent_to(b, level + 1);
                dump_indent2(b, json_get(v, i), level + 1);
                if (i + 1 < v->c.count) sb_addc(b, ',');
                sb_addc(b, '\n');
            }
            indent_to(b, level);
            sb_addc(b, ']');
            return;
        }
        case JSON_OBJECT: {
            size_t i;
            if (v->c.count == 0) {
                sb_adds(b, "{}");
                return;
            }
            sb_adds(b, "{\n");
            for (i = 0; i < v->c.count; i++) {
                indent_to(b, level + 1);
                dump_json_string(b, v->c.keys[i]);
                sb_adds(b, ": ");
                dump_indent2(b, json_object_get_at(v, i), level + 1);
                if (i + 1 < v->c.count) sb_addc(b, ',');
                sb_addc(b, '\n');
            }
            indent_to(b, level);
            sb_addc(b, '}');
            return;
        }
        default: sb_adds(b, "null"); return;
    }
}

static void group_digits(long long v, char *out, size_t outsz) {
    char raw[32];
    size_t n, i, j = 0;
    snprintf(raw, sizeof(raw), "%lld", v < 0 ? -v : v);
    n = strlen(raw);
    if (v < 0) out[j++] = '-';
    for (i = 0; i < n && j + 4 < outsz; i++) {
        out[j++] = raw[i];
        if ((n - i - 1) % 3 == 0 && i + 1 < n) out[j++] = ',';
    }
    out[j] = '\0';
}

/* ⟪HERMES-CONTEXT-COMPRESSION: {omitted} of {total} chars omitted here by
 * Hermes's context compressor.⟫ */
static char *elision_marker(long long omitted, long long total) {
    char o[40], t[40];
    sb_t b;
    group_digits(omitted, o, sizeof(o));
    group_digits(total, t, sizeof(t));
    sb_init(&b);
    sb_adds(&b, "\xe2\x9f\xaa" "HERMES-CONTEXT-COMPRESSION: ");
    sb_adds(&b, o);
    sb_adds(&b, " of ");
    sb_adds(&b, t);
    sb_adds(&b, " chars omitted here by Hermes's context compressor.\xe2\x9f\xab");
    return sb_take(&b);
}

/* agent.compression_marker.elide */
static char *elide_text(const char *text, long limit) {
    long total;
    long marker_len;
    char *marker;
    char *kept;
    sb_t b;
    if (!text) text = "";
    total = (long)cp_count(text);
    if (total <= limit) return xstrdup(text);
    marker = elision_marker(total, total);
    marker_len = (long)cp_count(marker);
    free(marker);
    if (limit - marker_len <= 0) return elision_marker(total, total);
    kept = rstrip_copy(cp_prefix(text, (size_t)(limit - marker_len)));
    marker = elision_marker(total - (long)cp_count(kept), total);
    sb_init(&b);
    sb_adds(&b, kept);
    sb_adds(&b, marker);
    free(kept);
    free(marker);
    return sb_take(&b);
}

/* _serialize_slack_blocks_for_agent */
static char *serialize_blocks(const json_t *blocks, long max_chars) {
    json_t *inspectable = json_array();
    json_t *payload_tree;
    sb_t dump;
    char *payload;
    char *elided;
    sb_t out;
    size_t i;
    for (i = 0; is_arr(blocks) && i < json_len(blocks); i++) {
        json_t *block = json_get(blocks, i);
        json_t *bv = jo(block, "type");
        const char *btype = is_str(bv) ? bv->str_val : "";
        if (strcmp(btype, "rich_text") == 0 || strcmp(btype, "table") == 0) continue;
        json_append(inspectable, block ? json_copy(block) : json_null());
    }
    if (json_len(inspectable) == 0) {
        json_free(inspectable);
        return xstrdup("");
    }
    payload_tree = sanitize_block_value(inspectable);
    json_free(inspectable);
    sb_init(&dump);
    dump_indent2(&dump, payload_tree, 0);
    json_free(payload_tree);
    payload = sb_take(&dump);
    elided = elide_text(payload, max_chars);
    free(payload);
    sb_init(&out);
    sb_adds(&out, "[Slack Block Kit payload for this message]\n```json\n");
    sb_adds(&out, elided);
    sb_adds(&out, "\n```");
    free(elided);
    return sb_take(&out);
}

/* _extract_urls_from_slack_blocks */
static void url_walk(const json_t *node, svec_t *found, int depth) {
    if (depth > 64 || !node) return;
    if (node->type == JSON_OBJECT) {
        static const char *KEYS[3] = {"url", "image_url", "external_url"};
        size_t i;
        for (i = 0; i < 3; i++) {
            json_t *v = jo(node, KEYS[i]);
            if (is_str(v) && (strncmp(v->str_val, "http://", 7) == 0 ||
                              strncmp(v->str_val, "https://", 8) == 0)) {
                size_t k;
                int seen = 0;
                for (k = 0; k < found->n; k++) {
                    if (strcmp(found->v[k], v->str_val) == 0) {
                        seen = 1;
                        break;
                    }
                }
                if (!seen) sv_push(found, xstrdup(v->str_val));
            }
        }
        for (i = 0; i < node->c.count; i++) url_walk(node->c.items[i], found, depth + 1);
    } else if (node->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < json_len(node); i++) url_walk(json_get(node, i), found, depth + 1);
    }
}

json_t *w27_p3_unescape_entities(const json_t *req) {
    json_t *v = jo(req, "text");
    char *out = unescape_entities(is_str(v) ? v->str_val : (v ? "" : NULL));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p3_normalize_dedupe(const json_t *req) {
    json_t *v = jo(req, "text");
    char *out = normalize_dedupe(is_str(v) ? v->str_val : NULL, rq_str(req, "bot_uid", ""));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p3_additional_text(const json_t *req) {
    json_t *p = jo(req, "primary_text");
    char *out = additional_text_blocks(jo(req, "blocks"), is_str(p) ? p->str_val : "",
                                       rq_str(req, "bot_uid", ""));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p3_serialize_blocks(const json_t *req) {
    json_t *mv = jo(req, "max_chars");
    long maxc = is_num(mv) ? (long)mv->num_val : 6000;
    char *out = serialize_blocks(jo(req, "blocks"), maxc);
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p3_extract_urls(const json_t *req) {
    svec_t found;
    json_t *arr = json_array();
    size_t i;
    sv_init(&found);
    url_walk(jo(req, "blocks"), &found, 0);
    for (i = 0; i < found.n; i++) json_append(arr, json_string(found.v[i]));
    for (i = 0; i < found.n; i++) free(found.v[i]);
    free(found.v);
    return arr;
}

/* ===================== gate p4: inbound file classification / diagnostics ===================== */

/* os.path.splitext(...)[1].lower() */
static char *path_ext_lower(const char *name) {
    const char *base;
    const char *dot;
    const char *p;
    char *out;
    size_t n, i;
    base = name ? name : "";
    for (p = base; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    dot = NULL;
    for (p = base; *p; p++) {
        if (*p == '.') dot = p;
    }
    if (!dot || dot == base) return xstrdup("");
    n = strlen(dot);
    out = (char *)malloc(n + 1);
    for (i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)dot[i]);
    out[n] = '\0';
    return out;
}

static const char *STT_EXTS[] = {".mp3", ".mp4", ".mpeg", ".mpga", ".m4a", ".wav", ".webm",
                                 ".ogg", ".aac", ".flac"};

static const struct {
    const char *mime;
    const char *ext;
} AUDIO_MIME_EXT[] = {
    {"audio/ogg", ".ogg"},   {"audio/opus", ".ogg"},  {"audio/mpeg", ".mp3"},
    {"audio/mp3", ".mp3"},   {"audio/wav", ".wav"},   {"audio/x-wav", ".wav"},
    {"audio/webm", ".webm"}, {"audio/mp4", ".m4a"},   {"audio/x-m4a", ".m4a"},
    {"audio/m4a", ".m4a"},   {"audio/aac", ".m4a"},   {"audio/flac", ".flac"},
    {"audio/x-flac", ".flac"},
};

/* _resolve_slack_audio_ext */
static char *resolve_audio_ext(const json_t *file_obj, const char *mimetype) {
    json_t *nv = jo(file_obj, "name");
    char *name;
    char *ext;
    char *mime_key;
    size_t i;
    name = is_str(nv) ? strip_copy(nv->str_val) : xstrdup("");
    ext = path_ext_lower(name);
    free(name);
    for (i = 0; i < sizeof(STT_EXTS) / sizeof(STT_EXTS[0]); i++) {
        if (strcmp(ext, STT_EXTS[i]) == 0) return ext;
    }
    free(ext);
    {
        const char *src = mimetype ? mimetype : "";
        const char *semi = strchr(src, ';');
        size_t n = semi ? (size_t)(semi - src) : strlen(src);
        char *head = (char *)malloc(n + 1);
        memcpy(head, src, n);
        head[n] = '\0';
        mime_key = strip_copy(head);
        free(head);
        for (i = 0; mime_key[i]; i++) mime_key[i] = (char)tolower((unsigned char)mime_key[i]);
    }
    for (i = 0; i < sizeof(AUDIO_MIME_EXT) / sizeof(AUDIO_MIME_EXT[0]); i++) {
        if (strcmp(mime_key, AUDIO_MIME_EXT[i].mime) == 0) {
            char *out = xstrdup(AUDIO_MIME_EXT[i].ext);
            free(mime_key);
            return out;
        }
    }
    free(mime_key);
    return xstrdup(".m4a");
}

/* _is_slack_voice_clip */
static int is_voice_clip(const json_t *file_obj) {
    json_t *sv = jo(file_obj, "subtype");
    json_t *nv = jo(file_obj, "name");
    char *sub;
    char *name;
    size_t i;
    int out = 0;
    if (is_str(sv)) {
        sub = strip_copy(sv->str_val);
        for (i = 0; sub[i]; i++) sub[i] = (char)tolower((unsigned char)sub[i]);
        if (strcmp(sub, "slack_audio") == 0) out = 1;
        free(sub);
    }
    if (!out && is_str(nv)) {
        name = strip_copy(nv->str_val);
        for (i = 0; name[i]; i++) name[i] = (char)tolower((unsigned char)name[i]);
        if (strncmp(name, "audio_message", 13) == 0) out = 1;
        free(name);
    }
    return out;
}

/* _attachment_label */
static char *attachment_label(const json_t *file_obj) {
    json_t *nv = jo(file_obj, "name");
    json_t *iv = jo(file_obj, "id");
    if (py_truthy(nv)) return py_str(nv);
    if (py_truthy(iv)) return py_str(iv);
    return xstrdup("this attachment");
}

/* _slack_file_kind */
static char *file_kind(const json_t *f, const char *mimetype) {
    const char *m = mimetype ? mimetype : "";
    if (strncmp(m, "image/", 6) == 0) return xstrdup("image");
    if (strncmp(m, "audio/", 6) == 0) return xstrdup("audio");
    if (strncmp(m, "video/", 6) == 0) return xstrdup(is_voice_clip(f) ? "voice clip" : "video");
    return xstrdup("document");
}

/* _describe_slack_api_error */
static char *describe_api_error(const json_t *response, const json_t *file_obj) {
    json_t *ev;
    char *error;
    char *label;
    sb_t b;
    if (!is_obj(response)) return NULL;
    ev = jo(response, "error");
    error = is_str(ev) ? strip_copy(ev->str_val) : xstrdup("");
    if (!error[0]) {
        free(error);
        return NULL;
    }
    label = attachment_label(file_obj);
    sb_init(&b);
    if (strcmp(error, "missing_scope") == 0) {
        json_t *nv = jo(response, "needed");
        json_t *pv = jo(response, "provided");
        char *needed = is_str(nv) ? strip_copy(nv->str_val) : xstrdup("");
        char *provided = is_str(pv) ? strip_copy(pv->str_val) : xstrdup("");
        sb_adds(&b, "Slack attachment access failed for ");
        sb_adds(&b, label);
        sb_adds(&b, ". ");
        if (needed[0]) {
            sb_adds(&b, "Missing scope: ");
            sb_adds(&b, needed);
            sb_adds(&b, ".");
        } else {
            sb_adds(&b, "Missing required Slack scope.");
        }
        if (provided[0]) {
            sb_adds(&b, " Current bot scopes: ");
            sb_adds(&b, provided);
            sb_adds(&b, ".");
        }
        sb_adds(&b,
                " Update the Slack app scopes/settings and reinstall the app to the workspace.");
        free(needed);
        free(provided);
    } else if (strcmp(error, "not_authed") == 0 || strcmp(error, "invalid_auth") == 0 ||
               strcmp(error, "account_inactive") == 0 || strcmp(error, "token_revoked") == 0) {
        sb_adds(&b, "Slack attachment access failed for ");
        sb_adds(&b, label);
        sb_adds(&b, " because the bot token is not authorized (");
        sb_adds(&b, error);
        sb_adds(&b, "). Refresh the token/reinstall the app.");
    } else if (strcmp(error, "file_not_found") == 0 || strcmp(error, "file_deleted") == 0) {
        sb_adds(&b, "Slack attachment ");
        sb_adds(&b, label);
        sb_adds(&b, " is no longer available (");
        sb_adds(&b, error);
        sb_adds(&b, ").");
    } else if (strcmp(error, "access_denied") == 0 || strcmp(error, "file_access_denied") == 0 ||
               strcmp(error, "no_permission") == 0 ||
               strcmp(error, "not_allowed_token_type") == 0 ||
               strcmp(error, "restricted_action") == 0) {
        sb_adds(&b, "Slack attachment access failed for ");
        sb_adds(&b, label);
        sb_adds(&b, " because the bot does not have permission (");
        sb_adds(&b, error);
        sb_adds(&b, "). Check workspace permissions/scopes and reinstall if needed.");
    } else {
        sb_free(&b);
        free(label);
        free(error);
        return NULL;
    }
    free(label);
    free(error);
    return sb_take(&b);
}

/* _describe_slack_download_failure (the httpx HTTPStatusError branch is a seam) */
static char *describe_download_failure(const json_t *response, const char *message,
                                       const json_t *file_obj) {
    char *api_detail = describe_api_error(response, file_obj);
    const char *msg = message ? message : "";
    if (api_detail) return api_detail;
    if (strstr(msg, "Slack returned HTML instead of media") || strstr(msg, "non-image data")) {
        char *label = attachment_label(file_obj);
        sb_t b;
        sb_init(&b);
        sb_adds(&b, "Slack attachment access failed for ");
        sb_adds(&b, label);
        sb_adds(&b,
                ": Slack returned an HTML/login or non-media response. This usually means a "
                "scope, auth, or file-permission problem.");
        free(label);
        return sb_take(&b);
    }
    return NULL;
}

static int lower_contains(const char *hay, const char *needle) { return strstr(hay, needle) != NULL; }

/* _is_retryable_upload_error; the base adapter's _is_retryable_error patterns are
 * ported here, the exception-object walk is narrowed to (message, response error). */
static int retryable_upload_error(const json_t *status_code, const char *message,
                                  const char *response_error) {
    static const char *TRANSIENT[] = {"rate_limited", "ratelimited", "429", "connection reset",
                                      "service unavailable", "temporarily unavailable"};
    static const char *BASE_PATTERNS[] = {"connecterror", "connectionerror", "connectionreset",
                                          "connectionrefused", "connecttimeout", "network",
                                          "broken pipe", "remotedisconnected", "eoferror"};
    size_t i;
    if (is_num(status_code)) {
        long sc = (long)status_code->num_val;
        return sc == 429 || sc >= 500;
    }
    {
        sb_t b;
        char *body;
        int out = 0;
        sb_init(&b);
        sb_adds(&b, message ? message : "");
        sb_addc(&b, ' ');
        sb_adds(&b, response_error ? response_error : "");
        body = sb_take(&b);
        for (i = 0; body[i]; i++) body[i] = (char)tolower((unsigned char)body[i]);
        for (i = 0; i < sizeof(TRANSIENT) / sizeof(TRANSIENT[0]); i++) {
            if (lower_contains(body, TRANSIENT[i])) {
                out = 1;
                break;
            }
        }
        if (!out) {
            for (i = 0; i < sizeof(BASE_PATTERNS) / sizeof(BASE_PATTERNS[0]); i++) {
                if (lower_contains(body, BASE_PATTERNS[i])) {
                    out = 1;
                    break;
                }
            }
        }
        free(body);
        return out;
    }
}

json_t *w27_p4_resolve_audio_ext(const json_t *req) {
    char *out = resolve_audio_ext(jo(req, "file_obj"), rq_str(req, "mimetype", ""));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p4_is_voice_clip(const json_t *req) {
    return json_bool(is_voice_clip(jo(req, "file_obj")));
}

json_t *w27_p4_attachment_label(const json_t *req) {
    char *out = attachment_label(jo(req, "file_obj"));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p4_file_kind(const json_t *req) {
    char *out = file_kind(jo(req, "file_obj"), rq_str(req, "mimetype", ""));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p4_describe_api_error(const json_t *req) {
    char *out = describe_api_error(jo(req, "response"), jo(req, "file_obj"));
    json_t *res;
    if (!out) return json_null();
    res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p4_describe_download_failure(const json_t *req) {
    char *out = describe_download_failure(jo(req, "response"), rq_str(req, "message", ""),
                                          jo(req, "file_obj"));
    json_t *res;
    if (!out) return json_null();
    res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p4_retryable_upload_error(const json_t *req) {
    return json_bool(retryable_upload_error(jo(req, "status_code"), rq_str(req, "message", ""),
                                            rq_str(req, "response_error", "")));
}

/* ===================== gate p5: bounded caches / workspace ids ===================== */

typedef struct {
    char *entry;
    long long sec;
    long long frac;
    char *third;
} tskey_t;

static int tskey_cmp(const void *pa, const void *pb) {
    const tskey_t *a = (const tskey_t *)pa;
    const tskey_t *b = (const tskey_t *)pb;
    if (a->sec != b->sec) return a->sec < b->sec ? -1 : 1;
    if (a->frac != b->frac) return a->frac < b->frac ? -1 : 1;
    return strcmp(a->third, b->third);
}

/* _slack_timestamp_sort_key (the (team, ts) marker is unwrapped first, so the
 * third component is str() of the inner ts, not of the tuple). */
static void timestamp_key(const json_t *tsv, long long *sec, long long *frac, char **third) {
    const json_t *val = tsv;
    if (is_arr(tsv) && json_len(tsv) == 2) val = json_get(tsv, 1);
    {
        char *s = py_str(val);
        char *dot = strchr(s, '.');
        char fracbuf[16];
        if (dot) {
            *dot = '\0';
            snprintf(fracbuf, sizeof(fracbuf), "%s000000", dot + 1);
            fracbuf[6] = '\0';
            *frac = int_or_zero(fracbuf);
        } else {
            *frac = int_or_zero("000000");
        }
        *sec = int_or_zero(s);
        free(s);
    }
    *third = py_str(val);
}

/* Build the chronologically sorted key list for a string array. */
static tskey_t *sorted_keys(const json_t *entries, const char *mode, size_t *n_out) {
    size_t n = is_arr(entries) ? json_len(entries) : 0, i;
    tskey_t *keys = (tskey_t *)calloc(n ? n : 1, sizeof(tskey_t));
    for (i = 0; i < n; i++) {
        json_t *e = json_get(entries, i);
        json_t *sortval = e;
        if (mode && strcmp(mode, "suffix") == 0 && is_str(e)) {
            /* e.split(":")[2] if e.count(":") >= 2 else "" */
            const char *s = e->str_val;
            int colons = 0;
            const char *p;
            for (p = s; *p; p++)
                if (*p == ':') colons++;
            if (colons >= 2) {
                const char *c1 = strchr(s, ':');
                const char *c2 = c1 ? strchr(c1 + 1, ':') : NULL;
                if (c2) {
                    const char *c3 = strchr(c2 + 1, ':');
                    size_t len = c3 ? (size_t)(c3 - (c2 + 1)) : strlen(c2 + 1);
                    char *seg = (char *)malloc(len + 1);
                    memcpy(seg, c2 + 1, len);
                    seg[len] = '\0';
                    sortval = json_string(seg);
                    free(seg);
                } else {
                    sortval = json_string("");
                }
            } else {
                sortval = json_string("");
            }
        } else {
            sortval = e ? json_copy(e) : json_null();
        }
        timestamp_key(sortval, &keys[i].sec, &keys[i].frac, &keys[i].third);
        keys[i].entry = py_str(e);
        json_free(sortval);
    }
    *n_out = n;
    if (n > 1) qsort(keys, n, sizeof(tskey_t), tskey_cmp);
    return keys;
}

static void free_keys(tskey_t *keys, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        free(keys[i].entry);
        free(keys[i].third);
    }
    free(keys);
}

static json_t *keys_to_array(tskey_t *keys, size_t from, size_t to) {
    json_t *arr = json_array();
    size_t i;
    for (i = from; i < to; i++) json_append(arr, json_string(keys[i].entry));
    return arr;
}

/* _metadata_team_id */
static char *metadata_team_id(const json_t *metadata) {
    static const char *KEYS[6] = {"scope_id", "slack_team_id", "team_id",
                                  "team",     "guild_id",      "workspace_id"};
    static const char *SRC_KEYS[4] = {"scope_id", "slack_team_id", "team_id", "guild_id"};
    size_t i;
    if (!py_truthy(metadata) || !is_obj(metadata)) return xstrdup("");
    for (i = 0; i < 6; i++) {
        json_t *v = json_obj_get(metadata, KEYS[i]);
        if (py_truthy(v)) return py_str(v);
    }
    {
        json_t *src = jo(metadata, "source");
        if (is_obj(src)) {
            for (i = 0; i < 4; i++) {
                json_t *v = json_obj_get(src, SRC_KEYS[i]);
                if (py_truthy(v)) return py_str(v);
            }
        }
    }
    return xstrdup("");
}

/* _is_ignored_channel */
static int ignored_channel(const json_t *channel_id, const json_t *ignored) {
    char *cid;
    char *head;
    size_t i;
    int out = 0;
    if (!py_truthy(channel_id)) return 0;
    cid = py_str(channel_id);
    if (is_arr(ignored)) {
        for (i = 0; i < json_len(ignored); i++) {
            json_t *v = json_get(ignored, i);
            if (is_str(v) && strcmp(v->str_val, "*") == 0) {
                out = 1;
                break;
            }
        }
    }
    if (!out) {
        head = xstrdup(cid);
        {
            char *colon = strchr(head, ':');
            if (colon) *colon = '\0';
        }
        if (is_arr(ignored)) {
            for (i = 0; i < json_len(ignored); i++) {
                json_t *v = json_get(ignored, i);
                if (is_str(v) && strcmp(v->str_val, head) == 0) {
                    out = 1;
                    break;
                }
            }
        }
        free(head);
    }
    free(cid);
    return out;
}

json_t *w27_p5_timestamp_sort_key(const json_t *req) {
    json_t *pair = jo(req, "pair");
    json_t *tsv = pair ? pair : jo(req, "ts");
    long long sec, frac;
    char *third;
    json_t *out = json_array();
    timestamp_key(tsv, &sec, &frac, &third);
    json_append(out, json_number((double)sec));
    json_append(out, json_number((double)frac));
    json_append(out, json_string(third));
    free(third);
    return out;
}

json_t *w27_p5_discard_oldest(const json_t *req) {
    json_t *entries = jo(req, "entries");
    const char *mode = rq_str(req, "mode", "identity");
    long count = (long)rq_num(req, "count", 0);
    tskey_t *keys;
    size_t n;
    json_t *out = json_object();
    keys = sorted_keys(entries, mode, &n);
    if (count < 0) count = 0;
    if ((size_t)count > n) count = (long)n;
    json_set(out, "removed", keys_to_array(keys, 0, (size_t)count));
    json_set(out, "remaining", keys_to_array(keys, (size_t)count, n));
    free_keys(keys, n);
    return out;
}

json_t *w27_p5_evict_oldest(const json_t *req) {
    json_t *entries = jo(req, "entries");
    const char *mode = rq_str(req, "mode", "identity");
    long cap = (long)rq_num(req, "cap", 0);
    tskey_t *keys;
    size_t n;
    json_t *out;
    long drop;
    keys = sorted_keys(entries, mode, &n);
    if ((long)n > cap) {
        drop = (long)n - cap / 2;
        if (drop < 0) drop = 0;
        if ((size_t)drop > n) drop = (long)n;
        out = keys_to_array(keys, (size_t)drop, n);
    } else {
        out = keys_to_array(keys, 0, n);
    }
    free_keys(keys, n);
    return out;
}

json_t *w27_p5_trim_dict_entries(const json_t *req) {
    json_t *mapping = jo(req, "mapping");
    long max_size = (long)rq_num(req, "max_size", 0);
    json_t *out = json_object();
    size_t n, i, excess;
    if (!is_obj(mapping)) return out;
    n = json_object_size(mapping);
    if ((long)n <= max_size) return json_copy(mapping);
    excess = n - (size_t)(max_size / 2);
    if (excess > n) excess = n;
    for (i = excess; i < n; i++) {
        const char *k = json_object_get_key_at(mapping, i);
        json_set(out, k, json_copy(json_object_get_at(mapping, i)));
    }
    return out;
}

json_t *w27_p5_metadata_team_id(const json_t *req) {
    char *out = metadata_team_id(jo(req, "metadata"));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p5_workspace_event_id(const json_t *req) {
    json_t *team = jo(req, "team_id");
    json_t *ev = jo(req, "event_id");
    sb_t b;
    json_t *res;
    if (py_truthy(team)) {
        char *t = py_str(team);
        char *e = py_str(ev);
        sb_init(&b);
        sb_adds(&b, t);
        sb_addc(&b, ':');
        sb_adds(&b, e);
        free(t);
        free(e);
        res = json_string(b.p ? b.p : "");
        sb_free(&b);
        return res;
    }
    {
        char *e = py_str(ev);
        res = json_string(e);
        free(e);
    }
    return res;
}

json_t *w27_p5_workspace_message_marker(const json_t *req) {
    json_t *team = jo(req, "team_id");
    json_t *msg = jo(req, "message_id");
    if (py_truthy(team)) {
        json_t *arr = json_array();
        char *t = py_str(team);
        char *m = py_str(msg);
        json_append(arr, json_string(t));
        json_append(arr, json_string(m));
        free(t);
        free(m);
        return arr;
    }
    {
        char *m = py_str(msg);
        json_t *res = json_string(m);
        free(m);
        return res;
    }
}

json_t *w27_p5_truthy_config(const json_t *req) {
    json_t *v = jo(req, "value");
    if (is_str(v)) {
        char *low = strip_copy(v->str_val);
        size_t i;
        int out;
        for (i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
        out = is_truthy_word(low);
        free(low);
        return json_bool(out);
    }
    return json_bool(py_truthy(v));
}

json_t *w27_p5_is_ignored_channel(const json_t *req) {
    return json_bool(ignored_channel(jo(req, "channel_id"), jo(req, "ignored")));
}

json_t *w27_p5_workspace_thread_key(const json_t *req) {
    json_t *team = jo(req, "team_id");
    json_t *ch = jo(req, "channel_id");
    json_t *ts = jo(req, "thread_ts");
    json_t *arr;
    char *t, *c, *s;
    if (!py_truthy(ch) || !py_truthy(ts)) return json_null();
    arr = json_array();
    t = py_truthy(team) ? py_str(team) : xstrdup("");
    c = py_str(ch);
    s = py_str(ts);
    json_append(arr, json_string(t));
    json_append(arr, json_string(c));
    json_append(arr, json_string(s));
    free(t);
    free(c);
    free(s);
    return arr;
}

json_t *w27_p5_agent_view_context_key(const json_t *req) {
    json_t *team = jo(req, "team_id");
    json_t *user = jo(req, "user_id");
    json_t *arr;
    char *t, *u;
    if (!py_truthy(team) || !py_truthy(user)) return json_null();
    arr = json_array();
    t = py_str(team);
    u = py_str(user);
    json_append(arr, json_string(t));
    json_append(arr, json_string(u));
    free(t);
    free(u);
    return arr;
}

/* ===================== gate p6: task cards, streams, bot policy ===================== */

/* _task_update_chunk */
static json_t *task_update_chunk(const json_t *task) {
    json_t *sv = jo(task, "status");
    json_t *iv = jo(task, "id");
    json_t *tv = jo(task, "task_id");
    json_t *ti = jo(task, "title");
    char *status;
    char *task_id;
    char *title;
    char *short_title;
    json_t *out = json_object();
    status = py_truthy(sv) ? py_str(sv) : xstrdup("in_progress");
    if (strcmp(status, "in_progress") != 0 && strcmp(status, "complete") != 0 &&
        strcmp(status, "error") != 0) {
        free(status);
        status = xstrdup("in_progress");
    }
    if (py_truthy(iv)) task_id = py_str(iv);
    else if (py_truthy(tv)) task_id = py_str(tv);
    else task_id = xstrdup("task");
    title = py_truthy(ti) ? py_str(ti) : xstrdup(task_id);
    short_title = cp_prefix(title, 256);
    json_set(out, "type", json_string("task_update"));
    json_set(out, "id", json_string(task_id));
    json_set(out, "title", json_string(short_title));
    json_set(out, "status", json_string(status));
    free(status);
    free(task_id);
    free(title);
    free(short_title);
    return out;
}

static int str_endswith(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    if (m > n) return 0;
    return strcmp(s + n - m, suf) == 0;
}

/* _strip_stream_cursor */
static char *strip_stream_cursor(const char *text, const json_t *glyphs) {
    static const char *DEFAULT_GLYPHS[4] = {"\xe2\x96\x89", "\xe2\x96\x8d", "\xe2\x96\x8c",
                                            "\xe2\x80\xa6"};
    char *stripped = rstrip_copy(text ? text : "");
    const char *hit = NULL;
    size_t i;
    if (is_arr(glyphs)) {
        for (i = 0; i < json_len(glyphs); i++) {
            json_t *g = json_get(glyphs, i);
            if (is_str(g) && g->str_val[0] && str_endswith(stripped, g->str_val)) {
                hit = g->str_val;
                break;
            }
        }
    } else {
        for (i = 0; i < 4; i++) {
            if (str_endswith(stripped, DEFAULT_GLYPHS[i])) {
                hit = DEFAULT_GLYPHS[i];
                break;
            }
        }
    }
    if (hit) {
        size_t n = strlen(stripped) - strlen(hit);
        char *cut = (char *)malloc(n + 1);
        char *out;
        memcpy(cut, stripped, n);
        cut[n] = '\0';
        out = rstrip_copy(cut);
        free(cut);
        free(stripped);
        return out;
    }
    free(stripped);
    return xstrdup(text ? text : "");
}

/* SlackAdapter._resolve_thread_ts re-used by _stream_key (reply_to is None). */
static json_t *resolve_thread_node(const json_t *metadata, int reply_in_thread) {
    const json_t *md = metadata;
    json_t *existing, *tid, *tts;
    if (!reply_in_thread) {
        tid = jo(md, "thread_id");
        tts = jo(md, "thread_ts");
        existing = py_truthy(tid) ? tid : (py_truthy(tts) ? tts : NULL);
        return existing ? json_copy(existing) : json_null();
    }
    tid = jo(md, "thread_id");
    if (py_truthy(tid)) return json_copy(tid);
    tts = jo(md, "thread_ts");
    if (py_truthy(tts)) return json_copy(tts);
    return json_null();
}

/* _mrkdwn_insensitive */
static char *mrkdwn_insensitive(const char *text) {
    sb_t b;
    size_t i;
    int pending = 0, wrote = 0;
    sb_init(&b);
    for (i = 0; text && text[i]; i++) {
        char c = text[i];
        if (c == '*' || c == '_' || c == '~') continue;
        if (isspace((unsigned char)c)) {
            pending = 1;
            continue;
        }
        if (pending && wrote) sb_addc(&b, ' ');
        pending = 0;
        sb_addc(&b, c);
        wrote = 1;
    }
    return sb_take(&b);
}

/* _stream_relation */
static void stream_relation(const char *sent, const char *text, const char **rel, char **delta) {
    char *core;
    *rel = "unrelated";
    *delta = xstrdup("");
    core = strip_copy(sent ? sent : "");
    if (!core[0]) {
        free(core);
        return;
    }
    if (strncmp(text ? text : "", sent ? sent : "", strlen(sent ? sent : "")) == 0) {
        char *d = xstrdup((text ? text : "") + strlen(sent ? sent : ""));
        *rel = d[0] ? "extends" : "equal";
        *delta = d;
        free(core);
        return;
    }
    {
        const char *t = text ? text : "";
        size_t lead = 0;
        while (t[lead] && isspace((unsigned char)t[lead])) lead++;
        if (strncmp(t + lead, core, strlen(core)) == 0) {
            char *d = xstrdup(t + lead + strlen(core));
            char *ds = strip_copy(d);
            *rel = ds[0] ? "extends" : "equal";
            free(ds);
            *delta = d;
            free(core);
            return;
        }
    }
    free(core);
}

/* _default_status_text */
static char *default_status_text(const json_t *started, double now) {
    long long elapsed;
    if (!started || started->type == JSON_NULL) {
        elapsed = 0;
    } else if (is_num(started)) {
        elapsed = (long long)(now - started->num_val);
    } else {
        elapsed = 0;
    }
    if (elapsed < 30) return xstrdup("is thinking...");
    {
        long long mins = elapsed / 60, secs = elapsed % 60;
        sb_t b;
        char el[32];
        if (mins) snprintf(el, sizeof(el), "%lldm%02llds", mins, secs);
        else snprintf(el, sizeof(el), "%llds", secs);
        sb_init(&b);
        sb_adds(&b, "still working\xe2\x80\xa6 (");
        sb_adds(&b, el);
        sb_addc(&b, ')');
        return sb_take(&b);
    }
}

/* _slack_api_human_users over a raw list/CSV value */
static json_t *api_human_users(const json_t *raw) {
    json_t *arr = json_array();
    svec_t parts;
    size_t i;
    sv_init(&parts);
    if (is_arr(raw)) {
        for (i = 0; i < json_len(raw); i++) {
            json_t *v = json_get(raw, i);
            char *s = py_str(v);
            char *t = strip_copy(s);
            if (t[0]) sv_push(&parts, t);
            else free(t);
            free(s);
        }
    } else {
        char *s = py_str(raw);
        char *cur = s;
        for (;;) {
            char *comma = strchr(cur, ',');
            char *seg;
            if (comma) *comma = '\0';
            seg = strip_copy(cur);
            if (seg[0]) sv_push(&parts, xstrdup(seg));
            free(seg);
            if (!comma) break;
            cur = comma + 1;
        }
        free(s);
    }
    /* frozenset: dedupe + sort for a deterministic projection */
    {
        svec_t uniq;
        size_t a, b;
        sv_init(&uniq);
        for (a = 0; a < parts.n; a++) {
            int seen = 0;
            for (b = 0; b < uniq.n; b++) {
                if (strcmp(uniq.v[b], parts.v[a]) == 0) {
                    seen = 1;
                    break;
                }
            }
            if (!seen) sv_push(&uniq, xstrdup(parts.v[a]));
        }
        for (a = 0; a < uniq.n; a++) {
            for (b = a + 1; b < uniq.n; b++) {
                if (strcmp(uniq.v[a], uniq.v[b]) > 0) {
                    char *tmp = uniq.v[a];
                    uniq.v[a] = uniq.v[b];
                    uniq.v[b] = tmp;
                }
            }
        }
        for (a = 0; a < uniq.n; a++) json_append(arr, json_string(uniq.v[a]));
        for (a = 0; a < uniq.n; a++) free(uniq.v[a]);
        free(uniq.v);
    }
    for (i = 0; i < parts.n; i++) free(parts.v[i]);
    free(parts.v);
    return arr;
}

static int list_has(const json_t *arr, const char *v) {
    size_t i;
    for (i = 0; is_arr(arr) && i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        if (is_str(e) && strcmp(e->str_val, v) == 0) return 1;
    }
    return 0;
}

/* _event_declares_bot_sender */
static int event_declares_bot_sender(const json_t *event, const json_t *humans) {
    json_t *profile, *sv;
    if (py_truthy(jo(event, "bot_id")) || py_truthy(jo(event, "bot_profile"))) return 1;
    sv = jo(event, "subtype");
    if (is_str(sv) && strcmp(sv->str_val, "bot_message") == 0) return 1;
    profile = jo(event, "user_profile");
    if (is_obj(profile) && py_truthy(jo(profile, "is_bot"))) return 1;
    if (py_truthy(jo(event, "app_id")) && !py_truthy(jo(event, "client_msg_id"))) {
        json_t *user = jo(event, "user");
        char *u = py_str(user);
        int known = list_has(humans, u);
        free(u);
        return !known;
    }
    return 0;
}

/* format_tool_preview */
static char *format_tool_preview(const char *text) {
    sb_t b;
    size_t i;
    sb_init(&b);
    sb_addc(&b, '`');
    for (i = 0; text && text[i]; i++) {
        if (text[i] == '`') sb_adds(&b, "\xcb\x8b"); /* U+02CB MODIFIER LETTER GRAVE */
        else sb_addc(&b, text[i]);
    }
    sb_addc(&b, '`');
    return sb_take(&b);
}

/* _markdown_block_payload */
static json_t *markdown_block_payload(const char *content, long max_chars) {
    json_t *arr;
    json_t *block;
    json_t *text;
    if (!content || !content[0]) return json_null();
    text = json_string(content);
    {
        char *st = strip_copy(content);
        int has = st[0] != '\0';
        free(st);
        if (!has || (long)cp_count(content) > max_chars) {
            json_free(text);
            return json_null();
        }
    }
    arr = json_array();
    block = json_object();
    json_set(block, "type", json_string("markdown"));
    json_set(block, "text", text);
    json_append(arr, block);
    return arr;
}

/* _feedback_block (locale strings are the shipped English defaults) */
static json_t *feedback_block(void) {
    json_t *block = json_object();
    json_t *elements = json_array();
    json_t *fb = json_object();
    json_t *pos = json_object();
    json_t *pos_text = json_object();
    json_t *neg = json_object();
    json_t *neg_text = json_object();
    json_set(pos_text, "type", json_string("plain_text"));
    json_set(pos_text, "text", json_string("Good Response"));
    json_set(pos, "text", pos_text);
    json_set(pos, "accessibility_label", json_string("Submit positive feedback on this response"));
    json_set(pos, "value", json_string("positive"));
    json_set(neg_text, "type", json_string("plain_text"));
    json_set(neg_text, "text", json_string("Bad Response"));
    json_set(neg, "text", neg_text);
    json_set(neg, "accessibility_label", json_string("Submit negative feedback on this response"));
    json_set(neg, "value", json_string("negative"));
    json_set(fb, "type", json_string("feedback_buttons"));
    json_set(fb, "action_id", json_string("hermes_feedback"));
    json_set(fb, "positive_button", pos);
    json_set(fb, "negative_button", neg);
    json_append(elements, fb);
    json_set(block, "type", json_string("context_actions"));
    json_set(block, "elements", elements);
    return block;
}

json_t *w27_p6_task_update_chunk(const json_t *req) {
    return task_update_chunk(jo(req, "task"));
}

json_t *w27_p6_strip_stream_cursor(const json_t *req) {
    char *out = strip_stream_cursor(rq_str(req, "text", ""), jo(req, "glyphs"));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p6_stream_key(const json_t *req) {
    json_t *metadata = jo(req, "metadata");
    json_t *rit = jo(req, "reply_in_thread");
    int reply_in_thread = (rit && rit->type == JSON_BOOL) ? rit->bool_val : 1;
    json_t *thread = resolve_thread_node(metadata, reply_in_thread);
    const char *chat = rq_str(req, "chat_id", "");
    char *team = metadata_team_id(metadata);
    char *ts;
    json_t *out;
    if (!team[0]) {
        free(team);
        team = xstrdup(rq_str(req, "scope_id", ""));
    }
    ts = py_truthy(thread) ? py_str(thread) : xstrdup("");
    if (!chat[0] || !ts[0]) {
        free(team);
        free(ts);
        json_free(thread);
        return json_null();
    }
    out = json_array();
    json_append(out, json_string(team));
    json_append(out, json_string(chat));
    json_append(out, json_string(ts));
    free(team);
    free(ts);
    json_free(thread);
    return out;
}

json_t *w27_p6_mrkdwn_insensitive(const json_t *req) {
    char *out = mrkdwn_insensitive(rq_str(req, "text", ""));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p6_stream_relation(const json_t *req) {
    const char *rel;
    char *delta;
    json_t *arr = json_array();
    stream_relation(rq_str(req, "sent", ""), rq_str(req, "text", ""), &rel, &delta);
    json_append(arr, json_string(rel));
    json_append(arr, json_string(delta));
    free(delta);
    return arr;
}

json_t *w27_p6_default_status_text(const json_t *req) {
    char *out = default_status_text(jo(req, "started"), rq_num(req, "now", 0.0));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p6_cron_continuable_surface(const json_t *req) {
    json_t *raw = jo(req, "raw");
    char *s = py_str(raw);
    char *t = strip_copy(s);
    size_t i;
    int in_channel;
    for (i = 0; t[i]; i++) t[i] = (char)tolower((unsigned char)t[i]);
    in_channel = strcmp(t, "in_channel") == 0;
    free(t);
    free(s);
    return json_string(in_channel ? "in_channel" : "thread");
}

json_t *w27_p6_slack_allow_bots(const json_t *req) {
    json_t *raw = jo(req, "raw");
    char *s = py_str(raw);
    char *t = strip_copy(s);
    size_t i;
    const char *out = "none";
    for (i = 0; t[i]; i++) t[i] = (char)tolower((unsigned char)t[i]);
    if (strcmp(t, "none") == 0) out = "none";
    else if (strcmp(t, "mentions") == 0) out = "mentions";
    else if (strcmp(t, "all") == 0) out = "all";
    free(t);
    free(s);
    return json_string(out);
}

json_t *w27_p6_api_human_users(const json_t *req) {
    json_t *raw = jo(req, "raw");
    json_t *env = jo(req, "env");
    if (!raw || raw->type == JSON_NULL) raw = env;
    if (!raw || raw->type == JSON_NULL) raw = json_string("");
    return api_human_users(raw);
}

json_t *w27_p6_event_declares_bot_sender(const json_t *req) {
    return json_bool(event_declares_bot_sender(jo(req, "event"), jo(req, "human_users")));
}

json_t *w27_p6_format_tool_preview(const json_t *req) {
    char *out = format_tool_preview(rq_str(req, "preview_text", ""));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p6_markdown_block_payload(const json_t *req) {
    json_t *mv = jo(req, "max_chars");
    long maxc = is_num(mv) ? (long)mv->num_val : 11500;
    return markdown_block_payload(rq_str(req, "content", ""), maxc);
}

json_t *w27_p6_append_feedback_block(const json_t *req) {
    json_t *blocks = jo(req, "blocks");
    json_t *en = jo(req, "enabled");
    int enabled = py_truthy(en);
    if (py_truthy(blocks) && enabled && json_len(blocks) < 50) {
        json_t *out = json_copy(blocks);
        json_append(out, feedback_block());
        return out;
    }
    return blocks ? json_copy(blocks) : json_null();
}

/* ===================== gate p7: thread_ts, reactions, users.info ===================== */

/* _resolve_thread_ts */
static json_t *resolve_thread_ts(const json_t *reply_to, const json_t *metadata,
                                 int reply_in_thread) {
    const json_t *md = metadata;
    json_t *tid, *tts, *existing;
    if (!reply_in_thread) {
        tid = jo(md, "thread_id");
        tts = jo(md, "thread_ts");
        existing = py_truthy(tid) ? tid : (py_truthy(tts) ? tts : NULL);
        if (existing && py_truthy(reply_to)) {
            char *a = py_str(existing);
            char *b = py_str(reply_to);
            int same = strcmp(a, b) == 0;
            free(a);
            free(b);
            if (same) existing = NULL;
        }
        return existing ? json_copy(existing) : json_null();
    }
    tid = jo(md, "thread_id");
    if (py_truthy(tid)) return json_copy(tid);
    tts = jo(md, "thread_ts");
    if (py_truthy(tts)) return json_copy(tts);
    return reply_to ? json_copy(reply_to) : json_null();
}

/* str(x).lower() not in {"false", "0", "no"} */
static int reactions_flag(const json_t *extra, const json_t *env) {
    const json_t *configured;
    json_t *value = jo(extra, "reactions");
    char *s;
    char *low;
    size_t i;
    int out;
    if (value && value->type != JSON_NULL &&
        !(is_str(value) && is_blank(value->str_val))) {
        configured = value;
    } else if (env && is_str(env) && !is_blank(env->str_val)) {
        configured = env;
    } else {
        return 1; /* default "true" */
    }
    s = py_str(configured);
    low = strip_copy(s);
    for (i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
    out = !(strcmp(low, "false") == 0 || strcmp(low, "0") == 0 || strcmp(low, "no") == 0);
    free(low);
    free(s);
    return out;
}

/* _extra_flag */
static int extra_flag(const json_t *extra, const char *key, int dflt) {
    json_t *raw = jo(extra, key);
    char *s;
    char *low;
    size_t i;
    int out;
    if (!raw || raw->type == JSON_NULL) return dflt;
    s = py_str(raw);
    low = strip_copy(s);
    for (i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
    out = is_truthy_word(low);
    free(low);
    free(s);
    return out;
}

/* _parse_users_info */
static json_t *parse_users_info(const json_t *payload, const char *user_id) {
    json_t *user = jo(payload, "user");
    json_t *profile;
    int is_bot;
    const json_t *name = NULL;
    json_t *out;
    if (!is_obj(user)) user = NULL;
    profile = user ? jo(user, "profile") : NULL;
    if (!is_obj(profile)) profile = NULL;
    is_bot = (user && py_truthy(jo(user, "is_bot"))) ||
             (user && py_truthy(jo(user, "is_workflow_bot"))) ||
             (profile && py_truthy(jo(profile, "bot_id")));
    if (profile) {
        json_t *v = jo(profile, "display_name");
        if (!py_truthy(v)) v = jo(profile, "real_name");
        name = v;
    }
    if ((!name || !py_truthy(name)) && user) {
        json_t *v = jo(user, "real_name");
        if (!py_truthy(v)) v = jo(user, "name");
        name = v;
    }
    out = json_array();
    if (name && py_truthy(name)) {
        char *s = py_str(name);
        json_append(out, json_string(s));
        free(s);
    } else {
        json_append(out, json_string(user_id ? user_id : ""));
    }
    json_append(out, json_bool(is_bot));
    return out;
}

/* _slack_reaction_triggers */
static json_t *reaction_triggers(const json_t *raw) {
    json_t *arr;
    svec_t parts;
    size_t i;
    if (!raw || raw->type == JSON_NULL) return json_null();
    if (raw->type == JSON_BOOL) return raw->bool_val ? json_array() : json_null();
    arr = json_array();
    sv_init(&parts);
    if (is_arr(raw)) {
        for (i = 0; i < json_len(raw); i++) {
            char *s = py_str(json_get(raw, i));
            char *t = strip_copy(s);
            size_t n = strlen(t);
            while (n && t[n - 1] == ':') t[--n] = '\0';
            while (t[0] == ':') memmove(t, t + 1, strlen(t));
            {
                char *u = strip_copy(t);
                if (u[0]) sv_push(&parts, u);
                else free(u);
            }
            free(t);
            free(s);
        }
    } else {
        char *s = py_str(raw);
        char *t = strip_copy(s);
        char *low = xstrdup(t);
        for (i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
        if (!t[0] || strcmp(low, "false") == 0 || strcmp(low, "0") == 0 ||
            strcmp(low, "no") == 0 || strcmp(low, "off") == 0) {
            free(low);
            free(t);
            free(s);
            free(parts.v);
            json_free(arr);
            return json_null();
        }
        if (strcmp(low, "true") == 0 || strcmp(low, "1") == 0 || strcmp(low, "yes") == 0 ||
            strcmp(low, "on") == 0 || strcmp(low, "all") == 0 || strcmp(low, "*") == 0) {
            free(low);
            free(t);
            free(s);
            free(parts.v);
            return arr; /* empty set: every emoji */
        }
        free(low);
        {
            /* re.split(r"[,\s]+", text) */
            char *cur = t;
            for (;;) {
                char *sep = cur;
                char *seg;
                while (*sep && *sep != ',' && !isspace((unsigned char)*sep)) sep++;
                {
                    char saved = *sep;
                    *sep = '\0';
                    seg = strip_copy(cur);
                    *sep = saved;
                    if (seg[0]) {
                        size_t n = strlen(seg);
                        while (n && seg[n - 1] == ':') seg[--n] = '\0';
                        while (seg[0] == ':') memmove(seg, seg + 1, strlen(seg));
                        if (seg[0]) sv_push(&parts, xstrdup(seg));
                    }
                    free(seg);
                    if (!saved) break;
                }
                cur = sep;
                while (*cur == ',' || isspace((unsigned char)*cur)) cur++;
                if (!*cur) break;
            }
        }
        free(t);
        free(s);
    }
    /* set: dedupe + sort for a deterministic projection */
    {
        svec_t uniq;
        size_t a, b;
        sv_init(&uniq);
        for (a = 0; a < parts.n; a++) {
            int seen = 0;
            for (b = 0; b < uniq.n; b++) {
                if (strcmp(uniq.v[b], parts.v[a]) == 0) {
                    seen = 1;
                    break;
                }
            }
            if (!seen) sv_push(&uniq, xstrdup(parts.v[a]));
        }
        for (a = 0; a < uniq.n; a++) {
            for (b = a + 1; b < uniq.n; b++) {
                if (strcmp(uniq.v[a], uniq.v[b]) > 0) {
                    char *tmp = uniq.v[a];
                    uniq.v[a] = uniq.v[b];
                    uniq.v[b] = tmp;
                }
            }
        }
        for (a = 0; a < uniq.n; a++) json_append(arr, json_string(uniq.v[a]));
        for (a = 0; a < uniq.n; a++) free(uniq.v[a]);
        free(uniq.v);
    }
    for (i = 0; i < parts.n; i++) free(parts.v[i]);
    free(parts.v);
    return arr;
}

/* _slack_reaction_trigger_target */
static void reaction_trigger_target(const json_t *raw, char **channel, char **thread) {
    char *s = py_truthy(raw) ? py_str(raw) : xstrdup("");
    char *t = strip_copy(s);
    char *colon = strchr(t, ':');
    if (colon) {
        *colon = '\0';
        *channel = strip_copy(t);
        *thread = strip_copy(colon + 1);
    } else {
        *channel = strip_copy(t);
        *thread = xstrdup("");
    }
    free(t);
    free(s);
}

/* _first_file_share */
static json_t *first_file_share(const json_t *file_obj, const char *channel_id) {
    json_t *shares = jo(file_obj, "shares");
    json_t *fallback = NULL;
    size_t i;
    if (!is_obj(shares)) return json_object();
    for (i = 0; i < json_object_size(shares); i++) {
        json_t *bucket = json_object_get_at(shares, i);
        json_t *channel_shares;
        if (!is_obj(bucket)) continue;
        channel_shares = json_obj_get(bucket, channel_id ? channel_id : "");
        if (py_truthy(channel_shares) && is_arr(channel_shares)) {
            json_t *first = json_get(channel_shares, 0);
            return py_truthy(first) ? json_copy(first) : json_object();
        }
        if (!fallback) {
            size_t k;
            for (k = 0; k < json_object_size(bucket); k++) {
                json_t *entries = json_object_get_at(bucket, k);
                if (py_truthy(entries) && is_arr(entries) && json_len(entries) > 0) {
                    json_t *first = json_get(entries, 0);
                    if (py_truthy(first) && is_obj(first)) fallback = first;
                    break;
                }
            }
        }
    }
    return fallback ? json_copy(fallback) : json_object();
}

static const char *reaction_emoji(const char *name) {
    static const struct {
        const char *name;
        const char *emoji;
    } MAP[] = {
        {"thumbsup", "\xf0\x9f\x91\x8d"},          {"+1", "\xf0\x9f\x91\x8d"},
        {"thumbsdown", "\xf0\x9f\x91\x8e"},        {"-1", "\xf0\x9f\x91\x8e"},
        {"white_check_mark", "\xe2\x9c\x85"},      {"heavy_check_mark", "\xe2\x9c\x85"},
        {"x", "\xe2\x9d\x8c"},                     {"no_entry", "\xe2\x9b\x94"},
        {"warning", "\xe2\x9a\xa0\xef\xb8\x8f"},   {"rotating_light", "\xf0\x9f\x9a\xa8"},
        {"eyes", "\xf0\x9f\x91\x80"},              {"rocket", "\xf0\x9f\x9a\x80"},
        {"tada", "\xf0\x9f\x8e\x89"},              {"fire", "\xf0\x9f\x94\xa5"},
        {"wave", "\xf0\x9f\x91\x8b"},
    };
    size_t i;
    for (i = 0; i < sizeof(MAP) / sizeof(MAP[0]); i++) {
        if (strcmp(name, MAP[i].name) == 0) return MAP[i].emoji;
    }
    return NULL;
}

/* _synthetic_reaction_event */
static json_t *synthetic_reaction_event(const json_t *event, const char *action,
                                        const char *thread_ts, const char *team_id,
                                        const char *target_channel, const char *target_thread) {
    json_t *item = jo(event, "item");
    json_t *channel_id = jo(item, "channel");
    json_t *msg_ts = jo(item, "ts");
    json_t *rv = jo(event, "reaction");
    json_t *uv = jo(event, "user");
    json_t *evts = jo(event, "event_ts");
    char *reaction_name = py_truthy(rv) ? py_str(rv) : xstrdup("");
    char *user_id = py_truthy(uv) ? py_str(uv) : xstrdup("None");
    char *msg = py_truthy(msg_ts) ? py_str(msg_ts) : xstrdup("None");
    char *chan = py_truthy(channel_id) ? py_str(channel_id) : xstrdup("None");
    const char *mapped = reaction_emoji(reaction_name);
    json_t *out = json_object();
    json_t *reaction_obj = json_object();
    sb_t text;
    sb_init(&text);
    sb_adds(&text, "reaction:");
    sb_adds(&text, action);
    sb_addc(&text, ':');
    sb_adds(&text, mapped ? mapped : reaction_name);
    json_set(out, "type", json_string("message"));
    json_set(out, "user", json_string(user_id));
    json_set(out, "text", json_string(text.p ? text.p : ""));
    sb_free(&text);
    json_set(out, "channel", json_string(chan));
    if (py_truthy(evts)) {
        char *s = py_str(evts);
        json_set(out, "ts", json_string(s));
        free(s);
    } else {
        sb_t ts;
        sb_init(&ts);
        sb_adds(&ts, "reaction-");
        sb_adds(&ts, msg);
        sb_addc(&ts, '-');
        sb_adds(&ts, reaction_name);
        sb_addc(&ts, '-');
        sb_adds(&ts, user_id);
        json_set(out, "ts", json_string(ts.p ? ts.p : ""));
        sb_free(&ts);
    }
    json_set(out, "thread_ts", json_string(thread_ts ? thread_ts : ""));
    json_set(out, "_hermes_force_process", json_bool(1));
    json_set(reaction_obj, "name", json_string(reaction_name));
    json_set(reaction_obj, "action", json_string(action));
    json_set(reaction_obj, "reacted_to_ts", json_string(msg));
    json_set(reaction_obj, "event_ts", evts ? json_copy(evts) : json_null());
    json_set(out, "_hermes_reaction", reaction_obj);
    if (team_id && team_id[0]) json_set(out, "team", json_string(team_id));
    if (target_channel && target_channel[0]) {
        json_set(out, "channel", json_string(target_channel));
        json_set(out, "channel_type",
                 json_string(target_channel[0] == 'D' ? "im" : "channel"));
        json_set(out, "_hermes_reaction_source_channel", json_string(chan));
        if (target_thread && target_thread[0]) {
            json_set(out, "thread_ts", json_string(target_thread));
        } else {
            json_obj_del(out, "thread_ts");
            json_set(out, "_hermes_no_thread_response", json_bool(1));
        }
    }
    free(reaction_name);
    free(user_id);
    free(msg);
    free(chan);
    return out;
}

/* _build_identity_prompt */
static char *build_identity_prompt(const char *name) {
    sb_t b;
    const char *n = name ? name : "";
    sb_init(&b);
    if (!n[0]) return sb_take(&b);
    sb_adds(&b, "You are connected to this Slack workspace as the bot \"@");
    sb_adds(&b, n);
    sb_adds(&b, "\". The adapter already applied mention and channel routing; treat every "
                "delivered turn as intentionally routed to you. Your routing mention \"@");
    sb_adds(&b, n);
    sb_adds(&b, "\" may have been stripped from the visible text \xe2\x80\x94 do not reject or "
                "ignore a message solely because \"@");
    sb_adds(&b, n);
    sb_adds(&b, "\" is absent. In messages, each line is prefixed with the sender's name, and "
                "visible mentions are shown as @DisplayName; a mention of any other participant "
                "is not a mention of you, even if their name is similar.");
    return sb_take(&b);
}

json_t *w27_p7_resolve_thread_ts(const json_t *req) {
    json_t *rit = jo(req, "reply_in_thread");
    int flag = (rit && rit->type == JSON_BOOL) ? rit->bool_val : 1;
    return resolve_thread_ts(jo(req, "reply_to"), jo(req, "metadata"), flag);
}

json_t *w27_p7_reactions_enabled(const json_t *req) {
    return json_bool(reactions_flag(jo(req, "extra"), jo(req, "env")));
}

json_t *w27_p7_extra_flag(const json_t *req) {
    json_t *dv = jo(req, "default");
    int dflt = py_truthy(dv);
    return json_bool(extra_flag(jo(req, "extra"), rq_str(req, "key", ""), dflt));
}

json_t *w27_p7_parse_users_info(const json_t *req) {
    return parse_users_info(jo(req, "payload"), rq_str(req, "user_id", ""));
}

json_t *w27_p7_reaction_triggers(const json_t *req) {
    return reaction_triggers(jo(req, "raw"));
}

json_t *w27_p7_reaction_trigger_target(const json_t *req) {
    char *channel, *thread;
    json_t *arr = json_array();
    reaction_trigger_target(jo(req, "raw"), &channel, &thread);
    json_append(arr, json_string(channel));
    json_append(arr, json_string(thread));
    free(channel);
    free(thread);
    return arr;
}

json_t *w27_p7_first_file_share(const json_t *req) {
    return first_file_share(jo(req, "file_obj"), rq_str(req, "channel_id", ""));
}

json_t *w27_p7_synthetic_reaction_event(const json_t *req) {
    return synthetic_reaction_event(jo(req, "event"), rq_str(req, "action", "added"),
                                    rq_str(req, "thread_ts", ""), rq_str(req, "team_id", ""),
                                    rq_str(req, "target_channel", ""),
                                    rq_str(req, "target_thread", ""));
}

json_t *w27_p7_build_identity_prompt(const json_t *req) {
    char *out = build_identity_prompt(rq_str(req, "name", ""));
    json_t *res = json_string(out);
    free(out);
    return res;
}

/* ===================== gate p8: mrkdwn conversion + block shaping ===================== */

static char *replace_all(const char *s, const char *needle, const char *rep) {
    sb_t b;
    const char *p = s;
    size_t nlen = strlen(needle);
    sb_init(&b);
    if (!nlen) {
        sb_adds(&b, s);
        return sb_take(&b);
    }
    for (;;) {
        const char *hit = strstr(p, needle);
        if (!hit) {
            sb_adds(&b, p);
            break;
        }
        sb_addn(&b, p, (size_t)(hit - p));
        sb_adds(&b, rep);
        p = hit + nlen;
    }
    return sb_take(&b);
}

typedef struct {
    svec_t vals;
} ph_t;

static char *ph_key(size_t idx) {
    char key[32];
    snprintf(key, sizeof(key), "\x01SL%u\x01", (unsigned)idx);
    return xstrdup(key);
}

static void ph_add(ph_t *ph, sb_t *out, const char *val, size_t len) {
    size_t idx = ph->vals.n;
    char *copy = (char *)malloc(len + 1);
    char *key;
    memcpy(copy, val, len);
    copy[len] = '\0';
    sv_push(&ph->vals, copy);
    key = ph_key(idx);
    sb_adds(out, key);
    free(key);
}

static char *ph_restore(char *text, ph_t *ph) {
    size_t i = ph->vals.n;
    while (i--) {
        char *key = ph_key(i);
        char *next = replace_all(text, key, ph->vals.v[i]);
        free(key);
        free(text);
        text = next;
    }
    return text;
}

static void ph_free(ph_t *ph) {
    size_t i;
    for (i = 0; i < ph->vals.n; i++) free(ph->vals.v[i]);
    free(ph->vals.v);
    ph->vals.v = NULL;
    ph->vals.n = ph->vals.cap = 0;
}

/* <!everyone|channel|here...> -> escape the leading '<' only */
static char *fm_broadcast(const char *s) {
    static const char *WORDS[3] = {"everyone", "channel", "here"};
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (s[i] == '<' && s[i + 1] == '!') {
            size_t w;
            for (w = 0; w < 3; w++) {
                size_t n = strlen(WORDS[w]), k;
                int match = 1;
                for (k = 0; k < n; k++) {
                    char c = s[i + 2 + k];
                    if (tolower((unsigned char)c) != WORDS[w][k]) {
                        match = 0;
                        break;
                    }
                }
                if (!match) continue;
                {
                    size_t p = i + 2 + n;
                    if (s[p] == '|') {
                        p++;
                        while (s[p] && s[p] != '>' && s[p] != '\n') p++;
                    }
                    if (s[p] == '>') {
                        sb_adds(&b, "&lt;");
                        sb_addn(&b, s + i + 1, p - i);
                        i = p + 1;
                        break;
                    }
                }
            }
            if (w < 3) continue;
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

/* (```(?:[^\n]*\n)?[\s\S]*?```) */
static int fence_plain_next(const char *s, size_t from, size_t *ms, size_t *me) {
    size_t i;
    for (i = from; s[i]; i++) {
        size_t p, q;
        if (s[i] != '`' || s[i + 1] != '`' || s[i + 2] != '`') continue;
        p = i + 3;
        if (s[p] && s[p] != '\n') {
            while (s[p] && s[p] != '\n') p++;
            if (s[p] == '\n') p++;
        } else if (s[p] == '\n') {
            p++;
        }
        q = p;
        while (s[q] && !(s[q] == '`' && s[q + 1] == '`' && s[q + 2] == '`')) q++;
        if (!s[q]) continue;
        *ms = i;
        *me = q + 3;
        return 1;
    }
    return 0;
}

/* strip a line-start language tag: \A```[^\s`]+[ \t]*(
?\n) -> ```\1 */
static char *strip_fence_language(const char *block, size_t len) {
    sb_t b;
    size_t p = 3;
    size_t tag_start;
    if (len < 4) return (char *)memcpy(malloc(len + 1), block, len + 1);
    tag_start = p;
    while (p < len && block[p] != ' ' && block[p] != '\t' && block[p] != '\n' &&
           block[p] != '\r' && block[p] != '`')
        p++;
    if (p == tag_start) return (char *)memcpy(malloc(len + 1), block, len + 1);
    {
        size_t q = p;
        while (q < len && (block[q] == ' ' || block[q] == '\t')) q++;
        if (q < len && block[q] == '\r') q++;
        if (q < len && block[q] == '\n') {
            sb_init(&b);
            sb_adds(&b, "```");
            sb_addn(&b, block + q, 1);
            sb_addn(&b, block + q + 1, len - q - 1);
            return sb_take(&b);
        }
    }
    return (char *)memcpy(malloc(len + 1), block, len + 1);
}

static char *fm_protect_fences(const char *s, ph_t *ph) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    for (;;) {
        size_t ms, me;
        char *block;
        if (!fence_plain_next(s, i, &ms, &me)) {
            sb_adds(&b, s + i);
            break;
        }
        sb_addn(&b, s + i, ms - i);
        block = (char *)malloc(me - ms + 1);
        memcpy(block, s + ms, me - ms);
        block[me - ms] = '\0';
        if (ms == 0 || s[ms - 1] == '\n') {
            char *stripped = strip_fence_language(block, me - ms);
            ph_add(ph, &b, stripped, strlen(stripped));
            free(stripped);
        } else {
            ph_add(ph, &b, block, me - ms);
        }
        free(block);
        i = me;
    }
    return sb_take(&b);
}

static char *fm_protect_inline(const char *s, ph_t *ph) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (s[i] == '`') {
            size_t p = i + 1;
            while (s[p] && s[p] != '`') p++;
            if (s[p] == '`' && p > i + 1) {
                ph_add(ph, &b, s + i, p + 1 - i);
                i = p + 1;
                continue;
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

static char *fm_links(const char *s, ph_t *ph) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (s[i] == '[' && !(i > 0 && s[i - 1] == '!')) {
            size_t label = i + 1;
            size_t p = label;
            while (s[p] && s[p] != ']') p++;
            if (s[p] == ']' && p > label && s[p + 1] == '(') {
                size_t ustart = p + 2;
                size_t q = ustart;
                int ok = 0;
                for (;;) {
                    while (s[q] && s[q] != '(' && s[q] != ')') q++;
                    if (s[q] == '(') {
                        size_t r = q + 1;
                        while (s[r] && s[r] != '(' && s[r] != ')') r++;
                        if (s[r] == ')') {
                            q = r + 1;
                            continue;
                        }
                        break;
                    }
                    if (s[q] == ')') {
                        ok = 1;
                        break;
                    }
                    break;
                }
                if (ok) {
                    size_t uend = q;
                    char *url = (char *)malloc(uend - ustart + 1);
                    char *trimmed;
                    sb_t made;
                    memcpy(url, s + ustart, uend - ustart);
                    url[uend - ustart] = '\0';
                    trimmed = strip_copy(url);
                    free(url);
                    if (trimmed[0] == '<' && strlen(trimmed) >= 2 &&
                        trimmed[strlen(trimmed) - 1] == '>') {
                        size_t n = strlen(trimmed);
                        char *inner = strip_copy(trimmed);
                        memmove(inner, inner + 1, n - 2);
                        inner[n - 2] = '\0';
                        free(trimmed);
                        trimmed = inner;
                    }
                    sb_init(&made);
                    sb_addc(&made, '<');
                    sb_adds(&made, trimmed);
                    sb_addc(&made, '|');
                    sb_addn(&made, s + label, p - label);
                    sb_addc(&made, '>');
                    ph_add(ph, &b, made.p ? made.p : "", made.n);
                    sb_free(&made);
                    free(trimmed);
                    i = q + 1;
                    continue;
                }
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

static char *fm_protect_entities(const char *s, ph_t *ph) {
    static const char *SCHEMES[4] = {"http", "https", "mailto", "tel"};
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (s[i] == '<') {
            int hit = 0;
            if (s[i + 1] == '@' || s[i + 1] == '#' || s[i + 1] == '!') hit = 1;
            else {
                size_t k;
                for (k = 0; k < 4; k++) {
                    size_t n = strlen(SCHEMES[k]);
                    if (strncmp(s + i + 1, SCHEMES[k], n) == 0 && s[i + 1 + n] == ':') {
                        hit = 1;
                        break;
                    }
                }
            }
            if (hit) {
                size_t p = i + 1;
                while (s[p] && s[p] != '>' && s[p] != '\n') p++;
                if (s[p] == '>' && p > i + 1) {
                    ph_add(ph, &b, s + i, p + 1 - i);
                    i = p + 1;
                    continue;
                }
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

static char *fm_protect_quotes(const char *s, ph_t *ph) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if ((i == 0 || s[i - 1] == '\n') && s[i] == '>') {
            size_t p = i;
            while (s[p] == '>') p++;
            if (s[p] && (s[p] == ' ' || s[p] == '\t')) {
                ph_add(ph, &b, s + i, p + 1 - i);
                i = p + 1;
                continue;
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

static char *fm_escape(const char *s) {
    char *un = unescape_entities(s);
    sb_t b;
    char *p;
    sb_init(&b);
    for (p = un; *p; p++) {
        if (*p == '&') sb_adds(&b, "&amp;");
        else if (*p == '<') sb_adds(&b, "&lt;");
        else if (*p == '>') sb_adds(&b, "&gt;");
        else sb_addc(&b, *p);
    }
    free(un);
    return sb_take(&b);
}

/* **x** -> x inside a header's inner text */
static char *strip_double_bold(const char *s) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (s[i] == '*' && s[i + 1] == '*') {
            size_t p = i + 2;
            while (s[p] && s[p] != '\n' && !(s[p] == '*' && s[p + 1] == '*')) p++;
            if (s[p] == '*' && s[p + 1] == '*' && p > i + 2) {
                sb_addn(&b, s + i + 2, p - i - 2);
                i = p + 2;
                continue;
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

static char *fm_headers(const char *s, ph_t *ph) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (i == 0 || s[i - 1] == '\n') {
            size_t h = i;
            while (s[h] == '#' && h - i < 7) h++;
            if (h > i && h - i <= 6 && (s[h] == ' ' || s[h] == '\t')) {
                size_t t = h;
                while (s[t] == ' ' || s[t] == '\t') t++;
                {
                    size_t e = t;
                    while (s[e] && s[e] != '\n') e++;
                    if (e > t) {
                        char *raw = (char *)malloc(e - t + 1);
                        char *inner;
                        char *stripped;
                        sb_t made;
                        memcpy(raw, s + t, e - t);
                        raw[e - t] = '\0';
                        stripped = strip_copy(raw);
                        inner = strip_double_bold(stripped);
                        sb_init(&made);
                        sb_addc(&made, '*');
                        sb_adds(&made, inner);
                        sb_addc(&made, '*');
                        ph_add(ph, &b, made.p ? made.p : "", made.n);
                        sb_free(&made);
                        free(inner);
                        free(stripped);
                        free(raw);
                        i = e;
                        continue;
                    }
                }
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

/* ***x*** -> *_x_* */
static char *fm_triple_bold(const char *s, ph_t *ph) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (strncmp(s + i, "***", 3) == 0) {
            size_t p = i + 3;
            while (s[p] && s[p] != '\n' && strncmp(s + p, "***", 3) != 0) p++;
            if (strncmp(s + p, "***", 3) == 0 && p > i + 3) {
                sb_t made;
                sb_init(&made);
                sb_adds(&made, "*_");
                sb_addn(&made, s + i + 3, p - i - 3);
                sb_adds(&made, "_*");
                ph_add(ph, &b, made.p ? made.p : "", made.n);
                sb_free(&made);
                i = p + 3;
                continue;
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

static int last_is_wordish(const char *inner, size_t n) {
    size_t adv;
    unsigned cp;
    if (!n) return 0;
    /* last code point */
    {
        size_t i = 0, last = n;
        while (i < n) {
            size_t a;
            cp_at(inner, i, &a);
            last = i;
            i += a;
        }
        cp = cp_at(inner, last, &adv);
    }
    if (cp == '_') return 0;
    if (cp < 128) return isalnum((int)cp) ? 1 : 0;
    return 0;
}

/* **x** -> *x[ZWSP]* */
static char *fm_bold(const char *s, ph_t *ph) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (s[i] == '*' && s[i + 1] == '*') {
            size_t p = i + 2;
            while (s[p] && s[p] != '\n' && !(s[p] == '*' && s[p + 1] == '*')) p++;
            if (s[p] == '*' && s[p + 1] == '*' && p > i + 2) {
                size_t n = p - i - 2;
                sb_t made;
                sb_init(&made);
                sb_addc(&made, '*');
                sb_addn(&made, s + i + 2, n);
                if (!last_is_wordish(s + i + 2, n)) sb_adds(&made, "\xe2\x80\x8b");
                sb_addc(&made, '*');
                ph_add(ph, &b, made.p ? made.p : "", made.n);
                sb_free(&made);
                i = p + 2;
                continue;
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

/* *x* -> _x_ when non-space touches both delimiters */
static char *fm_italic(const char *s, ph_t *ph) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (s[i] == '*' && !(i > 0 && s[i - 1] == '*')) {
            size_t p = i + 1;
            int ok = s[p] && s[p] != ' ' && s[p] != '\t' && s[p] != '\n' && s[p] != '\r' &&
                     s[p] != '*';
            if (ok) {
                while (s[p] && s[p] != '\n' && s[p] != '*') p++;
                if (s[p] == '*' && p > i + 1 && s[p + 1] != '*') {
                    size_t n = p - i - 1;
                    int good = 1;
                    if (n > 1) {
                        char lastc = s[p - 1];
                        if (lastc == ' ' || lastc == '\t' || lastc == '\n' || lastc == '\r')
                            good = 0;
                    }
                    if (good) {
                        sb_t made;
                        sb_init(&made);
                        sb_addc(&made, '_');
                        sb_addn(&made, s + i + 1, n);
                        sb_addc(&made, '_');
                        ph_add(ph, &b, made.p ? made.p : "", made.n);
                        sb_free(&made);
                        i = p + 1;
                        continue;
                    }
                }
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

/* ~~x~~ -> ~x~ */
static char *fm_strike(const char *s, ph_t *ph) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        if (s[i] == '~' && s[i + 1] == '~') {
            size_t p = i + 2;
            while (s[p] && s[p] != '\n' && !(s[p] == '~' && s[p + 1] == '~')) p++;
            if (s[p] == '~' && s[p + 1] == '~' && p > i + 2) {
                sb_t made;
                sb_init(&made);
                sb_addc(&made, '~');
                sb_addn(&made, s + i + 2, p - i - 2);
                sb_addc(&made, '~');
                ph_add(ph, &b, made.p ? made.p : "", made.n);
                sb_free(&made);
                i = p + 2;
                continue;
            }
        }
        sb_addc(&b, s[i++]);
    }
    return sb_take(&b);
}

/* SlackAdapter.format_message */
static char *format_message(const char *content) {
    char *t;
    ph_t ph;
    if (!content || !content[0]) return xstrdup(content ? content : "");
    sv_init(&ph.vals);
    t = wrap_markdown_tables(content);
    {
        char *n = fm_broadcast(t);
        free(t);
        t = n;
    }
    {
        char *n = fm_protect_fences(t, &ph);
        free(t);
        t = n;
    }
    {
        char *n = fm_protect_inline(t, &ph);
        free(t);
        t = n;
    }
    {
        char *n = fm_links(t, &ph);
        free(t);
        t = n;
    }
    {
        char *n = fm_protect_entities(t, &ph);
        free(t);
        t = n;
    }
    {
        char *n = fm_protect_quotes(t, &ph);
        free(t);
        t = n;
    }
    {
        char *n = fm_escape(t);
        free(t);
        t = n;
    }
    {
        char *n = fm_headers(t, &ph);
        free(t);
        t = n;
    }
    {
        char *n = fm_triple_bold(t, &ph);
        free(t);
        t = n;
    }
    {
        char *n = fm_bold(t, &ph);
        free(t);
        t = n;
    }
    {
        char *n = fm_italic(t, &ph);
        free(t);
        t = n;
    }
    {
        char *n = fm_strike(t, &ph);
        free(t);
        t = n;
    }
    t = ph_restore(t, &ph);
    ph_free(&ph);
    return t;
}

/* _append_block_text */
static char *append_block_text(const char *text, const json_t *blocks, const char *bot_uid) {
    char *blocks_text = additional_text_blocks(blocks, text ? text : "", bot_uid);
    char *stripped = strip_copy(blocks_text);
    char *cur = xstrdup(text ? text : "");
    if (stripped[0]) {
        sb_t b;
        char *s0 = strip_copy(cur);
        char *s1;
        sb_init(&b);
        sb_adds(&b, s0);
        sb_addc(&b, '\n');
        sb_adds(&b, stripped);
        s1 = strip_copy(b.p ? b.p : "");
        sb_free(&b);
        free(s0);
        free(cur);
        cur = s1;
    }
    {
        char *payload = serialize_blocks(blocks, 6000);
        if (payload[0]) {
            sb_t b;
            char *s0 = strip_copy(cur);
            char *s1;
            sb_init(&b);
            sb_adds(&b, s0);
            sb_adds(&b, "\n\n");
            sb_adds(&b, payload);
            s1 = strip_copy(b.p ? b.p : "");
            sb_free(&b);
            free(s0);
            free(cur);
            cur = s1;
        }
        free(payload);
    }
    free(blocks_text);
    free(stripped);
    return cur;
}

/* _normalize_changed_message */
static json_t *normalize_changed_message(const json_t *event, const json_t *processed) {
    json_t *updated = jo(event, "message");
    char *original;
    json_t *out;
    size_t i;
    if (!is_obj(updated)) return json_null();
    {
        json_t *tv = jo(updated, "ts");
        original = py_truthy(tv) ? py_str(tv) : xstrdup("");
    }
    if (original[0] && list_has(processed, original)) {
        free(original);
        return json_null();
    }
    {
        json_t *edited = jo(updated, "edited");
        char *edited_ts = "";
        char *outer_ts;
        char *changed;
        json_t *evts;
        char *ev_str = NULL;
        if (is_obj(edited)) {
            json_t *t = jo(edited, "ts");
            ev_str = py_truthy(t) ? py_str(t) : xstrdup("");
            edited_ts = ev_str;
        }
        evts = jo(event, "ts");
        outer_ts = py_truthy(evts) ? py_str(evts) : xstrdup("");
        if (py_truthy(jo(event, "event_ts"))) {
            changed = py_str(jo(event, "event_ts"));
        } else if (edited_ts[0]) {
            changed = xstrdup(edited_ts);
        } else if (outer_ts[0] && strcmp(outer_ts, original) != 0) {
            changed = xstrdup(outer_ts);
        } else if (original[0]) {
            sb_t b;
            sb_init(&b);
            sb_adds(&b, original);
            sb_adds(&b, ":changed");
            changed = sb_take(&b);
        } else {
            changed = xstrdup("");
        }
        free(outer_ts);
        if (ev_str) free(ev_str);
        out = json_copy(updated);
        {
            static const char *KEYS[4] = {"channel", "channel_type", "team", "team_id"};
            for (i = 0; i < 4; i++) {
                if (!py_truthy(jo(out, KEYS[i])) && py_truthy(jo(event, KEYS[i])))
                    json_set(out, KEYS[i], json_copy(jo(event, KEYS[i])));
            }
        }
        if (changed[0]) json_set(out, "_slack_changed_event_ts", json_string(changed));
        free(changed);
    }
    free(original);
    return out;
}

/* _slack_file_marker (adapter_forwarded.slack_file_marker) */
static char *slack_file_marker(const json_t *f) {
    json_t *nv = jo(f, "name");
    json_t *tv = jo(f, "title");
    json_t *iv = jo(f, "id");
    char *name;
    sb_t cleaned;
    const char *mime;
    sb_t out;
    if (py_truthy(nv)) name = py_str(nv);
    else if (py_truthy(tv)) name = py_str(tv);
    else if (py_truthy(iv)) name = py_str(iv);
    else name = xstrdup("file");
    sb_init(&cleaned);
    {
        size_t i;
        for (i = 0; name[i]; i++) {
            if (name[i] == '\r' || name[i] == '\n' || name[i] == '[' || name[i] == ']') {
                sb_addc(&cleaned, ' ');
            } else {
                sb_addc(&cleaned, name[i]);
            }
        }
    }
    {
        char *st = strip_copy(cleaned.p ? cleaned.p : "");
        free(name);
        name = st[0] ? st : (free(st), xstrdup("file"));
    }
    sb_free(&cleaned);
    mime = rq_str(f, "mimetype", "");
    if (!is_str(jo(f, "mimetype"))) mime = "";
    sb_init(&out);
    if (strncmp(mime, "image/", 6) == 0) {
        sb_adds(&out, "[image: ");
        sb_adds(&out, name);
        sb_addc(&out, ']');
    } else if (strncmp(mime, "video/", 6) == 0) {
        sb_adds(&out, "[video: ");
        sb_adds(&out, name);
        sb_addc(&out, ']');
    } else if (strncmp(mime, "audio/", 6) == 0) {
        sb_adds(&out, "[audio: ");
        sb_adds(&out, name);
        sb_addc(&out, ']');
    } else if (mime[0]) {
        sb_adds(&out, "[file: ");
        sb_adds(&out, name);
        sb_adds(&out, " (");
        sb_adds(&out, mime);
        sb_adds(&out, ")]");
    } else {
        sb_adds(&out, "[file: ");
        sb_adds(&out, name);
        sb_addc(&out, ']');
    }
    free(name);
    return sb_take(&out);
}

/* legacy attachments extraction (_extract_text_from_slack_attachments without
 * the shared forward sections, which only apply to is_share/linked unfurls) */
static char *attachments_text(const json_t *attachments, const json_t *own_ids,
                              const char *seen_text) {
    svec_t lines;
    size_t i;
    sv_init(&lines);
    (void)own_ids;
    (void)seen_text;
    for (i = 0; is_arr(attachments) && i < json_len(attachments); i++) {
        json_t *att = json_get(attachments, i);
        svec_t got;
        size_t k;
        if (!is_obj(att)) continue;
        if (py_truthy(jo(att, "is_msg_unfurl")) || py_truthy(jo(att, "is_share"))) continue;
        sv_init(&got);
        {
            static const char *KEYS[3] = {"pretext", "title", "text"};
            for (k = 0; k < 3; k++) {
                if (py_truthy(jo(att, KEYS[k]))) {
                    char *s = py_str(jo(att, KEYS[k]));
                    sv_push(&got, s);
                }
            }
        }
        {
            json_t *fields = jo(att, "fields");
            size_t fi;
            for (fi = 0; is_arr(fields) && fi < json_len(fields); fi++) {
                json_t *field = json_get(fields, fi);
                static const char *FKEYS[2] = {"title", "value"};
                if (!is_obj(field)) continue;
                for (k = 0; k < 2; k++) {
                    if (py_truthy(jo(field, FKEYS[k]))) {
                        char *s = py_str(jo(field, FKEYS[k]));
                        sv_push(&got, s);
                    }
                }
            }
        }
        if (py_truthy(jo(att, "blocks"))) {
            char *block_text = extract_text_blocks(jo(att, "blocks"));
            if (block_text[0]) sv_push(&got, xstrdup(block_text));
            free(block_text);
        }
        if (got.n == 0 && py_truthy(jo(att, "fallback"))) {
            char *s = py_str(jo(att, "fallback"));
            sv_push(&got, s);
        }
        {
            size_t gi;
            for (gi = 0; gi < got.n; gi++) {
                if (got.v[gi][0]) sv_push(&lines, xstrdup(got.v[gi]));
            }
            for (gi = 0; gi < got.n; gi++) free(got.v[gi]);
            free(got.v);
        }
    }
    {
        char *joined = sv_join(&lines, "\n");
        char *out = strip_copy(joined);
        free(joined);
        return out;
    }
}

/* _append_link_unfurls */
static char *append_link_unfurls(const char *text, const json_t *attachments) {
    svec_t parts;
    char *cur = xstrdup(text ? text : "");
    long budget = 6000;
    size_t i;
    long marker_max;
    {
        char *m = elision_marker(999999999999LL, 999999999999LL);
        marker_max = (long)cp_count(m);
        free(m);
    }
    sv_init(&parts);
    for (i = 0; is_arr(attachments) && i < json_len(attachments); i++) {
        json_t *att = json_get(attachments, i);
        char *title, *url, *body, *fallback, *footer;
        const char *att_url_v;
        if (!is_obj(att)) continue;
        if (py_truthy(jo(att, "is_msg_unfurl")) || py_truthy(jo(att, "is_share"))) continue;
        {
            json_t *tv = jo(att, "title");
            json_t *tl = jo(att, "title_link");
            json_t *fu = jo(att, "from_url");
            json_t *tx = jo(att, "text");
            json_t *ft = jo(att, "footer");
            json_t *fb = jo(att, "fallback");
            title = py_truthy(tv) ? py_str(tv) : xstrdup("");
            if (py_truthy(tl)) url = py_str(tl);
            else if (py_truthy(fu)) url = py_str(fu);
            else url = xstrdup("");
            body = py_truthy(tx) ? py_str(tx) : (py_truthy(fb) ? py_str(fb) : xstrdup(""));
            footer = py_truthy(ft) ? py_str(ft) : xstrdup("");
            fallback = py_truthy(fb) ? py_str(fb) : xstrdup("");
        }
        att_url_v = url;
        (void)att_url_v;
        {
            char *trimmed = strip_copy(body);
            free(body);
            body = trimmed;
        }
        if ((long)cp_count(body) > 500) {
            char *head = cp_prefix(body, 497);
            sb_t b;
            sb_init(&b);
            sb_adds(&b, head);
            sb_adds(&b, "...");
            free(head);
            free(body);
            body = sb_take(&b);
        }
        {
            char *nested = xstrdup("");
            if (budget > 0) {
                char *n = extract_text_blocks(jo(att, "blocks"));
                free(nested);
                nested = n;
                if ((long)cp_count(nested) > budget && budget <= marker_max) {
                    free(nested);
                    nested = xstrdup("");
                } else {
                    char *e = elide_text(nested, budget);
                    free(nested);
                    nested = e;
                }
            }
            if (nested[0] && !str_contains(body, nested)) {
                sb_t b;
                budget -= (long)cp_count(nested);
                sb_init(&b);
                if (body[0]) {
                    sb_adds(&b, body);
                    sb_addc(&b, '\n');
                    sb_adds(&b, nested);
                } else {
                    sb_adds(&b, nested);
                }
                {
                    char *joined = sb_take(&b);
                    char *t = strip_copy(joined);
                    free(joined);
                    free(body);
                    body = t;
                }
            }
            free(nested);
        }
        {
            sb_t section;
            int has_header = 0;
            char *section_str;
            sb_init(&section);
            if (title[0] && url[0]) {
                sb_adds(&section, "\xf0\x9f\x93\x8e [");
                sb_adds(&section, title);
                sb_adds(&section, "](");
                sb_adds(&section, url);
                sb_addc(&section, ')');
                has_header = 1;
            } else if (title[0] || url[0]) {
                sb_adds(&section, "\xf0\x9f\x93\x8e ");
                sb_adds(&section, title[0] ? title : url);
                has_header = 1;
            }
            if (has_header) {
                if (body[0]) {
                    sb_adds(&section, "\n   ");
                    sb_adds(&section, body);
                }
            } else if (body[0]) {
                sb_adds(&section, "\xf0\x9f\x93\x8e ");
                sb_adds(&section, body);
            } else {
                sb_free(&section);
                free(title);
                free(url);
                free(body);
                free(footer);
                free(fallback);
                continue;
            }
            section_str = sb_take(&section);
            if (str_contains(cur, section_str)) {
                free(section_str);
                free(title);
                free(url);
                free(body);
                free(footer);
                free(fallback);
                continue;
            }
            if (footer[0]) {
                sb_t f;
                sb_init(&f);
                sb_adds(&f, section_str);
                sb_adds(&f, "\n   _");
                sb_adds(&f, footer);
                sb_adds(&f, "_");
                free(section_str);
                section_str = sb_take(&f);
            }
            sv_push(&parts, section_str);
        }
        free(title);
        free(url);
        free(body);
        free(footer);
        free(fallback);
    }
    if (parts.n) {
        sb_t b;
        char *s0 = strip_copy(cur);
        char *joined = sv_join(&parts, "\n\n");
        char *all;
        sb_init(&b);
        sb_adds(&b, s0);
        sb_adds(&b, "\n\n");
        sb_adds(&b, joined);
        free(s0);
        free(joined);
        all = sb_take(&b);
        free(cur);
        cur = strip_copy(all);
        free(all);
    } else {
        free(parts.v);
    }
    return cur;
}

/* _render_message_text */
static int unseen_piece(const char *piece, const char *base, svec_t *extras) {
    size_t i;
    if (str_contains(base, piece)) return 0;
    for (i = 0; i < extras->n; i++) {
        if (str_contains(extras->v[i], piece)) return 0;
    }
    return 1;
}

static char *render_message_text(const json_t *msg, const char *bot_uid, const json_t *own_ids) {
    json_t *tv = jo(msg, "text");
    char *raw = py_truthy(tv) ? py_str(tv) : xstrdup("");
    char *msg_text = strip_copy(raw);
    json_t *blocks = jo(msg, "blocks");
    svec_t extras;
    free(raw);
    if (bot_uid && bot_uid[0]) {
        sb_t b;
        sb_init(&b);
        sb_adds(&b, "<@");
        sb_adds(&b, bot_uid);
        sb_addc(&b, '>');
        {
            char *repl = replace_all(msg_text, b.p ? b.p : "", "");
            free(msg_text);
            msg_text = strip_copy(repl);
            free(repl);
        }
        sb_free(&b);
    }
    sv_init(&extras);
    if (py_truthy(blocks)) {
        char *rich = additional_text_blocks(blocks, msg_text, bot_uid ? bot_uid : "");
        char *trimmed = strip_copy(rich);
        if (trimmed[0]) sv_push(&extras, xstrdup(trimmed));
        free(trimmed);
        free(rich);
        {
            size_t i;
            for (i = 0; i < json_len(blocks); i++) {
                json_t *block = json_get(blocks, i);
                json_t *bv = jo(block, "type");
                const char *btype = is_str(bv) ? bv->str_val : "";
                json_t *text_obj;
                char *sec;
                if (strcmp(btype, "section") != 0 && strcmp(btype, "header") != 0 &&
                    strcmp(btype, "context") != 0)
                    continue;
                text_obj = jo(block, "text");
                if (!is_obj(text_obj)) continue;
                {
                    json_t *inner = jo(text_obj, "text");
                    sec = py_truthy(inner) ? strip_copy(py_str(inner)) : xstrdup("");
                }
                if (sec[0] && unseen_piece(sec, msg_text, &extras)) sv_push(&extras, sec);
                else free(sec);
            }
        }
    }
    {
        json_t *attachments = jo(msg, "attachments");
        if (py_truthy(attachments)) {
            const json_t *ids = own_ids;
            json_t *derived = NULL;
            char *att_text;
            if (!py_truthy(ids) && bot_uid && bot_uid[0]) {
                derived = json_array();
                json_append(derived, json_string(bot_uid));
                ids = derived;
            }
            att_text = attachments_text(attachments, ids, msg_text);
            if (att_text[0] && unseen_piece(att_text, msg_text, &extras))
                sv_push(&extras, xstrdup(att_text));
            free(att_text);
            if (derived) json_free(derived);
        }
    }
    if (py_truthy(blocks)) {
        char *msg_text_raw = unescape_entities(msg_text);
        json_t *urls = json_array();
        size_t i;
        svec_t found;
        sv_init(&found);
        url_walk(blocks, &found, 0);
        for (i = 0; i < found.n; i++) {
            if (unseen_piece(found.v[i], msg_text_raw, &extras))
                json_append(urls, json_string(found.v[i]));
        }
        if (json_len(urls) > 0) {
            sb_t b;
            size_t k;
            sb_init(&b);
            sb_adds(&b, "URLs: ");
            for (k = 0; k < json_len(urls); k++) {
                if (k) sb_adds(&b, ", ");
                sb_adds(&b, json_get(urls, k)->str_val);
            }
            sv_push(&extras, sb_take(&b));
        }
        for (i = 0; i < found.n; i++) free(found.v[i]);
        free(found.v);
        json_free(urls);
        free(msg_text_raw);
    }
    {
        json_t *files = jo(msg, "files");
        svec_t markers;
        size_t i;
        sv_init(&markers);
        if (is_arr(files)) {
            size_t i;
            for (i = 0; i < json_len(files); i++) {
                json_t *f = json_get(files, i);
                if (is_obj(f)) sv_push(&markers, slack_file_marker(f));
            }
        }
        if (markers.n) {
            sb_t b;
            size_t i;
            sb_init(&b);
            for (i = 0; i < markers.n; i++) {
                if (i) sb_addc(&b, ' ');
                sb_adds(&b, markers.v[i]);
            }
            sv_push(&extras, sb_take(&b));
        }
        for (i = 0; i < markers.n; i++) free(markers.v[i]);
        free(markers.v);
    }
    if (extras.n) {
        char *addendum = sv_join(&extras, "\n");
        if (msg_text[0]) {
            sb_t b;
            sb_init(&b);
            sb_adds(&b, msg_text);
            sb_addc(&b, '\n');
            sb_adds(&b, addendum);
            {
                char *joined = sb_take(&b);
                char *t = strip_copy(joined);
                free(joined);
                free(msg_text);
                msg_text = t;
            }
        } else {
            free(msg_text);
            msg_text = strip_copy(addendum);
        }
        free(addendum);
    } else {
        free(extras.v);
    }
    return msg_text;
}

static const char *media_type_name(const json_t *media_types) {
    size_t i;
    if (!py_truthy(media_types)) return "TEXT";
    {
        int has_photo = 0, has_video = 0, has_voice = 0;
        for (i = 0; i < json_len(media_types); i++) {
            json_t *m = json_get(media_types, i);
            if (!is_str(m)) continue;
            if (strncmp(m->str_val, "image/", 6) == 0) has_photo = 1;
            if (strncmp(m->str_val, "video/", 6) == 0) has_video = 1;
            if (strncmp(m->str_val, "audio/", 6) == 0) has_voice = 1;
        }
        if (has_photo) return "PHOTO";
        if (has_video) return "VIDEO";
        if (has_voice) return "VOICE";
    }
    return "DOCUMENT";
}

json_t *w27_p8_format_message(const json_t *req) {
    char *out = format_message(rq_str(req, "content", ""));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p8_append_block_text(const json_t *req) {
    char *out = append_block_text(rq_str(req, "text", ""), jo(req, "blocks"),
                                  rq_str(req, "bot_uid", ""));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p8_normalize_changed_message(const json_t *req) {
    return normalize_changed_message(jo(req, "event"), jo(req, "processed"));
}

json_t *w27_p8_append_link_unfurls(const json_t *req) {
    char *out = append_link_unfurls(rq_str(req, "text", ""), jo(req, "attachments"));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p8_media_message_type(const json_t *req) {
    return json_string(media_type_name(jo(req, "media_types")));
}

json_t *w27_p8_session_thread_ts(const json_t *req) {
    json_t *event = jo(req, "event");
    json_t *meta = jo(req, "assistant_meta");
    const char *ts = rq_str(req, "ts", "");
    json_t *dm = jo(req, "is_dm");
    int is_dm = py_truthy(dm);
    json_t *dmt = jo(req, "dm_top_level");
    int dm_top_level = (dmt && dmt->type == JSON_BOOL) ? dmt->bool_val : 1;
    json_t *rit = jo(req, "reply_in_thread");
    int reply_in_thread = (rit && rit->type == JSON_BOOL) ? rit->bool_val : 1;
    json_t *ev_thread = jo(event, "thread_ts");
    if (is_dm) {
        json_t *thread = py_truthy(ev_thread) ? ev_thread : jo(meta, "thread_ts");
        if (!py_truthy(thread) && dm_top_level) return json_string(ts);
        return thread ? json_copy(thread) : json_null();
    }
    if (py_truthy(jo(event, "_hermes_no_thread_response"))) {
        return py_truthy(ev_thread) ? json_copy(ev_thread) : json_null();
    }
    if (py_truthy(ev_thread) && strcmp(is_str(ev_thread) ? ev_thread->str_val : "", ts) != 0)
        return json_copy(ev_thread);
    if (reply_in_thread) return json_string(ts);
    return json_null();
}

json_t *w27_p8_button(const json_t *req) {
    json_t *text_obj = json_object();
    json_t *btn = json_object();
    json_t *em = jo(req, "emoji");
    const char *style = rq_str(req, "style", "");
    json_set(text_obj, "type", json_string("plain_text"));
    json_set(text_obj, "text", json_string(rq_str(req, "text", "")));
    if (py_truthy(em)) json_set(text_obj, "emoji", json_bool(1));
    json_set(btn, "type", json_string("button"));
    json_set(btn, "text", text_obj);
    if (style[0]) json_set(btn, "style", json_string(style));
    json_set(btn, "action_id", json_string(rq_str(req, "action_id", "")));
    json_set(btn, "value", json_string(rq_str(req, "value", "")));
    return btn;
}

json_t *w27_p8_interaction_fields(const json_t *req) {
    json_t *body = jo(req, "body");
    json_t *action = jo(req, "action");
    json_t *message = jo(body, "message");
    json_t *channel = jo(body, "channel");
    json_t *user = jo(body, "user");
    json_t *arr = json_array();
    json_t *mv;
    if (!is_obj(message)) message = json_object();
    else message = json_copy(message);
    {
        json_t *av = jo(action, "action_id");
        json_append(arr, json_string(is_str(av) ? av->str_val : ""));
    }
    {
        json_t *vv = jo(action, "value");
        json_append(arr, json_string(is_str(vv) ? vv->str_val : ""));
    }
    json_append(arr, message);
    {
        json_t *tsv = jo(message, "ts");
        json_append(arr, json_string(is_str(tsv) ? tsv->str_val : ""));
    }
    {
        json_t *cv = jo(channel, "id");
        json_append(arr, json_string(is_str(cv) ? cv->str_val : ""));
    }
    mv = jo(user, "name");
    json_append(arr, mv && is_str(mv) ? json_string(mv->str_val) : json_string("unknown"));
    {
        json_t *uv = jo(user, "id");
        json_append(arr, json_string(is_str(uv) ? uv->str_val : ""));
    }
    return arr;
}

json_t *w27_p8_section_text(const json_t *req) {
    json_t *message = jo(req, "message");
    json_t *blocks = jo(message, "blocks");
    json_t *limitv = jo(req, "limit");
    char *original = xstrdup("");
    size_t i;
    for (i = 0; is_arr(blocks) && i < json_len(blocks); i++) {
        json_t *block = json_get(blocks, i);
        json_t *bv = jo(block, "type");
        if (is_str(bv) && strcmp(bv->str_val, "section") == 0) {
            json_t *text_obj = jo(block, "text");
            json_t *inner = is_obj(text_obj) ? jo(text_obj, "text") : NULL;
            free(original);
            original = py_truthy(inner) ? py_str(inner) : xstrdup("");
            break;
        }
    }
    if (limitv && limitv->type != JSON_NULL) {
        char *cut = cp_prefix(original, (size_t)limitv->num_val);
        json_t *res = json_string(cut);
        free(cut);
        free(original);
        return res;
    }
    {
        json_t *res = json_string(original);
        free(original);
        return res;
    }
}

json_t *w27_p8_thread_cache_key(const json_t *req) {
    sb_t b;
    json_t *res;
    sb_init(&b);
    sb_adds(&b, rq_str(req, "channel_id", ""));
    sb_addc(&b, ':');
    sb_adds(&b, rq_str(req, "thread_ts", ""));
    sb_addc(&b, ':');
    sb_adds(&b, rq_str(req, "team_id", ""));
    res = json_string(b.p ? b.p : "");
    sb_free(&b);
    return res;
}

json_t *w27_p8_thread_root_message(const json_t *req) {
    json_t *messages = jo(req, "messages");
    const char *thread_ts = rq_str(req, "thread_ts", "");
    size_t i;
    for (i = 0; is_arr(messages) && i < json_len(messages); i++) {
        json_t *m = json_get(messages, i);
        json_t *ts = is_obj(m) ? jo(m, "ts") : NULL;
        const char *s = is_str(ts) ? ts->str_val : "";
        if (strcmp(s, thread_ts) == 0) return json_copy(m);
    }
    return json_null();
}

json_t *w27_p8_render_message_text(const json_t *req) {
    char *out = render_message_text(jo(req, "msg"), rq_str(req, "bot_uid", ""),
                                    jo(req, "own_ids"));
    json_t *res = json_string(out);
    free(out);
    return res;
}

/* ===================== gate p9: workspace resolution + assistant meta ===================== */

static int sv_has(svec_t *v, const char *s) {
    size_t i;
    for (i = 0; i < v->n; i++) {
        if (strcmp(v->v[i], s) == 0) return 1;
    }
    return 0;
}

static void push_str_value(svec_t *out, const json_t *v) {
    sv_push(out, py_str(v));
}

/* _event_team_id */
static char *event_team_id(const json_t *event, const json_t *body, const json_t *installs,
                           int installs_present) {
    svec_t cand;
    size_t i;
    char *out = NULL;
    sv_init(&cand);
    if (is_obj(body)) {
        json_t *auths = jo(body, "authorizations");
        if (is_arr(auths)) {
            for (i = 0; i < json_len(auths); i++) {
                json_t *a = json_get(auths, i);
                json_t *tid = is_obj(a) ? jo(a, "team_id") : NULL;
                if (py_truthy(tid)) {
                    push_str_value(&cand, tid);
                    break;
                }
            }
        }
        {
            json_t *env = jo(body, "team_id");
            if (is_str(env) && env->str_val[0]) {
                push_str_value(&cand, env);
            } else if (is_obj(env) && py_truthy(jo(env, "id"))) {
                push_str_value(&cand, jo(env, "id"));
            }
        }
    }
    {
        const json_t *payloads[2];
        payloads[0] = event;
        payloads[1] = is_obj(body) ? body : NULL;
        for (i = 0; i < 2; i++) {
            const json_t *payload = payloads[i];
            const json_t *team;
            if (!is_obj(payload)) continue;
            team = jo(payload, "team_id");
            if (!py_truthy(team)) team = jo(payload, "team");
            if (is_str(team) && team->str_val[0]) {
                push_str_value(&cand, team);
            } else if (is_obj(team) && py_truthy(jo(team, "id"))) {
                push_str_value(&cand, jo(team, "id"));
            }
        }
    }
    if (installs_present) {
        size_t ninst = is_arr(installs) ? json_len(installs) : 0;
        if (ninst > 0) {
            for (i = 0; i < cand.n; i++) {
                size_t k;
                for (k = 0; k < ninst; k++) {
                    json_t *iv = json_get(installs, k);
                    if (is_str(iv) && strcmp(iv->str_val, cand.v[i]) == 0) {
                        out = xstrdup(cand.v[i]);
                        goto done;
                    }
                }
            }
            if (ninst == 1) {
                json_t *iv = json_get(installs, 0);
                out = xstrdup(is_str(iv) ? iv->str_val : "");
            } else {
                out = xstrdup("");
            }
            goto done;
        }
    }
    out = xstrdup(cand.n ? cand.v[0] : "");
done:
    for (i = 0; i < cand.n; i++) free(cand.v[i]);
    free(cand.v);
    return out;
}

static int installs_has(const json_t *installs, const char *team) {
    size_t i;
    for (i = 0; is_arr(installs) && i < json_len(installs); i++) {
        json_t *v = json_get(installs, i);
        if (is_str(v) && strcmp(v->str_val, team) == 0) return 1;
    }
    return 0;
}

/* _context_channel_id */
static char *context_channel_id(const json_t *context) {
    json_t *cv;
    if (!is_obj(context)) return xstrdup("");
    cv = jo(context, "channel_id");
    if (py_truthy(cv)) return py_str(cv);
    {
        json_t *entities = jo(context, "entities");
        size_t i;
        for (i = 0; is_arr(entities) && i < json_len(entities); i++) {
            json_t *entity = json_get(entities, i);
            json_t *value;
            if (!is_obj(entity)) continue;
            value = jo(entity, "value");
            if (is_obj(value) && py_truthy(jo(value, "channel_id")))
                return py_str(jo(value, "channel_id"));
            if (is_str(value)) {
                json_t *tv = jo(entity, "type");
                char *t = py_truthy(tv) ? py_str(tv) : xstrdup("");
                const char *suffix = "channel_id";
                size_t tn = strlen(t), sn = strlen(suffix);
                int ends = tn >= sn && strcmp(t + tn - sn, suffix) == 0;
                free(t);
                if (ends) return xstrdup(value->str_val);
            }
        }
    }
    return xstrdup("");
}

static json_t *str_node_or_empty(const json_t *v) {
    if (!py_truthy(v)) return json_string("");
    {
        char *s = py_str(v);
        json_t *res = json_string(s);
        free(s);
        return res;
    }
}

/* _extract_assistant_thread_metadata */
static json_t *extract_assistant_meta(const json_t *event, const json_t *body,
                                      const json_t *installs, int installs_present) {
    json_t *at = jo(event, "assistant_thread");
    json_t *context = NULL;
    json_t *out = json_object();
    json_t *channel_id, *thread_ts, *user_id;
    char *team;
    if (is_obj(at)) at = json_copy(at);
    else at = json_object();
    if (py_truthy(jo(at, "context"))) context = jo(at, "context");
    if (!py_truthy(context)) context = jo(event, "app_context");
    if (!py_truthy(context)) context = jo(event, "context");
    if (!is_obj(context)) context = json_object();
    else context = json_copy(context);
    {
        json_t *v = jo(at, "channel_id");
        if (!py_truthy(v)) v = jo(event, "channel");
        if (!py_truthy(v)) v = jo(context, "channel_id");
        channel_id = v ? json_copy(v) : json_null();
    }
    {
        json_t *v = jo(at, "thread_ts");
        if (!py_truthy(v)) v = jo(event, "thread_ts");
        if (!py_truthy(v)) v = jo(event, "message_ts");
        thread_ts = v ? json_copy(v) : json_null();
    }
    {
        json_t *v = jo(at, "user_id");
        if (!py_truthy(v)) v = jo(event, "user");
        if (!py_truthy(v)) v = jo(context, "user_id");
        user_id = v ? json_copy(v) : json_null();
    }
    {
        char *resolved = event_team_id(event, body, installs, installs_present);
        char *at_team = py_truthy(jo(at, "team_id")) ? py_str(jo(at, "team_id")) : xstrdup("");
        char *kept = installs_has(installs, at_team) ? xstrdup(at_team) : xstrdup("");
        if (resolved[0]) {
            team = resolved;
            free(kept);
        } else {
            team = kept;
            free(resolved);
        }
        free(at_team);
    }
    json_set(out, "channel_id", str_node_or_empty(channel_id));
    json_set(out, "thread_ts", str_node_or_empty(thread_ts));
    json_set(out, "user_id", str_node_or_empty(user_id));
    json_set(out, "team_id", json_string(team));
    {
        char *ctx = context_channel_id(context);
        json_set(out, "context_channel_id", json_string(ctx));
        free(ctx);
    }
    free(team);
    json_free(channel_id);
    json_free(thread_ts);
    json_free(user_id);
    json_free(context);
    json_free(at);
    return out;
}

json_t *w27_p9_event_team_id(const json_t *req) {
    json_t *installs = jo(req, "installs");
    int present = installs && installs->type != JSON_NULL;
    char *out = event_team_id(jo(req, "event"), jo(req, "body"), installs, present);
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p9_team_in_install_set(const json_t *req) {
    json_t *installs = jo(req, "installs");
    const char *team = rq_str(req, "team_id", "");
    if (!team[0]) return json_string("");
    return json_string(installs_has(installs, team) ? team : "");
}

json_t *w27_p9_is_foreign_author(const json_t *req) {
    const char *author = rq_str(req, "author_team", "");
    const char *resolved = rq_str(req, "resolved_team", "");
    json_t *installs = jo(req, "installs");
    int foreign = author[0] && resolved[0] && strcmp(author, resolved) != 0 &&
                  !installs_has(installs, author);
    return json_bool(foreign);
}

json_t *w27_p9_context_channel_id(const json_t *req) {
    char *out = context_channel_id(jo(req, "context"));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p9_extract_assistant_meta(const json_t *req) {
    json_t *installs = jo(req, "installs");
    int present = installs && installs->type != JSON_NULL;
    return extract_assistant_meta(jo(req, "event"), jo(req, "body"), installs, present);
}

json_t *w27_p9_agent_view_event_fields(const json_t *req) {
    json_t *event = jo(req, "event");
    json_t *body = jo(req, "body");
    json_t *installs = jo(req, "installs");
    int present = installs && installs->type != JSON_NULL;
    json_t *context = jo(event, "context");
    json_t *out = json_object();
    char *ctx, *team;
    json_t *uv;
    if (!py_truthy(context)) context = jo(event, "app_context");
    ctx = context_channel_id(context);
    uv = jo(event, "user");
    if (!py_truthy(uv)) uv = jo(event, "user_id");
    team = event_team_id(event, body, installs, present);
    json_set(out, "context_channel_id", json_string(ctx));
    json_set(out, "user_id", str_node_or_empty(uv));
    json_set(out, "team_id", json_string(team));
    free(ctx);
    free(team);
    return out;
}

json_t *w27_p9_suggested_prompts(const json_t *req) {
    json_t *raw = jo(req, "raw");
    json_t *out = json_array();
    json_t *prompts = json_array();
    char *title = xstrdup("");
    json_t *rows = raw;
    if (is_obj(raw)) {
        json_t *tv = jo(raw, "title");
        free(title);
        title = py_truthy(tv) ? strip_copy(py_str(tv)) : xstrdup("");
        rows = jo(raw, "prompts");
    }
    if (is_arr(rows)) {
        size_t i;
        for (i = 0; i < json_len(rows); i++) {
            json_t *item = json_get(rows, i);
            char *pt, *pm;
            if (!is_obj(item)) continue;
            pt = py_truthy(jo(item, "title")) ? strip_copy(py_str(jo(item, "title"))) : xstrdup("");
            pm = py_truthy(jo(item, "message")) ? strip_copy(py_str(jo(item, "message")))
                                                : xstrdup("");
            if (pt[0] && pm[0]) {
                json_t *row = json_object();
                char *short_title = cp_prefix(pt, 75);
                json_set(row, "title", json_string(short_title));
                json_set(row, "message", json_string(pm));
                json_append(prompts, row);
                free(short_title);
            }
            free(pt);
            free(pm);
            if (json_len(prompts) >= 4) break;
        }
    }
    json_append(out, json_string(title));
    json_append(out, prompts);
    free(title);
    return out;
}

/* ===================== gate p10: admission policy predicates ===================== */

/* _slack_require_mention */
static int require_mention_flag(const json_t *extra, const json_t *env) {
    const json_t *configured;
    json_t *value = jo(extra, "require_mention");
    if (value && value->type != JSON_NULL) {
        configured = value;
    } else if (env && is_str(env) && env->str_val[0]) {
        configured = env;
    } else {
        return 1;
    }
    if (is_str(configured)) {
        char *low = strip_copy(configured->str_val);
        size_t i;
        int out;
        for (i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
        out = !(strcmp(low, "false") == 0 || strcmp(low, "0") == 0 || strcmp(low, "no") == 0 ||
                strcmp(low, "off") == 0);
        free(low);
        return out;
    }
    return py_truthy(configured);
}

/* _extra_or_env_flag (blank_is_unset=False: a blank YAML string is a value) */
static int opt_in_flag(const json_t *extra, const char *key, const json_t *env, int do_strip) {
    const json_t *configured;
    json_t *value = jo(extra, key);
    if (env && is_str(env) && env->str_val[0] && !is_blank(env->str_val)) {
        configured = env;
    } else if (value && value->type != JSON_NULL) {
        configured = value;
    } else {
        return 0;
    }
    if (is_str(configured)) {
        char *low = do_strip ? strip_copy(configured->str_val) : xstrdup(configured->str_val);
        char *trimmed = strip_copy(low);
        size_t i;
        int out;
        for (i = 0; trimmed[i]; i++) trimmed[i] = (char)tolower((unsigned char)trimmed[i]);
        out = is_truthy_word(trimmed);
        free(low);
        free(trimmed);
        return out;
    }
    return py_truthy(configured);
}

/* ^\s*<@([^>|\s]+)(?:\|[^>]*)?> */
static char *addressed_user(const char *text) {
    size_t i = 0, start, end;
    if (!text) return NULL;
    while (text[i] && isspace((unsigned char)text[i])) i++;
    if (text[i] != '<' || text[i + 1] != '@') return NULL;
    start = i + 2;
    end = start;
    while (text[end] && text[end] != '>' && text[end] != '|' &&
           !isspace((unsigned char)text[end]))
        end++;
    if (end == start) return NULL;
    {
        size_t p = end;
        if (text[p] == '|') {
            while (text[p] && text[p] != '>') p++;
        }
        if (text[p] != '>') return NULL;
        {
            char *out = (char *)malloc(end - start + 1);
            memcpy(out, text + start, end - start);
            out[end - start] = '\0';
            return out;
        }
    }
}

/* _slack_message_mentions_self: <@uid> or <@uid|label> anywhere */
static int mentions_self(const char *text, const json_t *uids) {
    size_t i;
    if (!py_truthy(uids)) return 0;
    for (i = 0; i < json_len(uids); i++) {
        json_t *u = json_get(uids, i);
        const char *uid;
        size_t n;
        const char *p;
        if (!is_str(u) || !u->str_val[0]) continue;
        uid = u->str_val;
        n = strlen(uid);
        for (p = text ? text : ""; (p = strstr(p, "<@")) != NULL; p += 2) {
            if (strncmp(p + 2, uid, n) != 0) continue;
            if (p[2 + n] == '>') return 1;
            if (p[2 + n] == '|') {
                const char *q = p + 2 + n + 1;
                while (*q && *q != '>') q++;
                if (*q == '>') return 1;
            }
        }
    }
    return 0;
}

static int str_list_has(const json_t *arr, const char *s) {
    size_t i;
    for (i = 0; is_arr(arr) && i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (is_str(v) && strcmp(v->str_val, s) == 0) return 1;
    }
    return 0;
}

/* _is_slack_cdn_url (urlsplit; scheme + hostname allowlist) */
static int slack_cdn_url(const char *url) {
    const char *p, *host;
    size_t n;
    int https;
    if (!url) return 0;
    p = strstr(url, "://");
    if (!p) return 0;
    n = (size_t)(p - url);
    {
        char *scheme = (char *)malloc(n + 1);
        size_t i;
        memcpy(scheme, url, n);
        scheme[n] = '\0';
        for (i = 0; i < n; i++) scheme[i] = (char)tolower((unsigned char)scheme[i]);
        https = strcmp(scheme, "https") == 0;
        free(scheme);
    }
    if (!https) return 0;
    host = p + 3;
    while (*host && *host != '/' && *host != '?' && *host != '#') host++;
    /* host part is [p+3, host); drop userinfo */
    {
        const char *hstart = p + 3;
        const char *at = NULL, *q;
        size_t hl;
        char *h;
        for (q = hstart; q < host; q++)
            if (*q == '@') at = q;
        if (at) hstart = at + 1;
        hl = (size_t)(host - hstart);
        /* drop port */
        {
            const char *colon = NULL;
            for (q = hstart; q < host; q++)
                if (*q == ':') colon = q;
            if (colon) hl = (size_t)(colon - hstart);
        }
        h = (char *)malloc(hl + 1);
        memcpy(h, hstart, hl);
        h[hl] = '\0';
        {
            size_t hi;
            for (hi = 0; hi < hl; hi++) h[hi] = (char)tolower((unsigned char)h[hi]);
        }
        while (hl > 0 && h[hl - 1] == '.') h[--hl] = '\0';
        if (!hl) {
            free(h);
            return 0;
        }
        {
            static const char *EXACT[2] = {"slack.com", "slack-files.com"};
            static const char *SUFFIX[2] = {".slack.com", ".slack-files.com"};
            size_t i;
            int ok = 0;
            for (i = 0; i < 2; i++) {
                if (strcmp(h, EXACT[i]) == 0) ok = 1;
            }
            if (!ok) {
                size_t hn = strlen(h);
                for (i = 0; i < 2; i++) {
                    size_t sn = strlen(SUFFIX[i]);
                    if (hn > sn && strcmp(h + hn - sn, SUFFIX[i]) == 0) ok = 1;
                }
            }
            free(h);
            return ok;
        }
    }
}

json_t *w27_p10_require_mention(const json_t *req) {
    return json_bool(require_mention_flag(jo(req, "extra"), jo(req, "env")));
}

json_t *w27_p10_extra_or_env_flag(const json_t *req) {
    json_t *sv = jo(req, "strip");
    return json_bool(opt_in_flag(jo(req, "extra"), rq_str(req, "key", ""), jo(req, "env"),
                                 py_truthy(sv)));
}

json_t *w27_p10_addressed_other_user(const json_t *req) {
    json_t *text = jo(req, "text");
    char *uid = addressed_user(is_str(text) ? text->str_val : "");
    int out = 0;
    if (uid) {
        out = !str_list_has(jo(req, "self_uids"), uid);
        free(uid);
    }
    return json_bool(out);
}

json_t *w27_p10_is_free_channel(const json_t *req) {
    const char *cid = rq_str(req, "channel_id", "");
    json_t *reqch = jo(req, "require_mention_channels");
    json_t *freech = jo(req, "free_response_channels");
    json_t *rm = jo(req, "require_mention");
    int require_mention = (rm && rm->type == JSON_BOOL) ? rm->bool_val : 1;
    int out = !str_list_has(reqch, cid) && (str_list_has(freech, cid) || !require_mention);
    return json_bool(out);
}

json_t *w27_p10_reply_expected(const json_t *req) {
    json_t *uids = json_array();
    const char *bot_uid = rq_str(req, "bot_uid", "");
    const char *self_bot = rq_str(req, "self_bot_uid", "");
    const char *routing_text = rq_str(req, "routing_text", "");
    int addressed = py_truthy(jo(req, "addressed"));
    int opens_own = py_truthy(jo(req, "opens_own_session"));
    int followup = py_truthy(jo(req, "channel_thread_followup"));
    int silence = py_truthy(jo(req, "thread_followup_silence"));
    int free_channel = py_truthy(jo(req, "free_channel"));
    json_t *res;
    if (bot_uid[0]) json_append(uids, json_string(bot_uid));
    if (self_bot[0] && strcmp(self_bot, bot_uid) != 0) json_append(uids, json_string(self_bot));
    if (addressed || mentions_self(routing_text, uids)) {
        json_free(uids);
        return json_bool(1);
    }
    {
        char *other = addressed_user(routing_text);
        int other_uid = other && !str_list_has(uids, other);
        free(other);
        if (other_uid) {
            json_free(uids);
            return json_bool(0);
        }
    }
    json_free(uids);
    if (followup && silence) return json_bool(0);
    if (opens_own && free_channel) return json_bool(0);
    return json_null();
}

json_t *w27_p10_message_mentions_self(const json_t *req) {
    json_t *text = jo(req, "text");
    return json_bool(mentions_self(is_str(text) ? text->str_val : "", jo(req, "self_uids")));
}

/* _extra_or_env_channel_set */
static json_t *channel_set(const json_t *raw, const json_t *env, int coerce_scalar) {
    json_t *arr = json_array();
    svec_t parts;
    size_t i;
    sv_init(&parts);
    if (env && is_str(env) && env->str_val[0] && !is_blank(env->str_val)) raw = env;
    if (is_arr(raw)) {
        for (i = 0; i < json_len(raw); i++) {
            char *s = py_str(json_get(raw, i));
            char *t = strip_copy(s);
            if (t[0]) sv_push(&parts, t);
            else free(t);
            free(s);
        }
    } else if (!coerce_scalar && raw && raw->type != JSON_STRING && raw->type != JSON_NULL) {
        /* a bare non-str scalar only counts with coerce_scalar */
    } else {
        char *s;
        if (coerce_scalar && (!raw || raw->type == JSON_NULL)) s = xstrdup("");
        else s = py_str(raw);
        {
            char *t = strip_copy(s);
            if (t[0]) {
                char *cur = t;
                for (;;) {
                    char *comma = strchr(cur, ',');
                    char *seg;
                    if (comma) *comma = '\0';
                    seg = strip_copy(cur);
                    if (seg[0]) sv_push(&parts, xstrdup(seg));
                    free(seg);
                    if (!comma) break;
                    cur = comma + 1;
                }
            }
            free(t);
        }
        free(s);
    }
    {
        svec_t uniq;
        size_t a, b;
        sv_init(&uniq);
        for (a = 0; a < parts.n; a++) {
            int seen = 0;
            for (b = 0; b < uniq.n; b++) {
                if (strcmp(uniq.v[b], parts.v[a]) == 0) {
                    seen = 1;
                    break;
                }
            }
            if (!seen) sv_push(&uniq, xstrdup(parts.v[a]));
        }
        for (a = 0; a < uniq.n; a++) {
            for (b = a + 1; b < uniq.n; b++) {
                if (strcmp(uniq.v[a], uniq.v[b]) > 0) {
                    char *tmp = uniq.v[a];
                    uniq.v[a] = uniq.v[b];
                    uniq.v[b] = tmp;
                }
            }
        }
        for (a = 0; a < uniq.n; a++) json_append(arr, json_string(uniq.v[a]));
        for (a = 0; a < uniq.n; a++) free(uniq.v[a]);
        free(uniq.v);
    }
    for (i = 0; i < parts.n; i++) free(parts.v[i]);
    free(parts.v);
    return arr;
}

json_t *w27_p10_extra_or_env_channel_set(const json_t *req) {
    return channel_set(jo(req, "raw"), jo(req, "env"), py_truthy(jo(req, "coerce_scalar")));
}

json_t *w27_p10_slash_channel_gated(const json_t *req) {
    const char *cid = rq_str(req, "channel_id", "");
    json_t *ignored = jo(req, "ignored");
    json_t *allowed = jo(req, "allowed");
    int gated;
    if (ignored_channel(jo(req, "channel_id"), ignored)) return json_bool(1);
    gated = py_truthy(allowed) && !(cid[0] == 'D') && !str_list_has(allowed, cid);
    return json_bool(gated);
}

json_t *w27_p10_is_slack_cdn_url(const json_t *req) {
    return json_bool(slack_cdn_url(rq_str(req, "url", "")));
}

json_t *w27_p10_thread_rehydration_key(const json_t *req) {
    sb_t b;
    json_t *res;
    sb_init(&b);
    sb_adds(&b, rq_str(req, "team_id", ""));
    sb_addc(&b, ':');
    sb_adds(&b, rq_str(req, "channel_id", ""));
    sb_addc(&b, ':');
    sb_adds(&b, rq_str(req, "thread_ts", ""));
    if (py_truthy(jo(req, "per_user"))) {
        sb_addc(&b, ':');
        sb_adds(&b, rq_str(req, "user_id", ""));
    }
    res = json_string(b.p ? b.p : "");
    sb_free(&b);
    return res;
}

/* ===================== gate p11: standalone sender shaping ===================== */

/* _standalone_post_kwargs */
static json_t *post_kwargs(const json_t *chat_id, const json_t *text, const json_t *unfurl,
                           const json_t *thread_id) {
    json_t *out = json_object();
    json_set(out, "channel", chat_id ? json_copy(chat_id) : json_string(""));
    json_set(out, "text", text ? json_copy(text) : json_null());
    json_set(out, "mrkdwn", json_bool(1));
    if (is_obj(unfurl)) {
        size_t i;
        for (i = 0; i < json_object_size(unfurl); i++) {
            const char *k = json_object_get_key_at(unfurl, i);
            json_set(out, k, json_copy(json_object_get_at(unfurl, i)));
        }
    }
    if (py_truthy(thread_id)) json_set(out, "thread_ts", json_copy(thread_id));
    return out;
}

/* _load_slack_bot_tokens: the CSV half plus the saved-token dict (file I/O is the seam) */
static json_t *bot_tokens(const char *raw_token, const json_t *saved) {
    json_t *arr = json_array();
    svec_t tokens;
    size_t i;
    sv_init(&tokens);
    {
        const char *s = raw_token ? raw_token : "";
        const char *cur = s;
        for (;;) {
            const char *comma = strchr(cur, ',');
            size_t n = comma ? (size_t)(comma - cur) : strlen(cur);
            char *part = (char *)malloc(n + 1);
            char *t;
            memcpy(part, cur, n);
            part[n] = '\0';
            t = strip_copy(part);
            if (t[0]) sv_push(&tokens, t);
            else free(t);
            free(part);
            if (!comma) break;
            cur = comma + 1;
        }
    }
    if (is_obj(saved)) {
        for (i = 0; i < json_object_size(saved); i++) {
            json_t *entry = json_object_get_at(saved, i);
            json_t *tok = is_obj(entry) ? jo(entry, "token") : NULL;
            if (is_str(tok) && tok->str_val[0]) {
                size_t k;
                int seen = 0;
                for (k = 0; k < tokens.n; k++) {
                    if (strcmp(tokens.v[k], tok->str_val) == 0) {
                        seen = 1;
                        break;
                    }
                }
                if (!seen) sv_push(&tokens, xstrdup(tok->str_val));
            }
        }
    }
    for (i = 0; i < tokens.n; i++) json_append(arr, json_string(tokens.v[i]));
    for (i = 0; i < tokens.n; i++) free(tokens.v[i]);
    free(tokens.v);
    return arr;
}

json_t *w27_p11_post_kwargs(const json_t *req) {
    return post_kwargs(jo(req, "chat_id"), jo(req, "text"), jo(req, "unfurl_kwargs"),
                       jo(req, "thread_id"));
}

json_t *w27_p11_bot_tokens(const json_t *req) {
    return bot_tokens(rq_str(req, "raw_token", ""), jo(req, "saved"));
}

json_t *w27_p11_format_mrkdwn(const json_t *req) {
    char *out = format_message(rq_str(req, "text", ""));
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w27_p11_model_picker_notice(const json_t *req) {
    (void)req;
    return json_string("\xe2\x8f\xb3 This model picker expired \xe2\x80\x94 please run /model "
                       "again.");
}

json_t *w27_p11_trim_dm_cache(const json_t *req) {
    json_t *mapping = jo(req, "mapping");
    json_t *mv = jo(req, "max_size");
    long max_size = is_num(mv) ? (long)mv->num_val : 5000;
    json_t *out = json_object();
    size_t n, i, skip;
    if (!is_obj(mapping)) return out;
    n = json_object_size(mapping);
    skip = 0;
    while ((long)(n - skip) > max_size) skip++;
    for (i = skip; i < n; i++)
        json_set(out, json_object_get_key_at(mapping, i),
                 json_copy(json_object_get_at(mapping, i)));
    return out;
}

json_t *w27_p11_is_connected(const json_t *req) {
    json_t *tok = jo(req, "token");
    int out = is_str(tok) && !is_blank(tok->str_val);
    return json_bool(out);
}
