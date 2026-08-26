/* Link shims: off-path symbols pulled in by context_compressor_pure.c that
 * this gate's traced functions never execute (mirrors the shims pattern used
 * by sibling context_compressor gates). */
#include <stddef.h>
char *context_compressor_content_text(const char *m) { (void)m; return NULL; }
char *context_compressor__extract_pruned_skill_names(const char *m) { (void)m; return NULL; }
int get_model_context_length(const char *m) { (void)m; return 0; }
size_t estimate_messages_tokens_rough(const void *msgs) { (void)msgs; return 0; }
char **arh_tool_call_id_variants(const char *id) { (void)id; return NULL; }
/* aux LLM call: network-coupled, off-path for this gate (mirrors the
   Python no-aux-client expected-failure contract). */
char *aux__call_llm_impl(const char *task, const char *kwargs_json) { (void)task;(void)kwargs_json; return NULL; }
