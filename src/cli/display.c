/*
 * cli_display.c — CLI display helpers for Hermes C.
 * Higher-level display wrappers over deps/cli_display.c.
 */


/* PoP: CLI display rendering (port of hermes_cli/display, hermes_cli/colors.py, hermes_cli/cli_output.py) */
/* PoP: should_use_color @ hermes_cli/colors.py:should_use_color */
/* PoP: color @ hermes_cli/colors.py:color */
/* PoP: display_print_info @ hermes_cli/cli_output.py:print_info */
/* PoP: display_print_success @ hermes_cli/cli_output.py:print_success */
/* PoP: display_print_warning @ hermes_cli/cli_output.py:print_warning */
/* PoP: display_print_error @ hermes_cli/cli_output.py:print_error */
/* PoP: display_print_header @ hermes_cli/cli_output.py:print_header */

#include "hermes_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void cli_display_response(const char *role, const char *content,
                           const char *reasoning)
{
    (void)role;
    (void)content;

    if (!content && !reasoning) return;

    if (reasoning && reasoning[0]) {
        display_printf(DISPLAY_YELLOW, DISPLAY_ITALIC, "%s", reasoning);
        printf("\n\n");
    }

    if (content && content[0]) {
        display_printf(DISPLAY_WHITE, DISPLAY_NORMAL, "%s", content);
        printf("\n");
    }
}

void cli_display_error(const char *msg) {
    display_printf(DISPLAY_RED, DISPLAY_BOLD, "Error: ");
    display_printf(DISPLAY_RED, DISPLAY_NORMAL, "%s\n", msg);
}

void cli_display_status(const char *msg) {
    display_printf(DISPLAY_BLUE, DISPLAY_DIM, "  %s\n", msg);
}

void cli_display_thinking(void) {
    display_printf(DISPLAY_YELLOW, DISPLAY_DIM, "  thinking...\n");
}

/* Port of Python hermes_cli/cli_output.py:print_info(). */
/* ================================================================
 *  Output Helpers — colored print wrappers (Python cli_output.py parity)
 * ================================================================ */

void display_print_info(const char *text) {
    if (!text) return;
    display_printf(DISPLAY_DEFAULT, DISPLAY_DIM, "  %s\n", text);
    fflush(stdout);
}

/* Port of Python hermes_cli/cli_output.py:print_success(). */
void display_print_success(const char *text) {
    if (!text) return;
    display_printf(DISPLAY_GREEN, DISPLAY_NORMAL, "✓ %s\n", text);
    fflush(stdout);
}

/* AG26: Port of Python hermes_cli/cli_output.py:print_warning(). */
void display_print_warning(const char *text) {
    if (!text) return;
    display_printf(DISPLAY_YELLOW, DISPLAY_NORMAL, "⚠ %s\n", text);
    fflush(stdout);
}

/* Port of Python hermes_cli/cli_output.py:print_error(). */
void display_print_error(const char *text) {
    if (!text) return;
    display_printf(DISPLAY_RED, DISPLAY_NORMAL, "✗ %s\n", text);
    fflush(stdout);
}

void display_print_error_rich(const char *error_msg, const char *suggestion) {
    if (!error_msg) return;
    /* Error header — bold red */
    display_set_fg(DISPLAY_RED);
    display_set_style(DISPLAY_BOLD);
    printf("  ✗ Error: ");
    display_reset();
    /* Error message — red */
    display_set_fg(DISPLAY_RED);
    printf("%s\n", error_msg);
    display_reset();
    /* Suggestion — dim yellow if present */
    if (suggestion && suggestion[0]) {
        display_printf(DISPLAY_YELLOW, DISPLAY_DIM, "    ┊ %s\n", suggestion);
    }
    /* Separator line */
    display_printf(DISPLAY_RED, DISPLAY_DIM,
        "    └────────────────────────────────────\n");
}

void display_print_header(const char *text) {
    if (!text) return;
    printf("\n");
    display_printf(DISPLAY_YELLOW, DISPLAY_BOLD, "  %s\n", text);
}
