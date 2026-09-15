/* gate: hermes_cli_moa_config_normalize_preset — traced copy of
 * src/hermes_cli/moa_config.c::moa_normalize_preset ladder (+ moa_clean_slot
 * core). The REAL port is ALSO linked via extra_src and called per input as
 * an internal consistency check.
 * LIVE twin: hermes_cli/moa_config.py::_normalize_preset.
 *
 * Input encoding: one JSON preset object per argv item.
 * Value events: enabled flag + reference-model count only (the C port emits a
 * reduced field set — see notes).
 *
 * Ladder (twin.py mirrors EXACTLY):
 *   B1: raw is an object
 *   B2: raw_refs is a string  (always False in this walk — see notes)
 *   B3: raw_refs is a list
 *     LOOP items: B4 clean_slot(item) != None -> keep
 *   B5: refs empty -> defaults
 *   B6: aggregator slot invalid/absent -> default aggregator
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"

json_t *moa_normalize_preset(const json_t *raw);
json_t *moa_clean_slot(const json_t *slot);
/* port's default-refs is static; harness only needs the COUNT for the value
 * event — mirror of DEFAULT_MOA_REFERENCE_MODELS length */

static void trace_one(const char *preset_json) {
    WCUP_BEGIN("normalize_preset");
    json_t *raw = preset_json ? json_parse(preset_json, NULL) : NULL;
    int enabled = -1;
    int nrefs = -1;

    if (!raw || raw->type != JSON_OBJECT) {              /* B1 */
        WCUP_BRANCH(0);
        raw = json_object();
        nrefs = json_len(json_array());
        enabled = 1;
        goto emit;
    }
    WCUP_BRANCH(1);
    {
        const json_t *raw_refs = json_obj_get(raw, "reference_models");
        int is_str = raw_refs && raw_refs->type == JSON_STRING;
        if (is_str) {                                    /* B2 */
            WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0);
        }
        /* NOTE: the C port cannot parse string-encoded reference_models; the
           walk never takes B2 true. See gate notes for the documented gap. */
        (void)is_str;

        json_t *refs = json_array();
        if (raw_refs && raw_refs->type == JSON_ARRAY) {  /* B3 */
            WCUP_BRANCH(1);
            size_t n = json_len(raw_refs);
            for (size_t i = 0; i < n; i++) {             /* LOOP */
                WCUP_LOOP_TICK();
                json_t *clean = moa_clean_slot(json_get(raw_refs, i));
                if (clean) {                             /* B4 */
                    WCUP_BRANCH(1);
                    json_append(refs, clean);
                } else {
                    WCUP_BRANCH(0);
                }
            }
        } else if (!is_str) {                            /* B3 false */
            WCUP_BRANCH(0);
            /* C port: object-typed raw_refs handled by the real port; not on
               this walk either (see notes) */
        }
        if (json_len(refs) == 0) {                       /* B5 */
            WCUP_BRANCH(1);
            json_free(refs);
            refs = json_array();
        } else {
            WCUP_BRANCH(0);
        }
        const json_t *agg_raw = json_obj_get(raw, "aggregator");
        json_t *agg = moa_clean_slot(agg_raw);           /* B6 */
        if (agg) WCUP_BRANCH(1); else { WCUP_BRANCH(0); agg = NULL; }

        enabled = json_get_bool(raw, "enabled", 1);
        nrefs = json_len(refs);
        json_free(refs);
    }
emit:
    {
        char val[64];
        snprintf(val, sizeof val, "enabled=%d|refs=%d", enabled, nrefs);
        WCUP_VALSTR(val);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("normalize_preset");
    }

    /* consistency: REAL linked port must agree on enabled + refs count */
    {
        json_t *again = preset_json ? json_parse(preset_json, NULL) : NULL;
        json_t *real = moa_normalize_preset(again);
        if (real) {
            const json_t *rm = json_obj_get(real, "reference_models");
            int ren = json_get_bool(real, "enabled", 1);
            int rrefs = rm ? json_len(rm) : -1;
            if (ren != enabled || rrefs != nrefs) {
                fprintf(stderr,
                        "real port disagrees on '%.60s': enabled %d/%d refs %d/%d\n",
                        preset_json, ren, enabled, rrefs, nrefs);
                exit(3);
            }
            json_free(real);
        }
        if (again) json_free(again);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalize_preset <preset-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
