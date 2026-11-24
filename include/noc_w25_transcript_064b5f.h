/*
 * noc_w25_transcript_064b5f.h - C surface for upstream
 * tools/transcription_whisper_cpp.py (gate w25_transcript_064b5f).
 *
 * The pure value core of the managed whisper.cpp engine: the model artifact
 * table, the profile cache directory, the shlex-quoted command template and the
 * model-name gate.  The package manager, the profile home and the download are
 * seams: this file owns only what a fixed binary/models dir decides.
 */
#ifndef NOC_W25_TRANSCRIPT_064B5F_H
#define NOC_W25_TRANSCRIPT_064B5F_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req {"path": <str>} -> shlex.quote(Path(path).as_posix()). */
json_t *w25_transcript_064b5f_quote(const json_t *req);

/* req {"home": <str>} -> (Path(home) / "cache" / "whisper.cpp").as_posix(). */
json_t *w25_transcript_064b5f_model_dir(const json_t *req);

/* req {"binary": <str|null>, "models_dir": <str>, "language": <str>} ->
 * the whisper.cpp command template, or null when the engine is not installed. */
json_t *w25_transcript_064b5f_command(const json_t *req);

/* req {"model_name": <any>} -> null when supported, else the ValueError text. */
json_t *w25_transcript_064b5f_validate_model(const json_t *req);

/* _MODEL_FILES as [{"name","url","sha256"}, ...]. */
json_t *w25_transcript_064b5f_model_files(void);

#ifdef __cplusplus
}
#endif
#endif
