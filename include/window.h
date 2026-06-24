/*
 * window.h — Cross-platform C11 Window API
 *
 * Platform-agnostic window abstraction.
 * Backends: Wayland (Linux), Win32 (Windows), Cocoa (macOS).
 *
 * X11 is NOT supported (banned). Linux targets Wayland only.
 * On headless/fallback: stub backend for headless builds / CI.
 *
 * Usage:
 *   window_t *w = window_create("Title", 1280, 900);
 *   while (window_poll(w)) {
 *     window_swap_buffers(w);
 *   }
 *   window_destroy(w);
 */
#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ─────────────────────────────────────────────────────────── */
#define WINDOW_MAX_TITLE 256
#define WINDOW_KEY_COUNT 512
#define WINDOW_MAX_TEXT_INPUT 1024

/* ── Key codes (platform-agnostic subset) ──────────────────────────────── */
typedef enum {
    KEY_NONE = 0,
    KEY_A = 'A', KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
    KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S,
    KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_0 = '0', KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_ENTER = 0x100, KEY_BACKSPACE, KEY_TAB, KEY_ESCAPE, KEY_DELETE,
    KEY_SPACE = ' ',
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_HOME, KEY_END, KEY_PAGE_UP, KEY_PAGE_DOWN,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9,
    KEY_F10, KEY_F11, KEY_F12,
    KEY_LSHIFT, KEY_RSHIFT, KEY_LCTRL, KEY_RCTRL, KEY_LALT, KEY_RALT,
    KEY_LSUPER, KEY_RSUPER,
} window_key_t;

/* ── Modifier flags ────────────────────────────────────────────────────── */
#define MOD_SHIFT   (1 << 0)
#define MOD_CTRL    (1 << 1)
#define MOD_ALT     (1 << 2)
#define MOD_SUPER   (1 << 3)

/* ── Mouse buttons ─────────────────────────────────────────────────────── */
typedef enum {
    MOUSE_LEFT = 0,
    MOUSE_MIDDLE,
    MOUSE_RIGHT,
} mouse_button_t;

/* ── Event types ───────────────────────────────────────────────────────── */
typedef enum {
    /* Window events */
    EVENT_NONE = 0,
    EVENT_CLOSE,              /* Window closed */
    EVENT_RESIZE,             /* w, h set to new size */
    EVENT_EXPOSE,             /* Window needs redraw */
    EVENT_FOCUS_IN,
    EVENT_FOCUS_OUT,

    /* Keyboard events */
    EVENT_KEY_DOWN,           /* key, mods set */
    EVENT_KEY_UP,             /* key, mods set */
    EVENT_TEXT_INPUT,         /* utf-8 text, text_len set */

    /* Mouse events */
    EVENT_MOUSE_DOWN,         /* button, x, y, mods */
    EVENT_MOUSE_UP,           /* button, x, y, mods */
    EVENT_MOUSE_MOVE,         /* x, y, mods */
    EVENT_MOUSE_SCROLL,       /* dx, dy, x, y */

    /* Touch events (if available) */
    EVENT_TOUCH_DOWN,
    EVENT_TOUCH_UP,
    EVENT_TOUCH_MOVE,
} window_event_type_t;

/* ── Event struct ──────────────────────────────────────────────────────── */
typedef struct {
    window_event_type_t type;

    /* Resize */
    int width, height;

    /* Keyboard */
    window_key_t key;
    uint32_t mods;
    const char *text;        /* UTF-8 text for TEXT_INPUT */
    size_t text_len;

    /* Mouse */
    mouse_button_t button;
    int x, y;                /* Position in window coordinates */
    float scroll_dx, scroll_dy;
} window_event_t;

/* ── Window configuration ──────────────────────────────────────────────── */
typedef struct {
    const char *title;
    int width;
    int height;
    bool resizable;
    bool fullscreen;
    bool borderless;
    bool centered;
    int min_width;
    int min_height;
    /* Parent window (for embedded/child windows) */
    void *parent;
} window_config_t;

/* ── Opaque window type ────────────────────────────────────────────────── */
typedef struct window window_t;

/* ── Opaque renderer type ──────────────────────────────────────────────── */
typedef struct window_renderer window_renderer_t;

/* ── Cursor types ──────────────────────────────────────────────────────── */
typedef enum {
    CURSOR_ARROW = 0,
    CURSOR_IBEAM,
    CURSOR_HAND,
    CURSOR_HRESIZE,
    CURSOR_VRESIZE,
    CURSOR_CROSSHAIR,
} window_cursor_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  Window lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

/* Create a window. Returns NULL on failure. */
window_t *window_create(const window_config_t *config);

/* Destroy a window and free all resources. */
void window_destroy(window_t *w);

/* Set the window title. */
void window_set_title(window_t *w, const char *title);

/* Set minimum window size. */
void window_set_min_size(window_t *w, int min_w, int min_h);

