/* probe_registry_setup_beat.c -- what does registry_dispatch actually return for setup_beat? */
#include "hermes_json.h"
#include "registry.h"
#include "hermes_agent.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void registry_init_setup_beat(void);

/* infrastructure stubs (same set the unit test uses) */
const char *get_hermes_home(void) { return NULL; }
const char *hc_get_hermes_home_override(void) { return NULL; }
char *hs_resolve_journal_mode(void) { return NULL; }
void hermes_log(int level, const char *module, const char *fmt, ...) { (void)level; (void)module; (void)fmt; }
char *tool_error(const char *message, ...) { (void)message; return NULL; }
char *tool_error_with_class(const char *kind, const char *message) { (void)kind; (void)message; return NULL; }
const char *tool_classify_error(const char *message) { (void)message; return "error"; }
void *tool_idem_new(const char *a, const char *b) { (void)a; (void)b; return NULL; }
void tool_idem_free(void *p) { (void)p; }
const char *tool_idem_key(const char *a) { (void)a; return NULL; }
const char *tool_idem_get(const char *a) { (void)a; return NULL; }
void tool_idem_attach(const char *a, const char *b) { (void)a; (void)b; }
void tool_idem_put(const char *a, const char *b, const char *c) { (void)a; (void)b; (void)c; }
bool tool_result_is_error(const char *a) { (void)a; return false; }

int main(void) {
    registry_init_setup_beat();
    const char *cases[] = { "{\"beat\":\"bogus\"}", "{\"beat\":\"next\"}", NULL };
    for (int i = 0; cases[i]; i++) {
        char *out = registry_dispatch("setup_beat", cases[i], "");
        printf("args=%s\n  raw=%s\n", cases[i], out ? out : "(NULL)");
        json_t *parsed = out ? json_parse(out, NULL) : NULL;
        printf("  parses=%s\n", parsed ? "yes" : "NO");
        json_free(parsed);
        free(out);
    }
    return 0;
}
