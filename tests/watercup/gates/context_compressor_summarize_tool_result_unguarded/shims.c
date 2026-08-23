/* Link shims: off-path symbols from context_compressor_pure.c. */
#include <stddef.h>
char *context_compressor_content_text(const char *m) { (void)m; return NULL; }
char *context_compressor__extract_pruned_skill_names(const char *m) { (void)m; return NULL; }
int hermes_redact_force(char *buf, size_t n, const char *s) { (void)buf; (void)n; (void)s; return 0; }
int get_model_context_length(const char *m) { (void)m; return 0; }
