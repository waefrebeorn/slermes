#ifndef NOC_W17_SESSIONPIC_8975AA_H
#define NOC_W17_SESSIONPIC_8975AA_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_sessionpic_8975aa_live_picker_page @ gateway/session_picker_fields.py:live_picker_page
 * workspace_members/current_segments/fill_picker_fields are scripted seams:
 *   members = JSON array of session ids returned by workspace_members(store, ids, workspace).
 *   When has_workspace, only rows whose session_id is in members are kept; the result is
 *   sliced with Python `[:limit]` semantics (negative limit drops from the end).
 * The picker flag triggers current_segments + fill_picker_fields, both seeded no-ops, so the
 * returned rows are the live feed's page unchanged. */
struct json_t *w17_sessionpic_8975aa_live_picker_page(const struct json_t *sessions,
                                    int limit, int has_workspace, const char *workspace,
                                    const struct json_t *members, int picker);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_SESSIONPIC_8975AA_H */
