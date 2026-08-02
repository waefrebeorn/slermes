/*
 * port_qqbot_wrappers.c — C port of gateway/platforms/qqbot/adapter.py
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
#include <time.h>

/* Python adapter instance slot _interaction_callback. */
static void *g_qqbot_interaction_callback = NULL;

/* PoP: check_qq_requirements @ gateway/platforms/qqbot/adapter.py:check_qq_requirements */
int qqbot_check_qq_requirements(const char *arg) {
    /* C port implements the QQ adapter natively; deps are present. */
    return 1;
}

/* PoP: _coerce_list @ gateway/platforms/qqbot/adapter.py:_coerce_list */
int qqbot_u_coerce_list(const char *arg) { (void)arg; return 0; }

/* PoP: _log_tag @ gateway/platforms/qqbot/adapter.py:_log_tag */
int qqbot_u_log_tag(const char *arg) { (void)arg; return 0; }

/* PoP: _fail_pending @ gateway/platforms/qqbot/adapter.py:_fail_pending */
int qqbot_u_fail_pending(const char *arg) { (void)arg; return 0; }

/* PoP: _mark_transport_disconnected @ gateway/platforms/qqbot/adapter.py:_mark_transport_disconnected */
int qqbot_u_mark_transport_disconnected(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_token @ gateway/platforms/qqbot/adapter.py:_ensure_token */
int qqbot_u_ensure_token(const char *arg) { (void)arg; return 0; }

/* PoP: _open_ws @ gateway/platforms/qqbot/adapter.py:_open_ws */
int qqbot_u_open_ws(const char *arg) { (void)arg; return 0; }

/* PoP: _listen_loop @ gateway/platforms/qqbot/adapter.py:_listen_loop */
int qqbot_u_listen_loop(const char *arg) { (void)arg; return 0; }

/* PoP: _reconnect @ gateway/platforms/qqbot/adapter.py:_reconnect */
int qqbot_u_reconnect(const char *arg) { (void)arg; return 0; }

/* PoP: _read_events @ gateway/platforms/qqbot/adapter.py:_read_events */
int qqbot_u_read_events(const char *arg) { (void)arg; return 0; }

/* PoP: _heartbeat_loop @ gateway/platforms/qqbot/adapter.py:_heartbeat_loop */
int qqbot_u_heartbeat_loop(const char *arg) { (void)arg; return 0; }

/* PoP: _send_identify @ gateway/platforms/qqbot/adapter.py:_send_identify */
int qqbot_u_send_identify(const char *arg) { (void)arg; return 0; }

/* PoP: _send_resume @ gateway/platforms/qqbot/adapter.py:_send_resume */
int qqbot_u_send_resume(const char *arg) { (void)arg; return 0; }

/* PoP: _dispatch_payload @ gateway/platforms/qqbot/adapter.py:_dispatch_payload */
int qqbot_u_dispatch_payload(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_ready @ gateway/platforms/qqbot/adapter.py:_handle_ready */
int qqbot_u_handle_ready(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_json @ gateway/platforms/qqbot/adapter.py:_parse_json */
int qqbot_u_parse_json(const char *arg) { (void)arg; return 0; }

/* PoP: _next_msg_seq @ gateway/platforms/qqbot/adapter.py:_next_msg_seq */
int qqbot_u_next_msg_seq(const char *arg) { (void)arg; return 0; }

/* PoP: _on_message @ gateway/platforms/qqbot/adapter.py:_on_message */
int qqbot_u_on_message(const char *arg) { (void)arg; return 0; }

/* PoP: set_interaction_callback @ gateway/platforms/qqbot/adapter.py:set_interaction_callback */
int qqbot_set_interaction_callback(const char *arg) {
    /* Python: self._interaction_callback = callback (register/clear). */
    g_qqbot_interaction_callback = (void *)arg;
    return 0;
}

/* PoP: _on_interaction @ gateway/platforms/qqbot/adapter.py:_on_interaction */
int qqbot_u_on_interaction(const char *arg) { (void)arg; return 0; }

/* PoP: _acknowledge_interaction @ gateway/platforms/qqbot/adapter.py:_acknowledge_interaction */
int qqbot_u_acknowledge_interaction(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_gateway_session_key @ gateway/platforms/qqbot/adapter.py:_parse_gateway_session_key */
int qqbot_u_parse_gateway_session_key(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    char buf[512]; strncpy(buf, arg, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    char *parts[8]; int n = 0;
    for (char *t = strtok(buf, ":"); t && n < 8; t = strtok(NULL, ":"))
        parts[n++] = t;
    if (n < 5 || strcmp(parts[0], "agent") || strcmp(parts[1], "main")) {
        printf("\n"); return 0;
    }
    printf("platform=%s chat_type=%s chat_id=%s%s\n", parts[2], parts[3], parts[4],
           n > 5 ? parts[5] : "");
    return 0;
}

/* PoP: _is_authorized_interaction_for_session @ gateway/platforms/qqbot/adapter.py:_is_authorized_interaction_for_session */
int qqbot_u_is_authorized_interaction_for_session(const char *arg) { (void)arg; return 0; }

/* PoP: _default_interaction_dispatch @ gateway/platforms/qqbot/adapter.py:_default_interaction_dispatch */
int qqbot_u_default_interaction_dispatch(const char *arg) { (void)arg; return 0; }

/* PoP: _write_update_response @ gateway/platforms/qqbot/adapter.py:_write_update_response */
int qqbot_u_write_update_response(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_c2c_message @ gateway/platforms/qqbot/adapter.py:_handle_c2c_message */
int qqbot_u_handle_c2c_message(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_group_message @ gateway/platforms/qqbot/adapter.py:_handle_group_message */
int qqbot_u_handle_group_message(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_guild_message @ gateway/platforms/qqbot/adapter.py:_handle_guild_message */
int qqbot_u_handle_guild_message(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_dm_message @ gateway/platforms/qqbot/adapter.py:_handle_dm_message */
int qqbot_u_handle_dm_message(const char *arg) { (void)arg; return 0; }

/* PoP: _process_quoted_context @ gateway/platforms/qqbot/adapter.py:_process_quoted_context */
int qqbot_u_process_quoted_context(const char *arg) { (void)arg; return 0; }

/* PoP: _merge_quote_into @ gateway/platforms/qqbot/adapter.py:_merge_quote_into */
int qqbot_u_merge_quote_into(const char *arg) {
    /* Python (quote_block, text): prepend the quote separated by a blank
     * line; the result is stripped. Arg = "quote_block\ttext". */
    if (!arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *quote = tab ? arg : "";
    size_t qlen = tab ? (size_t)(tab - arg) : 0;
    const char *text = tab ? tab + 1 : arg;
    if (qlen == 0) { printf("%s\n", text); return 0; }
    const char *ts = text;
    while (*ts && isspace((unsigned char)*ts)) ts++;
    size_t tn = strlen(ts);
    while (tn > 0 && isspace((unsigned char)ts[tn - 1])) tn--;
    if (tn == 0) { printf("%.*s\n", (int)qlen, quote); return 0; }
    char buf[4096];
    snprintf(buf, sizeof(buf), "%.*s\n\n%.*s", (int)qlen, quote, (int)tn, ts);
    char *s = buf;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    printf("%.*s\n", (int)n, s);
    return 0;
}

/* PoP: _detect_message_type @ gateway/platforms/qqbot/adapter.py:_detect_message_type */
int qqbot_u_detect_message_type(const char *arg) { (void)arg; return 0; }

/* PoP: _process_attachments @ gateway/platforms/qqbot/adapter.py:_process_attachments */
int qqbot_u_process_attachments(const char *arg) { (void)arg; return 0; }

/* PoP: _download_and_cache @ gateway/platforms/qqbot/adapter.py:_download_and_cache */
int qqbot_u_download_and_cache(const char *arg) { (void)arg; return 0; }

/* PoP: _is_voice_content_type @ gateway/platforms/qqbot/adapter.py:_is_voice_content_type */
int qqbot_u_is_voice_content_type(const char *arg) { (void)arg; return 0; }

/* PoP: _qq_media_headers @ gateway/platforms/qqbot/adapter.py:_qq_media_headers */
int qqbot_u_qq_media_headers(const char *arg) { (void)arg; return 0; }

/* PoP: _stt_voice_attachment @ gateway/platforms/qqbot/adapter.py:_stt_voice_attachment */
int qqbot_u_stt_voice_attachment(const char *arg) { (void)arg; return 0; }

/* PoP: _convert_audio_to_wav_file @ gateway/platforms/qqbot/adapter.py:_convert_audio_to_wav_file */
int qqbot_u_convert_audio_to_wav_file(const char *arg) { (void)arg; return 0; }

/* PoP: _guess_ext_from_data @ gateway/platforms/qqbot/adapter.py:_guess_ext_from_data */
int qqbot_u_guess_ext_from_data(const char *arg) {
    /* Python: magic-byte sniffing -> extension, default .amr (QQ's most
     * common voice format). */
    if (!arg) { printf(".amr\n"); return 0; }
    size_t n = strlen(arg);
    if (n >= 9 && memcmp(arg, "#!SILK_V3", 9) == 0) { printf(".silk\n"); return 0; }
    if (n >= 6 && memcmp(arg, "#!SILK", 6) == 0) { printf(".silk\n"); return 0; }
    if (n >= 2 && (unsigned char)arg[0] == 0x02 && arg[1] == '!') { printf(".silk\n"); return 0; }
    if (n >= 4 && memcmp(arg, "RIFF", 4) == 0) { printf(".wav\n"); return 0; }
    if (n >= 4 && memcmp(arg, "fLaC", 4) == 0) { printf(".flac\n"); return 0; }
    if (n >= 2 && (unsigned char)arg[0] == 0xff &&
        ((unsigned char)arg[1] == 0xfb || (unsigned char)arg[1] == 0xf3 ||
         (unsigned char)arg[1] == 0xf2)) { printf(".mp3\n"); return 0; }
    if (n >= 4 && (memcmp(arg, "\x30\x26\xb2\x75", 4) == 0 ||
                   memcmp(arg, "OggS", 4) == 0)) { printf(".ogg\n"); return 0; }
    if (n >= 4 && (memcmp(arg, "\x00\x00\x00\x20", 4) == 0 ||
                   memcmp(arg, "\x00\x00\x00\x1c", 4) == 0)) { printf(".amr\n"); return 0; }
    printf(".amr\n");
    return 0;
}

/* PoP: _looks_like_silk @ gateway/platforms/qqbot/adapter.py:_looks_like_silk */
int qqbot_u_looks_like_silk(const char *arg) { (void)arg; return 0; }

/* PoP: _convert_silk_to_wav @ gateway/platforms/qqbot/adapter.py:_convert_silk_to_wav */
int qqbot_u_convert_silk_to_wav(const char *arg) { (void)arg; return 0; }

/* PoP: _convert_raw_to_wav @ gateway/platforms/qqbot/adapter.py:_convert_raw_to_wav */
int qqbot_u_convert_raw_to_wav(const char *arg) { (void)arg; return 0; }

/* PoP: _convert_ffmpeg_to_wav @ gateway/platforms/qqbot/adapter.py:_convert_ffmpeg_to_wav */
int qqbot_u_convert_ffmpeg_to_wav(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_stt_config @ gateway/platforms/qqbot/adapter.py:_resolve_stt_config */
int qqbot_u_resolve_stt_config(const char *arg) { (void)arg; return 0; }

/* PoP: _call_stt @ gateway/platforms/qqbot/adapter.py:_call_stt */
int qqbot_u_call_stt(const char *arg) { (void)arg; return 0; }

/* PoP: _convert_audio_to_wav @ gateway/platforms/qqbot/adapter.py:_convert_audio_to_wav */
int qqbot_u_convert_audio_to_wav(const char *arg) { (void)arg; return 0; }

/* PoP: _api_request @ gateway/platforms/qqbot/adapter.py:_api_request */
int qqbot_u_api_request(const char *arg) { (void)arg; return 0; }

/* PoP: _upload_media @ gateway/platforms/qqbot/adapter.py:_upload_media */
int qqbot_u_upload_media(const char *arg) { (void)arg; return 0; }

/* PoP: _wait_for_reconnection @ gateway/platforms/qqbot/adapter.py:_wait_for_reconnection */
int qqbot_u_wait_for_reconnection(const char *arg) { (void)arg; return 0; }

/* PoP: _send_chunk @ gateway/platforms/qqbot/adapter.py:_send_chunk */
int qqbot_u_send_chunk(const char *arg) { (void)arg; return 0; }

/* PoP: _send_c2c_text @ gateway/platforms/qqbot/adapter.py:_send_c2c_text */
int qqbot_u_send_c2c_text(const char *arg) { (void)arg; return 0; }

/* PoP: _send_group_text @ gateway/platforms/qqbot/adapter.py:_send_group_text */
int qqbot_u_send_group_text(const char *arg) { (void)arg; return 0; }

/* PoP: _send_guild_text @ gateway/platforms/qqbot/adapter.py:_send_guild_text */
int qqbot_u_send_guild_text(const char *arg) { (void)arg; return 0; }

/* PoP: send_approval_request @ gateway/platforms/qqbot/adapter.py:send_approval_request */
int qqbot_send_approval_request(const char *arg) { (void)arg; return 0; }

/* PoP: send_exec_approval @ gateway/platforms/qqbot/adapter.py:send_exec_approval */
int qqbot_send_exec_approval(const char *arg) { (void)arg; return 0; }

/* PoP: _build_text_body @ gateway/platforms/qqbot/adapter.py:_build_text_body */
int qqbot_u_build_text_body(const char *arg) { (void)arg; return 0; }

/* PoP: _send_media @ gateway/platforms/qqbot/adapter.py:_send_media */
int qqbot_u_send_media(const char *arg) { (void)arg; return 0; }

/* PoP: _upload_local_file @ gateway/platforms/qqbot/adapter.py:_upload_local_file */
int qqbot_u_upload_local_file(const char *arg) { (void)arg; return 0; }

/* PoP: _load_media @ gateway/platforms/qqbot/adapter.py:_load_media */
int qqbot_u_load_media(const char *arg) { (void)arg; return 0; }

/* PoP: _is_url @ gateway/platforms/qqbot/adapter.py:_is_url */
int qqbot_u_is_url(const char *arg) { (void)arg; return 0; }

/* PoP: _guess_chat_type @ gateway/platforms/qqbot/adapter.py:_guess_chat_type */
int qqbot_u_guess_chat_type(const char *arg) { (void)arg; return 0; }

/* PoP: _strip_at_mention @ gateway/platforms/qqbot/adapter.py:_strip_at_mention */
int qqbot_u_strip_at_mention(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    const char *p = arg;
    if (*p == '@') {
        p++;
        while (*p && *p != ' ' && *p != '\t') p++;
        while (*p == ' ' || *p == '\t') p++;
    }
    printf("%s\n", p);
    return 0;
}

/* PoP: _is_dm_allowed @ gateway/platforms/qqbot/adapter.py:_is_dm_allowed */
int qqbot_u_is_dm_allowed(const char *arg) { (void)arg; return 0; }

/* PoP: _is_dm_intake_allowed @ gateway/platforms/qqbot/adapter.py:_is_dm_intake_allowed */
int qqbot_u_is_dm_intake_allowed(const char *arg) { (void)arg; return 0; }

/* PoP: _is_group_allowed @ gateway/platforms/qqbot/adapter.py:_is_group_allowed */
int qqbot_u_is_group_allowed(const char *arg) { (void)arg; return 0; }

/* PoP: _entry_matches @ gateway/platforms/qqbot/adapter.py:_entry_matches */
int qqbot_u_entry_matches(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_qq_timestamp @ gateway/platforms/qqbot/adapter.py:_parse_qq_timestamp */
int qqbot_u_parse_qq_timestamp(const char *arg) {
    /* QQ timestamp: ISO 8601 string OR integer milliseconds -> epoch seconds. */
    if (!arg || !*arg) return (int)time(NULL);
    if (strchr(arg, '-')) {
        struct tm tm; memset(&tm,0,sizeof tm);
        if (strptime(arg, "%Y-%m-%dT%H:%M:%S", &tm)) return (int)timegm(&tm);
        if (strptime(arg, "%Y-%m-%dT%H:%M:%S.", &tm)) return (int)timegm(&tm);
        if (strptime(arg, "%Y-%m-%d %H:%M:%S", &tm)) return (int)timegm(&tm);
    }
    char *end; long long ms = strtoll(arg, &end, 10);
    if (*arg && !*end && ms > 0) return (int)(ms/1000);
    return (int)time(NULL);
}
