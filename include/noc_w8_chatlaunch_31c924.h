#ifndef NOC_W8_CHATLAUNCH_31C924_H
#define NOC_W8_CHATLAUNCH_31C924_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_chatlaunch_31c924_short_resume_id @ hermes_cli/chat_launch.py:_short_resume_id
 * `resolve_map` maps a 22-char prefix to its resolved id; `resolve_error`
 * injects a store failure. Returns the printable resume id (str). */
struct json_t *w8_chatlaunch_31c924_short_resume_id(const char *session_id,
                                                    const struct json_t *resolve_map,
                                                    int resolve_error);

/* PoP: w8_chatlaunch_31c924_branch_lineage @ hermes_cli/chat_launch.py:_branch_lineage
 * `sessions` maps id -> {"_branched_from": id-or-null}. Returns the JSON array
 * of ids: the start session plus each /branch ancestor, de-duplicated. */
struct json_t *w8_chatlaunch_31c924_branch_lineage(const struct json_t *sessions,
                                                   const char *start_id);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_CHATLAUNCH_31C924_H */
