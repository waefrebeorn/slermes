#ifndef NOC_W14_SKILLMANAG_328D03_H
#define NOC_W14_SKILLMANAG_328D03_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_skillmanag_328d03_refusal @ tools/skill_manager_guards.py:_refusal */
struct json_t *w14_skillmanag_328d03_refusal(const char *message,
                                             const struct json_t *extra);

/* pinned tri-state: -1 = None (unreadable record), 0 = False, 1 = True */
/* PoP: w14_skillmanag_328d03_pinned_guard @ tools/skill_manager_guards.py:_pinned_guard */
struct json_t *w14_skillmanag_328d03_pinned_guard(const char *name,
                                                  const struct json_t *essential,
                                                  int pinned);

/* PoP: w14_skillmanag_328d03_read_before_write_guard @ tools/skill_manager_guards.py:_background_review_read_before_write_guard */
struct json_t *w14_skillmanag_328d03_read_before_write_guard(
    const char *name, const char *target, const char *action,
    const char *file_label, int is_background_review, int has_read);

/* PoP: w14_skillmanag_328d03_curator_consolidation_delete_guard @ tools/skill_manager_guards.py:_curator_consolidation_delete_guard */
struct json_t *w14_skillmanag_328d03_curator_consolidation_delete_guard(
    const char *name, const char *absorbed_into, int is_background_review);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SKILLMANAG_328D03_H */
