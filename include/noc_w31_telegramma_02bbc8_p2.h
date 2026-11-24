/*
 * noc_w31_telegramma_02bbc8_p2.h - C surface for upstream
 * plugins/platforms/telegram/telegram_markdown.py (gate
 * w31_telegramma_02bbc8_p2): the pure text primitives the MarkdownV2 path is
 * built from - the escaper, the URL-tail trimmer and the two predicates that
 * decide what may become a link and where a blockquote edge falls.
 */
#ifndef NOC_W31_TELEGRAMMA_02BBC8_P2_H
#define NOC_W31_TELEGRAMMA_02BBC8_P2_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Escape every MarkdownV2 special character with a preceding backslash. */
char *w31_telegramma_02bbc8_p2_escape_mdv2(const char *text);

/* Length of ``url`` without trailing prose punctuation; a ``)`` survives only
 * while it closes a ``(``. */
long w31_telegramma_02bbc8_p2_url_end(const char *url);

/* True when Telegram can open the link target (a scheme URL or a bare domain). */
int w31_telegramma_02bbc8_p2_link_target_renders(const char *target);

/* True when a newline in text[start:end] borders a blockquote line.
 * start/end are byte offsets into the UTF-8 text. */
int w31_telegramma_02bbc8_p2_crosses_quote_edge(const char *text, long start, long end);

#ifdef __cplusplus
}
#endif
#endif
