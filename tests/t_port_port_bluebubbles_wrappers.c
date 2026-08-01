/* AUTO-GENERATED integration oracle harness for port_bluebubbles_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_bluebubbles_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int bb_check_bluebubbles_requirements(const char *);
extern int bb_u_normalize_server_url(const char *);
extern int bb_u_api_url(const char *);
extern int bb_u_compile_mention_patterns(const char *);
extern int bb_u_message_matches_mention_patterns(const char *);
extern int bb_u_clean_mention_text(const char *);
extern int bb_u_api_post(const char *);
extern int bb_u_webhook_url(const char *);
extern int bb_u_webhook_register_url(const char *);
extern int bb_u_webhook_register_url_for_log(const char *);
extern int bb_u_find_registered_webhooks(const char *);
extern int bb_u_register_webhook(const char *);
extern int bb_u_unregister_webhook(const char *);
extern int bb_u_resolve_chat_guid(const char *);
extern int bb_u_create_chat_for_handle(const char *);
extern int bb_mark_read(const char *);
extern int bb_u_download_attachment(const char *);
extern int bb_u_extract_payload_record(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_bb_check_bluebubbles_requirements(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_check_bluebubbles_requirements(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_check_bluebubbles_requirements"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_normalize_server_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_normalize_server_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_normalize_server_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_api_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_api_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_api_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_compile_mention_patterns(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_compile_mention_patterns(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_compile_mention_patterns"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_message_matches_mention_patterns(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_message_matches_mention_patterns(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_message_matches_mention_patterns"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_clean_mention_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_clean_mention_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_clean_mention_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_api_post(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_api_post(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_api_post"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_webhook_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_webhook_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_webhook_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_webhook_register_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_webhook_register_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_webhook_register_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_webhook_register_url_for_log(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_webhook_register_url_for_log(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_webhook_register_url_for_log"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_find_registered_webhooks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_find_registered_webhooks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_find_registered_webhooks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_register_webhook(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_register_webhook(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_register_webhook"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_unregister_webhook(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_unregister_webhook(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_unregister_webhook"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_resolve_chat_guid(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_resolve_chat_guid(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_resolve_chat_guid"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_create_chat_for_handle(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_create_chat_for_handle(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_create_chat_for_handle"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_mark_read(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_mark_read(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_mark_read"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_download_attachment(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_download_attachment(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_download_attachment"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_bb_u_extract_payload_record(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)bb_u_extract_payload_record(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("bb_u_extract_payload_record"));
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
        if (strcmp(op, "bb_check_bluebubbles_requirements") == 0) o = emit_bb_check_bluebubbles_requirements(c);
        if (strcmp(op, "bb_u_normalize_server_url") == 0) o = emit_bb_u_normalize_server_url(c);
        if (strcmp(op, "bb_u_api_url") == 0) o = emit_bb_u_api_url(c);
        if (strcmp(op, "bb_u_compile_mention_patterns") == 0) o = emit_bb_u_compile_mention_patterns(c);
        if (strcmp(op, "bb_u_message_matches_mention_patterns") == 0) o = emit_bb_u_message_matches_mention_patterns(c);
        if (strcmp(op, "bb_u_clean_mention_text") == 0) o = emit_bb_u_clean_mention_text(c);
        if (strcmp(op, "bb_u_api_post") == 0) o = emit_bb_u_api_post(c);
        if (strcmp(op, "bb_u_webhook_url") == 0) o = emit_bb_u_webhook_url(c);
        if (strcmp(op, "bb_u_webhook_register_url") == 0) o = emit_bb_u_webhook_register_url(c);
        if (strcmp(op, "bb_u_webhook_register_url_for_log") == 0) o = emit_bb_u_webhook_register_url_for_log(c);
        if (strcmp(op, "bb_u_find_registered_webhooks") == 0) o = emit_bb_u_find_registered_webhooks(c);
        if (strcmp(op, "bb_u_register_webhook") == 0) o = emit_bb_u_register_webhook(c);
        if (strcmp(op, "bb_u_unregister_webhook") == 0) o = emit_bb_u_unregister_webhook(c);
        if (strcmp(op, "bb_u_resolve_chat_guid") == 0) o = emit_bb_u_resolve_chat_guid(c);
        if (strcmp(op, "bb_u_create_chat_for_handle") == 0) o = emit_bb_u_create_chat_for_handle(c);
        if (strcmp(op, "bb_mark_read") == 0) o = emit_bb_mark_read(c);
        if (strcmp(op, "bb_u_download_attachment") == 0) o = emit_bb_u_download_attachment(c);
        if (strcmp(op, "bb_u_extract_payload_record") == 0) o = emit_bb_u_extract_payload_record(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
