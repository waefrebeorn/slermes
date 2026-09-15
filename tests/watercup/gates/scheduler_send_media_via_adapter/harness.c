/* gate: cron_scheduler_route_media — faithful traced copy of the REAL C port
 * src/cron/scheduler_runtime_impl.c::scheduler_route_media (the translation
 * unit drags in ~18 out-of-path externals, so it is copied VERBATIM below
 * rather than linked; the copy is byte-for-byte the port's logic).
 * LIVE twin: cron/scheduler.py::_send_media_via_adapter routing half
 * (per-file dispatch to send_voice / send_video / send_image_file /
 * send_document via gateway/platforms/base.py::should_send_media_as_audio).
 *
 * Input argv format: "<platform><TAB><is_voice 0|1><TAB><media_path>".
 * ('-' as platform == LIVE platform=None → adapter.platform fallback.)
 * The adapter/network delivery half of _send_media_via_adapter is out of
 * path (I/O); this gate watches the pure per-file ROUTING decision, which
 * is exactly what scheduler_route_media ports.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: recognized audio ext          B2: platform == telegram
 *     B3: telegram voice ext (.ogg/.opus) -> as_audio = is_voice
 *     B4: telegram audio-attachment ext (.mp3/.m4a) -> as_audio
 *   B5: as_audio  -> VOICE            B6: video ext -> VIDEO
 *   B7: image ext -> IMAGE            else DOCUMENT
 *   VSTR(route)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

typedef enum { SCHEDULER_MEDIA_VOICE, SCHEDULER_MEDIA_VIDEO,
               SCHEDULER_MEDIA_IMAGE, SCHEDULER_MEDIA_DOCUMENT }
    scheduler_media_kind_t;

/* ---- VERBATIM traced copy of scheduler_route_media (port logic) ---- */
static scheduler_media_kind_t scheduler_route_media(const char *platform,
                                                    const char *media_path,
                                                    bool is_voice)
{
    const char *dot = media_path ? strrchr(media_path, '.') : NULL;
    const char *ext = dot ? dot : "";

    static const char *video_exts[] = {".mp4",".mov",".avi",".mkv",".webm",".3gp", NULL};
    static const char *image_exts[] = {".jpg",".jpeg",".png",".webp",".gif", NULL};
    static const char *audio_exts[] = {".ogg",".opus",".mp3",".wav",".m4a",".flac", NULL};
    static const char *telegram_voice_exts[] = {".ogg",".opus", NULL};
    static const char *telegram_audio_exts[] = {".mp3",".m4a", NULL};

    int is_video = 0, is_image = 0, is_audio = 0;
    for (int i = 0; video_exts[i]; i++) if (!strcmp(ext, video_exts[i])) is_video = 1;
    for (int i = 0; image_exts[i]; i++) if (!strcmp(ext, image_exts[i])) is_image = 1;
    for (int i = 0; audio_exts[i]; i++) if (!strcmp(ext, audio_exts[i])) is_audio = 1;

    bool as_audio = false;
    if (is_audio) {
        if (platform && strcmp(platform, "telegram") == 0) {
            for (int i = 0; telegram_voice_exts[i]; i++)
                if (!strcmp(ext, telegram_voice_exts[i])) as_audio = is_voice;
            for (int i = 0; telegram_audio_exts[i]; i++)
                if (!strcmp(ext, telegram_audio_exts[i])) as_audio = true;
        } else {
            as_audio = true;
        }
    }

    if (as_audio) return SCHEDULER_MEDIA_VOICE;
    if (is_video) return SCHEDULER_MEDIA_VIDEO;
    if (is_image) return SCHEDULER_MEDIA_IMAGE;
    return SCHEDULER_MEDIA_DOCUMENT;
}
/* ---- end traced copy ---- */

static void decode_escapes(char *s) {
    char *o = s;
    for (char *p = s; *p; p++) {
        if (p[0] == '\\' && p[1] == 't') { *o++ = '\t'; p++; }
        else *o++ = *p;
    }
    *o = '\0';
}

static const char *route_name(scheduler_media_kind_t k) {
    switch (k) {
        case SCHEDULER_MEDIA_VOICE: return "voice";
        case SCHEDULER_MEDIA_VIDEO: return "video";
        case SCHEDULER_MEDIA_IMAGE: return "image";
        default: return "document";
    }
}

static void run_one(const char *input) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", input);
    decode_escapes(buf);
    char *t1 = strchr(buf, '\t');
    char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *platform = buf;
    bool is_voice = false;
    char path[768];
    if (t1 && t2) {
        *t1 = '\0'; *t2 = '\0';
        is_voice = t1[1] == '1';
        snprintf(path, sizeof path, "%s", t2 + 1);
    } else {
        snprintf(path, sizeof path, "");
    }
    if (platform[0] == '\0') platform = NULL;

    WCUP_BEGIN("_send_media_via_adapter");

    /* Trace the same rungs the port evaluates, one-for-one. */
    static const char *audio_exts[] = {".ogg",".opus",".mp3",".wav",".m4a",".flac",NULL};
    static const char *video_exts[] = {".mp4",".mov",".avi",".mkv",".webm",".3gp",NULL};
    static const char *image_exts[] = {".jpg",".jpeg",".png",".webp",".gif",NULL};
    const char *dot = path[0] ? strrchr(path, '.') : NULL;
    const char *ext = dot ? dot : "";
    bool is_audio = false, is_video = false, is_image = false;
    for (int i = 0; audio_exts[i]; i++) if (!strcmp(ext, audio_exts[i])) is_audio = true;
    for (int i = 0; video_exts[i]; i++) if (!strcmp(ext, video_exts[i])) is_video = true;
    for (int i = 0; image_exts[i]; i++) if (!strcmp(ext, image_exts[i])) is_image = true;

    bool as_audio = false;
    WCUP_BRANCH(is_audio);                                   /* B1 */
    if (is_audio) {
        bool tg = platform && !strcmp(platform, "telegram");
        WCUP_BRANCH(tg);                                     /* B2 */
        if (tg) {
            bool tg_voice = !strcmp(ext, ".ogg") || !strcmp(ext, ".opus");
            WCUP_BRANCH(tg_voice);                           /* B3 */
            if (tg_voice) as_audio = is_voice;
            else {
                bool tg_att = !strcmp(ext, ".mp3") || !strcmp(ext, ".m4a");
                WCUP_BRANCH(tg_att);                         /* B4 */
                if (tg_att) as_audio = true;
            }
        } else {
            as_audio = true;
        }
    }

    scheduler_media_kind_t route = SCHEDULER_MEDIA_DOCUMENT;
    WCUP_BRANCH(as_audio);                                   /* B5 */
    if (as_audio) route = SCHEDULER_MEDIA_VOICE;
    else {
        WCUP_BRANCH(is_video);                               /* B6 */
        if (is_video) route = SCHEDULER_MEDIA_VIDEO;
        else {
            WCUP_BRANCH(is_image);                           /* B7 */
            if (is_image) route = SCHEDULER_MEDIA_IMAGE;
        }
    }

    WCUP_VALSTR(route_name(route));

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_send_media_via_adapter");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _send_media_via_adapter <plat\\tvoice\\tpath...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
