/* Link shims: off-path token estimator. */
#include <stddef.h>
size_t estimate_messages_tokens_rough(const void *msgs) { (void)msgs; return 0; }
char *aux__call_llm_impl(const char *task, const char *kwargs_json) { (void)task;(void)kwargs_json; return NULL; }
char **arh_tool_call_id_variants(const char *id) { (void)id; return NULL; }
