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
#include "pngdec.h"

/* ── Render mode names ──────────────────────────────────────────────── */
static const char *g_render_mode_names[PET_MODE_COUNT] = {
    "auto", "kitty", "iterm", "sixel", "unicode", "off"
};

/* PoP: pet_detect_terminal_graphics @ agent/pet/render.py:detect_terminal_graphics */
pet_render_mode_t pet_detect_terminal_graphics(void) {
    const char *term_env = getenv("TERM");
    const char *term_program_env = getenv("TERM_PROGRAM");

    /* Lowercase both (Python does .lower()); the C port compares lowercase. */
    char term[128] = "", term_program[64] = "";
    if (term_env) {
        size_t i;
        for (i = 0; term_env[i] && i < sizeof(term) - 1; i++) {
            char c = term_env[i];
            term[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        term[i] = '\0';
    }
    if (term_program_env) {
        size_t i;
        for (i = 0; term_program_env[i] && i < sizeof(term_program) - 1; i++) {
            char c = term_program_env[i];
            term_program[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        term_program[i] = '\0';
    }

    /* VS Code / Cursor — unicode half-blocks (inline images opt-in). The
     * embedded xterm.js can't display kitty/iterm protocols. */
    if (strcmp(term_program, "vscode") == 0)
        return PET_MODE_UNICODE;

    /* kitty graphics protocol */
    if (getenv("KITTY_WINDOW_ID") || strstr(term, "kitty") || strstr(term, "ghostty"))
        return PET_MODE_KITTY;
    if (strcmp(term_program, "ghostty") == 0)
        return PET_MODE_KITTY;
    if (strcmp(term_program, "wezterm") == 0)
        return PET_MODE_KITTY;
    if (getenv("WEZTERM_PANE"))
        return PET_MODE_KITTY;

    /* iTerm2 */
    if (strcmp(term_program, "iterm.app") == 0)
        return PET_MODE_ITERM;
    if (getenv("ITERM_SESSION_ID"))
        return PET_MODE_ITERM;

    /* Sixel */
    if (strcmp(term_program, "mintty") == 0 || strstr(term, "foot") ||
        strstr(term, "mlterm") || strstr(term, "sixel"))
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

/* PoP: pet_state_frame_count @ agent/pet/render.py:state_frame_counts */
/* PoP: frame_count @ agent/pet/render.py:frame_count */
/* PoP: pet_frame_count @ agent/pet/render.py:frame_count */
int pet_state_frame_count(const char *spritesheet_path, pet_state_t state) {
    if (!spritesheet_path || !*spritesheet_path) return 0;
    if (access(spritesheet_path, F_OK) != 0) return 0;

    /* Decode the sheet with the from-scratch PNG decoder and step across the
     * state's row, stopping at the first blank (padding-trimmed) frame —
     * exactly Python's _raw_frames. */
    pngdec_image_t *sheet = pngdec_decode_file(spritesheet_path);
    if (!sheet) return 0;

    int cols = sheet->width / PET_FRAME_W;
    int rows = sheet->height / PET_FRAME_H;
    if (cols < 1 || rows < 1) { pngdec_image_free(sheet); return 0; }

    int row = pet_state_row_index(state, rows);
    int top = row * PET_FRAME_H;
    if (top + PET_FRAME_H > sheet->height)
        top = sheet->height - PET_FRAME_H;
    if (top < 0) top = 0;

    /* Alpha channel offset depends on decoded channels. */
    int ch = sheet->channels;
    int alpha_off = (ch == 2) ? 1 : (ch == 4) ? 3 : -1;

    int count = 0;
    for (int i = 0; i < PET_FRAMES_PER_STATE && i < cols; i++) {
        int left = i * PET_FRAME_W;
        /* Max alpha across the frame; blank if <= 8 (Python _BLANK_ALPHA). */
        int max_alpha = 0;
        for (int fy = 0; fy < PET_FRAME_H; fy++) {
            const uint8_t *rowp = sheet->pixels +
                ((size_t)(top + fy) * sheet->width + left) * (size_t)ch;
            for (int fx = 0; fx < PET_FRAME_W; fx++) {
                if (alpha_off >= 0) {
                    if (rowp[fx * ch + alpha_off] > max_alpha)
                        max_alpha = rowp[fx * ch + alpha_off];
                } else if (rowp[fx * ch] > max_alpha) {
                    max_alpha = rowp[fx * ch];
                }
            }
        }
        if (max_alpha <= 8) break; /* trailing transparent padding */
        count++;
    }

    pngdec_image_free(sheet);
    return count;
}

/* PoP: pet_renderer_available @ agent/pet/render.py:PetRenderer.available */
bool pet_renderer_available(pet_render_mode_t mode, const char *spritesheet_path) {
    if (mode == PET_MODE_OFF) return false;
    if (!spritesheet_path || !*spritesheet_path) return false;
    return (access(spritesheet_path, F_OK) == 0);
}

/* PoP: pet_sprite_frames @ agent/pet/render.py:_raw_frames */
pet_frame_t *pet_sprite_frames(const char *spritesheet_path, pet_state_t state,
                               int *out_count) {
    if (out_count) *out_count = 0;
    if (!spritesheet_path || !*spritesheet_path) return NULL;

    pngdec_image_t *sheet = pngdec_decode_file(spritesheet_path);
    if (!sheet) return NULL;

    int cols = sheet->width / PET_FRAME_W;
    int rows = sheet->height / PET_FRAME_H;
    if (cols < 1 || rows < 1) { pngdec_image_free(sheet); return NULL; }

    int row = pet_state_row_index(state, rows);
    int top = row * PET_FRAME_H;
    if (top + PET_FRAME_H > sheet->height)
        top = sheet->height - PET_FRAME_H;
    if (top < 0) top = 0;

    int ch = sheet->channels;
    int alpha_off = (ch == 2) ? 1 : (ch == 4) ? 3 : -1;

    /* First pass: count real (padding-trimmed) frames. */
    int count = 0;
    for (int i = 0; i < PET_FRAMES_PER_STATE && i < cols; i++) {
        int left = i * PET_FRAME_W;
        int max_alpha = 0;
        for (int fy = 0; fy < PET_FRAME_H; fy++) {
            const uint8_t *rp = sheet->pixels +
                ((size_t)(top + fy) * sheet->width + left) * (size_t)ch;
            for (int fx = 0; fx < PET_FRAME_W; fx++) {
                int a = (alpha_off >= 0) ? rp[fx * ch + alpha_off] : rp[fx * ch];
                if (a > max_alpha) max_alpha = a;
            }
        }
        if (max_alpha <= 8) break;
        count++;
    }
    if (count < 1) { pngdec_image_free(sheet); return NULL; }

    /* Second pass: crop each frame to RGBA. */
    pet_frame_t *frames = calloc((size_t)count, sizeof(pet_frame_t));
    if (!frames) { pngdec_image_free(sheet); return NULL; }
    for (int i = 0; i < count; i++) {
        int left = i * PET_FRAME_W;
        frames[i].width = PET_FRAME_W;
        frames[i].height = PET_FRAME_H;
        frames[i].rgba = malloc((size_t)PET_FRAME_W * PET_FRAME_H * 4);
        if (!frames[i].rgba) {
            for (int j = 0; j < i; j++) free(frames[j].rgba);
            free(frames);
            pngdec_image_free(sheet);
            return NULL;
        }
        for (int fy = 0; fy < PET_FRAME_H; fy++) {
            const uint8_t *rp = sheet->pixels +
                ((size_t)(top + fy) * sheet->width + left) * (size_t)ch;
            uint8_t *dp = frames[i].rgba + (size_t)fy * PET_FRAME_W * 4;
            for (int fx = 0; fx < PET_FRAME_W; fx++) {
                if (ch >= 3) {
                    dp[fx * 4] = rp[fx * ch];
                    dp[fx * 4 + 1] = rp[fx * ch + 1];
                    dp[fx * 4 + 2] = rp[fx * ch + 2];
                } else {
                    dp[fx * 4] = rp[fx * ch];
                    dp[fx * 4 + 1] = rp[fx * ch];
                    dp[fx * 4 + 2] = rp[fx * ch];
                }
                dp[fx * 4 + 3] = (alpha_off >= 0) ? rp[fx * ch + alpha_off] : 255;
            }
        }
    }

    pngdec_image_free(sheet);
    if (out_count) *out_count = count;
    return frames;
}

/* PoP: pet_active_spritesheet @ agent/pet/store.py:InstalledPet.spritesheet */
const char *pet_active_spritesheet(void) {
    static char path[1024];
    pet_installed_t p;
    if (!pet_resolve_active_pet(NULL, &p) || !p.spritesheet_path[0])
        return "";
    snprintf(path, sizeof(path), "%s", p.spritesheet_path);
    return path;
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
