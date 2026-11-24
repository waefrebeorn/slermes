/*
 * noc_w21_adapteropu_d51886.h -- once-per-process opus codec probe for Discord
 * voice playback (port of plugins/platforms/discord/adapter_opus.py).
 *
 * PoP: w21_adapteropu_d51886_bundled_opus_path @ plugins/platforms/discord/adapter_opus.py:_find_discord_windows_bundled_opus
 * PoP: w21_adapteropu_d51886_load_opus_codec_plan @ plugins/platforms/discord/adapter_opus.py:load_opus_codec
 *
 * The module's collaborators are the OS/library boundary: sys.platform,
 * struct.calcsize("P"), ctypes.util.find_library, os.path.isfile / Path.is_file
 * and discord.opus itself. The port takes each of those as input and keeps the
 * module's own decisions: the bundled-DLL naming, the candidate ORDER (bundled,
 * then find_library, and the Homebrew paths only when find_library missed), the
 * once-per-process guard, the per-candidate try/except, and the single
 * "codec not found" warning.
 */
#ifndef NOC_W21_ADAPTEROPU_D51886_H
#define NOC_W21_ADAPTEROPU_D51886_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _find_discord_windows_bundled_opus(discord): discord.py's bundled Windows
 * opus DLL, or null. *files* is the set of paths that exist (os.path.isfile).
 * Returns null off win32, when opus.module.__file__ is missing, or when the
 * bin/libopus-0.<arch>.dll is not there. */
json_t *w21_adapteropu_d51886_bundled_opus_path(const json_t *opus_file,
                                               const json_t *platform,
                                               int ptr_bits,
                                               const json_t *files);

/* load_opus_codec() as an ordered plan. Input keys:
 *   platform, opus_file, ptr_bits, files[], find_library,
 *   already_loaded (bool), probed (bool), load_script (["ok"|"fail"|"no"...])
 * -> {"attempts": [path...], "load_failures": N, "not_found_warning": bool,
 *     "returned_early": bool} */
json_t *w21_adapteropu_d51886_load_opus_codec_plan(const json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_ADAPTEROPU_D51886_H */
