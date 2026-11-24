/*
 * transcription_chunking.c - C home for upstream Hermes tools/transcription_chunking.py.
 *
 * PoP: w16_transcripti_8d219c_upload_limit @ tools/transcription_chunking.py:upload_limit
 * PoP: w16_transcripti_8d219c_fits @ tools/transcription_chunking.py:_fits
 * PoP: w16_transcripti_8d219c_exceeds_upload_limit @ tools/transcription_chunking.py:exceeds_upload_limit
 * PoP: w16_transcripti_8d219c_cut_points @ tools/transcription_chunking.py:_cut_points
 * PoP: w16_transcripti_8d219c_too_large @ tools/transcription_chunking.py:_too_large
 *
 * os.path.getsize() and ffprobe duration are supplied as fixture seams; all
 * table lookups, comparisons and cut-point maths match the live module.
 */
#include "noc_w16_transcripti_8d219c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MB (1024.0 * 1024.0)

static int fits_calc(double size, double max_bytes,
                     int have_max_seconds, double max_seconds,
                     int have_duration, double duration) {
    if (size > max_bytes) return 0;
    return !have_max_seconds || !have_duration || duration <= max_seconds;
}

json_t *w16_transcripti_8d219c_upload_limit(const char *provider, const char *model,
                                            double max_file_size) {
    double max_bytes;
    int have_seconds = 0;
    double max_seconds = 0;
    if (!strcmp(provider, "openai")) {
        max_bytes = 25 * MB;
        const char *m = model ? model : "";
        if (!strcmp(m, "gpt-4o-transcribe") || !strcmp(m, "gpt-4o-mini-transcribe")) {
            have_seconds = 1; max_seconds = 450.0;
        } else if (!strcmp(m, "whisper-1")) {
            have_seconds = 1; max_seconds = 600.0;
        }
    } else if (!strcmp(provider, "groq")) {
        max_bytes = 25 * MB;
    } else if (!strcmp(provider, "mistral")) {
        max_bytes = 500 * MB; have_seconds = 1; max_seconds = 3600.0;
    } else if (!strcmp(provider, "xai")) {
        max_bytes = 500 * MB;
    } else if (!strcmp(provider, "elevenlabs")) {
        max_bytes = 5 * 1024 * MB - 1;
    } else {
        max_bytes = max_file_size;
    }
    json_t *out = json_object();
    json_set(out, "max_bytes", json_number(max_bytes));
    json_set(out, "max_seconds", have_seconds ? json_number(max_seconds) : json_null());
    return out;
}

json_t *w16_transcripti_8d219c_fits(double size, double max_bytes,
                                    int have_max_seconds, double max_seconds,
                                    int have_duration, double duration) {
    return json_bool(fits_calc(size, max_bytes, have_max_seconds, max_seconds,
                               have_duration, duration) ? true : false);
}
json_t *w16_transcripti_8d219c_exceeds_upload_limit(double size, double max_bytes,
                                                   int have_max_seconds, double max_seconds,
                                                   int have_probe_duration, double probe_duration) {
    if (size > max_bytes) return json_bool(true);
    int exceeds = have_max_seconds &&
        !fits_calc(size, max_bytes, have_max_seconds, max_seconds,
                   have_probe_duration, probe_duration);
    return json_bool(exceeds ? true : false);
}

json_t *w16_transcripti_8d219c_cut_points(double duration, double target, const json_t *silences) {
    json_t *out = json_array();
    if (!silences || silences->type != JSON_ARRAY || target <= 0) return out;
    double position = 0.0;
    double lookback = 60.0 < target / 4.0 ? 60.0 : target / 4.0;
    /* guard against a non-terminating loop when target is not positive */
    int guard = 0;
    while (duration - position > target && guard++ < 100000) {
        double ideal = position + target;
        int found = 0;
        double best = 0;
        for (size_t i = 0; i < json_len(silences); ++i) {
            json_t *v = json_get(silences, i);
            if (!v || v->type != JSON_NUMBER) continue;
            double s = v->num_val;
            if (ideal - lookback <= s && s <= ideal && s > position && (!found || s > best)) {
                best = s; found = 1;
            }
        }
        position = found ? best : ideal;
        json_append(out, json_number(position));
    }
    return out;
}

json_t *w16_transcripti_8d219c_too_large(double size, double max_bytes, const char *reason) {
    char msg[1024];
    snprintf(msg, sizeof(msg),
             "File too large: %.1fMB (max %.0fMB) and it could not be split: %s",
             size / MB, max_bytes / MB, reason);
    json_t *out = json_object();
    json_set(out, "success", json_bool(false));
    json_set(out, "transcript", json_string(""));
    json_set(out, "error", json_string(msg));
    return out;
}
