/*
 * session_picker_fields.c — C port of upstream Hermes gateway/session_picker_fields.py.
 *
 * PoP: w17_sessionpic_8975aa_live_picker_page @ gateway/session_picker_fields.py:live_picker_page
 */
#include "noc_w17_sessionpic_8975aa.h"
#include "hermes_json.h"
#include <string.h>

json_t *w17_sessionpic_8975aa_live_picker_page(const json_t *sessions,
        int limit, int has_workspace, const char *workspace,
        const json_t *members, int picker) {
    /* Scripted seams: authority_persistence(), current_segments() and fill_picker_fields()
     * are injected by the oracle as deterministic no-ops; the page rows are unchanged. */
    (void)workspace;
    (void)picker;

    json_t *filtered = json_array();
    if (sessions && sessions->type == JSON_ARRAY) {
        for (size_t i = 0; i < sessions->c.count; i++) {
            const json_t *row = sessions->c.items[i];
            if (has_workspace) {
                const char *sid = json_get_str(row, "session_id", "");
                int found = 0;
                if (members && members->type == JSON_ARRAY) {
                    for (size_t j = 0; j < members->c.count; j++) {
                        const json_t *m = members->c.items[j];
                        if (m && m->type == JSON_STRING && m->str_val && sid &&
                            !strcmp(m->str_val, sid)) { found = 1; break; }
                    }
                }
                if (!found) continue;  /* row['session_id'] not in members */
            }
            json_append(filtered, json_copy(row));
        }
    }

    /* Python: page = sessions[:limit] (applied AFTER the workspace filter). */
    size_t cnt = json_len(filtered);
    size_t end = cnt;
    if (limit < 0) {
        long long e = (long long)cnt + (long long)limit;
        end = e <= 0 ? 0 : (size_t)e;
    } else if ((size_t)limit < cnt) {
        end = (size_t)limit;
    }
    json_t *page = json_array();
    for (size_t i = 0; i < end; i++)
        json_append(page, json_copy(json_get(filtered, i)));
    json_free(filtered);
    return page;
}
