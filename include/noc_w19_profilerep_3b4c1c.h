/* noc_w19_profilerep_3b4c1c.h - public C API for persistence/sqlite/admin/profile_repair.py. */
#ifndef NOC_W19_PROFILEREP_3B4C1C_H
#define NOC_W19_PROFILEREP_3B4C1C_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_profilerep_3b4c1c_stored_profile @ persistence/sqlite/admin/profile_repair.py:_stored_profile */
json_t *w19_profilerep_3b4c1c_stored_profile(json_t *value);
/* PoP: w19_profilerep_3b4c1c_find_crossed @ persistence/sqlite/admin/profile_repair.py:find_crossed_profile_sessions */
json_t *w19_profilerep_3b4c1c_find_crossed(json_t *rows, const char *owner, json_t *ledgered);
/* PoP: w19_profilerep_3b4c1c_less_topic_rows @ persistence/sqlite/admin/profile_repair.py:find_profile_less_telegram_topic_rows */
json_t *w19_profilerep_3b4c1c_less_topic_rows(json_t *rows, int has_profile);
/* PoP: w19_profilerep_3b4c1c_key_profiles @ persistence/sqlite/admin/profile_repair.py:key_profiles_for_chat */
json_t *w19_profilerep_3b4c1c_key_profiles(json_t *session_keys);
/* PoP: w19_profilerep_3b4c1c_import_moved @ persistence/sqlite/admin/profile_repair.py:import_moved_session */
json_t *w19_profilerep_3b4c1c_import_moved(json_t *session, const char *profile_name,
                                          int id_present, int parent_present, int title_present);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_PROFILEREP_3B4C1C_H */
