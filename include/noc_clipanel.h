#ifndef NOC_CLIPANEL_H
#define NOC_CLIPANEL_H
/*
 * Public C API for the port of hermes_cli/cli_panel.py (gate clipanel).
 * The Python _Panel is a fragment accumulator of (style, text) tuples; this
 * struct is its faithful C counterpart.  All symbols are module-prefixed.
 */
#include <stddef.h>

typedef struct {
    char *style;
    char *text;
} clipanel_line_t;

typedef struct {
    clipanel_line_t *lines;
    size_t count;
    size_t cap;
    char *border;
    int width;
} clipanel_panel_t;

/* _cells(): terminal-cell width, matching prompt_toolkit get_cwidth. */
int clipanel_cells(const char *text);

/* _Panel.__init__, .row(), .blank(), .close() and cleanup. */
void clipanel_init(clipanel_panel_t *panel, const char *border, int width,
                   const char *title, const char *title_style);
void clipanel_row(clipanel_panel_t *panel, const char *style, const char *text);
void clipanel_blank(clipanel_panel_t *panel);
void clipanel_close(clipanel_panel_t *panel);
void clipanel_free(clipanel_panel_t *panel);

#endif /* NOC_CLIPANEL_H */