/* Show / hide the window. */
void window_show(window_t *w);
void window_hide(window_t *w);

/* Focus the window (raise + activate). */
void window_focus(window_t *w);

/* Request close (triggers EVENT_CLOSE). */
void window_request_close(window_t *w);

/* Window state management (minimize/maximize/restore). */
void window_minimize(window_t *w);
void window_maximize(window_t *w);
void window_restore(window_t *w);

/* ═══════════════════════════════════════════════════════════════════════
 *  Event processing
 * ═══════════════════════════════════════════════════════════════════════ */

/*
 * Poll for the next event.
 * Returns true if an event was written to `ev`, false if no event is pending.
 * Does NOT block — use `window_wait_event()` to block until an event arrives.
 */
bool window_poll_event(window_t *w, window_event_t *ev);

/*
 * Wait for the next event (blocking).
 * Returns true if an event was written, false on error.
 * Returns EVENT_CLOSE if the window was closed.
 */
bool window_wait_event(window_t *w, window_event_t *ev);

/*
 * Wait for events with a timeout.
 * Returns true if an event was written, false on timeout or error.
 */
bool window_wait_event_timeout(window_t *w, window_event_t *ev, int timeout_ms);

/* ═══════════════════════════════════════════════════════════════════════
 *  Size / position
 * ═══════════════════════════════════════════════════════════════════════ */

void window_get_size(window_t *w, int *width, int *height);
void window_set_size(window_t *w, int width, int height);
void window_get_position(window_t *w, int *x, int *y);
void window_set_position(window_t *w, int x, int y);

/* Get the content scale factor (for HiDPI displays). */
float window_get_scale(window_t *w);

/* Set fullscreen mode. */
void window_set_fullscreen(window_t *w, bool fullscreen);
bool window_is_fullscreen(window_t *w);

/* ═══════════════════════════════════════════════════════════════════════
 *  Rendering
 * ═══════════════════════════════════════════════════════════════════════ */

/*
 * Get the platform renderer.
 * The renderer type depends on the platform:
 *   - Linux/Wayland:  window_renderer_t with EGL + OpenGL/Vulkan context
 *   - Windows:        window_renderer_t with WGL/DXGI
 *   - macOS:          window_renderer_t with Metal/OpenGL (CVDisplayLink)
 */
window_renderer_t *window_get_renderer(window_t *w);

/*
 * Swap front/back buffers.
 * Rendering commands between window_render_start() and window_render_end()
 * are composited into the back buffer and presented.
 */
void window_swap_buffers(window_t *w);

/*
 * Begin a render pass (clears the back buffer).
 * After this call, 2D drawing commands can be issued.
 */
void window_render_begin(window_t *w, float r, float g, float b, float a);

/* End the render pass. */
void window_render_end(window_t *w);

/* ── 2D Drawing Primitives (immediate mode) ──────────────────────────── */

/* Clear the entire window to a solid color. */
void window_clear(window_t *w, float r, float g, float b, float a);

/* Draw a rectangle outline. */
void window_draw_rect(window_t *w, float x, float y, float width, float height,
                     float r, float g, float b, float a);

/* Draw a filled rectangle. */
void window_fill_rect(window_t *w, float x, float y, float width, float height,
                     float r, float g, float b, float a);

/* Draw a rectangle with rounded corners. */
void window_fill_rect_rounded(window_t *w, float x, float y, float width, float height,
                             float radius, float r, float g, float b, float a);

/* Draw a circle. */
void window_fill_circle(window_t *w, float cx, float cy, float radius,
                       float r, float g, float b, float a);

/* Draw text (UTF-8, uses the renderer's font atlas). */
void window_draw_text(window_t *w, const char *text, float x, float y,
                     float size, float r, float g, float b, float a);

/* Measure text before drawing. */
float window_text_width(window_t *w, const char *text, float size);

/* Draw a line. */
void window_draw_line(window_t *w, float x0, float y0, float x1, float y1,
                     float line_width, float r, float g, float b, float a);

/* Draw an image from RGBA pixel data. */
void window_draw_image(window_t *w, const uint8_t *pixels,
                      int img_w, int img_h,
                      float x, float y, float width, float height);

/* ═══════════════════════════════════════════════════════════════════════
 *  Mouse / cursor
 * ═══════════════════════════════════════════════════════════════════════ */

void window_set_cursor(window_t *w, window_cursor_t cursor);
void window_get_mouse_pos(window_t *w, int *x, int *y);
bool window_get_mouse_button(window_t *w, mouse_button_t button);
void window_set_mouse_pos(window_t *w, int x, int y);

/* ═══════════════════════════════════════════════════════════════════════
 *  Clipboard
 * ═══════════════════════════════════════════════════════════════════════ */

const char *window_clipboard_get(window_t *w);
void window_clipboard_set(window_t *w, const char *text);

/* ═══════════════════════════════════════════════════════════════════════
 *  Drag and drop
 * ═══════════════════════════════════════════════════════════════════════ */

