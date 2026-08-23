/* gate: auxiliary_client_extract_content_or_reasoning — drives the REAL
 * src/agent/auxiliary_client.c::auxc_extract_content_or_reasoning (linked
 * via extra_src). LIVE twin: agent/auxiliary_client.py::
 * extract_content_or_reasoning.
 *
 * Input: one message-object JSON per cup ("-" encodes an absent payload).
 * Rung order (must match twin.py EXACTLY):
 *   B1  text resolved non-empty / V extracted text ("" otherwise)
 *
 * Agreed sub-contract: probes restricted to the field-scan agreement window
 * (plain content values and reasoning_content fallbacks). The C port lacks
 * think-block stripping and the reasoning/reasoning_details rungs — those
 * inputs diverge and are excluded (see gate.json notes).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

char *auxc_extract_content_or_reasoning(const char *message_json);

/* Link shims: auxiliary_client.c's HTTP/auth surface is off the extraction
 * path; wc_run's flag ordering can't reach their defining objects. */
void *http_new(void) { return NULL; }
void http_free(void *h) { (void)h; }
void http_resp_free(void *r) { (void)r; }
void *http_get(void *c, const char *u) { (void)c; (void)u; return NULL; }
int url_host_matches(const char *a, const char *b) { (void)a; (void)b; return 0; }
char *auth_store_load_provider_status(const char *p, char **o) { (void)p; if (o) *o = NULL; return NULL; }
void *auth_store_load_provider_state(const char *p) { (void)p; return NULL; }
int hermes_log(int lvl, const char *fmt, ...) { (void)lvl; (void)fmt; return 0; }
char *secret_scope_get_secret(const char *name) { (void)name; return NULL; }
char *anthropic_prompt_cache_policy(const char *model) { (void)model; return NULL; }
void *relay_llm_complete_logical_call(const char *id, void *req) { (void)id; (void)req; return NULL; }
int relay_llm_stream_current(const char *id, void *req, void *cb, void *ctx) { (void)id; (void)req; (void)cb; (void)ctx; return -1; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s extract <message-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("extract_content_or_reasoning");
        char *text = auxc_extract_content_or_reasoning(
            strcmp(argv[i], "-") == 0 ? NULL : argv[i]);
        WCUP_BRANCH(text && *text);                /* B1 */
        char val[1024];
        snprintf(val, sizeof val, "%s", text ? text : "");
        WCUP_VALSTR(val);                          /* V */
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("extract_content_or_reasoning");
        free(text);
    }
    return 0;
}
