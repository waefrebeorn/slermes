/* Link shims: off-path symbols pulled in by config chain deps. */
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include "hermes_json.h"
char *auxiliary_normalize_main_runtime(void) { return NULL; }
int llm_chat_completion_stream(const char *req, void **out) { (void)req; (void)out; return -1; }
void llm_response_free(void *r) { (void)r; }
int llm_chat_completion(const char *req, void **out) { (void)req; (void)out; return -1; }
char *provider_base_url_hostname(const char *u) { (void)u; return NULL; }
int provider_base_url_host_matches(const char *u, const char *h) { (void)u; (void)h; return 0; }
char *auxiliary_normalize_provider(const char *p) { (void)p; return NULL; }
char *hermes_redact(char *dst, size_t n, const char *s) { (void)dst; (void)n; (void)s; return NULL; }
int mirror_to_session(const char *a, const char *b) { (void)a; (void)b; return 0; }
long session_get_or_create(const char *a, const char *b) { (void)a; (void)b; return 0; }
char **scheduler_get_running_job_ids(int *n) { if (n) *n = 0; return NULL; }
void session_title_set(long sid, const char *t) { (void)sid; (void)t; }
char *session_title_next_in_lineage(const char *t) { (void)t; return NULL; }
const char *scheduler_plugin_env_var_lookup(const char *k) { (void)k; return NULL; }
char *provider_derive_api_key_name(const char *p) { (void)p; return NULL; }
char *profile_get_active_name(void) { return NULL; }
json_t *reasoning_parse_effort(const char *effort, bool effort_is_bool_false) { (void)effort; (void)effort_is_bool_false; return NULL; }
bool hermes_config_load_platforms(void *cfg) { (void)cfg; return false; }
int aux__call_llm_impl(const char *a, const char *b, char **out) { (void)a; (void)b; (void)out; return -1; }
bool hermes_config_load(void *cfg, const char *dir) { (void)cfg; (void)dir; return false; }
json_t *config_py_get_compatible_custom_providers(const char *a) { (void)a; return NULL; }
