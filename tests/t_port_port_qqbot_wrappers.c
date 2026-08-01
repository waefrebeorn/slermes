/* AUTO-GENERATED integration oracle harness for port_qqbot_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_qqbot_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int qqbot_check_qq_requirements(const char *);
extern int qqbot_u_coerce_list(const char *);
extern int qqbot_u_log_tag(const char *);
extern int qqbot_u_fail_pending(const char *);
extern int qqbot_u_mark_transport_disconnected(const char *);
extern int qqbot_u_ensure_token(const char *);
extern int qqbot_u_open_ws(const char *);
extern int qqbot_u_listen_loop(const char *);
extern int qqbot_u_reconnect(const char *);
extern int qqbot_u_read_events(const char *);
extern int qqbot_u_heartbeat_loop(const char *);
extern int qqbot_u_send_identify(const char *);
extern int qqbot_u_send_resume(const char *);
extern int qqbot_u_dispatch_payload(const char *);
extern int qqbot_u_handle_ready(const char *);
extern int qqbot_u_parse_json(const char *);
extern int qqbot_u_next_msg_seq(const char *);
extern int qqbot_u_on_message(const char *);
extern int qqbot_set_interaction_callback(const char *);
extern int qqbot_u_on_interaction(const char *);
extern int qqbot_u_acknowledge_interaction(const char *);
extern int qqbot_u_parse_gateway_session_key(const char *);
extern int qqbot_u_is_authorized_interaction_for_session(const char *);
extern int qqbot_u_default_interaction_dispatch(const char *);
extern int qqbot_u_write_update_response(const char *);
extern int qqbot_u_handle_c2c_message(const char *);
extern int qqbot_u_handle_group_message(const char *);
extern int qqbot_u_handle_guild_message(const char *);
extern int qqbot_u_handle_dm_message(const char *);
extern int qqbot_u_process_quoted_context(const char *);
extern int qqbot_u_merge_quote_into(const char *);
extern int qqbot_u_detect_message_type(const char *);
extern int qqbot_u_process_attachments(const char *);
extern int qqbot_u_download_and_cache(const char *);
extern int qqbot_u_is_voice_content_type(const char *);
extern int qqbot_u_qq_media_headers(const char *);
extern int qqbot_u_stt_voice_attachment(const char *);
extern int qqbot_u_convert_audio_to_wav_file(const char *);
extern int qqbot_u_guess_ext_from_data(const char *);
extern int qqbot_u_looks_like_silk(const char *);
extern int qqbot_u_convert_silk_to_wav(const char *);
extern int qqbot_u_convert_raw_to_wav(const char *);
extern int qqbot_u_convert_ffmpeg_to_wav(const char *);
extern int qqbot_u_resolve_stt_config(const char *);
extern int qqbot_u_call_stt(const char *);
extern int qqbot_u_convert_audio_to_wav(const char *);
extern int qqbot_u_api_request(const char *);
extern int qqbot_u_upload_media(const char *);
extern int qqbot_u_wait_for_reconnection(const char *);
extern int qqbot_u_send_chunk(const char *);
extern int qqbot_u_send_c2c_text(const char *);
extern int qqbot_u_send_group_text(const char *);
extern int qqbot_u_send_guild_text(const char *);
extern int qqbot_send_approval_request(const char *);
extern int qqbot_send_exec_approval(const char *);
extern int qqbot_u_build_text_body(const char *);
extern int qqbot_u_send_media(const char *);
extern int qqbot_u_upload_local_file(const char *);
extern int qqbot_u_load_media(const char *);
extern int qqbot_u_is_url(const char *);
extern int qqbot_u_guess_chat_type(const char *);
extern int qqbot_u_strip_at_mention(const char *);
extern int qqbot_u_is_dm_allowed(const char *);
extern int qqbot_u_is_dm_intake_allowed(const char *);
extern int qqbot_u_is_group_allowed(const char *);
extern int qqbot_u_entry_matches(const char *);
extern int qqbot_u_parse_qq_timestamp(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_qqbot_check_qq_requirements(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_check_qq_requirements(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_check_qq_requirements"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_coerce_list(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_coerce_list(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_coerce_list"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_log_tag(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_log_tag(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_log_tag"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_fail_pending(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_fail_pending(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_fail_pending"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_mark_transport_disconnected(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_mark_transport_disconnected(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_mark_transport_disconnected"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_ensure_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_ensure_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_ensure_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_open_ws(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_open_ws(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_open_ws"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_listen_loop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_listen_loop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_listen_loop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_reconnect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_reconnect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_reconnect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_read_events(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_read_events(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_read_events"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_heartbeat_loop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_heartbeat_loop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_heartbeat_loop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_send_identify(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_send_identify(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_send_identify"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_send_resume(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_send_resume(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_send_resume"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_dispatch_payload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_dispatch_payload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_dispatch_payload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_handle_ready(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_handle_ready(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_handle_ready"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_parse_json(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_parse_json(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_parse_json"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_next_msg_seq(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_next_msg_seq(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_next_msg_seq"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_on_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_on_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_on_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_set_interaction_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_set_interaction_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_set_interaction_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_on_interaction(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_on_interaction(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_on_interaction"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_acknowledge_interaction(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_acknowledge_interaction(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_acknowledge_interaction"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_parse_gateway_session_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_parse_gateway_session_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_parse_gateway_session_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_is_authorized_interaction_for_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_is_authorized_interaction_for_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_is_authorized_interaction_for_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_default_interaction_dispatch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_default_interaction_dispatch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_default_interaction_dispatch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_write_update_response(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_write_update_response(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_write_update_response"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_handle_c2c_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_handle_c2c_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_handle_c2c_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_handle_group_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_handle_group_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_handle_group_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_handle_guild_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_handle_guild_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_handle_guild_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_handle_dm_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_handle_dm_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_handle_dm_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_process_quoted_context(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_process_quoted_context(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_process_quoted_context"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_merge_quote_into(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_merge_quote_into(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_merge_quote_into"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_detect_message_type(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_detect_message_type(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_detect_message_type"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_process_attachments(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_process_attachments(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_process_attachments"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_download_and_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_download_and_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_download_and_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_is_voice_content_type(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_is_voice_content_type(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_is_voice_content_type"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_qq_media_headers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_qq_media_headers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_qq_media_headers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_stt_voice_attachment(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_stt_voice_attachment(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_stt_voice_attachment"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_convert_audio_to_wav_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_convert_audio_to_wav_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_convert_audio_to_wav_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_guess_ext_from_data(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_guess_ext_from_data(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_guess_ext_from_data"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_looks_like_silk(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_looks_like_silk(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_looks_like_silk"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_convert_silk_to_wav(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_convert_silk_to_wav(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_convert_silk_to_wav"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_convert_raw_to_wav(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_convert_raw_to_wav(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_convert_raw_to_wav"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_convert_ffmpeg_to_wav(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_convert_ffmpeg_to_wav(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_convert_ffmpeg_to_wav"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_resolve_stt_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_resolve_stt_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_resolve_stt_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_call_stt(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_call_stt(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_call_stt"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_convert_audio_to_wav(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_convert_audio_to_wav(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_convert_audio_to_wav"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_api_request(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_api_request(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_api_request"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_upload_media(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_upload_media(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_upload_media"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_wait_for_reconnection(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_wait_for_reconnection(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_wait_for_reconnection"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_send_chunk(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_send_chunk(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_send_chunk"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_send_c2c_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_send_c2c_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_send_c2c_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_send_group_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_send_group_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_send_group_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_send_guild_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_send_guild_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_send_guild_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_send_approval_request(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_send_approval_request(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_send_approval_request"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_send_exec_approval(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_send_exec_approval(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_send_exec_approval"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_build_text_body(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_build_text_body(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_build_text_body"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_send_media(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_send_media(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_send_media"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_upload_local_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_upload_local_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_upload_local_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_load_media(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_load_media(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_load_media"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_is_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_is_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_is_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_guess_chat_type(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_guess_chat_type(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_guess_chat_type"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_strip_at_mention(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_strip_at_mention(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_strip_at_mention"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_is_dm_allowed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_is_dm_allowed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_is_dm_allowed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_is_dm_intake_allowed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_is_dm_intake_allowed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_is_dm_intake_allowed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_is_group_allowed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_is_group_allowed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_is_group_allowed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_entry_matches(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_entry_matches(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_entry_matches"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_qqbot_u_parse_qq_timestamp(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)qqbot_u_parse_qq_timestamp(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("qqbot_u_parse_qq_timestamp"));
    json_set(o, "out", json_int(v)); return o;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL; json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }
    int n = json_array_size(root);
    for (int i = 0; i < n; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "qqbot_check_qq_requirements") == 0) o = emit_qqbot_check_qq_requirements(c);
        if (strcmp(op, "qqbot_u_coerce_list") == 0) o = emit_qqbot_u_coerce_list(c);
        if (strcmp(op, "qqbot_u_log_tag") == 0) o = emit_qqbot_u_log_tag(c);
        if (strcmp(op, "qqbot_u_fail_pending") == 0) o = emit_qqbot_u_fail_pending(c);
        if (strcmp(op, "qqbot_u_mark_transport_disconnected") == 0) o = emit_qqbot_u_mark_transport_disconnected(c);
        if (strcmp(op, "qqbot_u_ensure_token") == 0) o = emit_qqbot_u_ensure_token(c);
        if (strcmp(op, "qqbot_u_open_ws") == 0) o = emit_qqbot_u_open_ws(c);
        if (strcmp(op, "qqbot_u_listen_loop") == 0) o = emit_qqbot_u_listen_loop(c);
        if (strcmp(op, "qqbot_u_reconnect") == 0) o = emit_qqbot_u_reconnect(c);
        if (strcmp(op, "qqbot_u_read_events") == 0) o = emit_qqbot_u_read_events(c);
        if (strcmp(op, "qqbot_u_heartbeat_loop") == 0) o = emit_qqbot_u_heartbeat_loop(c);
        if (strcmp(op, "qqbot_u_send_identify") == 0) o = emit_qqbot_u_send_identify(c);
        if (strcmp(op, "qqbot_u_send_resume") == 0) o = emit_qqbot_u_send_resume(c);
        if (strcmp(op, "qqbot_u_dispatch_payload") == 0) o = emit_qqbot_u_dispatch_payload(c);
        if (strcmp(op, "qqbot_u_handle_ready") == 0) o = emit_qqbot_u_handle_ready(c);
        if (strcmp(op, "qqbot_u_parse_json") == 0) o = emit_qqbot_u_parse_json(c);
        if (strcmp(op, "qqbot_u_next_msg_seq") == 0) o = emit_qqbot_u_next_msg_seq(c);
        if (strcmp(op, "qqbot_u_on_message") == 0) o = emit_qqbot_u_on_message(c);
        if (strcmp(op, "qqbot_set_interaction_callback") == 0) o = emit_qqbot_set_interaction_callback(c);
        if (strcmp(op, "qqbot_u_on_interaction") == 0) o = emit_qqbot_u_on_interaction(c);
        if (strcmp(op, "qqbot_u_acknowledge_interaction") == 0) o = emit_qqbot_u_acknowledge_interaction(c);
        if (strcmp(op, "qqbot_u_parse_gateway_session_key") == 0) o = emit_qqbot_u_parse_gateway_session_key(c);
        if (strcmp(op, "qqbot_u_is_authorized_interaction_for_session") == 0) o = emit_qqbot_u_is_authorized_interaction_for_session(c);
        if (strcmp(op, "qqbot_u_default_interaction_dispatch") == 0) o = emit_qqbot_u_default_interaction_dispatch(c);
        if (strcmp(op, "qqbot_u_write_update_response") == 0) o = emit_qqbot_u_write_update_response(c);
        if (strcmp(op, "qqbot_u_handle_c2c_message") == 0) o = emit_qqbot_u_handle_c2c_message(c);
        if (strcmp(op, "qqbot_u_handle_group_message") == 0) o = emit_qqbot_u_handle_group_message(c);
        if (strcmp(op, "qqbot_u_handle_guild_message") == 0) o = emit_qqbot_u_handle_guild_message(c);
        if (strcmp(op, "qqbot_u_handle_dm_message") == 0) o = emit_qqbot_u_handle_dm_message(c);
        if (strcmp(op, "qqbot_u_process_quoted_context") == 0) o = emit_qqbot_u_process_quoted_context(c);
        if (strcmp(op, "qqbot_u_merge_quote_into") == 0) o = emit_qqbot_u_merge_quote_into(c);
        if (strcmp(op, "qqbot_u_detect_message_type") == 0) o = emit_qqbot_u_detect_message_type(c);
        if (strcmp(op, "qqbot_u_process_attachments") == 0) o = emit_qqbot_u_process_attachments(c);
        if (strcmp(op, "qqbot_u_download_and_cache") == 0) o = emit_qqbot_u_download_and_cache(c);
        if (strcmp(op, "qqbot_u_is_voice_content_type") == 0) o = emit_qqbot_u_is_voice_content_type(c);
        if (strcmp(op, "qqbot_u_qq_media_headers") == 0) o = emit_qqbot_u_qq_media_headers(c);
        if (strcmp(op, "qqbot_u_stt_voice_attachment") == 0) o = emit_qqbot_u_stt_voice_attachment(c);
        if (strcmp(op, "qqbot_u_convert_audio_to_wav_file") == 0) o = emit_qqbot_u_convert_audio_to_wav_file(c);
        if (strcmp(op, "qqbot_u_guess_ext_from_data") == 0) o = emit_qqbot_u_guess_ext_from_data(c);
        if (strcmp(op, "qqbot_u_looks_like_silk") == 0) o = emit_qqbot_u_looks_like_silk(c);
        if (strcmp(op, "qqbot_u_convert_silk_to_wav") == 0) o = emit_qqbot_u_convert_silk_to_wav(c);
        if (strcmp(op, "qqbot_u_convert_raw_to_wav") == 0) o = emit_qqbot_u_convert_raw_to_wav(c);
        if (strcmp(op, "qqbot_u_convert_ffmpeg_to_wav") == 0) o = emit_qqbot_u_convert_ffmpeg_to_wav(c);
        if (strcmp(op, "qqbot_u_resolve_stt_config") == 0) o = emit_qqbot_u_resolve_stt_config(c);
        if (strcmp(op, "qqbot_u_call_stt") == 0) o = emit_qqbot_u_call_stt(c);
        if (strcmp(op, "qqbot_u_convert_audio_to_wav") == 0) o = emit_qqbot_u_convert_audio_to_wav(c);
        if (strcmp(op, "qqbot_u_api_request") == 0) o = emit_qqbot_u_api_request(c);
        if (strcmp(op, "qqbot_u_upload_media") == 0) o = emit_qqbot_u_upload_media(c);
        if (strcmp(op, "qqbot_u_wait_for_reconnection") == 0) o = emit_qqbot_u_wait_for_reconnection(c);
        if (strcmp(op, "qqbot_u_send_chunk") == 0) o = emit_qqbot_u_send_chunk(c);
        if (strcmp(op, "qqbot_u_send_c2c_text") == 0) o = emit_qqbot_u_send_c2c_text(c);
        if (strcmp(op, "qqbot_u_send_group_text") == 0) o = emit_qqbot_u_send_group_text(c);
        if (strcmp(op, "qqbot_u_send_guild_text") == 0) o = emit_qqbot_u_send_guild_text(c);
        if (strcmp(op, "qqbot_send_approval_request") == 0) o = emit_qqbot_send_approval_request(c);
        if (strcmp(op, "qqbot_send_exec_approval") == 0) o = emit_qqbot_send_exec_approval(c);
        if (strcmp(op, "qqbot_u_build_text_body") == 0) o = emit_qqbot_u_build_text_body(c);
        if (strcmp(op, "qqbot_u_send_media") == 0) o = emit_qqbot_u_send_media(c);
        if (strcmp(op, "qqbot_u_upload_local_file") == 0) o = emit_qqbot_u_upload_local_file(c);
        if (strcmp(op, "qqbot_u_load_media") == 0) o = emit_qqbot_u_load_media(c);
        if (strcmp(op, "qqbot_u_is_url") == 0) o = emit_qqbot_u_is_url(c);
        if (strcmp(op, "qqbot_u_guess_chat_type") == 0) o = emit_qqbot_u_guess_chat_type(c);
        if (strcmp(op, "qqbot_u_strip_at_mention") == 0) o = emit_qqbot_u_strip_at_mention(c);
        if (strcmp(op, "qqbot_u_is_dm_allowed") == 0) o = emit_qqbot_u_is_dm_allowed(c);
        if (strcmp(op, "qqbot_u_is_dm_intake_allowed") == 0) o = emit_qqbot_u_is_dm_intake_allowed(c);
        if (strcmp(op, "qqbot_u_is_group_allowed") == 0) o = emit_qqbot_u_is_group_allowed(c);
        if (strcmp(op, "qqbot_u_entry_matches") == 0) o = emit_qqbot_u_entry_matches(c);
        if (strcmp(op, "qqbot_u_parse_qq_timestamp") == 0) o = emit_qqbot_u_parse_qq_timestamp(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
