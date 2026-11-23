/*
 * base_media_flags.c — C port of upstream Hermes gateway/platforms/base_media_flags.py.
 * PoP: mediapl_pad_inline_flags @ gateway/platforms/base_media_flags.py:pad_inline_flags
 */
#include "noc_mediapl.h"
#include "hermes_json.h"

json_t *mediapl_pad_inline_flags(json_t *flags, json_t *media_urls) {
    json_t *out = json_array();
    json_t *farr = (flags && flags->type == JSON_ARRAY) ? flags : NULL;
    size_t nflags = farr ? json_len(farr) : 0;
    json_t *urls = (media_urls && media_urls->type == JSON_ARRAY) ? media_urls : NULL;
    size_t nurls = urls ? json_len(urls) : 0;

    /* flags = list(getattr(event, "media_text_inlined", []) or []) */
    for (size_t i = 0; i < nflags; i++) {
        json_t *v = json_get(farr, i);
        json_append(out, v ? json_copy(v) : json_null());
    }
    /* flags + [None] * (len(event.media_urls) - len(flags)) */
    if (nurls > nflags) {
        for (size_t i = nflags; i < nurls; i++) json_append(out, json_null());
    }
    return out;
}
