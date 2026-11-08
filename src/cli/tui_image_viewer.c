/*
 * tui_image_viewer.c — sixel/kitty inline image display overlay.
 *
 * Subsystem split (soul.md: self-contained modules). Touches only
 * tui.image_viewer + tui.panes + tui.modal_mode (via tui_state.h) and the
 * two render-core entry points. No file-scope state of its own.
 */
#include "tui_state.h"
#include <curses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void tui_image_viewer_init(void) {
    tui.image_viewer.sixel_supported = false;
    tui.image_viewer.kitty_supported = false;
    tui.image_viewer.image_displayed = false;
    tui.image_viewer.zoom_level = 100;
    tui.image_viewer.pan_x = 0;
    tui.image_viewer.pan_y = 0;

    /* Check TERM for sixel support */
    const char *term = getenv("TERM");
    if (term && strstr(term, "sixel"))
        tui.image_viewer.sixel_supported = true;

    /* Check KITTY_WINDOW_ID for kitty protocol */
    if (getenv("KITTY_WINDOW_ID"))
        tui.image_viewer.kitty_supported = true;
}
bool tui_display_image_sixel(const char *path) {
    if (!path || !tui.image_viewer.sixel_supported) return false;

    /* Read file and output sixel data */
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    /* Check sixel header */
    char header[8] = {0};
    if (fread(header, 1, 2, f) != 2 || header[0] != 0x1b || header[1] != 'P') {
        /* Not a sixel file, try converting with imagemagick or similar */
        fclose(f);

        /* Try using ImageMagick to convert to sixel */
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "convert \"%s\" sixel:- 2>/dev/null | head -c 16384", path);
        FILE *pipe = popen(cmd, "r");
        if (!pipe) return false;

        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) {
            /* Write sixel data directly to terminal (escape ncurses) */
            endwin();
            fwrite(buf, 1, n, stdout);
            fflush(stdout);
            refresh();
        }
        pclose(pipe);
        return true;
    }

    fclose(f);
    return false;
}
bool tui_display_image_kitty(const char *path, int max_width, int max_height) {
    (void)max_width;
    (void)max_height;
    if (!path || !tui.image_viewer.kitty_supported) return false;

    /* Kitty protocol: \e_Ga=T,f=100,...;payload\e\\ */
    /* We'll use convert to base64 encode the image first */
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);

    /* Read and base64 encode file */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "base64 -w 0 \"%s\" 2>/dev/null | head -c 65536", path);
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return false;

    char b64[65536];
    size_t n = fread(b64, 1, sizeof(b64) - 1, pipe);
    b64[n] = '\0';
    pclose(pipe);

    if (n == 0) return false;

    /* Send kitty protocol command */
    endwin();
    printf("\033_Ga=T,f=100,m=1;");
    fflush(stdout);

    /* Send in chunks if needed */
    size_t chunk_size = 4096;
    for (size_t i = 0; i < n; i += chunk_size) {
        size_t remain = n - i;
        if (remain > chunk_size) remain = chunk_size;

        printf("%.*s", (int)remain, b64 + i);
        if (i + chunk_size < n) {
            printf("\033_Gm=1;");
        }
        fflush(stdout);
    }
    printf("\033\\\n");
    fflush(stdout);
    refresh();

    tui.image_viewer.image_displayed = true;
    return true;
}
void tui_display_image(const char *path) {
    if (!path) return;

    strncpy(tui.image_viewer.image_path, path, sizeof(tui.image_viewer.image_path) - 1);

    /* Try kitty protocol */
    if (tui_display_image_kitty(path, tui.panes[PANE_HISTORY].cols, tui.panes[PANE_HISTORY].rows))
        return;

    /* Try sixel */
    if (tui_display_image_sixel(path))
        return;

    /* Fallback: show image info */
    tui_history_add(MSG_ROLE_INFO, "[Image] Terminal does not support inline images", false);
    tui_history_add(MSG_ROLE_INFO, path, false);
    tui_redraw_history();
}
int tui_image_viewer_handle(int ch) {
    switch (ch) {
        case 27: /* ESC — exit */
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
        case '+':
        case '=':
            if (tui.image_viewer.zoom_level < 400) {
                tui.image_viewer.zoom_level += 25;
                if (tui.image_viewer.image_path[0])
                    tui_display_image(tui.image_viewer.image_path);
            }
            return 1;
        case '-':
        case '_':
            if (tui.image_viewer.zoom_level > 25) {
                tui.image_viewer.zoom_level -= 25;
                if (tui.image_viewer.image_path[0])
                    tui_display_image(tui.image_viewer.image_path);
            }
            return 1;
        case KEY_LEFT:
            tui.image_viewer.pan_x -= 10;
            return 1;
        case KEY_RIGHT:
            tui.image_viewer.pan_x += 10;
            return 1;
        case KEY_UP:
            tui.image_viewer.pan_y -= 10;
            return 1;
        case KEY_DOWN:
            tui.image_viewer.pan_y += 10;
            return 1;
        case 'r':
        case 'R':
            tui.image_viewer.pan_x = 0;
            tui.image_viewer.pan_y = 0;
            tui.image_viewer.zoom_level = 100;
            if (tui.image_viewer.image_path[0])
                tui_display_image(tui.image_viewer.image_path);
            return 1;
        default:
            return 0;
    }
}
void tui_draw_image_viewer(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " IMAGE VIEWER ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    int y = 2;
    if (tui.image_viewer.image_path[0]) {
        mvwprintw(win, y++, 0, " Path: %s", tui.image_viewer.image_path);
        y++;
        mvwprintw(win, y++, 0, " Zoom: %d%%", tui.image_viewer.zoom_level);
        mvwprintw(win, y++, 0, " Pan:  (%d, %d)", tui.image_viewer.pan_x, tui.image_viewer.pan_y);
        y++;
        if (tui.image_viewer.image_displayed) {
            mvwprintw(win, y++, 0, " Image displayed inline.");
        } else {
            mvwprintw(win, y++, 0, " Terminal does not support inline image display.");
        }
    } else {
        mvwprintw(win, y++, 0, " No image loaded.");
        mvwprintw(win, y++, 0, " Use /image <path> to display an image.");
    }
    y++;

    wattron(win, A_DIM);
    mvwprintw(win, y++, 0, " Controls:");
    mvwprintw(win, y++, 0, "   ESC       Close viewer");
    mvwprintw(win, y++, 0, "   +/-       Zoom in/out");
    mvwprintw(win, y++, 0, "   Arrows    Pan image");
    mvwprintw(win, y++, 0, "   R         Reset zoom/pan");
    wattroff(win, A_DIM);

    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, tui.panes[PANE_HISTORY].rows - 1, 0, " Press ESC to close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}
