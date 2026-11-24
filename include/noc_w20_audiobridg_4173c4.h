#ifndef NOC_W20_AUDIOBRIDG_4173C4_H
#define NOC_W20_AUDIOBRIDG_4173C4_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/google_meet/audio_bridge.py (gate w20_audiobridg_4173c4).
 *
 * pactl/system_profiler are seams: `setup` takes the scripted process results
 * and answers the same dict (or RuntimeError) the live class produces, plus the
 * argv of every process it would have spawned. Teardown never raises.
 */

/* PoP: w20_audiobridg_4173c4_parse_module_id @ plugins/google_meet/audio_bridge.py:AudioBridge._parse_module_id
 * The last token of the first line, as an int:
 *   {"status":"ok","value":N} | {"status":"raised","type":...,"message":...} */
json_t *w20_audiobridg_4173c4_parse_module_id(const char *stdout_text);

/* PoP: w20_audiobridg_4173c4_setup @ plugins/google_meet/audio_bridge.py:AudioBridge.setup
 * `system` is platform.system(); `script` scripts the processes:
 *   {"prefix":"hermes_meet",
 *    "sink":{"missing":false,"rc":0,"stdout":"5","stderr":""},
 *    "src":{"missing":false,"rc":0,"stdout":"7","stderr":""},
 *    "profiler":{"missing":false,"rc":0,"out":"...","err":""}}
 * Answer: {"status":"ok","value":{...},"calls":[argv,...]} |
 *         {"status":"raised","type":...,"message":...,"calls":[...]}. */
json_t *w20_audiobridg_4173c4_setup(const char *system, const json_t *script);

/* PoP: w20_audiobridg_4173c4_teardown @ plugins/google_meet/audio_bridge.py:AudioBridge.teardown
 * reversed(module_ids), each unload suppressed: {"unloaded":["3","1"]}. */
json_t *w20_audiobridg_4173c4_teardown(const json_t *module_ids);

/* PoP: w20_audiobridg_4173c4_finish @ plugins/google_meet/audio_bridge.py:AudioBridge._finish
 * The setup() dict: platform, device_name, sample_rate 48000, channels 2,
 * module_ids (a copy) and write_target. */
json_t *w20_audiobridg_4173c4_finish(const char *tag, const char *device,
                                     const char *write_target, const json_t *module_ids);

/* PoP: w20_audiobridg_4173c4_ready @ plugins/google_meet/audio_bridge.py:AudioBridge._ready
 * device_name / write_target before setup(): RuntimeError("AudioBridge not
 * set up yet"). */
json_t *w20_audiobridg_4173c4_ready(const json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_AUDIOBRIDG_4173C4_H */
