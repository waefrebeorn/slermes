/*
 * setup_wizard_ui.h - setup-wizard console UI helpers.
 *
 * Module-internal header (setup_wizard.c <-> setup_wizard_ui.c).
 */
#ifndef SETUP_WIZARD_UI_H
#define SETUP_WIZARD_UI_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

#define WIZ_GREEN  "\033[0;32m"
#define WIZ_CYAN   "\033[0;36m"
#define WIZ_YELLOW "\033[0;33m"
#define WIZ_RED    "\033[0;31m"
#define WIZ_BOLD   "\033[1m"
#define WIZ_RESET  "\033[0m"

bool wiz_ensure_dir(const char *path);
int prompt_int(const char *label, int min_val, int max_val, int default_val);
void prompt_str(const char *label, char *buf, size_t sz, const char *def);
bool prompt_yes_no(const char *question, bool default_yes);
bool read_line(char *buf, size_t sz);
void wiz_banner(const char *text);
void wiz_err(const char *msg);
void wiz_ok(const char *msg);
void wiz_prompt(const char *text);
void wiz_warn(const char *msg);

#endif /* SETUP_WIZARD_UI_H */
