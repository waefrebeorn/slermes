/* gate: tools_skills_guard_load_skill_ignore — traced copy of the per-line
 * parse loop of src/hermes_cli/skills_guard.c::
 * cli_tools_skills_guard__load_skill_ignore, wrapped in the two-filename
 * loop that LIVE tools/skills_guard.py::_load_skill_ignore performs
 * (.skillignore then .clawhubignore). Linking the real port is not clean:
 * it pulls hermes_log deps and reads one caller-chosen file, so this is a
 * verbatim traced copy of its fgets/trim/skip ladder.
 *
 * LIVE twin: tools/skills_guard.py::_load_skill_ignore (parse half).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   LOOP ignore filenames: L, B1 file-exists
 *     LOOP lines: L, B2 blank-or-comment -> skip,
 *                 B3 empty-after-CRLF-strip -> skip, else keep
 *
 * Fixture: each input is "<mode>|<line>|<line>..."; '|' encodes newline.
 * Modes: skill=write .skillignore only, claw=.clawhubignore only,
 *        both=same content in both, missing=no file. Both sides build the
 *        identical fixture at $TMPDIR/wcup_skills_guard_g2 (setup outside
 *        the traced path).
 *
 * KNOWN DIVERGENCE (documented): C trims leading whitespace + CR/LF but
 * keeps trailing spaces; LIVE strip()s both ends — pattern TEXT differs on
 * trailing-space lines though control flow is identical. Inputs avoid
 * trailing-space lines.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "watercup.h"

static const char *const IGNORE_FILENAMES[] = {".skillignore", ".clawhubignore", NULL};

/* single shared tracer for the whole parse walk */
static wcup_t *WC;

static void write_fixture(const char *dir, const char *name, const char *content) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(content, f);
    fclose(f);
}

static void trace_load(const char *dir) {
    for (int fi = 0; IGNORE_FILENAMES[fi]; fi++) {
        wcup_loop(WC);
        char path[512];
        snprintf(path, sizeof path, "%s/%s", dir, IGNORE_FILENAMES[fi]);
        FILE *f = fopen(path, "r");
        if (!f) {                                            /* B1 */
            wcup_branch(WC, false);
            continue;
        }
        wcup_branch(WC, true);

        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            wcup_loop(WC);
            /* CRLF strip first so whitespace-only lines classify identically
             * to LIVE strip(); net keep/skip decisions are unchanged. */
            size_t len = strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
                line[--len] = '\0';
            char *p = line;
            while (*p && (*p == ' ' || *p == '\t')) p++;
            if (*p == '\0' || *p == '#') {                   /* B2 */
                wcup_branch(WC, true);
                continue;
            }
            wcup_branch(WC, false);
            /* kept: pattern appended to matcher (not emitted as V) */
        }
        fclose(f);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s load_skill_ignore <mode|lines...>\n", argv[0]);
        return 2;
    }
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    static char dir[512];
    snprintf(dir, sizeof dir, "%s/wcup_skills_guard_g2", tmp);

    static wcup_t tracer;
    WC = &tracer;

    for (int i = 2; i < argc; i++) {
        /* ---- fixture setup (outside the traced path) ---- */
        char rm[256];
        snprintf(rm, sizeof rm, "rm -rf %s 2>/dev/null", dir);
        (void)system(rm);
        mkdir(dir, 0755);
        char work[2048];
        snprintf(work, sizeof work, "%s", argv[i]);
        char *sep = strchr(work, '|');
        if (!sep) { fprintf(stderr, "bad input\n"); return 2; }
        *sep = '\0';
        const char *mode = work;
        char body[1536];
        size_t blen = 0;
        body[0] = '\0';
        char *tok = sep + 1;
        if (*tok == '\0') tok = NULL;  /* empty tail -> empty fixture */
        while (tok) {
            char *nx = strchr(tok, '|');
            if (nx) *nx = '\0';
            int w = snprintf(body + blen, sizeof body - blen, "%s\n", tok);
            if (w > 0) blen += (size_t)w;
            tok = nx ? nx + 1 : NULL;
        }
        if (strcmp(mode, "skill") == 0 || strcmp(mode, "both") == 0)
            write_fixture(dir, ".skillignore", body);
        if (strcmp(mode, "claw") == 0 || strcmp(mode, "both") == 0)
            write_fixture(dir, ".clawhubignore", body);
        if (strcmp(mode, "missing") == 0)
            (void)system(rm);

        /* ---- traced parse ---- */
        wcup_begin(&tracer, "load_skill_ignore");
        trace_load(dir);
        char cup[32];
        wcup_finalize(&tracer, cup, sizeof cup);
        printf("%s\n", cup);
    }
    return 0;
}
