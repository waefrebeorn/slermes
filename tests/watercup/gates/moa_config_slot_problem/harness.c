/* gate: moa_config_slot_problem — traced copy of
 * src/hermes_cli/moa_config_pure.c::moa_slot_problem
 * (LIVE twin: hermes_cli/moa_config.py::_slot_problem).
 *
 * Input encoding (one argv token per case):
 *   "!"                 -> slot is not an object (NULL json on C side)
 *   "provider|model"    -> JSON object {"provider": p, "model": m}
 * '|' may not appear inside provider/model in this gate.
 *
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: slot not an object        -> fixed problem string
 *   V1/V2: stripped provider / stripped model
 *   B2: both empty                -> "provider and model are required"
 *   B3: provider empty            -> "provider is required"
 *   B4: model empty               -> "model is required (provider '<p>' ...)"
 *   B5: provider.lower()=="moa"   -> recursive-MoA problem
 *   V3: result (problem string or "OK")
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../../../lib/libjson/json.h"
#include "watercup.h"

/* Faithful traced copy of src/hermes_cli/moa_config_pure.c::moa_slot_problem */
static char *wc_moa_slot_problem(const json_t *slot) {
    char *out = NULL;
    WCUP_BEGIN("moa_slot_problem");
    if (!slot || slot->type != JSON_OBJECT) {           /* B1 */
        WCUP_BRANCH(1);
        out = strdup("must be an object with 'provider' and 'model'");
        goto done;
    }
    WCUP_BRANCH(0);
    {
        const json_t *pv = json_obj_get((json_t *)slot, "provider");
        const json_t *mv = json_obj_get((json_t *)slot, "model");
        const char *provider = (pv && pv->type == JSON_STRING) ? pv->str_val : "";
        const char *model = (mv && mv->type == JSON_STRING) ? mv->str_val : "";
        char pb[256], mb[256];
        snprintf(pb, sizeof pb, "%s", provider);
        snprintf(mb, sizeof mb, "%s", model);
        char *pp = pb; while (*pp == ' ' || *pp == '\t') pp++;
        size_t PL = strlen(pp); while (PL > 0 && (pp[PL-1]==' '||pp[PL-1]=='\t')) pp[--PL]='\0';
        char *mp = mb; while (*mp == ' ' || *mp == '\t') mp++;
        size_t ML = strlen(mp); while (ML > 0 && (mp[ML-1]==' '||mp[ML-1]=='\t')) mp[--ML]='\0';
        WCUP_VALSTR(pp);                                /* V1 */
        WCUP_VALSTR(mp);                                /* V2 */
        int b2 = (pp[0] == '\0' && mp[0] == '\0');      /* B2 */
        WCUP_BRANCH(b2);
        if (b2) { out = strdup("provider and model are required"); goto done; }
        int b3 = (pp[0] == '\0');                       /* B3 */
        WCUP_BRANCH(b3);
        if (b3) { out = strdup("provider is required"); goto done; }
        int b4 = (mp[0] == '\0');                       /* B4 */
        WCUP_BRANCH(b4);
        if (b4) {
            out = (char *)malloc(strlen(pp) + 64);
            snprintf(out, strlen(pp) + 64,
                     "model is required (provider '%s' has no model selected)", pp);
            goto done;
        }
        char plow[256];
        snprintf(plow, sizeof plow, "%s", pp);
        for (char *q = plow; *q; q++) *q = (char)tolower((unsigned char)*q);
        int b5 = (strcmp(plow, "moa") == 0);            /* B5 */
        WCUP_BRANCH(b5);
        if (b5) { out = strdup("the Mixture of Agents provider cannot be used inside a preset (recursive MoA)"); goto done; }
        out = NULL;
    }
done:
    WCUP_VALSTR(out ? out : "OK");                      /* V3 */
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        free(out);
        WCUP_END("moa_slot_problem");
    }
    return NULL;
}

static void run_case(const char *enc) {
    json_t *slot = NULL;
    if (strcmp(enc, "!") != 0) {
        char buf[512];
        snprintf(buf, sizeof buf, "%s", enc);
        char *bar = strchr(buf, '|');
        if (bar) *bar = '\0';
        slot = json_object();
        json_set(slot, "provider", json_string(buf));
        json_set(slot, "model", json_string(bar ? bar + 1 : ""));
    } else {
        slot = json_number(42.0);   /* non-object slot */
    }
    (void)wc_moa_slot_problem(slot);
    json_free(slot);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s moa_slot_problem <case>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_case(argv[i]);
    return 0;
}
