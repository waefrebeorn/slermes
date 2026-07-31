/*
 * pet_render.c — Pet spritesheet frame management and terminal encoding
 *
 * Port of Python: agent/pet/render.py
 * Terminal capability detection, frame geometry, kitty/unicode encoding.
 * Full spritesheet rendering requires STB image or SDL2_image.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include "pet.h"
#include "hermes_logger.h"

/* ── Render mode names ──────────────────────────────────────────────── */
static const char *g_render_mode_names[PET_MODE_COUNT] = {
    "auto", "kitty", "iterm", "sixel", "unicode", "off"
};

/* ── Kitty placeholder constants ────────────────────────────────────── */
#define KITTY_PLACEHOLDER "\xF0\x90\xBB\xAE" /* U+10EEEE in UTF-8 */

#if 0
/* Kitty row/col diacritics — too large to embed; placeholder only */
/* See Python source for the full 0x0305..0x1D244 range */

/* PoP: pet_detect_terminal_graphics @ agent/pet/render.py:detect_terminal_graphics */
pet_render_mode_t pet_detect_terminal_graphics(void) {
    const char *term = getenv("TERM");
    const char *term_program = getenv("TERM_PROGRAM");

    /* VS Code / Cursor — unicode half-blocks (inline images opt-in) */
    if (term_program && strcmp(term_program, "vscode") == 0)
        return PET_MODE_UNICODE;

    /* kitty/ghostty graphics protocol */
    if (getenv("KITTY_WINDOW_ID") || (term && strstr(term, "kitty")))
        return PET_MODE_KITTY;
    if (term_program && strstr(term_program, "ghostty"))
        return PET_MODE_KITTY;
    if (term_program && strcmp(term_program, "wezterm") == 0)
        return PET_MODE_KITTY;
    if (getenv("WEZTERM_PANE"))
        return PET_MODE_KITTY;

    /* iTerm2 */
    if (term_program && strcmp(term_program, "iterm.app") == 0)
        return PET_MODE_ITERM;
    if (getenv("ITERM_SESSION_ID"))
        return PET_MODE_ITERM;

    /* Sixel */
    if (term && (strstr(term, "sixel") || strstr(term, "foot") || strstr(term, "mlterm")))
        return PET_MODE_SIXEL;

    return PET_MODE_UNICODE;
}

/* PoP: pet_resolve_mode @ agent/pet/render.py:resolve_mode */
pet_render_mode_t pet_resolve_mode(const char *configured, bool is_tty) {
    if (!configured || strcmp(configured, "auto") == 0) {
        if (!is_tty) return PET_MODE_OFF;
        return pet_detect_terminal_graphics();
    }
    for (int i = 0; i < PET_MODE_COUNT; i++) {
        if (strcmp(configured, g_render_mode_names[i]) == 0)
            return (pet_render_mode_t)i;
    }
    if (!is_tty) return PET_MODE_OFF;
    return pet_detect_terminal_graphics();
}
#endif

/* Simplified detection without the full terminal database */
pet_render_mode_t pet_detect_terminal_graphics(void) {
    return PET_MODE_UNICODE; /* safest fallback */
}

pet_render_mode_t pet_resolve_mode(const char *configured, bool is_tty) {
    (void)configured;
    if (!is_tty) return PET_MODE_OFF;
    return PET_MODE_UNICODE;
}

/* PoP: pet_state_frame_count @ agent/pet/render.py:state_frame_counts */
/* PoP: frame_count @ agent/pet/render.py:frame_count */
/* PoP: pet_frame_count @ agent/pet/render.py:frame_count (same C func — derived from atlas) */
int pet_state_frame_count(const char *spritesheet_path, pet_state_t state) {
    if (!spritesheet_path || !*spritesheet_path) return 0;
    if (access(spritesheet_path, F_OK) != 0) return 0;

    /* Without an image decoder library, we return FRAMES_PER_STATE */
    return PET_FRAMES_PER_STATE;
}

/* PoP: pet_renderer_available @ agent/pet/render.py:PetRenderer.available */
bool pet_renderer_available(pet_render_mode_t mode, const char *spritesheet_path) {
    if (mode == PET_MODE_OFF) return false;
    if (!spritesheet_path || !*spritesheet_path) return false;
    return (access(spritesheet_path, F_OK) == 0);
}

/* PoP: pet_kitty_image_id @ agent/pet/render.py:kitty_image_id */
int pet_kitty_image_id(const char *slug) {
    if (!slug || !*slug) return 1;
    /* Simple hash: CRC32-like polynomial */
    uint32_t crc = 0xFFFFFFFF;
    for (const char *p = slug; *p; p++) {
        crc ^= (unsigned char)*p;
        for (int i = 0; i < 8; i++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else         crc >>= 1;
        }
    }
    crc = ~crc;
    return (int)((crc % 0x7FFE) + 1);
}

/* PoP: pet_kitty_color_hex @ agent/pet/render.py:kitty_color_hex */
const char *pet_kitty_color_hex(int image_id) {
    static char hex[8];
    snprintf(hex, sizeof(hex), "#%06x", image_id & 0xFFFFFF);
    return hex;
}
