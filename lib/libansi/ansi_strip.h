#ifndef HERMES_ANSI_STRIP_H
#define HERMES_ANSI_STRIP_H

/*
 * strip_ansi.h — Strip ANSI escape sequences from text.
 * Port of Python tools/strip_ansi.py.
 *
 * Implements the full ECMA-48 spec: CSI (including private-mode ? prefix,
 * colon-separated params, intermediate bytes), OSC (BEL and ST terminators),
 * DCS/SOS/PM/APC string sequences, nF multi-byte escapes, Fp/Fe/Fs
 * single-byte escapes, 8-bit C1 control characters.
 *
 * Uses a byte-by-byte state machine — no regex overhead.
 *
 * Usage:
 *   char *clean = strip_ansi("hello \033[31mworld\033[0m");
 *   // clean == "hello world"
 *   free(clean);
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Port of Python tools/ansi_strip.py:strip_ansi(). */
/* Strip ANSI escape sequences from a NUL-terminated string.
 * Returns a malloc'd string with all ANSI sequences removed.
 * On NULL input returns NULL.
 * On allocation failure returns NULL (caller should check). */
char *strip_ansi(const char *text);

/* Port of Python tools/ansi_strip.py:sanitize_display_text().
 * Returns a malloc'd string with ANSI escapes AND bare C0 control
 * characters removed (preserving only newlines and tabs; CR is
 * normalized to LF). On NULL input returns NULL. */
char *sanitize_display_text(const char *text);

/* Port of Python tools/ansi_strip.py:strip_unicode_tags().
 * Removes invisible Unicode TAG characters (U+E0000–U+E007F) from a
 * NUL-terminated UTF-8 string. Valid emoji tag sequences
 * (U+1F3F4 base + U+E0020..U+E007E spec + U+E007F CANCEL TAG) are
 * preserved. Returns a malloc'd string; on NULL input returns NULL. */
char *strip_unicode_tags(const char *text);

/* Same as strip_unicode_tags() but writes into a caller-provided buffer.
 * buf must be at least (strlen(text) + 1) bytes. Returns buf, or NULL on
 * failure. Safe to call with buf == text (in-place). */
char *strip_unicode_tags_buf(const char *text, char *buf, size_t buf_size);

/* Same as strip_ansi() but writes into a caller-provided buffer.
 * buf must be at least (strlen(text) + 1) bytes.
 * Returns buf on success, NULL on failure.
 * Safe to call with buf == text (in-place stripping). */
char *ansi_strip_buf(const char *text, char *buf, size_t buf_size);

/* Fast-path check: returns true if text contains any ESC or C1 byte.
 * Useful when you want to skip the full strip on clean text. */
bool ansi_has_escape(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ANSI_STRIP_H */
