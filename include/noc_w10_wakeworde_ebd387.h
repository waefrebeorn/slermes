#ifndef NOC_W10_WAKEWORDE_EBD387_H
#define NOC_W10_WAKEWORDE_EBD387_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure helpers / process algorithms of tools/wake_word_engines.py.
 * os.path.exists, os.sep and get_hermes_home are injected as fixture seams. */
json_t *w10_wakeworde_ebd387_sub(const json_t *cfg, const char *key);
json_t *w10_wakeworde_ebd387_looks_like_path(const char *value, int exists, const char *sep);
json_t *w10_wakeworde_ebd387_sherpa_model_root(const char *home);
json_t *w10_wakeworde_ebd387_openwake_process(const json_t *scores, double threshold, int confirm_needed, int initial_streak);
json_t *w10_wakeworde_ebd387_sherpa_process(const json_t *results, const json_t *display_to_profile);
json_t *w10_wakeworde_ebd387_porcupine_process(const json_t *values);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_WAKEWORDE_EBD387_H */
