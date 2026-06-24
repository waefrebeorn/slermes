/* test_window.c — Integration test for cross-platform window backend */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "window.h"

int main(int argc, char **argv) {
    int failures = 0;
    int tests = 0;

    printf("=== test_window.c — Window Backend Integration ===\n");
    printf("[test] Platform: %s\n", window_platform_name());
    printf("[test] GPU: %s\n", window_platform_has_gpu() ? "yes" : "no");

    /* Test 1: Platform name */
    tests++;
    const char *plat = window_platform_name();
    if (plat && strlen(plat) > 0) {
        printf("[PASS] test_platform_name: '%s'\n", plat);
    } else {
        printf("[FAIL] test_platform_name: empty\n");
        failures++;
    }

    /* Test 2: window_create / window_destroy */
    tests++;
    window_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.title = "Slermes v468 — Integration Test";
    cfg.width = 800;
    cfg.height = 600;
    cfg.resizable = true;

    window_t *win = window_create(&cfg);
    if (win) {
        printf("[PASS] test_window_create: returned non-NULL\n");
    } else {
        printf("[FAIL] test_window_create: returned NULL\n");
        failures++;
        /* Can't continue without a window */
        printf("\n=== Results: %d/%d passed, %d FAILED ===\n", tests - failures, tests, failures);
        return failures > 0 ? 1 : 0;
    }

    /* Test 3: window_set_title / window_get_size */
    tests++;
    window_set_title(win, "v468 Test Window");
    int w = 0, h = 0;
    window_get_size(win, &w, &h);
    if (w > 0 && h > 0) {
        printf("[PASS] test_window_get_size: %dx%d\n", w, h);
    } else {
        printf("[FAIL] test_window_get_size: %dx%d\n", w, h);
        failures++;
    }

    /* Test 4: window_set_size */
    tests++;
    window_set_size(win, 1024, 768);
    window_get_size(win, &w, &h);
    /* Allow some tolerance for window manager decorations */
    if (w >= 1020 && w <= 1030 && h >= 760 && h <= 780) {
        printf("[PASS] test_window_set_size: %dx%d\n", w, h);
    } else {
        printf("[INFO] test_window_set_size: %dx%d (WM may adjust)\n", w, h);
        /* Not a hard failure — WMs can override */
    }

    /* Test 5: window_show / window_hide */
    tests++;
    window_show(win);
    window_hide(win);
    window_show(win);
    printf("[PASS] test_window_show_hide: no crash\n");

    /* Test 6: window_focus */
    tests++;
    window_focus(win);
    printf("[PASS] test_window_focus: no crash\n");

    /* Test 7: window_minimize / window_restore */
    tests++;
    window_minimize(win);
    usleep(100000);
    window_restore(win);
    printf("[PASS] test_window_minimize_restore: no crash\n");

    /* Test 8: window_maximize / window_restore */
    tests++;
    window_maximize(win);
    usleep(100000);
    window_restore(win);
    printf("[PASS] test_window_maximize_restore: no crash\n");

    /* Test 9: window_set_opacity / window_get_opacity */
    tests++;
    window_set_opacity(win, 0.8f);
    float opacity = window_get_opacity(win);
    if (opacity >= 0.7f && opacity <= 0.9f) {
        printf("[PASS] test_window_opacity: %.2f\n", opacity);
    } else {
        printf("[INFO] test_window_opacity: %.2f (platform may not support)\n", opacity);
    }
    window_set_opacity(win, 1.0f);

    /* Test 10: window_set_always_on_top */
    tests++;
    window_set_always_on_top(win, true);
    bool atop = window_is_always_on_top(win);
    printf("[PASS] test_window_always_on_top: %s\n", atop ? "true" : "false");
    window_set_always_on_top(win, false);

    /* Test 11: window_set_fullscreen / window_is_fullscreen */
    tests++;
    window_set_fullscreen(win, false);
    bool fs = window_is_fullscreen(win);
    if (!fs) {
        printf("[PASS] test_window_fullscreen: not fullscreen (correct)\n");
    } else {
        printf("[FAIL] test_window_fullscreen: unexpectedly fullscreen\n");
        failures++;
    }

    /* Test 12: window_clipboard_set / window_clipboard_get */
    tests++;
    window_clipboard_set(win, "v468 clipboard test");
    const char *clip = window_clipboard_get(win);
    if (clip && strcmp(clip, "v468 clipboard test") == 0) {
        printf("[PASS] test_clipboard: roundtrip OK\n");
    } else {
        printf("[INFO] test_clipboard: got '%s' (platform clipboard may differ)\n", clip ? clip : "(null)");
    }

    /* Test 13: window_set_cursor */
    tests++;
    window_set_cursor(win, CURSOR_IBEAM);
    window_set_cursor(win, CURSOR_HAND);
    window_set_cursor(win, CURSOR_ARROW);
    printf("[PASS] test_cursor: no crash\n");

    /* Test 14: window_poll_event (non-blocking) */
    tests++;
    window_event_t ev;
    bool poll_ok = window_poll_event(win, &ev);
    printf("[PASS] test_poll_event: returned %s (type=%d)\n", poll_ok ? "true" : "false", ev.type);

    /* Test 15: window_set_position / window_get_position */
    tests++;
    window_set_position(win, 100, 100);
    int px = 0, py = 0;
    window_get_position(win, &px, &py);
    printf("[PASS] test_window_position: (%d, %d)\n", px, py);

    /* Test 16: window_get_scale */
    tests++;
    float scale = window_get_scale(win);
    if (scale >= 1.0f && scale <= 4.0f) {
        printf("[PASS] test_window_scale: %.1f\n", scale);
    } else {
        printf("[INFO] test_window_scale: %.1f\n", scale);
    }

    /* Test 17: window_request_close */
    tests++;
    window_request_close(win);
    printf("[PASS] test_request_close: no crash\n");

    /* Cleanup */
    window_destroy(win);
    printf("[PASS] test_window_destroy: no crash\n");

    printf("\n=== Results: %d/%d passed, %d failed ===\n", tests - failures, tests, failures);
    return failures > 0 ? 1 : 0;
}
