/* gate: curator_extract_absorbed_into_declarations
 * Calls the REAL C port src/agent/curator_ports.c::cur_extract_absorbed_into_declarations
 * (compiled from source) against LIVE Python agent/curator.py::
 * _extract_absorbed_into_declarations. Input argv: a JSON array of tool calls.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"

char *cur_extract_absorbed_into_declarations(const char *tool_calls_json);

/* out-of-path externals from curator_ports.c (LLM/YAML/db paths never traced) */
void yaml_free(void *p){(void)p;}
char *yaml_to_json_string(const char *s){(void)s;return NULL;}
char *yaml_parse(const char *s){(void)s;return NULL;}
void apply_automatic_transitions(void){}
char *llm_resolve_provider_env(void){return NULL;}
void *message_new(void){return NULL;}
void message_free(void *p){(void)p;}
char *llm_chat_completion(const char *a, const char *b){(void)a;(void)b;return NULL;}
void llm_response_free(void *p){(void)p;}

static void run_one(const char *input) {
    WCUP_BEGIN("extract_absorbed_into_declarations");
    char *res = cur_extract_absorbed_into_declarations(input);
    json_t *out = res ? json_parse(res, NULL) : NULL;
    free(res);
    int n = (out && out->type == JSON_OBJECT && out->c.keys) ? (int)out->c.count : 0;
    WCUP_BRANCH(n > 0);
    /* sorted key order for deterministic event streams */
    const char *keys[32];
    for (int i = 0; i < n && i < 32; i++) keys[i] = out->c.keys ? out->c.keys[i] : "";
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(keys[i], keys[j]) > 0) { const char *t = keys[i]; keys[i] = keys[j]; keys[j] = t; }
    for (int i = 0; i < n; i++) {
        WCUP_LOOP_TICK();
        json_t *e = json_obj_get(out, keys[i]);
        const char *into = e ? json_get_str(e, "into", "") : "";
        char buf[600];
        snprintf(buf, sizeof buf, "%s|%s", keys[i], into ? into : "");
        WCUP_VALSTR(buf);
    }
    if (out) json_free(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("extract_absorbed_into_declarations");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s extract_absorbed_into_declarations <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
