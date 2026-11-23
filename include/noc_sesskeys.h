#ifndef NOC_SESSKEYS_H
#define NOC_SESSKEYS_H
#include "hermes_json.h"
/* Both return malloc'd strings (caller frees); NULL represents Python None. */
/* PoP: sesskeys_profile_from_session_key_namespace @ persistence/core/keys.py:profile_from_session_key_namespace */
char *sesskeys_profile_from_session_key_namespace(const char *ns);
/* PoP: sesskeys_session_key_profile @ persistence/core/keys.py:session_key_profile */
char *sesskeys_session_key_profile(const char *session_key);
#endif
