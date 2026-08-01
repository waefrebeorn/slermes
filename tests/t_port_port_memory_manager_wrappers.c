/* AUTO-GENERATED integration oracle harness for port_memory_manager_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_memory_manager_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int mm_memory_provider_tools_enabled(const char *);
extern int mm_inject_memory_provider_tools(const char *);
extern int mm_u_find_boundary_open_tag(const char *);
extern int mm_u_max_pending_open_suffix(const char *);
extern int mm_u_has_block_opener_suffix(const char *);
extern int mm_u_append_visible(const char *);
extern int mm_u_update_block_boundary(const char *);
extern int mm_add_provider(const char *);
extern int mm_prefetch_all(const char *);
extern int mm_u_prefetch_provider(const char *);
extern int mm_queue_prefetch_all(const char *);
extern int mm_u_provider_sync_accepts_messages(const char *);
extern int mm_sync_all(const char *);
extern int mm_u_submit_background(const char *);
extern int mm_u_forget_background_future(const char *);
extern int mm_u_get_sync_executor(const char *);
extern int mm_flush_pending(const char *);
extern int mm_get_all_tool_schemas(const char *);
extern int mm_get_all_tool_names(const char *);
extern int mm_on_turn_start(const char *);
extern int mm_commit_session_boundary_async(const char *);
extern int mm_on_session_switch(const char *);
extern int mm_on_pre_compress(const char *);
extern int mm_u_provider_memory_write_metadata_mode(const char *);
extern int mm_on_memory_write(const char *);
extern int mm_u_memory_tool_result_succeeded(const char *);
extern int mm_notify_memory_tool_write(const char *);
extern int mm_shutdown_drain_state(const char *);
extern int mm_u_drain_sync_executor(const char *);
extern int mm_initialize_all(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_mm_memory_provider_tools_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_memory_provider_tools_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_memory_provider_tools_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_inject_memory_provider_tools(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_inject_memory_provider_tools(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_inject_memory_provider_tools"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_find_boundary_open_tag(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_find_boundary_open_tag(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_find_boundary_open_tag"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_max_pending_open_suffix(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_max_pending_open_suffix(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_max_pending_open_suffix"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_has_block_opener_suffix(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_has_block_opener_suffix(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_has_block_opener_suffix"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_append_visible(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_append_visible(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_append_visible"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_update_block_boundary(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_update_block_boundary(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_update_block_boundary"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_add_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_add_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_add_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_prefetch_all(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_prefetch_all(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_prefetch_all"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_prefetch_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_prefetch_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_prefetch_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_queue_prefetch_all(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_queue_prefetch_all(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_queue_prefetch_all"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_provider_sync_accepts_messages(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_provider_sync_accepts_messages(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_provider_sync_accepts_messages"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_sync_all(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_sync_all(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_sync_all"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_submit_background(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_submit_background(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_submit_background"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_forget_background_future(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_forget_background_future(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_forget_background_future"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_get_sync_executor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_get_sync_executor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_get_sync_executor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_flush_pending(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_flush_pending(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_flush_pending"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_get_all_tool_schemas(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_get_all_tool_schemas(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_get_all_tool_schemas"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_get_all_tool_names(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_get_all_tool_names(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_get_all_tool_names"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_on_turn_start(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_on_turn_start(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_on_turn_start"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_commit_session_boundary_async(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_commit_session_boundary_async(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_commit_session_boundary_async"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_on_session_switch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_on_session_switch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_on_session_switch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_on_pre_compress(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_on_pre_compress(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_on_pre_compress"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_provider_memory_write_metadata_mode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_provider_memory_write_metadata_mode(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_provider_memory_write_metadata_mode"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_on_memory_write(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_on_memory_write(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_on_memory_write"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_memory_tool_result_succeeded(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_memory_tool_result_succeeded(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_memory_tool_result_succeeded"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_notify_memory_tool_write(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_notify_memory_tool_write(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_notify_memory_tool_write"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_shutdown_drain_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_shutdown_drain_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_shutdown_drain_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_u_drain_sync_executor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_u_drain_sync_executor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_u_drain_sync_executor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mm_initialize_all(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mm_initialize_all(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mm_initialize_all"));
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
        if (strcmp(op, "mm_memory_provider_tools_enabled") == 0) o = emit_mm_memory_provider_tools_enabled(c);
        if (strcmp(op, "mm_inject_memory_provider_tools") == 0) o = emit_mm_inject_memory_provider_tools(c);
        if (strcmp(op, "mm_u_find_boundary_open_tag") == 0) o = emit_mm_u_find_boundary_open_tag(c);
        if (strcmp(op, "mm_u_max_pending_open_suffix") == 0) o = emit_mm_u_max_pending_open_suffix(c);
        if (strcmp(op, "mm_u_has_block_opener_suffix") == 0) o = emit_mm_u_has_block_opener_suffix(c);
        if (strcmp(op, "mm_u_append_visible") == 0) o = emit_mm_u_append_visible(c);
        if (strcmp(op, "mm_u_update_block_boundary") == 0) o = emit_mm_u_update_block_boundary(c);
        if (strcmp(op, "mm_add_provider") == 0) o = emit_mm_add_provider(c);
        if (strcmp(op, "mm_prefetch_all") == 0) o = emit_mm_prefetch_all(c);
        if (strcmp(op, "mm_u_prefetch_provider") == 0) o = emit_mm_u_prefetch_provider(c);
        if (strcmp(op, "mm_queue_prefetch_all") == 0) o = emit_mm_queue_prefetch_all(c);
        if (strcmp(op, "mm_u_provider_sync_accepts_messages") == 0) o = emit_mm_u_provider_sync_accepts_messages(c);
        if (strcmp(op, "mm_sync_all") == 0) o = emit_mm_sync_all(c);
        if (strcmp(op, "mm_u_submit_background") == 0) o = emit_mm_u_submit_background(c);
        if (strcmp(op, "mm_u_forget_background_future") == 0) o = emit_mm_u_forget_background_future(c);
        if (strcmp(op, "mm_u_get_sync_executor") == 0) o = emit_mm_u_get_sync_executor(c);
        if (strcmp(op, "mm_flush_pending") == 0) o = emit_mm_flush_pending(c);
        if (strcmp(op, "mm_get_all_tool_schemas") == 0) o = emit_mm_get_all_tool_schemas(c);
        if (strcmp(op, "mm_get_all_tool_names") == 0) o = emit_mm_get_all_tool_names(c);
        if (strcmp(op, "mm_on_turn_start") == 0) o = emit_mm_on_turn_start(c);
        if (strcmp(op, "mm_commit_session_boundary_async") == 0) o = emit_mm_commit_session_boundary_async(c);
        if (strcmp(op, "mm_on_session_switch") == 0) o = emit_mm_on_session_switch(c);
        if (strcmp(op, "mm_on_pre_compress") == 0) o = emit_mm_on_pre_compress(c);
        if (strcmp(op, "mm_u_provider_memory_write_metadata_mode") == 0) o = emit_mm_u_provider_memory_write_metadata_mode(c);
        if (strcmp(op, "mm_on_memory_write") == 0) o = emit_mm_on_memory_write(c);
        if (strcmp(op, "mm_u_memory_tool_result_succeeded") == 0) o = emit_mm_u_memory_tool_result_succeeded(c);
        if (strcmp(op, "mm_notify_memory_tool_write") == 0) o = emit_mm_notify_memory_tool_write(c);
        if (strcmp(op, "mm_shutdown_drain_state") == 0) o = emit_mm_shutdown_drain_state(c);
        if (strcmp(op, "mm_u_drain_sync_executor") == 0) o = emit_mm_u_drain_sync_executor(c);
        if (strcmp(op, "mm_initialize_all") == 0) o = emit_mm_initialize_all(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
