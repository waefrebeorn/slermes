/*
 * clipboard.h — Platform clipboard read/write for C11 desktop app
 *
 * Provides cross-platform clipboard access.
 * Linux:   xclip/xsel (Wayland: wl-clipboard)
 * macOS:   pbcopy/pbpaste
 * Windows: Win32 Clipboard API
 *
 * PoP: clipboard_read  @ electron/main.cjs:readClipboard
 * PoP: clipboard_write @ electron/main.cjs:writeClipboard
 */

#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────────────── */
#define CLIPBOARD_MAX_TEXT 1048576  /* 1MB max clipboard text */

/* ── Text clipboard ────────────────────────────────────────────────────── */

/* PoP: clipboard_read @ electron/main.cjs:readClipboard */
/* Read text from the system clipboard.
 * Returns allocated string (caller must free), or NULL on error/empty. */
char *clipboard_read_text(void);

/* PoP: clipboard_write @ electron/main.cjs:writeClipboard */
/* Write text to the system clipboard.
 * Returns true on success. */
bool clipboard_write_text(const char *text);

/* ── Image clipboard (stub) ─────────────────────────────────────────────── */

/* Read image from clipboard as PNG data.
 * Returns allocated buffer (caller must free), sets *out_len. */
void *clipboard_read_image(size_t *out_len);

/* Write PNG image data to clipboard. */
bool clipboard_write_image(const void *png_data, size_t len);

/* ── Clipboard availability ─────────────────────────────────────────────── */

/* Check if clipboard is available (not all headless environments have one). */
bool clipboard_available(void);

/* Check if clipboard contains text. */
bool clipboard_has_text(void);

#ifdef __cplusplus
}
#endif

#endif /* CLIPBOARD_H */
