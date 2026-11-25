/*
 * desktop_controller.c - boot sequence, gateway status and profile for the desktop shell.
 *
 * include/desktop_controller.h declared this module and src/desktop_gui.c calls it (line 91-128),
 * but no translation unit ever implemented it: those calls were satisfied by a two-function port
 * stub and the other five declared entry points did not exist at all. This is the module the
 * header describes.
 *
 * The controller is pure state plus one drawing call. It deliberately performs no I/O: the boot
 * sequence is driven by the caller (desktop_gui.c sets BOOT_CONNECTING, the gateway threads set
 * BOOT_READY or BOOT_ERROR), which is what makes it testable without a window.
 */
#include "desktop_controller.h"

#include "app_state.h"
#include "gui_core.h"

#include <stdio.h>
#include <string.h>

#define DC_MSG_MAX     256
#define DC_URL_MAX     512
#define DC_PROFILE_MAX 128

static struct {
    boot_state_t state;
    char msg[DC_MSG_MAX];
    char gateway_url[DC_URL_MAX];
    char profile[DC_PROFILE_MAX];
    bool gateway_connected;
} g_dc;

static void dc_copy(char *dst, size_t sz, const char *src)
{
    if (!dst || sz == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= sz) n = sz - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void desktop_controller_init(void)
{
    memset(&g_dc, 0, sizeof g_dc);
    g_dc.state = BOOT_INIT;
    dc_copy(g_dc.msg, sizeof g_dc.msg, "Starting...");
    dc_copy(g_dc.profile, sizeof g_dc.profile, "default");
}

void desktop_controller_set_boot_state(boot_state_t state, const char *msg)
{
    /* An out-of-range value would make the overlay pick an arbitrary branch, so clamp it: the
     * only states the shell can draw are the four the enum names. */
    if (state < BOOT_INIT || state > BOOT_ERROR) state = BOOT_ERROR;
    g_dc.state = state;
    dc_copy(g_dc.msg, sizeof g_dc.msg, msg && *msg ? msg : "");
}

boot_state_t desktop_controller_get_boot_state(void)
{
    return g_dc.state;
}

bool desktop_controller_is_ready(void)
{
    return g_dc.state == BOOT_READY;
}

void desktop_controller_set_gateway_url(const char *url)
{
    dc_copy(g_dc.gateway_url, sizeof g_dc.gateway_url, url);
}

void desktop_controller_set_gateway_connected(bool connected)
{
    g_dc.gateway_connected = connected;
    /* A connection that succeeds while still "connecting" *is* the transition to ready; a drop
     * after ready does not un-ready the shell, it is reported in the status bar instead. */
    if (connected && g_dc.state == BOOT_CONNECTING) {
        desktop_controller_set_boot_state(BOOT_READY, "Ready");
    }
}

void desktop_controller_set_active_profile(const char *profile)
{
    dc_copy(g_dc.profile, sizeof g_dc.profile, profile && *profile ? profile : "default");
}

const char *desktop_controller_get_message(void)
{
    return g_dc.msg;
}

const char *desktop_controller_get_active_profile(void)
{
    return g_dc.profile;
}

bool desktop_controller_gateway_connected(void)
{
    return g_dc.gateway_connected;
}

void desktop_controller_draw_boot_overlay(app_state_t *app)
{
    /* Nothing to cover once the shell is up. */
    if (g_dc.state == BOOT_READY) return;

    gc_window_t *win = app_get_window(app);
    if (!win) return;

    int w = gc_window_w(win);
    int h = gc_window_h(win);
    if (w <= 0 || h <= 0) return;

    gc_theme_t theme = gc_get_theme(win);
    gc_color_t panel = theme.bg_card;
    gc_color_t ink = theme.text;
    gc_color_t edge = theme.border;

    int pw = w * 2 / 3;
    if (pw < 320) pw = w > 320 ? 320 : w;
    int ph = 140;
    if (ph > h) ph = h;
    gc_rect_t box = { (w - pw) / 2, (h - ph) / 2, pw, ph };

    gc_fill_rect(win, box, panel);
    gc_draw_rect(win, box, 1, g_dc.state == BOOT_ERROR ? theme.error : edge);

    gc_rect_t line = { box.x + 16, box.y + 24, box.w - 32, 24 };
    gc_font_t *font = gc_get_font_bold(win);
    const char *head = g_dc.state == BOOT_ERROR ? "Startup failed" : "Starting Slermes";
    gc_draw_text_centered(win, font, head, line, g_dc.state == BOOT_ERROR ? theme.error : ink);

    if (g_dc.msg[0]) {
        gc_rect_t sub = { box.x + 16, box.y + 60, box.w - 32, 20 };
        gc_draw_text_centered(win, gc_get_font(win), g_dc.msg, sub, theme.text_secondary);
    }
    if (g_dc.gateway_url[0]) {
        char buf[DC_URL_MAX + 16];
        snprintf(buf, sizeof buf, "%s%s", g_dc.gateway_url,
                 g_dc.gateway_connected ? " (connected)" : " (offline)");
        gc_rect_t sub = { box.x + 16, box.y + 88, box.w - 32, 18 };
        gc_draw_text_centered(win, gc_get_font_small(win), buf, sub, theme.text_dim);
    }
}
