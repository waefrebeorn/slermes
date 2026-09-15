/* gate: update_cmd_resolve_pre_update_backup_mode — traced C copy of
 * src/hermes_cli/update_cmd.c::uc_resolve_pre_update_backup_mode.
 * Faithful port of LIVE Python hermes_cli/update_cmd.py::_resolve_pre_update_backup_mode
 * (config load itself is outside the traced path — the resolved raw value is
 * passed in; JSON booleans arrive as "true"/"false").
 *
 * Input encoding (one argv slot per input): "<no_backup:0|1> <backup:0|1> <raw|->"
 * where "-" means the config key is absent.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

static const char *uc_resolve(int no_backup, int backup, const char *raw) {
    WCUP_BEGIN("resolve_pre_update_backup_mode");
    const char *r;
    WCUP_BRANCH(no_backup);
    if (no_backup) { r = "off"; goto emit; }
    WCUP_BRANCH(backup);
    if (backup) { r = "full"; goto emit; }

    int missing = (!raw || !*raw);
    WCUP_BRANCH(missing);
    if (missing) { r = "quick"; goto emit; }

    /* Legacy boolean form: JSON true/false. */
    WCUP_BRANCH(strcmp(raw, "true") == 0);
    if (strcmp(raw, "true") == 0) { r = "full"; goto emit; }
    WCUP_BRANCH(strcmp(raw, "false") == 0);
    if (strcmp(raw, "false") == 0) { r = "off"; goto emit; }

    /* Python: str(raw).strip().lower() */
    char modebuf[128];
    snprintf(modebuf, sizeof modebuf, "%s", raw);
    char *m = modebuf;
    while (*m == ' ' || *m == '\t') m++;
    size_t ML = strlen(m);
    while (ML > 0 && (m[ML-1] == ' ' || m[ML-1] == '\t')) m[--ML] = '\0';
    for (char *q = m; *q; q++) *q = (char)tolower((unsigned char)*q);

    int is_off = (strcmp(m, "off") == 0 || strcmp(m, "false") == 0 ||
                  strcmp(m, "none") == 0 || strcmp(m, "disabled") == 0);
    WCUP_BRANCH(is_off);
    if (is_off) { r = "off"; goto emit; }
    int is_full = (strcmp(m, "full") == 0 || strcmp(m, "zip") == 0 ||
                   strcmp(m, "true") == 0);
    WCUP_BRANCH(is_full);
    if (is_full) { r = "full"; goto emit; }
    WCUP_BRANCH(strcmp(m, "quick") == 0);
    r = "quick"; /* quick, or unknown -> warn + quick */
emit:
    WCUP_VALSTR(r);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("resolve_pre_update_backup_mode");
    fprintf(stderr, "%s\n", r);
    return r;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_resolve_pre_update_backup_mode <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char slot[256];
        snprintf(slot, sizeof slot, "%s", argv[i]);
        char *sp1 = strchr(slot, ' ');
        char *sp2 = sp1 ? strchr(sp1 + 1, ' ') : NULL;
        if (!sp1 || !sp2) { fprintf(stderr, "bad input: %s\n", argv[i]); return 2; }
        *sp1 = '\0'; *sp2 = '\0';
        uc_resolve(slot[0] == '1', sp1[1] == '1',
                   strcmp(sp2 + 1, "-") == 0 ? NULL : sp2 + 1);
    }
    return 0;
}
