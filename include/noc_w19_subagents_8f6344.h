/* noc_w19_subagents_8f6344.h - public C API for persistence/authority/subagents.py port. */
#ifndef NOC_W19_SUBAGENTS_8F6344_H
#define NOC_W19_SUBAGENTS_8F6344_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_subagents_8f6344_parent_superseded_note @ persistence/authority/subagents.py:parent_superseded_note
 * Why the parent no longer wants its child resumed, from the live tip of its
 * compression chain: "its parent session no longer exists" when the tip is gone,
 * "its parent session ended (<reason>)" for a deliberate end, JSON null while the
 * parent still wants it. tip_end_reason may be NULL (no end stamp). */
json_t *w19_subagents_8f6344_parent_superseded_note(const char *tip_end_reason, int tip_exists);

/* PoP: w19_subagents_8f6344_owed @ persistence/authority/subagents.py:SubagentRecord.owed
 * bool(document["auto_promised"]) and document["auto_gave_up"] is None. */
json_t *w19_subagents_8f6344_owed(json_t *document);

/* Mirrored vocabulary of persistence/conversation/sessions.py:is_automatic_end_reason:
 * 1 when `reason` is one of the automatic-cleanup end stamps (recoverable reasons plus
 * the lifecycle ones), 0 for NULL / unknown / non-string. */
int w19_subagents_8f6344_is_automatic_end_reason(const char *reason);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_SUBAGENTS_8F6344_H */
