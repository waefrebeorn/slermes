/*
 * cli_undo_hint.c — C port of upstream Hermes hermes_cli/cli_undo_hint.py.
 *
 * PoP: cliundo_undone_span @ hermes_cli/cli_undo_hint.py:_undone_span
 *
 * _user_turn_indices (hermes_cli.cli_session_mixin) is injected from the
 * fixture, mirroring the dependency stub in the oracle.
 */
#include "noc_cliundo.h"
#include "hermes_json.h"
#include <stdlib.h>

json_t *cliundo_undone_span(const json_t *before, const json_t *indices,
                            const char *turns_undone_decimal) {
    long long td = strtoll(turns_undone_decimal ? turns_undone_decimal : "0", NULL, 10);
    json_t *out = json_array();
    size_t nidx = (indices && indices->type == JSON_ARRAY) ? json_len(indices) : 0;
    if (nidx == 0 || td < 1) {
        json_append(out, json_null());
        json_append(out, json_array());
        return out;
    }
    /* cut = indices[max(len(indices) - turns_undone, 0)] */
    long long pos = (long long)nidx - td;
    if (pos < 0) pos = 0;
    json_t *ci = json_get(indices, (size_t)pos);
    long long cut = (ci && ci->type == JSON_NUMBER) ? (long long)ci->num_val : 0;

    json_t *ts = json_null();
    const json_t *barr = (before && before->type == JSON_ARRAY) ? before : NULL;
    if (barr && cut >= 0 && (size_t)cut < json_len(barr)) {
        json_t *m = json_get(barr, (size_t)cut);
        if (json_is_object(m)) {
            json_t *v = json_obj_get(m, "timestamp");
            ts = v ? json_copy(v) : json_null();
        }
    }
    json_append(out, ts);

    json_t *kept = json_array();
    if (barr && cut > 0) {
        for (long long i = 0; i < cut && (size_t)i < json_len(barr); i++)
            json_append(kept, json_copy(json_get(barr, (size_t)i)));
    }
    json_append(out, kept);
    return out;
}
