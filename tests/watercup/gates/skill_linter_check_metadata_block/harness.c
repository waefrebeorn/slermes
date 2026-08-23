/* gate: skill_linter_check_metadata_block — traced C copies of
 * src/tools/skill_linter.c::check_metadata_block + check_author_caps
 * (both static in that TU, so faithful traced copies here).
 * Twin: LIVE tools/skill_linter.py::_check_metadata_block.
 * Input encoding (one argv token per input):
 *   "<version|->|<author|->|<license|->|<0|1 has metadata>|<0|1 has metadata.hermes>|<tags|->"
 * Finding encoding (the traced value): ";".join of
 *   missing-metadata:<key> | missing-metadata:hermes | missing-metadata:tags
 *   | author-caps
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static void add(char *findings, const char *code) {
    if (*findings) strcat(findings, ";");
    strcat(findings, code);
}

static int is_missing(const char *tok) {
    return !tok || !*tok || strcmp(tok, "-") == 0;
}

static void check(const char *version, const char *author, const char *license_s,
                  int has_metadata, int has_hermes_meta, const char *hermes_tags) {
    WCUP_BEGIN("check_metadata_block");
    char findings[512] = "";
    const char *keys[3] = {"version", "author", "license"};
    const char *vals[3] = {version, author, license_s};
    for (int i = 0; i < 3; i++) {
        WCUP_LOOP_TICK();
        int missing = is_missing(vals[i]);
        WCUP_BRANCH(missing);
        if (missing) {
            char code[64];
            snprintf(code, sizeof code, "missing-metadata:%s", keys[i]);
            add(findings, code);
        }
    }
    /* hermes_meta = meta.get("hermes") if isinstance(meta, dict) else None */
    int hermes_is_dict = has_metadata && has_hermes_meta;
    WCUP_BRANCH(!hermes_is_dict);
    if (!hermes_is_dict) {
        add(findings, "missing-metadata:hermes");
    } else {
        int no_tags = !hermes_tags || !*hermes_tags || strcmp(hermes_tags, "-") == 0;
        WCUP_BRANCH(no_tags);
        if (no_tags) add(findings, "missing-metadata:tags");
    }
    /* author-caps: LIVE uses author.strip().lower() — inputs carry no
     * surrounding whitespace (C port lowercpy's the raw author). */
    char lower[128] = "";
    if (!is_missing(author)) {
        size_t k = 0;
        for (; author[k] && k < sizeof lower - 1; k++)
            lower[k] = (char)((author[k] >= 'A' && author[k] <= 'Z') ? author[k] + 32 : author[k]);
        lower[k] = '\0';
    }
    int suspicious = !is_missing(author) &&
                     (strcmp(lower, "hermes") == 0 || strcmp(lower, "agent") == 0 ||
                      strcmp(lower, "hermes agent") == 0);
    WCUP_BRANCH(suspicious);
    if (suspicious) {
        int proper = strcmp(author, "Hermes Agent") == 0;
        WCUP_BRANCH(!proper);
        if (!proper) add(findings, "author-caps");
    }
    WCUP_VALSTR(findings);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("check_metadata_block");
}

/* split "a|b|c|d|e|f" into 6 fields */
static void parse_input(char *s, const char **f) {
    int i = 0;
    f[0] = s;
    for (; *s && i < 5; s++) {
        if (*s == '|') { *s = '\0'; f[++i] = s + 1; }
    }
    for (; i < 5;) f[++i] = "";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s check_metadata_block <ver|auth|lic|meta|herm|tags>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[512];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        const char *f[6];
        parse_input(buf, f);
        check(f[0], f[1], f[2], f[3][0] == '1', f[4][0] == '1', f[5]);
    }
    return 0;
}
