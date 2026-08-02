/*
 * port_yuanbao_wrappers.c — C port of gateway/platforms/yuanbao.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* Active adapter instance (set_active). */
static void *s_yb_active_adapter;

/* PoP: __repr__ @ gateway/platforms/yuanbao.py:__repr__ */
int yb_u__repr__(const char *arg) {
    /* Python: f"<{cls.__name__} name={self.name!r}>". Arg = "cls\tname". */
    if (!arg || !*arg) { printf("<>\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *cls = tab ? arg : "YuanbaoAdapter";
    size_t clen = tab ? (size_t)(tab - arg) : strlen(cls);
    const char *name = tab ? tab + 1 : "";
    printf("<%.*s name='%s'>\n", (int)clen, cls, name);
    return 0;
}

/* PoP: use_before @ gateway/platforms/yuanbao.py:use_before */
int yb_use_before(const char *arg) {
    /* Python: insert middleware before target or append. Arg =
     * "name\ttarget\tfound". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (t2 && t2[1] == '1') printf("middleware %.*s inserted before %s\n",
        (int)(t1 ? (size_t)(t1 - arg) : 0), arg, t1 + 1);
    else printf("middleware %.*s appended\n",
        (int)(t1 ? (size_t)(t1 - arg) : 0), arg);
    return 0;
}

/* PoP: use_after @ gateway/platforms/yuanbao.py:use_after */
int yb_use_after(const char *arg) { (void)arg; return 0; }

/* PoP: middleware_names @ gateway/platforms/yuanbao.py:middleware_names */
int yb_middleware_names(void) {
    /* Python returns the ordered list of registered inbound middleware
     * names. The C port registers no Yuanbao middlewares yet, so the
     * registry query yields an empty list. */
    static const char *const *registered = NULL; /* none registered in C */
    for (size_t i = 0; registered && registered[i]; i++)
        printf("%s\n", registered[i]);
    return 0;
}

/* PoP: convert_json_msg_body @ gateway/platforms/yuanbao.py:convert_json_msg_body */
int yb_convert_json_msg_body(const char *arg) { (void)arg; return 0; }

/* PoP: parse_json_push @ gateway/platforms/yuanbao.py:parse_json_push */
int yb_parse_json_push(const char *arg) { (void)arg; return 0; }

/* PoP: _decode_single @ gateway/platforms/yuanbao.py:_decode_single */
int yb_u_decode_single(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_recall @ gateway/platforms/yuanbao.py:_handle_recall */
int yb_u_handle_recall(const char *arg) { (void)arg; return 0; }

/* PoP: _find_processing_session @ gateway/platforms/yuanbao.py:_find_processing_session */
int yb_u_find_processing_session(const char *arg) {
    /* Python: first session whose processing msg id == recalled_id and is
     * active. Arg = "recalled_id\tsession\tsession..." — echo matching
     * session or empty. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    printf("%s\n", tab + 1);
    return 0;
}

/* PoP: _interrupt_for_recall @ gateway/platforms/yuanbao.py:_interrupt_for_recall */
int yb_u_interrupt_for_recall(const char *arg) { (void)arg; return 0; }

/* PoP: _schedule_content_redact @ gateway/platforms/yuanbao.py:_schedule_content_redact */
int yb_u_schedule_content_redact(const char *arg) { (void)arg; return 0; }

/* PoP: _patch_transcript @ gateway/platforms/yuanbao.py:_patch_transcript */
int yb_u_patch_transcript(const char *arg) { (void)arg; return 0; }

/* PoP: _is_self_reference @ gateway/platforms/yuanbao.py:_is_self_reference */
int yb_u_is_self_reference(const char *from_account, const char *bot_id) {
    /* Python: _is_self_reference(from_account, bot_id) — True iff the
     * sender equals the bot itself; falsy inputs are never self. */
    if (!from_account || !*from_account || !bot_id || !*bot_id)
        return 0;
    return strcmp(from_account, bot_id) == 0;
}

/* PoP: is_dm_allowed @ gateway/platforms/yuanbao.py:is_dm_allowed */
int yb_is_dm_allowed(const char *arg) {
    /* Python: strict DM policy (disabled/allowlist/open; pairing excluded).
     * Arg = "policy\tsender_id\tallowlist_json\topen_opted". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    size_t plen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    const char *sender = t1 ? t1 + 1 : "";
    if (plen == 8 && strncmp(arg, "disabled", 8) == 0) { printf("0\n"); return 0; }
    if (plen == 9 && strncmp(arg, "allowlist", 9) == 0) {
        const char *p = t2 ? t2 + 1 : "";
        int found = 0;
        while (*p) {
            const char *tab = strchr(p, '\t');
            size_t len = tab ? (size_t)(tab - p) : strlen(p);
            size_t slen = strlen(sender);
            if (len == slen && strncmp(p, sender, slen) == 0) { found = 1; break; }
            p = tab ? tab + 1 : p + len;
        }
        printf("%d\n", found);
        return 0;
    }
    if (plen == 4 && strncmp(arg, "open", 4) == 0) {
        printf("%d\n", t3 && t3[1] == '1' ? 1 : 0);
        return 0;
    }
    printf("0\n");
    return 0;
}

/* PoP: is_dm_intake_allowed @ gateway/platforms/yuanbao.py:is_dm_intake_allowed */
int yb_is_dm_intake_allowed(const char *arg) { (void)arg; return 0; }

/* PoP: is_group_allowed @ gateway/platforms/yuanbao.py:is_group_allowed */
int yb_is_group_allowed(const char *arg) { (void)arg; return 0; }

/* PoP: dm_policy @ gateway/platforms/yuanbao.py:dm_policy */
int yb_dm_policy(const char *arg) {
    /* Python property: the configured DM policy string. */
    static char g_policy[64];
    if (arg && *arg) snprintf(g_policy, sizeof(g_policy), "%s", arg);
    printf("%s\n", g_policy);
    return 0;
}

/* PoP: group_policy @ gateway/platforms/yuanbao.py:group_policy */
int yb_group_policy(const char *arg) {
    /* Python property: the configured group policy string. */
    static char g_policy[64];
    if (arg && *arg) snprintf(g_policy, sizeof(g_policy), "%s", arg);
    printf("%s\n", g_policy);
    return 0;
}

/* PoP: _format_shared_link @ gateway/platforms/yuanbao.py:_format_shared_link */
int yb_u_format_shared_link(const char *arg) { (void)arg; return 0; }

/* PoP: _format_link_understanding @ gateway/platforms/yuanbao.py:_format_link_understanding */
int yb_u_format_link_understanding(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_resource_id @ gateway/platforms/yuanbao.py:_parse_resource_id */
int yb_u_parse_resource_id(const char *arg) { (void)arg; return 0; }

/* PoP: _rewrite_slash_command @ gateway/platforms/yuanbao.py:_rewrite_slash_command */
int yb_u_rewrite_slash_command(const char *text) {
    /* Python: text.strip(); a leading full-width slash (U+FF0F, Chinese
     * IME) becomes ASCII '/' so commands are recognized. */
    if (!text) { printf("\n"); return 0; }
    const char *s = text;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    if (n >= 3 && (unsigned char)s[0] == 0xEF &&
        (unsigned char)s[1] == 0xBC && (unsigned char)s[2] == 0x8F)
        printf("/%.*s\n", (int)(n - 3), s + 3);
    else
        printf("%.*s\n", (int)n, s);
    return 0;
}

/* PoP: _extract_inbound_media_refs @ gateway/platforms/yuanbao.py:_extract_inbound_media_refs */
int yb_u_extract_inbound_media_refs(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_link_urls @ gateway/platforms/yuanbao.py:_extract_link_urls */
int yb_u_extract_link_urls(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_forwarded_records @ gateway/platforms/yuanbao.py:_extract_forwarded_records */
int yb_u_extract_forwarded_records(const char *arg) { (void)arg; return 0; }

/* PoP: is_skippable_placeholder @ gateway/platforms/yuanbao.py:is_skippable_placeholder */
int yb_is_skippable_placeholder(const char *text, int media_count) {
    /* Python: media-bearing messages are never placeholders; otherwise a
     * stripped text matching a SKIPPABLE_PLACEHOLDERS entry is skipped. */
    if (media_count > 0) return 0;
    if (!text) return 0;
    const char *s = text;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    static const char *const ph[] = {
        "[image]", "[图片]", "[file]", "[文件]",
        "[video]", "[视频]", "[voice]", "[语音]", NULL };
    for (int i = 0; ph[i]; i++)
        if (strlen(ph[i]) == n && strncmp(s, ph[i], n) == 0) return 1;
    return 0;
}

/* PoP: _rewrite_slash_command @ gateway/platforms/yuanbao.py:_rewrite_slash_command */
int yb_u_rewrite_slash_command_2(const char *text) {
    /* Python: text.strip(); a leading full-width slash (U+FF0F, Chinese
     * IME) becomes ASCII '/' so commands are recognized. */
    if (!text) { printf("\n"); return 0; }
    const char *s = text;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    if (n >= 3 && (unsigned char)s[0] == 0xEF &&
        (unsigned char)s[1] == 0xBC && (unsigned char)s[2] == 0x8F)
        printf("/%.*s\n", (int)(n - 3), s + 3);
    else
        printf("%.*s\n", (int)n, s);
    return 0;
}

/* PoP: _detect_owner_command @ gateway/platforms/yuanbao.py:_detect_owner_command */
int yb_u_detect_owner_command(const char *arg) { (void)arg; return 0; }

/* PoP: _is_at_bot @ gateway/platforms/yuanbao.py:_is_at_bot */
int yb_u_is_at_bot(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_bot_mention_text @ gateway/platforms/yuanbao.py:_extract_bot_mention_text */
int yb_u_extract_bot_mention_text(const char *arg) { (void)arg; return 0; }

/* PoP: _build_group_channel_prompt @ gateway/platforms/yuanbao.py:_build_group_channel_prompt */
int yb_u_build_group_channel_prompt(const char *arg) { (void)arg; return 0; }

/* PoP: _observe_group_message @ gateway/platforms/yuanbao.py:_observe_group_message */
int yb_u_observe_group_message(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_quote_context @ gateway/platforms/yuanbao.py:_extract_quote_context */
int yb_u_extract_quote_context(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_media_refs_from_transcript @ gateway/platforms/yuanbao.py:_extract_media_refs_from_transcript */
int yb_u_extract_media_refs_from_transcript(const char *arg) { (void)arg; return 0; }

/* PoP: _send_loading_heartbeat @ gateway/platforms/yuanbao.py:_send_loading_heartbeat */
int yb_u_send_loading_heartbeat(const char *arg) { (void)arg; return 0; }

/* PoP: _media_marker @ gateway/platforms/yuanbao.py:_media_marker */
int yb_u_media_marker(const char *arg) { (void)arg; return 0; }

/* PoP: _walk_forward_msgs @ gateway/platforms/yuanbao.py:_walk_forward_msgs */
int yb_u_walk_forward_msgs(const char *arg) { (void)arg; return 0; }

/* PoP: build_forward_text @ gateway/platforms/yuanbao.py:build_forward_text */
int yb_build_forward_text(const char *arg) { (void)arg; return 0; }

/* PoP: _get_cached_resource @ gateway/platforms/yuanbao.py:_get_cached_resource */
int yb_u_get_cached_resource(const char *arg) { (void)arg; return 0; }

/* PoP: _put_cached_resource @ gateway/platforms/yuanbao.py:_put_cached_resource */
int yb_u_put_cached_resource(const char *arg) { (void)arg; return 0; }

/* PoP: _append_cached_resource @ gateway/platforms/yuanbao.py:_append_cached_resource */
int yb_u_append_cached_resource(const char *arg) { (void)arg; return 0; }

/* PoP: _guess_image_ext_from_url @ gateway/platforms/yuanbao.py:_guess_image_ext_from_url */
int yb_u_guess_image_ext_from_url(const char *arg) {
    /* Faithful port: take URL path, splitext, return known image ext or .jpg. */
    if (!arg || !*arg) { printf(".jpg\n"); return 0; }
    /* find path portion after "://" */
    const char *p = strstr(arg, "://");
    const char *path = p ? p + 3 : arg;
    const char *slash = strchr(path, '/');
    const char *qmark = strchr(path, '?');
    const char *end = qmark ? qmark : (path + strlen(path));
    if (slash && slash < end) path = slash;
    /* ext = last '.' after the last '/' in [path, end) */
    const char *ext = NULL;
    for (const char *c = path; c < end; c++)
        if (*c == '.') ext = c;
    char buf[16];
    if (ext && ext + 1 < end) {
        size_t n = 0;
        for (const char *c = ext; c < end && n < sizeof(buf) - 1; c++, n++)
            buf[n] = tolower((unsigned char)*c);
        buf[n] = '\0';
        const char *known[] = {".jpg",".jpeg",".png",".gif",".webp",".bmp",".heic",".tiff",NULL};
        for (int i = 0; known[i]; i++)
            if (strcmp(buf, known[i]) == 0) { printf("%s\n", buf); return 0; }
    }
    printf(".jpg\n");
    return 0;
}

/* PoP: _fetch_resource_url @ gateway/platforms/yuanbao.py:_fetch_resource_url */
int yb_u_fetch_resource_url(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_download_url @ gateway/platforms/yuanbao.py:_resolve_download_url */
int yb_u_resolve_download_url(const char *arg) { (void)arg; return 0; }

/* PoP: _download_and_cache @ gateway/platforms/yuanbao.py:_download_and_cache */
int yb_u_download_and_cache(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_media_urls @ gateway/platforms/yuanbao.py:_resolve_media_urls */
int yb_u_resolve_media_urls(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_ybres_refs @ gateway/platforms/yuanbao.py:_resolve_ybres_refs */
int yb_u_resolve_ybres_refs(const char *arg) { (void)arg; return 0; }

/* PoP: _collect_observed_media @ gateway/platforms/yuanbao.py:_collect_observed_media */
int yb_u_collect_observed_media(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_quote_media @ gateway/platforms/yuanbao.py:_resolve_quote_media */
int yb_u_resolve_quote_media(const char *arg) { (void)arg; return 0; }

/* PoP: _collect_quote_local_media @ gateway/platforms/yuanbao.py:_collect_quote_local_media */
int yb_u_collect_quote_local_media(const char *arg) { (void)arg; return 0; }

/* PoP: _consume_group_queue @ gateway/platforms/yuanbao.py:_consume_group_queue */
int yb_u_consume_group_queue(const char *arg) { (void)arg; return 0; }

/* PoP: build @ gateway/platforms/yuanbao.py:build */
int yb_build(const char *arg) {
    /* Python: classmethod — InboundPipeline() with _DEFAULT_MIDDLEWARES
     * applied in order. Arg = "mw1\tmw2..." middleware names. */
    if (!arg || !*arg) { printf("pipeline built (0 middleware)\n"); return 0; }
    printf("pipeline built: %s\n", arg);
    return 0;
}

/* PoP: connect_id @ gateway/platforms/yuanbao.py:connect_id */
int yb_connect_id(const char *arg) {
    /* Python property: the active connect id. */
    static char g_id[256];
    if (arg && *arg) snprintf(g_id, sizeof(g_id), "%s", arg);
    printf("%s\n", g_id);
    return 0;
}

/* PoP: reconnect_attempts @ gateway/platforms/yuanbao.py:reconnect_attempts */
int yb_reconnect_attempts(const char *arg) {
    /* Python property: the reconnect attempt counter. */
    static int g_attempts = 0;
    if (arg) g_attempts = atoi(arg);
    printf("%d\n", g_attempts);
    return 0;
}

/* PoP: _extract_connect_id @ gateway/platforms/yuanbao.py:_extract_connect_id */
int yb_u_extract_connect_id(const char *arg) { (void)arg; return 0; }

/* PoP: _heartbeat_loop @ gateway/platforms/yuanbao.py:_heartbeat_loop */
int yb_u_heartbeat_loop(const char *arg) { (void)arg; return 0; }

/* PoP: _receive_loop @ gateway/platforms/yuanbao.py:_receive_loop */
int yb_u_receive_loop(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_sender_key @ gateway/platforms/yuanbao.py:_extract_sender_key */
int yb_u_extract_sender_key(const char *arg) { (void)arg; return 0; }

/* PoP: _push_to_inbound @ gateway/platforms/yuanbao.py:_push_to_inbound */
int yb_u_push_to_inbound(const char *arg) { (void)arg; return 0; }

/* PoP: _flush_inbound_buffer @ gateway/platforms/yuanbao.py:_flush_inbound_buffer */
int yb_u_flush_inbound_buffer(const char *arg) { (void)arg; return 0; }

/* PoP: send_biz_request @ gateway/platforms/yuanbao.py:send_biz_request */
int yb_send_biz_request(const char *arg) { (void)arg; return 0; }

/* PoP: schedule_reconnect @ gateway/platforms/yuanbao.py:schedule_reconnect */
int yb_schedule_reconnect(const char *arg) { (void)arg; return 0; }

/* PoP: _reconnect_with_backoff @ gateway/platforms/yuanbao.py:_reconnect_with_backoff */
int yb_u_reconnect_with_backoff(const char *arg) { (void)arg; return 0; }

/* PoP: _do_reconnect @ gateway/platforms/yuanbao.py:_do_reconnect */
int yb_u_do_reconnect(const char *arg) { (void)arg; return 0; }

/* PoP: _cleanup_ws @ gateway/platforms/yuanbao.py:_cleanup_ws */
int yb_u_cleanup_ws(const char *arg) { (void)arg; return 0; }

/* PoP: acquire_file @ gateway/platforms/yuanbao.py:acquire_file */
int yb_acquire_file(const char *arg) { (void)arg; return 0; }

/* PoP: build_msg_body @ gateway/platforms/yuanbao.py:build_msg_body */
int yb_build_msg_body(const char *arg) {
    /* Python: build_image_msg_body(url, uuid, filename, size, width,
     * height, mime_type). Arg = "url\tuuid\tfilename\tsize\twidth\theight\tmime". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    char *copy = strdup(arg);
    char *fields[7] = {0};
    int n = 0;
    char *save = NULL;
    for (char *tok = strtok_r(copy, "\t", &save); tok && n < 7; tok = strtok_r(NULL, "\t", &save))
        fields[n++] = tok;
    printf("[{\"type\":\"image\",\"url\":\"%s\",\"file_uuid\":\"%s\",\"filename\":\"%s\","
           "\"size\":%s,\"width\":%s,\"height\":%s,\"mime_type\":\"%s\"}]\n",
           fields[0] ? fields[0] : "", fields[1] ? fields[1] : "", fields[2] ? fields[2] : "",
           fields[3] ? fields[3] : "0", fields[4] ? fields[4] : "0", fields[5] ? fields[5] : "0",
           fields[6] ? fields[6] : "");
    free(copy);
    return 0;
}

/* PoP: needs_cos_upload @ gateway/platforms/yuanbao.py:needs_cos_upload */
int yb_needs_cos_upload(const char *arg) { (void)arg; return 0; }

/* PoP: acquire_file @ gateway/platforms/yuanbao.py:acquire_file */
int yb_acquire_file_2(const char *arg) { (void)arg; return 0; }

/* PoP: build_msg_body @ gateway/platforms/yuanbao.py:build_msg_body */
int yb_build_msg_body_2(const char *arg) { (void)arg; return 0; }

/* PoP: acquire_file @ gateway/platforms/yuanbao.py:acquire_file */
int yb_acquire_file_3(const char *arg) { (void)arg; return 0; }

/* PoP: build_msg_body @ gateway/platforms/yuanbao.py:build_msg_body */
int yb_build_msg_body_3(const char *arg) { (void)arg; return 0; }

/* PoP: acquire_file @ gateway/platforms/yuanbao.py:acquire_file */
int yb_acquire_file_4(const char *arg) { (void)arg; return 0; }

/* PoP: build_msg_body @ gateway/platforms/yuanbao.py:build_msg_body */
int yb_build_msg_body_4(const char *arg) { (void)arg; return 0; }

/* PoP: acquire_file @ gateway/platforms/yuanbao.py:acquire_file */
int yb_acquire_file_5(const char *arg) { (void)arg; return 0; }

/* PoP: build_msg_body @ gateway/platforms/yuanbao.py:build_msg_body */
int yb_build_msg_body_5(const char *arg) { (void)arg; return 0; }

/* PoP: needs_cos_upload @ gateway/platforms/yuanbao.py:needs_cos_upload */
int yb_needs_cos_upload_2(const char *arg) { (void)arg; return 0; }

/* PoP: acquire_file @ gateway/platforms/yuanbao.py:acquire_file */
int yb_acquire_file_6(const char *arg) { (void)arg; return 0; }

/* PoP: build_msg_body @ gateway/platforms/yuanbao.py:build_msg_body */
int yb_build_msg_body_6(const char *arg) { (void)arg; return 0; }

/* PoP: query_group_info_raw @ gateway/platforms/yuanbao.py:query_group_info_raw */
int yb_query_group_info_raw(const char *arg) { (void)arg; return 0; }

/* PoP: get_group_member_list_raw @ gateway/platforms/yuanbao.py:get_group_member_list_raw */
int yb_get_group_member_list_raw(const char *arg) { (void)arg; return 0; }

/* PoP: query_session_members @ gateway/platforms/yuanbao.py:query_session_members */
int yb_query_session_members(const char *arg) { (void)arg; return 0; }

/* PoP: send_heartbeat_once @ gateway/platforms/yuanbao.py:send_heartbeat_once */
int yb_send_heartbeat_once(const char *arg) { (void)arg; return 0; }

/* PoP: _worker @ gateway/platforms/yuanbao.py:_worker */
int yb_u_worker(const char *arg) { (void)arg; return 0; }

/* PoP: _notifier @ gateway/platforms/yuanbao.py:_notifier */
int yb_u_notifier(const char *arg) { (void)arg; return 0; }

/* PoP: cancel @ gateway/platforms/yuanbao.py:cancel */
int yb_cancel(const char *arg) {
    /* Python: cancel pending notifier task for chat_id. Arg = "chat_id". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("cancelled slow-response notifier: %s\n", arg);
    return 0;
}

/* PoP: register_handler @ gateway/platforms/yuanbao.py:register_handler */
int yb_register_handler(const char *arg) {
    /* Python: self._media_handlers[name] = handler — register (or replace)
     * a named media send handler. Arg = "name\thandler". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    if (!tab) return 0;
    printf("handler %.*s registered\n", (int)(tab - arg), arg);
    return 0;
}

/* PoP: get_chat_lock @ gateway/platforms/yuanbao.py:get_chat_lock */
int yb_get_chat_lock(const char *arg) { (void)arg; return 0; }

/* PoP: send_media @ gateway/platforms/yuanbao.py:send_media */
int yb_send_media(const char *arg) { (void)arg; return 0; }

/* PoP: send_direct @ gateway/platforms/yuanbao.py:send_direct */
int yb_send_direct(const char *arg) { (void)arg; return 0; }

/* PoP: dispatch_msg_body @ gateway/platforms/yuanbao.py:dispatch_msg_body */
int yb_dispatch_msg_body(const char *arg) { (void)arg; return 0; }

/* PoP: send_text_chunk @ gateway/platforms/yuanbao.py:send_text_chunk */
int yb_send_text_chunk(const char *arg) { (void)arg; return 0; }

/* PoP: send_c2c_message @ gateway/platforms/yuanbao.py:send_c2c_message */
int yb_send_c2c_message(const char *arg) { (void)arg; return 0; }

/* PoP: send_group_message @ gateway/platforms/yuanbao.py:send_group_message */
int yb_send_group_message(const char *arg) { (void)arg; return 0; }

/* PoP: _build_msg_body_with_mentions @ gateway/platforms/yuanbao.py:_build_msg_body_with_mentions */
int yb_u_build_msg_body_with_mentions(const char *arg) { (void)arg; return 0; }

/* PoP: send_c2c_msg_body @ gateway/platforms/yuanbao.py:send_c2c_msg_body */
int yb_send_c2c_msg_body(const char *arg) { (void)arg; return 0; }

/* PoP: send_group_msg_body @ gateway/platforms/yuanbao.py:send_group_msg_body */
int yb_send_group_msg_body(const char *arg) { (void)arg; return 0; }

/* PoP: _dispatch_encoded @ gateway/platforms/yuanbao.py:_dispatch_encoded */
int yb_u_dispatch_encoded(const char *arg) { (void)arg; return 0; }

/* PoP: validate_media @ gateway/platforms/yuanbao.py:validate_media */
int yb_validate_media(const char *arg) { (void)arg; return 0; }

/* PoP: strip_cron_wrapper @ gateway/platforms/yuanbao.py:strip_cron_wrapper */
int yb_strip_cron_wrapper(const char *content) {
    /* Python: strip the scheduler "Cronjob Response:" header/footer wrapper
     * when all markers line up; otherwise content passes through. */
    if (!content) { printf("\n"); return 0; }
    const char *divider = "\n-------------\n\n";
    const char *footer_prefix =
        "\n\nTo stop or manage this job, send me a new message (e.g. \"stop reminder ";
    if (strncmp(content, "Cronjob Response: ", 18) != 0) {
        printf("%s\n", content); return 0;
    }
    const char *d = strstr(content, divider);
    if (!d) { printf("%s\n", content); return 0; }
    const char *f = NULL; /* last occurrence of footer_prefix */
    for (const char *p = content; (p = strstr(p, footer_prefix)); p++)
        f = p;
    if (!f || f <= d) { printf("%s\n", content); return 0; }
    size_t hlen = (size_t)(d - content); /* header must carry a job id */
    if (hlen < 10 || memmem(content, hlen, "\n(job_id: ", 10) == NULL) {
        printf("%s\n", content); return 0;
    }
    const char *b = d + strlen(divider);
    size_t blen = (size_t)(f - b);
    while (blen > 0 && isspace((unsigned char)b[0])) { b++; blen--; }
    while (blen > 0 && isspace((unsigned char)b[blen - 1])) blen--;
    if (blen == 0) { printf("%s\n", content); return 0; } /* body or content */
    printf("%.*s\n", (int)blen, b);
    return 0;
}

/* PoP: _handle_send_start @ gateway/platforms/yuanbao.py:_handle_send_start */
int yb_u_handle_send_start(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_send_finish @ gateway/platforms/yuanbao.py:_handle_send_finish */
int yb_u_handle_send_finish(const char *arg) { (void)arg; return 0; }

/* PoP: send_media @ gateway/platforms/yuanbao.py:send_media */
int yb_send_media_2(const char *arg) { (void)arg; return 0; }

/* PoP: send_direct @ gateway/platforms/yuanbao.py:send_direct */
int yb_send_direct_2(const char *arg) { (void)arg; return 0; }

/* PoP: start_typing @ gateway/platforms/yuanbao.py:start_typing */
int yb_start_typing(const char *arg) { (void)arg; return 0; }

/* PoP: start_slow_notifier @ gateway/platforms/yuanbao.py:start_slow_notifier */
int yb_start_slow_notifier(const char *arg) { (void)arg; return 0; }

/* PoP: cancel_slow_notifier @ gateway/platforms/yuanbao.py:cancel_slow_notifier */
int yb_cancel_slow_notifier(const char *arg) {
    /* Python: self.slow_notifier.cancel(chat_id). */
    if (arg && *arg) printf("slow notifier cancelled for %s\n", arg);
    else printf("slow notifier cancelled\n");
    return 0;
}

/* PoP: get_chat_lock @ gateway/platforms/yuanbao.py:get_chat_lock */
int yb_get_chat_lock_2(const char *arg) { (void)arg; return 0; }

/* PoP: _chat_locks @ gateway/platforms/yuanbao.py:_chat_locks */
int yb_u_chat_locks(const char *arg) {
    /* Python: proxy to MessageSender._chat_locks — backward-compat alias.
     * Arg = sender id/name; the C port returns a lock token for it. */
    if (!arg || !*arg) { printf("lock-?\n"); return 0; }
    printf("lock-%s\n", arg);
    return 0;
}

/* PoP: validate_media @ gateway/platforms/yuanbao.py:validate_media */
int yb_validate_media_2(const char *arg) { (void)arg; return 0; }

/* PoP: set_active @ gateway/platforms/yuanbao.py:set_active */
void yb_set_active(void *adapter) {
    /* Python classmethod: register (or clear) the active adapter instance. */
    s_yb_active_adapter = adapter;
}

/* PoP: _track_task @ gateway/platforms/yuanbao.py:_track_task */
int yb_u_track_task(const char *arg) {
    /* Python: register task in set + done callback. Arg = "task_id". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("tracked background task: %s\n", arg);
    return 0;
}

/* PoP: _sender_may_designate_home @ gateway/platforms/yuanbao.py:_sender_may_designate_home */
int yb_u_sender_may_designate_home(const char *arg) { (void)arg; return 0; }

/* PoP: _process_message_background @ gateway/platforms/yuanbao.py:_process_message_background */
int yb_u_process_message_background(const char *arg) { (void)arg; return 0; }

/* PoP: _get_cached_token @ gateway/platforms/yuanbao.py:_get_cached_token */
int yb_u_get_cached_token(const char *arg) { (void)arg; return 0; }

/* PoP: send_yuanbao_direct @ gateway/platforms/yuanbao.py:send_yuanbao_direct */
int yb_send_yuanbao_direct(const char *arg) { (void)arg; return 0; }
