/*
 * session_operator.c — C home for upstream Hermes gateway/session_operator.py.
 *
 * PoP: sessop_is_viewer_input @ gateway/session_operator.py:is_viewer_input
 * PoP: sessop_py_str @ gateway/session_operator.py:is_viewer_input
 *
 * is_viewer_input classifies a row submitted through session.submit as a
 * viewer's own prompt (vs a producer admission / hosted-room task / other
 * envelope).  The rule is pure set logic over request_id and payload keys.
 */
#include "noc_sessop.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* frozenset({'text','attachments_v1','finite','unattended','surface_v1',
 *            'display_v1','cli_turn_v1','kanban_card_v1'}) | {'local_operator_v1'} */
static const char *const kAllowed[] = {
    "text", "attachments_v1", "finite", "unattended", "surface_v1",
    "display_v1", "cli_turn_v1", "kanban_card_v1", "local_operator_v1"
};
static const size_t kAllowedN = sizeof(kAllowed) / sizeof(kAllowed[0]);

static int is_allowed(const char *key) {
    for (size_t i = 0; i < kAllowedN; ++i)
        if (!strcmp(key, kAllowed[i])) return 1;
    return 0;
}

char *sessop_py_str(const json_t *v) {
    const char *s = NULL;
    char buf[64];
    if (!v || v->type == JSON_NULL) {
        s = "None";
    } else {
        switch (v->type) {
            case JSON_BOOL:  s = v->bool_val ? "True" : "False"; break;
            case JSON_STRING: s = v->str_val ? v->str_val : ""; break;
            case JSON_NUMBER: {
                double d = v->num_val;
                /* Python str(123) == '123' ; json numbers are doubles here, so
                 * emit integral values without a trailing .0, as Python ints do. */
                if (d == (double)(long long)d && d >= -9e15 && d <= 9e15)
                    snprintf(buf, sizeof(buf), "%lld", (long long)d);
                else
                    snprintf(buf, sizeof(buf), "%g", d);
                s = buf;
                break;
            }
            default: s = "None"; break;
        }
    }
    char *out = (char *)malloc(strlen(s) + 1);
    if (out) strcpy(out, s);
    return out; /* caller frees */
}

int sessop_is_viewer_input(const char *request_id_str,
                           const char *const *keys, size_t n_keys) {
    /* not str(row['request_id']).startswith('hosted:') */
    if (request_id_str && !strncmp(request_id_str, "hosted:", 7))
        return 0;

    /* {'text'} <= set(payload) <= allowed */
    int has_text = 0;
    for (size_t i = 0; i < n_keys; ++i) {
        const char *k = keys[i];
        if (!k) continue;
        if (!is_allowed(k)) return 0;
        if (!strcmp(k, "text")) has_text = 1;
    }
    return has_text ? 1 : 0;
}
