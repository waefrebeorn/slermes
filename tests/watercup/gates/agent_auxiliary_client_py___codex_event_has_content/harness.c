/* gate: agent_auxiliary_client_py___codex_event_has_content — traced copy of _codex_event_has_content */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* The port takes a parsed event: codex_event_has_content(const json_t *). The walk hands over raw text, so parse
 * it here; an input that is not a JSON object carries no payload, and Python's _field() on a
 * non-object also answers False. */
#include "hermes_json.h"
extern int codex_event_has_content(const json_t *event);
static int codex_event_has_content_shim(const char *input) {
    if (!input || !*input || strcmp(input, "NULL") == 0) return 0;
    char *err = NULL;
    json_t *ev = json_parse(input, &err);
    free(err);
    int r = ev ? codex_event_has_content(ev) : 0;
    if (ev) json_free(ev);
    return r;
}

static void trace_one(const char *input) {
    WCUP_BEGIN("_codex_event_has_content");
    int result = codex_event_has_content_shim(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_codex_event_has_content");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _codex_event_has_content <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
