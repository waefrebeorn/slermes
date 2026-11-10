/*
 * setup_wizard_ui.c - setup-wizard console UI helpers.
 *
 * Split from setup_wizard.c (soul.md: self-contained modules). Coloured
 * banner/status lines, line reading and the prompt helpers. No file-scope
 * state.
 */
#include "setup_wizard_ui.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
 void wiz_banner(const char *text) {
    printf("\n%s%s%s%s\n", WIZ_BOLD, WIZ_CYAN, text, WIZ_RESET);
}
 void wiz_ok(const char *msg) {
    printf("  %s\xE2\x9C\x93%s %s\n", WIZ_GREEN, WIZ_RESET, msg);
}
 void wiz_warn(const char *msg) {
    printf("  %s\xE2\x9A\xA0%s %s\n", WIZ_YELLOW, WIZ_RESET, msg);
}
 void wiz_err(const char *msg) {
    printf("  %s\xE2\x9C\x97%s %s\n", WIZ_RED, WIZ_RESET, msg);
}
 void wiz_prompt(const char *text) {
    printf("  %s?%s %s: ", WIZ_CYAN, WIZ_RESET, text);
    fflush(stdout);
}
 bool read_line(char *buf, size_t sz) {
    if (!fgets(buf, (int)sz, stdin)) return false;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
        buf[--len] = '\0';
    return true;
}
 bool wiz_ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    return mkdir(path, 0755) == 0;
}
 int prompt_int(const char *label, int min_val, int max_val, int default_val) {
    char input[16];
    while (1) {
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "%s [%d-%d]", label, min_val, max_val);
        wiz_prompt(prompt);
        if (!read_line(input, sizeof(input))) continue;
        if (input[0] == '\0') return default_val;
        int v = atoi(input);
        if (v >= min_val && v <= max_val) return v;
        printf("  Enter %d-%d.\n", min_val, max_val);
    }
}
 bool prompt_yes_no(const char *question, bool default_yes) {
    char input[16];
    wiz_prompt(question);
    printf(" (%s/%s) ", default_yes ? "Y" : "y", default_yes ? "n" : "N");
    if (!read_line(input, sizeof(input))) return default_yes;
    if (input[0] == '\0') return default_yes;
    return input[0] == 'y' || input[0] == 'Y';
}


/* Read a multi-choice string */ void prompt_str(const char *label, char *buf, size_t sz, const char *def) {
    wiz_prompt(label);
    if (!read_line(buf, sz) || buf[0] == '\0') {
        if (def) snprintf(buf, sz, "%s", def);
        else buf[0] = '\0';
        return;
    }
}
