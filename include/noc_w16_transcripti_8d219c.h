#ifndef NOC_W16_TRANSCRIPTI_8D219C_H
#define NOC_W16_TRANSCRIPTI_8D219C_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of tools/transcription_chunking.py (gate w16_transcripti_8d219c).
 * Filesystem size / ffprobe duration are injected as fixture seams. */

json_t *w16_transcripti_8d219c_upload_limit(const char *provider, const char *model,
                                            double max_file_size);
json_t *w16_transcripti_8d219c_fits(double size, double max_bytes,
                                    int have_max_seconds, double max_seconds,
                                    int have_duration, double duration);
json_t *w16_transcripti_8d219c_exceeds_upload_limit(double size, double max_bytes,
                                                   int have_max_seconds, double max_seconds,
                                                   int have_probe_duration, double probe_duration);
json_t *w16_transcripti_8d219c_cut_points(double duration, double target, const json_t *silences);
json_t *w16_transcripti_8d219c_too_large(double size, double max_bytes, const char *reason);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_TRANSCRIPTI_8D219C_H */
