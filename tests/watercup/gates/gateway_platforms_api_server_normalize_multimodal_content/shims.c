/* Link shims: off-path symbols pulled in by api_server_ports.c. */
#include <stddef.h>
int context_compressor__is_compaction_summary_message(const char *s) { (void)s; return 0; }
int hermes_redact_force(char *buf, size_t n, const char *s) { (void)buf; (void)n; (void)s; return 0; }
char *project_compaction_message_for_display(const char *s) { (void)s; return NULL; }
char *profile_normalize_name(const char *s) { (void)s; return NULL; }
char *profile_dir_for(const char *s) { (void)s; return NULL; }
const char *profile_default_home(void) { return "."; }
int validate_media_delivery_path(const char *a, const char *b, char *err, size_t n) { (void)a;(void)b;(void)err;(void)n; return 1; }
