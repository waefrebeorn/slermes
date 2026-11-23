/*
 * clarify_batch.c — C port of upstream Hermes tools/clarify_batch.py.
 * PoP: clarbatch_settle_batch @ tools/clarify_batch.py:settle_batch
 */
#include "noc_clarbatch.h"
#include <string.h>

static int in_outcomes(const char *s) {
    return s && (!strcmp(s, "submitted") || !strcmp(s, "timed_out") ||
                 !strcmp(s, "cancelled"));
}

/* dict(locked) — a shallow value copy preserving insertion order. */
static json_t *copy_answers(json_t *locked) {
    json_t *a = json_object();
    if (locked && locked->type == JSON_OBJECT) {
        for (size_t i = 0; i < locked->c.count; i++)
            json_set(a, locked->c.keys[i], json_copy(locked->c.items[i]));
    }
    return a;
}

json_t *clarbatch_settle_batch(json_t *locked, json_t *frame_result, const char *ended) {
    if (ended != NULL) {
        json_t *out = json_object();
        json_set(out, "answers", copy_answers(locked));
        json_set(out, "outcome", json_string(in_outcomes(ended) ? ended : "cancelled"));
        return out;
    }

    if (frame_result && frame_result->type == JSON_OBJECT) {
        json_t *fa = json_obj_get(frame_result, "answers");
        if (fa && fa->type == JSON_OBJECT) {
            /* {**frame_result, "answers": {**locked, **fa}, "outcome": ...} */
            json_t *out = json_copy(frame_result);
            json_t *merged = copy_answers(locked);
            for (size_t i = 0; i < fa->c.count; i++)
                json_set(merged, fa->c.keys[i], json_copy(fa->c.items[i]));
            json_set(out, "answers", merged); /* replaces in place, keeps key order */

            json_t *ov = json_obj_get(frame_result, "outcome");
            const char *ostr = (ov && ov->type == JSON_STRING) ? ov->str_val : NULL;
            json_set(out, "outcome", json_string(in_outcomes(ostr) ? ostr : "submitted"));
            return out;
        }
    }

    /* Anything else (None, {}, error frame, answers not a dict) is cancel-all. */
    json_t *out = json_object();
    json_set(out, "answers", copy_answers(locked));
    json_set(out, "outcome", json_string("cancelled"));
    return out;
}
