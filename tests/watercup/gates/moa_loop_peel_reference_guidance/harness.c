/* gate: moa_loop_peel_reference_guidance — drives the REAL
 * src/agent/moa_loop_ports.c::moa_peel_reference_guidance (linked via
 * extra_src). LIVE twin: agent/moa_loop.py::peel_reference_guidance.
 *
 * Input: "<messages_json>|<guidance>" per cup ('|' cannot appear unescaped
 * in the probes, so it safely splits the two halves).
 * Rung order (must match twin.py EXACTLY):
 *   B1  peel applicable (guidance non-empty AND messages parse to a
 *       non-empty array)
 *   V   resulting messages JSON (serialized identically on both sides;
 *       "" when the C port fails closed)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"

char *moa_peel_reference_guidance(const char *messages_json, const char *guidance);

/* Link shims: symbols referenced by other functions in moa_loop_ports.c,
 * none of which are on the peel_reference_guidance path. */
typedef struct wc_msg { void *x; } wc_msg;
wc_msg *message_new(const char *a, const char *b){(void)a;(void)b;return NULL;}
void message_free(wc_msg *m){(void)m;}
void *llm_chat_completion(void *a, wc_msg **b, size_t c){(void)a;(void)b;(void)c;return NULL;}
void llm_response_free(void *r){(void)r;}
char *provider_host_derived_api_key(const char *p){(void)p;return NULL;}
void *provider_registry_get(const char *p){(void)p;return NULL;}
void *config_py_load_config_readonly(void){return NULL;}
char *moa_resolve_preset(const char *p){(void)p;return NULL;}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s peel <msgs|guidance>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[2048];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *pipe = strchr(buf, '|');
        if (pipe) *pipe = '\0';
        const char *msgs = buf;
        const char *guidance = pipe ? pipe + 1 : "";

        WCUP_BEGIN("peel_reference_guidance");
        /* mirror the port's applicability guard */
        int applicable = guidance[0] != '\0';
        json_t *parsed = NULL;
        if (applicable) {
            char *err = NULL;
            parsed = json_parse(msgs, &err);
            free(err);
            if (!parsed || !json_is_array(parsed) || json_array_size(parsed) == 0)
                applicable = 0;
        }
        WCUP_BRANCH(applicable);                       /* B1 */
        char *out = applicable
            ? moa_peel_reference_guidance(msgs, guidance)
            : NULL;
        char val[1536];
        snprintf(val, sizeof val, "%s", out ? out : "");
        WCUP_VALSTR(val);                              /* V */
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("peel_reference_guidance");
        free(out);
        if (parsed) json_free(parsed);
    }
    return 0;
}
