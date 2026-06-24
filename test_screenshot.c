/* test_screenshot.c — Create Wayland window and save screenshot */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "window.h"

static void save_framebuffer_ppm(const char *filename, int w, int h) {
    unsigned char *pixels = malloc(w * h * 3);
    if (!pixels) return;
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    FILE *f = fopen(filename, "wb");
    if (!f) { free(pixels); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; y--)
        fwrite(pixels + y * w * 3, 1, w * 3, f);
    fclose(f);
    free(pixels);
    printf("[screenshot] Saved %s\n", filename);
}

int main(int argc, char **argv) {
    printf("[test] Slermes v464 — Wayland Window + Screenshot\n");

    window_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.title = "Slermes v464 — 8688/8688 PORTED";
    cfg.width = 800;
    cfg.height = 600;
    cfg.resizable = true;

    window_t *win = window_create(&cfg);
    if (!win) { fprintf(stderr, "FAILED: window_create NULL\n"); return 1; }

    printf("[test] Window %dx%d created on %s\n", cfg.width, cfg.height, window_platform_name());
    usleep(300000); /* let compositor map the window */

    /* Render using the window API */
    window_render_begin(win, 0.08f, 0.08f, 0.12f, 1.0f); /* dark bg */
    window_render_end(win);
    window_swap_buffers(win);
    
    usleep(100000);
    save_framebuffer_ppm("/tmp/slermes_v464.ppm", cfg.width, cfg.height);

    /* Keep visible for 3s */
    window_event_t ev;
    time_t start = time(NULL);
    while (time(NULL) - start < 3) {
        while (window_poll_event(win, &ev))
            if (ev.type == EVENT_CLOSE) goto done;
        usleep(10000);
    }
done:
    window_destroy(win);
    printf("[test] Done.\n");
    return 0;
}
