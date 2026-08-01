/* AUTO-GENERATED integration oracle harness for port_platforms_base_helpers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_platforms_base_helpers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int gw_base__is_animation_url(const char *);
extern int gw_base__is_command(const char *);
extern int gw_base__ssrf_redirect_guard(const char *);
extern int gw_base__should_auto_tts_for_chat(const char *);
extern int gw_base__is_retryable_error(const char *);
extern int gw_base__stop_typing_refresh(const char *);
extern int gw_base__discard_text_debounce(const char *);
extern int gw_base__release_session_guard(const char *);
extern int gw_base__session_task_is_stale(const char *);
extern int gw_base__heal_stale_session_lock(const char *);
extern int gw_base__drain_pending_after_session_command(const char *);
extern int gw_base__process_message_background(const char *);
extern int gw_base__cleanup_finished_session_task(const char *);
extern int gw_base__has_pending_interrupt(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_gw_base__is_animation_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__is_animation_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__is_animation_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__is_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__is_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__is_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__ssrf_redirect_guard(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__ssrf_redirect_guard(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__ssrf_redirect_guard"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__should_auto_tts_for_chat(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__should_auto_tts_for_chat(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__should_auto_tts_for_chat"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__is_retryable_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__is_retryable_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__is_retryable_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__stop_typing_refresh(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__stop_typing_refresh(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__stop_typing_refresh"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__discard_text_debounce(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__discard_text_debounce(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__discard_text_debounce"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__release_session_guard(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__release_session_guard(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__release_session_guard"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__session_task_is_stale(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__session_task_is_stale(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__session_task_is_stale"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__heal_stale_session_lock(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__heal_stale_session_lock(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__heal_stale_session_lock"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__drain_pending_after_session_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__drain_pending_after_session_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__drain_pending_after_session_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__process_message_background(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__process_message_background(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__process_message_background"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__cleanup_finished_session_task(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__cleanup_finished_session_task(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__cleanup_finished_session_task"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_base__has_pending_interrupt(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_base__has_pending_interrupt(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_base__has_pending_interrupt"));
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
        if (strcmp(op, "gw_base__is_animation_url") == 0) o = emit_gw_base__is_animation_url(c);
        if (strcmp(op, "gw_base__is_command") == 0) o = emit_gw_base__is_command(c);
        if (strcmp(op, "gw_base__ssrf_redirect_guard") == 0) o = emit_gw_base__ssrf_redirect_guard(c);
        if (strcmp(op, "gw_base__should_auto_tts_for_chat") == 0) o = emit_gw_base__should_auto_tts_for_chat(c);
        if (strcmp(op, "gw_base__is_retryable_error") == 0) o = emit_gw_base__is_retryable_error(c);
        if (strcmp(op, "gw_base__stop_typing_refresh") == 0) o = emit_gw_base__stop_typing_refresh(c);
        if (strcmp(op, "gw_base__discard_text_debounce") == 0) o = emit_gw_base__discard_text_debounce(c);
        if (strcmp(op, "gw_base__release_session_guard") == 0) o = emit_gw_base__release_session_guard(c);
        if (strcmp(op, "gw_base__session_task_is_stale") == 0) o = emit_gw_base__session_task_is_stale(c);
        if (strcmp(op, "gw_base__heal_stale_session_lock") == 0) o = emit_gw_base__heal_stale_session_lock(c);
        if (strcmp(op, "gw_base__drain_pending_after_session_command") == 0) o = emit_gw_base__drain_pending_after_session_command(c);
        if (strcmp(op, "gw_base__process_message_background") == 0) o = emit_gw_base__process_message_background(c);
        if (strcmp(op, "gw_base__cleanup_finished_session_task") == 0) o = emit_gw_base__cleanup_finished_session_task(c);
        if (strcmp(op, "gw_base__has_pending_interrupt") == 0) o = emit_gw_base__has_pending_interrupt(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
