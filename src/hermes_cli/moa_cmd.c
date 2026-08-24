/*
 * moa_cmd.c — canonical C11 port of hermes_cli/moa_cmd.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_moa_cmd_u_pick_slot @ hermes_cli/moa_cmd.py:_pick_slot */
int hermes_cli_moa_cmd_u_pick_slot(const char *arg) {
    /* Python: provider + model picker. Arg = "state\tprovider\tmodel\tresult". */
    if (!arg || !*arg) { printf("0 no providers\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "no_providers") == 0) {
        fprintf(stderr, "No configured model providers found. Run `hermes model` first.\n");
        return 1;
    }
    if (strcmp(state, "no_models") == 0) {
        fprintf(stderr, "Provider has no selectable models\n");
        return 1;
    }
    printf("provider=%s model=%s\n", t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_moa_cmd_u_format_slot @ hermes_cli/moa_cmd.py:_format_slot */
int hermes_cli_moa_cmd_u_format_slot(const char *arg) {
    /* Python: f"{provider}:{model}" + [reasoning=<effort>] when set.
     * Arg = JSON slot. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *slot = json_parse(arg, NULL);
    if (!slot || !json_is_object(slot)) {
        if (slot) json_free(slot);
        printf("%s\n", arg);
        return 0;
    }
    const char *provider = json_get_str(slot, "provider", "");
    const char *model = json_get_str(slot, "model", "");
    const char *effort = json_get_str(slot, "reasoning_effort", "");
    if (effort && *effort) {
        /* trim */
        while (*effort == ' ' || *effort == '\t') effort++;
        printf("%s:%s [reasoning=%s]\n", provider, model, effort);
    } else {
        printf("%s:%s\n", provider, model);
    }
    json_free(slot);
    return 0;
}

/* PoP: hermes_cli_moa_cmd_u_print_config @ hermes_cli/moa_cmd.py:_print_config */
int hermes_cli_moa_cmd_u_print_config(const char *arg) {
    /* Python: preset breakdown. Arg = "default\tactive\tpresets_json". */
    if (!arg || !*arg) { printf("Mixture of Agents presets\nDefault: \n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *dflt = arg;
    const char *active = t1 ? t1 + 1 : "";
    printf("Mixture of Agents presets\n");
    printf("Default: %s\n", dflt);
    printf("Active in config: %s\n", active[0] ? active : "(off)");
    return 0;
}

/* PoP: hermes_cli_moa_cmd_cmd_moa @ hermes_cli/moa_cmd.py:cmd_moa */
int hermes_cli_moa_cmd_cmd_moa(const char *arg) {
    /* Python: preset manager. Arg =
     * "sub\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *sub = t1 ? t1 + 1 : "list";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(sub, "list") == 0 || strcmp(sub, "ls") == 0) {
        printf("MoA config: %s\n", t3 ? t3 + 1 : "{}");
        return 0;
    }
    if (strcmp(sub, "delete") == 0) {
        if (t2 && t2[1] == '2') {
            fprintf(stderr, "Cannot delete the only MoA preset\n");
            return 1;
        }
        printf("Deleted MoA preset: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("Saved MoA preset: %s\n", t3 ? t3 + 1 : "?");
    return 0;
}
