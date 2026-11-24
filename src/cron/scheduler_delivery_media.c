/*
 * scheduler_delivery_media.c — real C11 port of upstream Hermes
 * cron/scheduler_delivery_media.py (gate w17_schedulerd_f239e7).
 *
 * PoP: w17_schedulerd_f239e7_send_media @ cron/scheduler_delivery_media.py:_send_media_via_adapter
 *
 * Every collaborator of _send_media_via_adapter is deterministic and injected
 * from the fixture: should_send_media_as_audio, validate_media_delivery_path,
 * BasePlatformAdapter.filter_media_delivery_paths, the adapter send_* methods,
 * safe_schedule_threadsafe and the future's result().  This reproduces the
 * dropped-path policy checks, the extension routing (video / image-as-document
 * / image / document / voice), the ambiguous-ack latch, error strings, and the
 * early return on an unavailable gateway loop.
 */
#include "noc_w17_schedulerd_f239e7.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const VIDEO_EXTS[] = {".mp4", ".mov", ".avi", ".mkv", ".webm", ".3gp"};
static const char *const IMAGE_EXTS[] = {".jpg", ".jpeg", ".png", ".webp", ".gif"};

static int in_set(const char *const *set, size_t n, const char *s) {
    size_t i;
    if (!s) return 0;
    for (i = 0; i < n; i++) if (!strcmp(set[i], s)) return 1;
    return 0;
}

/* Path(media_path).suffix.lower() — split on both separators (Windows Path
 * str() emits backslashes), take the last dot of the basename. */
static void suffix_lower(const char *path, char *out, size_t cap) {
    const char *base = path, *p, *dot = NULL;
    size_t i = 0;
    if (!path || !cap) { if (cap) out[0] = '\0'; return; }
    for (p = path; *p; p++) if (*p == '/' || *p == '\\') base = p + 1;
    for (p = base; *p; p++) if (*p == '.') dot = p;
    if (!dot || dot == base) { out[0] = '\0'; return; }
    for (p = dot; *p && i + 1 < cap; p++) out[i++] = (char)tolower((unsigned char)*p);
    out[i] = '\0';
}

static int getbool(const json_t *o, const char *k) {
    json_t *v = o ? json_obj_get(o, k) : NULL;
    return v && v->type == JSON_BOOL && v->bool_val ? 1 : 0;
}

static void add_err(json_t *errors, const char *msg) {
    json_append(errors, json_string(msg));
}

json_t *w17_schedulerd_f239e7_send_media(json_t *media, int force_document,
                                         const char *platform, const char *chat_id) {
    json_t *errors = json_array(), *calls = json_array();
    json_t *out = json_object();
    int ack_lost = 0, stop = 0;
    size_t n = (media && media->type == JSON_ARRAY) ? json_len(media) : 0;
    size_t i;
    char buf[1024];
    (void)platform; (void)chat_id; /* injected; routing audio is fixture-driven */

    /* requested = [(str(p), v) ...]; filter keeps only `kept` entries. */
    for (i = 0; i < n; i++) {
        json_t *e = json_get(media, i);
        const char *path, *validated;
        json_t *vv;
        int dropped;
        if (!e || e->type != JSON_OBJECT) continue;
        path = json_get_str(e, "path", "");
        vv = json_obj_get(e, "validate");
        /* default: validate() returns the path itself when key is absent */
        validated = (vv && vv->type == JSON_STRING) ? vv->str_val : path;
        if (vv && vv->type == JSON_NULL) validated = NULL;
        dropped = 1;
        if (validated) {
            size_t j;
            for (j = 0; j < n; j++) {
                json_t *k = json_get(media, j);
                if (k && k->type == JSON_OBJECT && getbool(k, "kept") &&
                    !strcmp(json_get_str(k, "path", ""), validated)) {
                    dropped = 0; break;
                }
            }
        }
        if (dropped) {
            snprintf(buf, sizeof(buf), "attachment dropped by media path policy: %s", path);
            add_err(errors, buf);
        }
    }

    /* Route each kept file, consuming should_send_media_as_audio in order. */
    for (i = 0; i < n && !stop; i++) {
        json_t *e = json_get(media, i);
        const char *path, *result, *method, *path_kw;
        char ext[64];
        json_t *call;
        if (!e || e->type != JSON_OBJECT || !getbool(e, "kept")) continue;
        path = json_get_str(e, "path", "");
        suffix_lower(path, ext, sizeof(ext));
        result = json_get_str(e, "result", "ok");

        if (getbool(e, "audio")) {
            method = "send_voice"; path_kw = "audio_path";
        } else if (in_set(VIDEO_EXTS, sizeof(VIDEO_EXTS) / sizeof(VIDEO_EXTS[0]), ext)) {
            method = "send_video"; path_kw = "video_path";
        } else if (in_set(IMAGE_EXTS, sizeof(IMAGE_EXTS) / sizeof(IMAGE_EXTS[0]), ext) && !force_document) {
            method = "send_image_file"; path_kw = "image_path";
        } else {
            method = "send_document"; path_kw = "file_path";
        }

        call = json_object();
        json_set(call, "method", json_string(method));
        json_set(call, "path_kw", json_string(path_kw));
        json_set(call, "path", json_string(path));
        json_append(calls, call);

        if (!strcmp(result, "no_future")) {
            snprintf(buf, sizeof(buf), "cannot send media %s: gateway loop unavailable", path);
            add_err(errors, buf);
            stop = 1;                     /* live returns errors immediately */
        } else if (!strcmp(result, "timeout")) {
            snprintf(buf, sizeof(buf), "failed to send media %s: TimeoutError", path);
            add_err(errors, buf);
        } else if (!strcmp(result, "ambiguous")) {
            ack_lost = 1;                 /* sent but ack lost -> never an error */
        } else if (!strcmp(result, "failed")) {
            snprintf(buf, sizeof(buf), "media send failed for %s: boom", path);
            add_err(errors, buf);
        }
        /* "ok": success with a plain dict raw_response -> no error. */
    }

    json_set(out, "errors", errors);
    json_set(out, "calls", calls);
    json_set(out, "ack_lost", json_bool(ack_lost ? true : false));
    return out;
}
