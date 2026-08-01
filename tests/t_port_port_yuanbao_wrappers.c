/* AUTO-GENERATED integration oracle harness for port_yuanbao_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_yuanbao_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yb_u__repr__(const char *);
extern int yb_use_before(const char *);
extern int yb_use_after(const char *);
extern int yb_middleware_names(const char *);
extern int yb_convert_json_msg_body(const char *);
extern int yb_parse_json_push(const char *);
extern int yb_u_decode_single(const char *);
extern int yb_u_handle_recall(const char *);
extern int yb_u_find_processing_session(const char *);
extern int yb_u_interrupt_for_recall(const char *);
extern int yb_u_schedule_content_redact(const char *);
extern int yb_u_patch_transcript(const char *);
extern int yb_u_is_self_reference(const char *);
extern int yb_is_dm_allowed(const char *);
extern int yb_is_dm_intake_allowed(const char *);
extern int yb_is_group_allowed(const char *);
extern int yb_dm_policy(const char *);
extern int yb_group_policy(const char *);
extern int yb_u_format_shared_link(const char *);
extern int yb_u_format_link_understanding(const char *);
extern int yb_u_parse_resource_id(const char *);
extern int yb_u_rewrite_slash_command(const char *);
extern int yb_u_extract_inbound_media_refs(const char *);
extern int yb_u_extract_link_urls(const char *);
extern int yb_u_extract_forwarded_records(const char *);
extern int yb_is_skippable_placeholder(const char *);
extern int yb_u_rewrite_slash_command_2(const char *);
extern int yb_u_detect_owner_command(const char *);
extern int yb_u_is_at_bot(const char *);
extern int yb_u_extract_bot_mention_text(const char *);
extern int yb_u_build_group_channel_prompt(const char *);
extern int yb_u_observe_group_message(const char *);
extern int yb_u_extract_quote_context(const char *);
extern int yb_u_extract_media_refs_from_transcript(const char *);
extern int yb_u_send_loading_heartbeat(const char *);
extern int yb_u_media_marker(const char *);
extern int yb_u_walk_forward_msgs(const char *);
extern int yb_build_forward_text(const char *);
extern int yb_u_get_cached_resource(const char *);
extern int yb_u_put_cached_resource(const char *);
extern int yb_u_append_cached_resource(const char *);
extern int yb_u_guess_image_ext_from_url(const char *);
extern int yb_u_fetch_resource_url(const char *);
extern int yb_u_resolve_download_url(const char *);
extern int yb_u_download_and_cache(const char *);
extern int yb_u_resolve_media_urls(const char *);
extern int yb_u_resolve_ybres_refs(const char *);
extern int yb_u_collect_observed_media(const char *);
extern int yb_u_resolve_quote_media(const char *);
extern int yb_u_collect_quote_local_media(const char *);
extern int yb_u_consume_group_queue(const char *);
extern int yb_build(const char *);
extern int yb_connect_id(const char *);
extern int yb_reconnect_attempts(const char *);
extern int yb_u_extract_connect_id(const char *);
extern int yb_u_heartbeat_loop(const char *);
extern int yb_u_receive_loop(const char *);
extern int yb_u_extract_sender_key(const char *);
extern int yb_u_push_to_inbound(const char *);
extern int yb_u_flush_inbound_buffer(const char *);
extern int yb_send_biz_request(const char *);
extern int yb_schedule_reconnect(const char *);
extern int yb_u_reconnect_with_backoff(const char *);
extern int yb_u_do_reconnect(const char *);
extern int yb_u_cleanup_ws(const char *);
extern int yb_acquire_file(const char *);
extern int yb_build_msg_body(const char *);
extern int yb_needs_cos_upload(const char *);
extern int yb_acquire_file_2(const char *);
extern int yb_build_msg_body_2(const char *);
extern int yb_acquire_file_3(const char *);
extern int yb_build_msg_body_3(const char *);
extern int yb_acquire_file_4(const char *);
extern int yb_build_msg_body_4(const char *);
extern int yb_acquire_file_5(const char *);
extern int yb_build_msg_body_5(const char *);
extern int yb_needs_cos_upload_2(const char *);
extern int yb_acquire_file_6(const char *);
extern int yb_build_msg_body_6(const char *);
extern int yb_query_group_info_raw(const char *);
extern int yb_get_group_member_list_raw(const char *);
extern int yb_query_session_members(const char *);
extern int yb_send_heartbeat_once(const char *);
extern int yb_u_worker(const char *);
extern int yb_u_notifier(const char *);
extern int yb_cancel(const char *);
extern int yb_register_handler(const char *);
extern int yb_get_chat_lock(const char *);
extern int yb_send_media(const char *);
extern int yb_send_direct(const char *);
extern int yb_dispatch_msg_body(const char *);
extern int yb_send_text_chunk(const char *);
extern int yb_send_c2c_message(const char *);
extern int yb_send_group_message(const char *);
extern int yb_u_build_msg_body_with_mentions(const char *);
extern int yb_send_c2c_msg_body(const char *);
extern int yb_send_group_msg_body(const char *);
extern int yb_u_dispatch_encoded(const char *);
extern int yb_validate_media(const char *);
extern int yb_strip_cron_wrapper(const char *);
extern int yb_u_handle_send_start(const char *);
extern int yb_u_handle_send_finish(const char *);
extern int yb_send_media_2(const char *);
extern int yb_send_direct_2(const char *);
extern int yb_start_typing(const char *);
extern int yb_start_slow_notifier(const char *);
extern int yb_cancel_slow_notifier(const char *);
extern int yb_get_chat_lock_2(const char *);
extern int yb_u_chat_locks(const char *);
extern int yb_validate_media_2(const char *);
extern int yb_set_active(const char *);
extern int yb_u_track_task(const char *);
extern int yb_u_sender_may_designate_home(const char *);
extern int yb_u_process_message_background(const char *);
extern int yb_u_get_cached_token(const char *);
extern int yb_send_yuanbao_direct(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_yb_u__repr__(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u__repr__(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u__repr__"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_use_before(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_use_before(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_use_before"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_use_after(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_use_after(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_use_after"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_middleware_names(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_middleware_names(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_middleware_names"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_convert_json_msg_body(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_convert_json_msg_body(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_convert_json_msg_body"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_parse_json_push(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_parse_json_push(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_parse_json_push"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_decode_single(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_decode_single(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_decode_single"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_handle_recall(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_handle_recall(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_handle_recall"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_find_processing_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_find_processing_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_find_processing_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_interrupt_for_recall(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_interrupt_for_recall(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_interrupt_for_recall"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_schedule_content_redact(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_schedule_content_redact(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_schedule_content_redact"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_patch_transcript(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_patch_transcript(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_patch_transcript"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_is_self_reference(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_is_self_reference(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_is_self_reference"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_is_dm_allowed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_is_dm_allowed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_is_dm_allowed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_is_dm_intake_allowed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_is_dm_intake_allowed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_is_dm_intake_allowed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_is_group_allowed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_is_group_allowed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_is_group_allowed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_dm_policy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_dm_policy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_dm_policy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_group_policy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_group_policy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_group_policy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_format_shared_link(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_format_shared_link(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_format_shared_link"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_format_link_understanding(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_format_link_understanding(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_format_link_understanding"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_parse_resource_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_parse_resource_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_parse_resource_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_rewrite_slash_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_rewrite_slash_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_rewrite_slash_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_extract_inbound_media_refs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_extract_inbound_media_refs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_extract_inbound_media_refs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_extract_link_urls(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_extract_link_urls(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_extract_link_urls"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_extract_forwarded_records(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_extract_forwarded_records(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_extract_forwarded_records"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_is_skippable_placeholder(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_is_skippable_placeholder(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_is_skippable_placeholder"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_rewrite_slash_command_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_rewrite_slash_command_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_rewrite_slash_command_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_detect_owner_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_detect_owner_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_detect_owner_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_is_at_bot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_is_at_bot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_is_at_bot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_extract_bot_mention_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_extract_bot_mention_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_extract_bot_mention_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_build_group_channel_prompt(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_build_group_channel_prompt(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_build_group_channel_prompt"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_observe_group_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_observe_group_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_observe_group_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_extract_quote_context(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_extract_quote_context(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_extract_quote_context"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_extract_media_refs_from_transcript(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_extract_media_refs_from_transcript(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_extract_media_refs_from_transcript"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_send_loading_heartbeat(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_send_loading_heartbeat(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_send_loading_heartbeat"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_media_marker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_media_marker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_media_marker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_walk_forward_msgs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_walk_forward_msgs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_walk_forward_msgs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_build_forward_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_build_forward_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_build_forward_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_get_cached_resource(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_get_cached_resource(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_get_cached_resource"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_put_cached_resource(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_put_cached_resource(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_put_cached_resource"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_append_cached_resource(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_append_cached_resource(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_append_cached_resource"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_guess_image_ext_from_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_guess_image_ext_from_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_guess_image_ext_from_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_fetch_resource_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_fetch_resource_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_fetch_resource_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_resolve_download_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_resolve_download_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_resolve_download_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_download_and_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_download_and_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_download_and_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_resolve_media_urls(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_resolve_media_urls(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_resolve_media_urls"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_resolve_ybres_refs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_resolve_ybres_refs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_resolve_ybres_refs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_collect_observed_media(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_collect_observed_media(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_collect_observed_media"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_resolve_quote_media(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_resolve_quote_media(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_resolve_quote_media"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_collect_quote_local_media(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_collect_quote_local_media(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_collect_quote_local_media"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_consume_group_queue(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_consume_group_queue(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_consume_group_queue"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_build(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_build(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_build"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_connect_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_connect_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_connect_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_reconnect_attempts(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_reconnect_attempts(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_reconnect_attempts"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_extract_connect_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_extract_connect_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_extract_connect_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_heartbeat_loop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_heartbeat_loop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_heartbeat_loop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_receive_loop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_receive_loop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_receive_loop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_extract_sender_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_extract_sender_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_extract_sender_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_push_to_inbound(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_push_to_inbound(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_push_to_inbound"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_flush_inbound_buffer(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_flush_inbound_buffer(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_flush_inbound_buffer"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_send_biz_request(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_send_biz_request(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_send_biz_request"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_schedule_reconnect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_schedule_reconnect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_schedule_reconnect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_reconnect_with_backoff(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_reconnect_with_backoff(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_reconnect_with_backoff"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_do_reconnect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_do_reconnect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_do_reconnect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_cleanup_ws(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_cleanup_ws(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_cleanup_ws"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_acquire_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_acquire_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_acquire_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_build_msg_body(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_build_msg_body(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_build_msg_body"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_needs_cos_upload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_needs_cos_upload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_needs_cos_upload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_acquire_file_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_acquire_file_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_acquire_file_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_build_msg_body_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_build_msg_body_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_build_msg_body_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_acquire_file_3(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_acquire_file_3(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_acquire_file_3"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_build_msg_body_3(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_build_msg_body_3(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_build_msg_body_3"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_acquire_file_4(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_acquire_file_4(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_acquire_file_4"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_build_msg_body_4(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_build_msg_body_4(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_build_msg_body_4"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_acquire_file_5(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_acquire_file_5(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_acquire_file_5"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_build_msg_body_5(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_build_msg_body_5(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_build_msg_body_5"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_needs_cos_upload_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_needs_cos_upload_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_needs_cos_upload_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_acquire_file_6(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_acquire_file_6(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_acquire_file_6"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_build_msg_body_6(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_build_msg_body_6(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_build_msg_body_6"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_query_group_info_raw(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_query_group_info_raw(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_query_group_info_raw"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_get_group_member_list_raw(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_get_group_member_list_raw(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_get_group_member_list_raw"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_query_session_members(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_query_session_members(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_query_session_members"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_send_heartbeat_once(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_send_heartbeat_once(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_send_heartbeat_once"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_worker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_worker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_worker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_notifier(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_notifier(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_notifier"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_cancel(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_cancel(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_cancel"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_register_handler(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_register_handler(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_register_handler"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_get_chat_lock(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_get_chat_lock(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_get_chat_lock"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_send_media(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_send_media(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_send_media"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_send_direct(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_send_direct(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_send_direct"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_dispatch_msg_body(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_dispatch_msg_body(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_dispatch_msg_body"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_send_text_chunk(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_send_text_chunk(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_send_text_chunk"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_send_c2c_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_send_c2c_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_send_c2c_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_send_group_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_send_group_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_send_group_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_build_msg_body_with_mentions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_build_msg_body_with_mentions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_build_msg_body_with_mentions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_send_c2c_msg_body(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_send_c2c_msg_body(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_send_c2c_msg_body"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_send_group_msg_body(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_send_group_msg_body(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_send_group_msg_body"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_dispatch_encoded(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_dispatch_encoded(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_dispatch_encoded"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_validate_media(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_validate_media(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_validate_media"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_strip_cron_wrapper(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_strip_cron_wrapper(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_strip_cron_wrapper"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_handle_send_start(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_handle_send_start(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_handle_send_start"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_handle_send_finish(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_handle_send_finish(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_handle_send_finish"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_send_media_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_send_media_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_send_media_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_send_direct_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_send_direct_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_send_direct_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_start_typing(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_start_typing(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_start_typing"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_start_slow_notifier(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_start_slow_notifier(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_start_slow_notifier"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_cancel_slow_notifier(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_cancel_slow_notifier(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_cancel_slow_notifier"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_get_chat_lock_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_get_chat_lock_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_get_chat_lock_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_chat_locks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_chat_locks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_chat_locks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_validate_media_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_validate_media_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_validate_media_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_set_active(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_set_active(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_set_active"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_track_task(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_track_task(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_track_task"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_sender_may_designate_home(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_sender_may_designate_home(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_sender_may_designate_home"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_process_message_background(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_process_message_background(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_process_message_background"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_u_get_cached_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_u_get_cached_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_u_get_cached_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_yb_send_yuanbao_direct(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)yb_send_yuanbao_direct(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("yb_send_yuanbao_direct"));
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
        if (strcmp(op, "yb_u__repr__") == 0) o = emit_yb_u__repr__(c);
        if (strcmp(op, "yb_use_before") == 0) o = emit_yb_use_before(c);
        if (strcmp(op, "yb_use_after") == 0) o = emit_yb_use_after(c);
        if (strcmp(op, "yb_middleware_names") == 0) o = emit_yb_middleware_names(c);
        if (strcmp(op, "yb_convert_json_msg_body") == 0) o = emit_yb_convert_json_msg_body(c);
        if (strcmp(op, "yb_parse_json_push") == 0) o = emit_yb_parse_json_push(c);
        if (strcmp(op, "yb_u_decode_single") == 0) o = emit_yb_u_decode_single(c);
        if (strcmp(op, "yb_u_handle_recall") == 0) o = emit_yb_u_handle_recall(c);
        if (strcmp(op, "yb_u_find_processing_session") == 0) o = emit_yb_u_find_processing_session(c);
        if (strcmp(op, "yb_u_interrupt_for_recall") == 0) o = emit_yb_u_interrupt_for_recall(c);
        if (strcmp(op, "yb_u_schedule_content_redact") == 0) o = emit_yb_u_schedule_content_redact(c);
        if (strcmp(op, "yb_u_patch_transcript") == 0) o = emit_yb_u_patch_transcript(c);
        if (strcmp(op, "yb_u_is_self_reference") == 0) o = emit_yb_u_is_self_reference(c);
        if (strcmp(op, "yb_is_dm_allowed") == 0) o = emit_yb_is_dm_allowed(c);
        if (strcmp(op, "yb_is_dm_intake_allowed") == 0) o = emit_yb_is_dm_intake_allowed(c);
        if (strcmp(op, "yb_is_group_allowed") == 0) o = emit_yb_is_group_allowed(c);
        if (strcmp(op, "yb_dm_policy") == 0) o = emit_yb_dm_policy(c);
        if (strcmp(op, "yb_group_policy") == 0) o = emit_yb_group_policy(c);
        if (strcmp(op, "yb_u_format_shared_link") == 0) o = emit_yb_u_format_shared_link(c);
        if (strcmp(op, "yb_u_format_link_understanding") == 0) o = emit_yb_u_format_link_understanding(c);
        if (strcmp(op, "yb_u_parse_resource_id") == 0) o = emit_yb_u_parse_resource_id(c);
        if (strcmp(op, "yb_u_rewrite_slash_command") == 0) o = emit_yb_u_rewrite_slash_command(c);
        if (strcmp(op, "yb_u_extract_inbound_media_refs") == 0) o = emit_yb_u_extract_inbound_media_refs(c);
        if (strcmp(op, "yb_u_extract_link_urls") == 0) o = emit_yb_u_extract_link_urls(c);
        if (strcmp(op, "yb_u_extract_forwarded_records") == 0) o = emit_yb_u_extract_forwarded_records(c);
        if (strcmp(op, "yb_is_skippable_placeholder") == 0) o = emit_yb_is_skippable_placeholder(c);
        if (strcmp(op, "yb_u_rewrite_slash_command_2") == 0) o = emit_yb_u_rewrite_slash_command_2(c);
        if (strcmp(op, "yb_u_detect_owner_command") == 0) o = emit_yb_u_detect_owner_command(c);
        if (strcmp(op, "yb_u_is_at_bot") == 0) o = emit_yb_u_is_at_bot(c);
        if (strcmp(op, "yb_u_extract_bot_mention_text") == 0) o = emit_yb_u_extract_bot_mention_text(c);
        if (strcmp(op, "yb_u_build_group_channel_prompt") == 0) o = emit_yb_u_build_group_channel_prompt(c);
        if (strcmp(op, "yb_u_observe_group_message") == 0) o = emit_yb_u_observe_group_message(c);
        if (strcmp(op, "yb_u_extract_quote_context") == 0) o = emit_yb_u_extract_quote_context(c);
        if (strcmp(op, "yb_u_extract_media_refs_from_transcript") == 0) o = emit_yb_u_extract_media_refs_from_transcript(c);
        if (strcmp(op, "yb_u_send_loading_heartbeat") == 0) o = emit_yb_u_send_loading_heartbeat(c);
        if (strcmp(op, "yb_u_media_marker") == 0) o = emit_yb_u_media_marker(c);
        if (strcmp(op, "yb_u_walk_forward_msgs") == 0) o = emit_yb_u_walk_forward_msgs(c);
        if (strcmp(op, "yb_build_forward_text") == 0) o = emit_yb_build_forward_text(c);
        if (strcmp(op, "yb_u_get_cached_resource") == 0) o = emit_yb_u_get_cached_resource(c);
        if (strcmp(op, "yb_u_put_cached_resource") == 0) o = emit_yb_u_put_cached_resource(c);
        if (strcmp(op, "yb_u_append_cached_resource") == 0) o = emit_yb_u_append_cached_resource(c);
        if (strcmp(op, "yb_u_guess_image_ext_from_url") == 0) o = emit_yb_u_guess_image_ext_from_url(c);
        if (strcmp(op, "yb_u_fetch_resource_url") == 0) o = emit_yb_u_fetch_resource_url(c);
        if (strcmp(op, "yb_u_resolve_download_url") == 0) o = emit_yb_u_resolve_download_url(c);
        if (strcmp(op, "yb_u_download_and_cache") == 0) o = emit_yb_u_download_and_cache(c);
        if (strcmp(op, "yb_u_resolve_media_urls") == 0) o = emit_yb_u_resolve_media_urls(c);
        if (strcmp(op, "yb_u_resolve_ybres_refs") == 0) o = emit_yb_u_resolve_ybres_refs(c);
        if (strcmp(op, "yb_u_collect_observed_media") == 0) o = emit_yb_u_collect_observed_media(c);
        if (strcmp(op, "yb_u_resolve_quote_media") == 0) o = emit_yb_u_resolve_quote_media(c);
        if (strcmp(op, "yb_u_collect_quote_local_media") == 0) o = emit_yb_u_collect_quote_local_media(c);
        if (strcmp(op, "yb_u_consume_group_queue") == 0) o = emit_yb_u_consume_group_queue(c);
        if (strcmp(op, "yb_build") == 0) o = emit_yb_build(c);
        if (strcmp(op, "yb_connect_id") == 0) o = emit_yb_connect_id(c);
        if (strcmp(op, "yb_reconnect_attempts") == 0) o = emit_yb_reconnect_attempts(c);
        if (strcmp(op, "yb_u_extract_connect_id") == 0) o = emit_yb_u_extract_connect_id(c);
        if (strcmp(op, "yb_u_heartbeat_loop") == 0) o = emit_yb_u_heartbeat_loop(c);
        if (strcmp(op, "yb_u_receive_loop") == 0) o = emit_yb_u_receive_loop(c);
        if (strcmp(op, "yb_u_extract_sender_key") == 0) o = emit_yb_u_extract_sender_key(c);
        if (strcmp(op, "yb_u_push_to_inbound") == 0) o = emit_yb_u_push_to_inbound(c);
        if (strcmp(op, "yb_u_flush_inbound_buffer") == 0) o = emit_yb_u_flush_inbound_buffer(c);
        if (strcmp(op, "yb_send_biz_request") == 0) o = emit_yb_send_biz_request(c);
        if (strcmp(op, "yb_schedule_reconnect") == 0) o = emit_yb_schedule_reconnect(c);
        if (strcmp(op, "yb_u_reconnect_with_backoff") == 0) o = emit_yb_u_reconnect_with_backoff(c);
        if (strcmp(op, "yb_u_do_reconnect") == 0) o = emit_yb_u_do_reconnect(c);
        if (strcmp(op, "yb_u_cleanup_ws") == 0) o = emit_yb_u_cleanup_ws(c);
        if (strcmp(op, "yb_acquire_file") == 0) o = emit_yb_acquire_file(c);
        if (strcmp(op, "yb_build_msg_body") == 0) o = emit_yb_build_msg_body(c);
        if (strcmp(op, "yb_needs_cos_upload") == 0) o = emit_yb_needs_cos_upload(c);
        if (strcmp(op, "yb_acquire_file_2") == 0) o = emit_yb_acquire_file_2(c);
        if (strcmp(op, "yb_build_msg_body_2") == 0) o = emit_yb_build_msg_body_2(c);
        if (strcmp(op, "yb_acquire_file_3") == 0) o = emit_yb_acquire_file_3(c);
        if (strcmp(op, "yb_build_msg_body_3") == 0) o = emit_yb_build_msg_body_3(c);
        if (strcmp(op, "yb_acquire_file_4") == 0) o = emit_yb_acquire_file_4(c);
        if (strcmp(op, "yb_build_msg_body_4") == 0) o = emit_yb_build_msg_body_4(c);
        if (strcmp(op, "yb_acquire_file_5") == 0) o = emit_yb_acquire_file_5(c);
        if (strcmp(op, "yb_build_msg_body_5") == 0) o = emit_yb_build_msg_body_5(c);
        if (strcmp(op, "yb_needs_cos_upload_2") == 0) o = emit_yb_needs_cos_upload_2(c);
        if (strcmp(op, "yb_acquire_file_6") == 0) o = emit_yb_acquire_file_6(c);
        if (strcmp(op, "yb_build_msg_body_6") == 0) o = emit_yb_build_msg_body_6(c);
        if (strcmp(op, "yb_query_group_info_raw") == 0) o = emit_yb_query_group_info_raw(c);
        if (strcmp(op, "yb_get_group_member_list_raw") == 0) o = emit_yb_get_group_member_list_raw(c);
        if (strcmp(op, "yb_query_session_members") == 0) o = emit_yb_query_session_members(c);
        if (strcmp(op, "yb_send_heartbeat_once") == 0) o = emit_yb_send_heartbeat_once(c);
        if (strcmp(op, "yb_u_worker") == 0) o = emit_yb_u_worker(c);
        if (strcmp(op, "yb_u_notifier") == 0) o = emit_yb_u_notifier(c);
        if (strcmp(op, "yb_cancel") == 0) o = emit_yb_cancel(c);
        if (strcmp(op, "yb_register_handler") == 0) o = emit_yb_register_handler(c);
        if (strcmp(op, "yb_get_chat_lock") == 0) o = emit_yb_get_chat_lock(c);
        if (strcmp(op, "yb_send_media") == 0) o = emit_yb_send_media(c);
        if (strcmp(op, "yb_send_direct") == 0) o = emit_yb_send_direct(c);
        if (strcmp(op, "yb_dispatch_msg_body") == 0) o = emit_yb_dispatch_msg_body(c);
        if (strcmp(op, "yb_send_text_chunk") == 0) o = emit_yb_send_text_chunk(c);
        if (strcmp(op, "yb_send_c2c_message") == 0) o = emit_yb_send_c2c_message(c);
        if (strcmp(op, "yb_send_group_message") == 0) o = emit_yb_send_group_message(c);
        if (strcmp(op, "yb_u_build_msg_body_with_mentions") == 0) o = emit_yb_u_build_msg_body_with_mentions(c);
        if (strcmp(op, "yb_send_c2c_msg_body") == 0) o = emit_yb_send_c2c_msg_body(c);
        if (strcmp(op, "yb_send_group_msg_body") == 0) o = emit_yb_send_group_msg_body(c);
        if (strcmp(op, "yb_u_dispatch_encoded") == 0) o = emit_yb_u_dispatch_encoded(c);
        if (strcmp(op, "yb_validate_media") == 0) o = emit_yb_validate_media(c);
        if (strcmp(op, "yb_strip_cron_wrapper") == 0) o = emit_yb_strip_cron_wrapper(c);
        if (strcmp(op, "yb_u_handle_send_start") == 0) o = emit_yb_u_handle_send_start(c);
        if (strcmp(op, "yb_u_handle_send_finish") == 0) o = emit_yb_u_handle_send_finish(c);
        if (strcmp(op, "yb_send_media_2") == 0) o = emit_yb_send_media_2(c);
        if (strcmp(op, "yb_send_direct_2") == 0) o = emit_yb_send_direct_2(c);
        if (strcmp(op, "yb_start_typing") == 0) o = emit_yb_start_typing(c);
        if (strcmp(op, "yb_start_slow_notifier") == 0) o = emit_yb_start_slow_notifier(c);
        if (strcmp(op, "yb_cancel_slow_notifier") == 0) o = emit_yb_cancel_slow_notifier(c);
        if (strcmp(op, "yb_get_chat_lock_2") == 0) o = emit_yb_get_chat_lock_2(c);
        if (strcmp(op, "yb_u_chat_locks") == 0) o = emit_yb_u_chat_locks(c);
        if (strcmp(op, "yb_validate_media_2") == 0) o = emit_yb_validate_media_2(c);
        if (strcmp(op, "yb_set_active") == 0) o = emit_yb_set_active(c);
        if (strcmp(op, "yb_u_track_task") == 0) o = emit_yb_u_track_task(c);
        if (strcmp(op, "yb_u_sender_may_designate_home") == 0) o = emit_yb_u_sender_may_designate_home(c);
        if (strcmp(op, "yb_u_process_message_background") == 0) o = emit_yb_u_process_message_background(c);
        if (strcmp(op, "yb_u_get_cached_token") == 0) o = emit_yb_u_get_cached_token(c);
        if (strcmp(op, "yb_send_yuanbao_direct") == 0) o = emit_yb_send_yuanbao_direct(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
