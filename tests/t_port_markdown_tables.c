/* Oracle harness: agent/markdown_tables.realign_markdown_tables vs LIVE Python.
 * Usage: ./t_port_markdown_tables <input_file> [available_width]
 * Prints the realigned markdown. The oracle (sta_oracle_markdown_tables.py)
 * prints the same from LIVE Python; diff the two for fidelity. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agent/port_markdown_tables.h"

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    size_t got = fread(buf, 1, (size_t)sz, f);
    buf[got] = '\0'; fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <input> [available_width]\n", argv[0]); return 2; }
    char *in = read_file(argv[1]);
    if (!in) return 2;
    int avail = (argc > 2) ? atoi(argv[2]) : -1;
    char *out = md_realign_markdown_tables(in, avail);
    fputs(out, stdout);
    free(out); free(in);
    return 0;
}
