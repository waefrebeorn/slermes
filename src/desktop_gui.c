/*
 * desktop_gui.c — Slermes Desktop GUI (Modular Entry Point)
 *
 * C11 SDL2 desktop application for Slermes Agent.
 * Uses modular architecture: app_state, session_db, sidebar, chat_view,
 * titlebar, event_handling, hud, desktop_controller, pet_ui.
 *
 * MIT License — Slermes Fork
 */

#define _GNU_SOURCE
#include "app_state.h"
#include "session_db.h"
#include "sidebar.h"
#include "chat_view.h"
#include "titlebar.h"
#include "event_handling.h"
#include "hud.h"
#include "desktop_controller.h"
#include "pet_ui.h"
#include "gui_core.h"
#include "slermes_home.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ══════════════════════════════════════════════════════════════════════
 * Main Application Entry Point
 * ══════════════════════════════════════════════════════════════════════ */

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --width N      Window width (default: 1200)\n");
    printf("  --height N     Window height (default: 800)\n");
    printf("  --theme NAME   Theme: dark, light, solarized, nord (default: dark)\n");
    printf("  --help         Show this help\n");
}

int main(int argc, char **argv) {
    /* Parse arguments */
    int width = 1200;
    int height = 800;
    const gc_theme_t *theme = &gc_theme_dark;
    
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            print_usage(argv[0]);
            return 0;
        } else if (!strcmp(argv[i], "--width") && i + 1 < argc) {
            width = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--height") && i + 1 < argc) {
            height = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--theme") && i + 1 < argc) {
            const char *t = argv[++i];
            if (!strcmp(t, "light")) theme = &gc_theme_light;
            else if (!strcmp(t, "solarized")) theme = &gc_theme_solarized;
            else if (!strcmp(t, "nord")) theme = &gc_theme_nord;
            else theme = &gc_theme_dark;
        }
    }
    
    /* Initialize GUI framework */
    if (gc_init() != 0) {
        fprintf(stderr, "Failed to initialize GUI framework\n");
        return 1;
    }
    
    /* Create window */
    gc_window_t *win = gc_create_window("Hermes Slermes", width, height, theme);
    if (!win) {
        fprintf(stderr, "Failed to create window\n");
        gc_quit();
        return 1;
    }
    
    /* Create application state */
    app_state_t *app = app_state_create();
    if (!app) {
        fprintf(stderr, "Failed to create app state\n");
        gc_destroy_window(win);
        gc_quit();
        return 1;
    }
    
    app_set_window(app, win);
    
    /* Initialize subsystems */
    desktop_controller_init();
    desktop_controller_set_boot_state(BOOT_INIT, "Initializing...");
    
    /* Open database */
    if (session_db_open(app) != 0) {
        fprintf(stderr, "Failed to open database\n");
        app_state_destroy(app);
        gc_destroy_window(win);
        gc_quit();
        return 1;
    }
    
    /* Load initial data */
    session_db_load_sessions(app);
    session_db_load_skills(app);
    session_db_load_profiles(app);
    session_db_load_cron(app);
    session_db_load_stats(app);
    
    /* Load messages for first session */
    if (app_session_count(app) > 0) {
        app_set_selected_session(app, 0);
        app_set_current_view(app, 0);
        app_set_current_view_name(app, "Chat");
        session_db_load_messages(app, 0);
    }
    
    /* Initialize pet if enabled */
    pet_ui_init(app);
    if (app_pet_active(app)) {
        app_set_pet_x(app, app_sidebar_w(app) / 2.0f);
        app_set_pet_y(app, TITLEBAR_H + app_sidebar_h(app) / 2.0f);
        app_set_pet_scale(app, 0.33f);
    }
    
    /* Mark boot as ready */
    desktop_controller_set_boot_state(BOOT_READY, "Ready");
    
    /* Run main event loop */
    event_run(app);
    
    /* Cleanup */
    session_db_close(app);
    app_state_destroy(app);
    gc_destroy_window(win);
    gc_quit();
    
    return 0;
}