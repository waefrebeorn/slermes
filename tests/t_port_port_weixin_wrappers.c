/* AUTO-GENERATED integration oracle harness for port_weixin_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_weixin_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int wx_u_make_ssl_connector(const char *);
extern int wx_save_weixin_account(const char *);
extern int wx_load_weixin_account(const char *);
extern int wx_u_api_get(const char *);
extern int wx_u_get_config(const char *);
extern int wx_u_get_upload_url(const char *);
extern int wx_u_upload_ciphertext(const char *);
extern int wx_u_download_bytes(const char *);
extern int wx_u_download_and_decrypt_media(const char *);
extern int wx_u_save_sync_buf(const char *);
extern int wx_qr_login(const char *);
extern int wx_u_poll_loop(const char *);
extern int wx_u_process_message_safe(const char *);
extern int wx_u_is_dm_intake_allowed(const char *);
extern int wx_u_text_batch_key(const char *);
extern int wx_u_enqueue_text_event(const char *);
extern int wx_u_flush_text_batch(const char *);
extern int wx_u_collect_media(const char *);
extern int wx_u_download_image(const char *);
extern int wx_u_download_video(const char *);
extern int wx_u_download_voice(const char *);
extern int wx_u_maybe_fetch_typing_ticket(const char *);
extern int wx_u_split_text(const char *);
extern int wx_u_open_rate_limit_circuit(const char *);
extern int wx_u_record_rate_limit_event(const char *);
extern int wx_u_reset_rate_limit_circuit(const char *);
extern int wx_u_send_text_chunk(const char *);
extern int wx_u_send_text_chunk_locked(const char *);
extern int wx_u_ensure_typing_ticket(const char *);
extern int wx_u_download_remote_media(const char *);
extern int wx_u_outbound_media_builder(const char *);
extern int wx_send_weixin_direct(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_wx_u_make_ssl_connector(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_make_ssl_connector(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_make_ssl_connector"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_save_weixin_account(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_save_weixin_account(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_save_weixin_account"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_load_weixin_account(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_load_weixin_account(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_load_weixin_account"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_api_get(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_api_get(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_api_get"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_get_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_get_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_get_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_get_upload_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_get_upload_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_get_upload_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_upload_ciphertext(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_upload_ciphertext(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_upload_ciphertext"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_download_bytes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_download_bytes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_download_bytes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_download_and_decrypt_media(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_download_and_decrypt_media(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_download_and_decrypt_media"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_save_sync_buf(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_save_sync_buf(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_save_sync_buf"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_qr_login(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_qr_login(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_qr_login"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_poll_loop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_poll_loop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_poll_loop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_process_message_safe(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_process_message_safe(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_process_message_safe"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_is_dm_intake_allowed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_is_dm_intake_allowed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_is_dm_intake_allowed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_text_batch_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_text_batch_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_text_batch_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_enqueue_text_event(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_enqueue_text_event(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_enqueue_text_event"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_flush_text_batch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_flush_text_batch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_flush_text_batch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_collect_media(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_collect_media(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_collect_media"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_download_image(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_download_image(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_download_image"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_download_video(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_download_video(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_download_video"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_download_voice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_download_voice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_download_voice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_maybe_fetch_typing_ticket(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_maybe_fetch_typing_ticket(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_maybe_fetch_typing_ticket"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_split_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_split_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_split_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_open_rate_limit_circuit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_open_rate_limit_circuit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_open_rate_limit_circuit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_record_rate_limit_event(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_record_rate_limit_event(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_record_rate_limit_event"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_reset_rate_limit_circuit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_reset_rate_limit_circuit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_reset_rate_limit_circuit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_send_text_chunk(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_send_text_chunk(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_send_text_chunk"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_send_text_chunk_locked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_send_text_chunk_locked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_send_text_chunk_locked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_ensure_typing_ticket(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_ensure_typing_ticket(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_ensure_typing_ticket"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_download_remote_media(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_download_remote_media(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_download_remote_media"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_u_outbound_media_builder(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_u_outbound_media_builder(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_u_outbound_media_builder"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wx_send_weixin_direct(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wx_send_weixin_direct(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wx_send_weixin_direct"));
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
        if (strcmp(op, "wx_u_make_ssl_connector") == 0) o = emit_wx_u_make_ssl_connector(c);
        if (strcmp(op, "wx_save_weixin_account") == 0) o = emit_wx_save_weixin_account(c);
        if (strcmp(op, "wx_load_weixin_account") == 0) o = emit_wx_load_weixin_account(c);
        if (strcmp(op, "wx_u_api_get") == 0) o = emit_wx_u_api_get(c);
        if (strcmp(op, "wx_u_get_config") == 0) o = emit_wx_u_get_config(c);
        if (strcmp(op, "wx_u_get_upload_url") == 0) o = emit_wx_u_get_upload_url(c);
        if (strcmp(op, "wx_u_upload_ciphertext") == 0) o = emit_wx_u_upload_ciphertext(c);
        if (strcmp(op, "wx_u_download_bytes") == 0) o = emit_wx_u_download_bytes(c);
        if (strcmp(op, "wx_u_download_and_decrypt_media") == 0) o = emit_wx_u_download_and_decrypt_media(c);
        if (strcmp(op, "wx_u_save_sync_buf") == 0) o = emit_wx_u_save_sync_buf(c);
        if (strcmp(op, "wx_qr_login") == 0) o = emit_wx_qr_login(c);
        if (strcmp(op, "wx_u_poll_loop") == 0) o = emit_wx_u_poll_loop(c);
        if (strcmp(op, "wx_u_process_message_safe") == 0) o = emit_wx_u_process_message_safe(c);
        if (strcmp(op, "wx_u_is_dm_intake_allowed") == 0) o = emit_wx_u_is_dm_intake_allowed(c);
        if (strcmp(op, "wx_u_text_batch_key") == 0) o = emit_wx_u_text_batch_key(c);
        if (strcmp(op, "wx_u_enqueue_text_event") == 0) o = emit_wx_u_enqueue_text_event(c);
        if (strcmp(op, "wx_u_flush_text_batch") == 0) o = emit_wx_u_flush_text_batch(c);
        if (strcmp(op, "wx_u_collect_media") == 0) o = emit_wx_u_collect_media(c);
        if (strcmp(op, "wx_u_download_image") == 0) o = emit_wx_u_download_image(c);
        if (strcmp(op, "wx_u_download_video") == 0) o = emit_wx_u_download_video(c);
        if (strcmp(op, "wx_u_download_voice") == 0) o = emit_wx_u_download_voice(c);
        if (strcmp(op, "wx_u_maybe_fetch_typing_ticket") == 0) o = emit_wx_u_maybe_fetch_typing_ticket(c);
        if (strcmp(op, "wx_u_split_text") == 0) o = emit_wx_u_split_text(c);
        if (strcmp(op, "wx_u_open_rate_limit_circuit") == 0) o = emit_wx_u_open_rate_limit_circuit(c);
        if (strcmp(op, "wx_u_record_rate_limit_event") == 0) o = emit_wx_u_record_rate_limit_event(c);
        if (strcmp(op, "wx_u_reset_rate_limit_circuit") == 0) o = emit_wx_u_reset_rate_limit_circuit(c);
        if (strcmp(op, "wx_u_send_text_chunk") == 0) o = emit_wx_u_send_text_chunk(c);
        if (strcmp(op, "wx_u_send_text_chunk_locked") == 0) o = emit_wx_u_send_text_chunk_locked(c);
        if (strcmp(op, "wx_u_ensure_typing_ticket") == 0) o = emit_wx_u_ensure_typing_ticket(c);
        if (strcmp(op, "wx_u_download_remote_media") == 0) o = emit_wx_u_download_remote_media(c);
        if (strcmp(op, "wx_u_outbound_media_builder") == 0) o = emit_wx_u_outbound_media_builder(c);
        if (strcmp(op, "wx_send_weixin_direct") == 0) o = emit_wx_send_weixin_direct(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
