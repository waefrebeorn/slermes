/* Link shims: off-path symbols from context_compressor_pure.c. */
#include <stddef.h>
char *context_compressor_content_text(const char *m) { (void)m; return NULL; }
char *context_compressor__extract_pruned_skill_names(const char *m) { (void)m; return NULL; }
int hermes_redact_force(char *buf, size_t n, const char *s) { (void)buf; (void)n; (void)s; return 0; }
int get_model_context_length(const char *m) { (void)m; return 0; }
size_t estimate_messages_tokens_rough(const void *msgs) { (void)msgs; return 0; }
/* Faithful shim for the ports-file helper (char* JSON in/out) — mirrors
   src/agent/context_compressor_ports.c::cc_strip_image_parts_from_parts
   exactly; the summarize gate's inputs never carry image parts, so this
   path returns NULL (no images found) on both sides. */
#include <stddef.h>
char *cc_strip_image_parts_from_parts(const char *parts_json) {
    (void)parts_json;
    return NULL;
}
char *aux__call_llm_impl(const char *task, const char *kwargs_json) { (void)task;(void)kwargs_json; return NULL; }
char **arh_tool_call_id_variants(const char *id) { (void)id; return NULL; }