void window_enable_dnd(window_t *w, bool enable);

/* ═══════════════════════════════════════════════════════════════════════
 *  Platform info
 * ═══════════════════════════════════════════════════════════════════════ */

/* Returns "wayland", "win32", "macos", "stub", or "unknown". */
const char *window_platform_name(void);

/* Returns true if the platform supports GPU-accelerated rendering. */
bool window_platform_has_gpu(void);

/* ═══════════════════════════════════════════════════════════════════════
 *  Titlebar, Menu Bar, Tray
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: window_titlebar @ apps/desktop/src/app/window/index.tsx */
/* Titlebar style options */
typedef enum {
    TITLEBAR_SYSTEM = 0,    /* Use system titlebar */
    TITLEBAR_CUSTOM,        /* Custom drawn titlebar */
    TITLEBAR_TRANSPARENT,   /* Transparent titlebar */
} window_titlebar_style_t;

void window_set_titlebar_style(window_t *w, window_titlebar_style_t style);
window_titlebar_style_t window_get_titlebar_style(window_t *w);

/* PoP: window_menu_bar @ apps/desktop/src/app/window/index.tsx */
/* Menu bar configuration */
typedef struct {
    const char *label;      /* Menu label (e.g., "File", "Edit") */
    const char **items;     /* Array of menu item labels */
    int          item_count; /* Number of items */
    bool         is_main;    /* true for main menu bar, false for context menu */
} window_menu_bar_t;

bool window_set_menu_bar(window_t *w, const window_menu_bar_t *menu);
bool window_remove_menu_bar(window_t *w);

/* PoP: window_tray @ apps/desktop/src/app/window/index.tsx */
/* Tray icon configuration */
typedef struct {
    const char *icon_path;  /* Path to tray icon */
    const char *tooltip;    /* Hover tooltip text */
    bool        visible;    /* Show/hide tray icon */
} window_tray_config_t;

bool window_set_tray_icon(window_t *w, const window_tray_config_t *config);
bool window_remove_tray(window_t *w);

/* ═══════════════════════════════════════════════════════════════════════
 *  Transparency, Always-on-Top, Focus
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: window_transparency @ apps/desktop/src/app/window/index.tsx */
/* Set window opacity (0.0 = fully transparent, 1.0 = fully opaque) */
void window_set_opacity(window_t *w, float opacity);
float window_get_opacity(window_t *w);

/* PoP: window_always_on_top @ apps/desktop/src/app/window/index.tsx */
void window_set_always_on_top(window_t *w, bool enabled);
bool window_is_always_on_top(window_t *w);

/* PoP: window_blur_behind @ apps/desktop/src/app/window/index.tsx */
/* Enable blur-behind effect (where supported by compositor) */
void window_set_blur_behind(window_t *w, bool enabled);
bool window_has_blur_behind(window_t *w);

/* ═══════════════════════════════════════════════════════════════════════
 *  Global Shortcuts
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: window_hotkey @ apps/desktop/src/app/window/index.tsx */
/* Hotkey registration */
typedef struct {
    const char *id;         /* Unique hotkey identifier */
    uint32_t    key;        /* Key code (window_key_t) */
    uint32_t    mods;       /* Modifier flags (MOD_SHIFT, MOD_CTRL, etc.) */
    const char *description; /* Human-readable description */
} window_hotkey_t;

bool window_register_hotkey(window_t *w, const window_hotkey_t *hotkey);
bool window_unregister_hotkey(window_t *w, const char *id);
int  window_list_hotkeys(window_t *w, window_hotkey_t *out, int max_count);

/* ═══════════════════════════════════════════════════════════════════════
 *  Deep Linking
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: window_deep_link @ apps/desktop/src/app/window/index.tsx */
/* Handle hermes:// deep link URL */
typedef void (*deep_link_cb)(const char *url, const char *action, const char *params);

void window_set_deep_link_callback(window_t *w, deep_link_cb cb);
bool window_handle_deep_link(window_t *w, const char *url);

/* ═══════════════════════════════════════════════════════════════════════
 *  Terminal Search & Web Links
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: terminal_search @ apps/desktop/src/app/terminal/index.tsx */
/* Search within terminal content */
typedef struct {
    const char *query;     /* Search query */
    bool        case_sensitive;
    bool        regex;      /* Use regex matching */
    int         direction;  /* 1 = forward, -1 = backward */
} terminal_search_t;

bool window_terminal_search(window_t *w, const terminal_search_t *search);
int  window_terminal_find_next(window_t *w, const char *query);

/* PoP: terminal_web_links @ apps/desktop/src/app/terminal/index.tsx */
/* Enable clickable web links in terminal */
void window_terminal_enable_hyperlinks(window_t *w, bool enable);
bool window_terminal_has_hyperlinks(window_t *w);

#ifdef __cplusplus
}
#endif

#endif /* WINDOW_H */
