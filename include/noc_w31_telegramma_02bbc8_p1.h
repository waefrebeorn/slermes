/*
 * noc_w31_telegramma_02bbc8_p1.h - C surface for upstream
 * plugins/platforms/telegram/telegram_markdown.py (gate
 * w31_telegramma_02bbc8_p1): the MarkdownV2 send path.
 *
 * ``to_markdown_v2`` is a pure value->value function: it takes the reply
 * markdown and returns the MarkdownV2 text Telegram accepts.  Everything the
 * gateway does around it (sending, retries, message splitting) is a seam and
 * stays in the caller.
 */
#ifndef NOC_W31_TELEGRAMMA_02BBC8_P1_H
#define NOC_W31_TELEGRAMMA_02BBC8_P1_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* markdown -> Telegram MarkdownV2.  Returns a malloc'd UTF-8 string (the empty
 * string for empty input); the caller frees it. */
char *w31_telegramma_02bbc8_p1_to_markdown_v2(const char *content);

/* Step 0's two cross-module passes, as declared seams (see the port file):
 * ``normalize_latex_macros`` (plugins/platforms/telegram/telegram_latex.py) and
 * ``convert_table_to_bullets`` (gateway/platforms/helpers.py), both already C
 * homes of their own gates.  A hook returns a malloc'd string, or NULL to mean
 * "unchanged".  Unset hooks leave the text as written. */
typedef char *(*w31_telegramma_02bbc8_p1_prose_fn)(const char *text, void *ctx);
void w31_telegramma_02bbc8_p1_set_seams(w31_telegramma_02bbc8_p1_prose_fn latex,
                                        w31_telegramma_02bbc8_p1_prose_fn tables,
                                        void *ctx);

#ifdef __cplusplus
}
#endif
#endif
