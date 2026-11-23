#ifndef NOC_W6_RUNVOICE_5CC499_H
#define NOC_W6_RUNVOICE_5CC499_H
#include <stddef.h>
/* <platform>:<chat> key, namespaced by profile except for the default profile. */
/* PoP: w6_runvoice_5cc499_voice_key @ gateway/run_voice.py:_voice_key */
char *w6_runvoice_5cc499_voice_key(const char *platform_value, const char *chat_id, const char *profile);
/* True for a pre-#12542 bare chat_id key (no valid platform prefix). */
/* PoP: w6_runvoice_5cc499_is_legacy_voice_key @ gateway/run_voice.py:_is_legacy_voice_key */
int w6_runvoice_5cc499_is_legacy_voice_key(const char *key);
void w6_runvoice_5cc499_free_str(char *s);
#endif
