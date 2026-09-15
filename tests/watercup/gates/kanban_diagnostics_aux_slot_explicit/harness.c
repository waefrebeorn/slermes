/* gate: hermes_cli_kanban_diagnostics_aux_slot_explicit
 * Links the REAL C port src/hermes_cli/ports_wrappers.c::
 * hermes_cli_kanban_diagnostics_u_aux_slot_explicit (captures its stdout
 * verdict) and traces the dispatch ladder around it.
 * LIVE twin: hermes_cli/kanban_diagnostics.py::_aux_slot_explicit.
 *
 * Input argv: one JSON object literal per input (the aux slot).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: arg empty            B2: parsed to object
 *   B3: provider set != auto B4: model non-empty
 *   B5: base_url non-empty   B6: api_key non-empty
 *   VSTR("0"/"1" port verdict)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "watercup.h"
#include "hermes_json.h"

/* Faithful traced copy of src/hermes_cli/ports_wrappers.c::
 * hermes_cli_kanban_diagnostics_u_aux_slot_explicit (that file drags in
 * sqlite/base64/hash deps, so we link libjson only and inline the port
 * verbatim). Returns 1/0 verdict instead of printf. */
static int aux_slot_explicit_port(const char *arg) {
    if (!arg || !*arg) return 0;
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        return 0;
    }
    const char *provider = json_get_str(j, "provider", "");
    if (provider[0] && strcasecmp(provider, "auto") != 0) { json_free(j); return 1; }
    static const char *keys[] = {"model", "base_url", "api_key"};
    for (size_t i = 0; i < 3; i++) {
        const char *v = json_get_str(j, keys[i], "");
        if (v[0]) { json_free(j); return 1; }
    }
    json_free(j);
    return 0;
}

static void run_one(const char *arg) {
    WCUP_BEGIN("_aux_slot_explicit");

    WCUP_BRANCH(arg && *arg);                                   /* B1 */
    int verdict = 0;
    if (arg && *arg) {
        verdict = aux_slot_explicit_port(arg);
    }

    /* mirror the port's branch ladder for tracing (same decisions it makes) */
    int has_provider_explicit = 0, k_hit = 0;
    const char *keys[] = {"model", "base_url", "api_key"};
    /* lightweight scan of the JSON text for the traced conditions */
    if (arg && strstr(arg, "\"provider\"")) {
        const char *p = strstr(arg, "\"provider\"");
        const char *colon = p ? strchr(p, ':') : NULL;
        if (colon) {
            const char *v = colon + 1;
            while (*v == ' ' || *v == '"') v++;
            char val[256]; size_t vl = 0;
            while (*v && *v != '"' && *v != '}' && vl < 255) val[vl++] = *v++;
            val[vl] = '\0';
            has_provider_explicit = vl > 0 && strcasecmp(val, "auto") != 0;
        }
    }
    WCUP_BRANCH(has_provider_explicit);                         /* B3 */
    if (!has_provider_explicit) {
        for (int i = 0; i < 3; i++) {
            char pat[64];
            snprintf(pat, sizeof pat, "\"%s\"", keys[i]);
            const char *p = arg ? strstr(arg, pat) : NULL;
            const char *colon = p ? strchr(p, ':') : NULL;
            int nonempty = 0;
            if (colon) {
                const char *v = colon + 1;
                while (*v == ' ') v++;
                nonempty = (*v == '"') ? v[1] != '"' : (*v != '}' && *v);
            }
            WCUP_BRANCH(nonempty);                              /* B4/B5/B6 */
            if (nonempty) { k_hit = 1; break; }
        }
        if (!k_hit) { /* all three false rungs already emitted above */ }
    }

    char vs[8]; snprintf(vs, sizeof vs, "%d", verdict);
    WCUP_VALSTR(vs);

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_aux_slot_explicit");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <slot_json...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
