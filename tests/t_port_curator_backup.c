/*
 * t_port_curator_backup.c — faithful verification harness for
 * curator_backup_config_enabled / curator_backup_config_keep in
 * src/tools/curator_backup.c (port of agent/curator_backup.py).
 *
 * The fixture (argv[1]) is a sequence of YAML documents separated by a
 * line containing only "---". For each document we write it to
 * $HERMES_HOME/.hermes/config.yaml, then call both functions and emit
 * one JSON line: {"enabled":<bool>,"keep":<int>}.
 *
 * The Python oracle (tests/sta_oracle_curator_backup.py) recomputes from
 * the LIVE agent/curator_backup.py (with load_config monkeypatched to the
 * parsed YAML); the runner diffs them.
 */

#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations — defined in src/tools/curator_backup.c (port of
 * agent/curator_backup.py). Avoid pulling hermes_curator.h (libdb chain). */
bool curator_backup_config_enabled(void);
int  curator_backup_config_keep(void);

static void write_config(const char *home, const char *yaml, size_t n) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(yaml, 1, n, f); fclose(f); }
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <docs.yaml>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 2; }
    size_t rn = fread(buf, 1, (size_t)sz, f); buf[rn] = '\0';
    fclose(f);

    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    /* split on lines that are exactly "---" */
    char *cur = buf;
    char *seg_start = buf;
    size_t i = 0;
    while (i <= rn) {
        /* detect a separator line at cur (from seg_start) */
        if (i == rn || (buf[i] == '\n')) {
            /* end of a line; check if it was "---" */
            size_t line_len = (size_t)(&buf[i] - cur);
            /* trim trailing \r */
            while (line_len > 0 && cur[line_len-1] == '\r') line_len--;
            int is_sep = 0;
            if (line_len == 3 && strncmp(cur, "---", 3) == 0) is_sep = 1;
            if (is_sep || i == rn) {
                /* flush previous segment (seg_start..cur) as a doc, unless empty */
                size_t doc_len = (size_t)(cur - seg_start);
                /* trim leading/trailing blank lines by scanning */
                const char *d = seg_start; const char *e = seg_start + doc_len;
                while (d < e && (*d == '\n' || *d == ' ' || *d == '\t' || *d == '\r')) d++;
                while (e > d && (e[-1] == '\n' || e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;
                size_t dlen = (size_t)(e - d);
                if (dlen > 0) {
                    write_config(home, d, dlen);
                    bool en = curator_backup_config_enabled();
                    int  kp = curator_backup_config_keep();
                    printf("{\"enabled\":%s,\"keep\":%d}\n", en ? "true" : "false", kp);
                }
                seg_start = &buf[i+1];
            }
            if (i == rn) break;
            cur = &buf[i+1];
        }
        i++;
    }
    free(buf);
    return 0;
}
