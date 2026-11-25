/*
 * Unit test for src/desktop_controller.c -- the boot state machine, without a window.
 *
 * The module is linked as its own object and the four drawing entry points it calls are stubbed
 * here, so the test proves the state transitions rather than the pixels.
 */
#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <string.h>

#include "desktop_controller.h"
#include "gui_core.h"

/* ---- fakes for the drawing layer ------------------------------------------------ */
gc_window_t *app_get_window(app_state_t *app) { (void)app; return NULL; }
int gc_window_w(gc_window_t *win) { (void)win; return 0; }
int gc_window_h(gc_window_t *win) { (void)win; return 0; }
gc_theme_t gc_get_theme(gc_window_t *win) { gc_theme_t t; memset(&t, 0, sizeof t); (void)win; return t; }
void gc_fill_rect(gc_window_t *win, gc_rect_t r, gc_color_t c) { (void)win; (void)r; (void)c; }
void gc_draw_rect(gc_window_t *win, gc_rect_t r, int bw, gc_color_t c) { (void)win; (void)r; (void)bw; (void)c; }
void gc_draw_text_centered(gc_window_t *win, gc_font_t *f, const char *t, gc_rect_t a, gc_color_t c)
{ (void)win; (void)f; (void)t; (void)a; (void)c; }
gc_font_t *gc_get_font(gc_window_t *win) { (void)win; return NULL; }
gc_font_t *gc_get_font_small(gc_window_t *win) { (void)win; return NULL; }
gc_font_t *gc_get_font_bold(gc_window_t *win) { (void)win; return NULL; }

static int fails;
#define CHECK(cond, what) do { if (!(cond)) { printf("  FAIL %s\n", what); fails++; } } while (0)

int main(void)
{
    desktop_controller_init();
    CHECK(desktop_controller_get_boot_state() == BOOT_INIT, "init -> BOOT_INIT");
    CHECK(!desktop_controller_is_ready(), "init is not ready");
    CHECK(strcmp(desktop_controller_get_active_profile(), "default") == 0, "default profile");
    CHECK(strcmp(desktop_controller_get_message(), "Starting...") == 0, "init message");

    desktop_controller_set_boot_state(BOOT_CONNECTING, "Connecting to gateway");
    CHECK(desktop_controller_get_boot_state() == BOOT_CONNECTING, "-> BOOT_CONNECTING");
    CHECK(strcmp(desktop_controller_get_message(), "Connecting to gateway") == 0, "message stored");

    /* A successful connection while connecting is the transition to ready. */
    desktop_controller_set_gateway_url("http://127.0.0.1:9100");
    desktop_controller_set_gateway_connected(true);
    CHECK(desktop_controller_get_boot_state() == BOOT_READY, "connect while connecting -> BOOT_READY");
    CHECK(desktop_controller_is_ready(), "ready after connect");
    CHECK(desktop_controller_gateway_connected(), "gateway flag stored");

    /* Losing the gateway after boot does not put the shell back into booting. */
    desktop_controller_set_gateway_connected(false);
    CHECK(desktop_controller_get_boot_state() == BOOT_READY, "disconnect keeps BOOT_READY");
    CHECK(!desktop_controller_gateway_connected(), "gateway flag cleared");

    desktop_controller_set_boot_state(BOOT_ERROR, "Gateway refused");
    CHECK(desktop_controller_get_boot_state() == BOOT_ERROR, "-> BOOT_ERROR");
    CHECK(!desktop_controller_is_ready(), "error is not ready");

    /* Out-of-range states must not be stored: the overlay only knows four. */
    desktop_controller_set_boot_state((boot_state_t)42, "bogus");
    CHECK(desktop_controller_get_boot_state() == BOOT_ERROR, "out-of-range clamps to BOOT_ERROR");

    /* NULL message must not crash and must not keep the previous text. */
    desktop_controller_set_boot_state(BOOT_INIT, NULL);
    CHECK(desktop_controller_get_message()[0] == '\0', "NULL message clears text");

    desktop_controller_set_active_profile("work");
    CHECK(strcmp(desktop_controller_get_active_profile(), "work") == 0, "profile set");
    desktop_controller_set_active_profile("");
    CHECK(strcmp(desktop_controller_get_active_profile(), "default") == 0, "empty profile -> default");
    desktop_controller_set_active_profile(NULL);
    CHECK(strcmp(desktop_controller_get_active_profile(), "default") == 0, "NULL profile -> default");

    /* Overlay with no window must return quietly rather than draw into NULL. */
    desktop_controller_set_boot_state(BOOT_INIT, "Starting...");
    desktop_controller_draw_boot_overlay(NULL);

    /* Long message is truncated, not overflowed. */
    char big[600];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    desktop_controller_set_boot_state(BOOT_CONNECTING, big);
    CHECK(strlen(desktop_controller_get_message()) < 256, "long message truncated");

    printf("FAILURES=%d\n", fails);
    return fails ? 1 : 0;
}
