#ifndef NOC_W20_STREAMCONS_6B51F8_H
#define NOC_W20_STREAMCONS_6B51F8_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/stream_consumer_directives.py (gate w20_streamcons_6b51f8).
 *
 * The module decides whether the tail of a streamed draft may still turn into a
 * hidden attachment, so an interim frame never shows text the next frame hides.
 * Three compiled patterns plus one flag check carry that decision; they are
 * ported here as hand-written scanners over the same grammar (no regex engine),
 * keeping the patterns' exact anchoring, boundary and case rules.
 *
 * ``strip_image_tags_for_display`` stays upstream: it calls
 * BasePlatformAdapter.extract_images() and rewrites matches in the middle of the
 * text, so it needs the delivery extractor, not just a predicate.
 */

/* _PENDING_MEDIA_RE.search(line): an unfinished ``MEDIA:`` directive, or a
 * prefix of ``MEDIA`` at the end that a later delta could still complete. */
int w20_streamcons_6b51f8_pending_media(const char *line);

/* _PENDING_FLAG_RE.search(line): the matched text (``[``...), or NULL when the
 * pattern does not match. Malloc'd; caller frees. */
char *w20_streamcons_6b51f8_pending_flag(const char *line);

/* `bool(flag) and any(f.startswith(flag) for f in _STREAM_FLAGS)` -- a partial
 * ``[[audio_as_voice]]`` / ``[[as_document]]`` opener. */
int w20_streamcons_6b51f8_flag_opens_stream(const char *line);

/* _PENDING_IMAGE_RE.search(text): the end of the text is still a prefix of an
 * image tag ``extract_images`` could hide (``![alt``, ``![alt](http``,
 * ``<img src=...``, ``</img``). Runs over the whole text, not one line: an
 * image tag's alt text may span several. */
int w20_streamcons_6b51f8_pending_image(const char *text);

/* PoP: w20_streamcons_6b51f8_has_pending @ gateway/stream_consumer_directives.py:has_pending_media_directive
 * The module entry point: the last line's media/flag prefixes plus the whole
 * text's image-tag prefix. NULL text reads as "". */
int w20_streamcons_6b51f8_has_pending(const char *cleaned);

/* `cleaned.rsplit("\n", 1)[-1]` -- the line a directive still being typed lives
 * on. Malloc'd; caller frees. */
char *w20_streamcons_6b51f8_last_line(const char *cleaned);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_STREAMCONS_6B51F8_H */
