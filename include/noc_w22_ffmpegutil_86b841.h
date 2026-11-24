#ifndef NOC_W22_FFMPEGUTIL_86B841_H
#define NOC_W22_FFMPEGUTIL_86B841_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/discord/ffmpeg_utils.py (gate w22_ffmpegutil_86b841).
 *
 * `resolve_ffmpeg_executable()` layers an explicit FFMPEG_PATH override and the
 * Windows winget install layout (which never touches PATH) on top of the shared
 * repo-wide ffmpeg lookup:
 *
 *   explicit  = (FFMPEG_PATH or "").strip()  -> expanduser -> expandvars
 *   shared    = _shared_find_ffmpeg()        (the repo-wide lookup; a seam)
 *   winget    = <LOCALAPPDATA>/Microsoft/WinGet/Packages/Gyan.FFmpeg_* / * /
 *               bin/ffmpeg.exe, sorted, last one wins
 *   fallback  = "ffmpeg"
 *
 * The environment is injected as values rather than read from the process so a
 * fixture drives it (Windows CRT `setenv(name,"",1)` unsets, Python keeps "").
 * The winget scan is a real directory walk of the LOCALAPPDATA handed in.
 */

/* PoP: w22_ffmpegutil_86b841_expand_path @ plugins/platforms/discord/ffmpeg_utils.py:resolve_ffmpeg_executable
 * os.path.expandvars(os.path.expanduser(value)): `~` expands from USERPROFILE
 * (or HOMEDRIVE+HOMEPATH), `$NAME`, `${NAME}` and `%NAME%` from the process
 * environment. An unknown variable is left exactly as written. Result is a
 * malloc'd NUL-terminated string, never NULL. */
char *w22_ffmpegutil_86b841_expand_path(const char *value);

/* PoP: w22_ffmpegutil_86b841_resolve @ plugins/platforms/discord/ffmpeg_utils.py:resolve_ffmpeg_executable
 * The ffmpeg command to run. ffmpeg_path_env is FFMPEG_PATH (NULL = unset),
 * shared_found is the shared lookup result (NULL/"" = nothing found) and
 * local_appdata is LOCALAPPDATA (NULL/"" = unset). Result is malloc'd. */
char *w22_ffmpegutil_86b841_resolve(const char *ffmpeg_path_env,
                                    const char *shared_found,
                                    const char *local_appdata);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_FFMPEGUTIL_86B841_H */
