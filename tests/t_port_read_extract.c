/*
 * t_port_read_extract.c — faithful verification harness for
 * src/tools/port_tools_read_extract.c (tools/read_extract.py).
 *
 * The runner feeds one *.in fixture per case (argv[1]); each .in contains the
 * basename of a sample document in the SAME fixture directory (sample.docx,
 * sample.xlsx, sample.ipynb). The harness extracts the document text via the
 * ported read_extract_document_text() and emits {"fn":<base>,"out":<text>}.
 * The Python oracle (sta_oracle_read_extract.py) recomputes the SAME extraction
 * from the LIVE tools/read_extract.py; the runner diffs them byte-for-byte.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* The ported entry points (no public header). */
extern bool read_extract_is_extractable_document(const char *path);
extern char *read_extract_document_text(const char *path, char *errbuf, size_t errsz);

static const char *js(const char *s)
{
    static char bufs[4][16384];
    static int bi = 0;
    char *b = bufs[bi];
    bi = (bi + 1) % 4;
    char *q = b;
    *q++ = '"';
    for (const char *p = s; p && *p && q - b < 16000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else if (c == '\t') { *q++ = '\\'; *q++ = 't'; }
        else if (c == '\r') { *q++ = '\\'; *q++ = 'r'; }
        else *q++ = c;
    }
    *q++ = '"';
    *q = '\0';
    return b;
}

static char *read_all(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <sample.in>\n"); return 2; }
    char *base = read_all(argv[1]);
    if (!base) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    /* trim trailing newline */
    size_t bl = strlen(base);
    while (bl && (base[bl-1] == '\n' || base[bl-1] == '\r' || base[bl-1] == ' ')) base[--bl] = '\0';

    /* Resolve the sample relative to the fixture directory (dirname of argv[1]). */
    char sample[PATH_MAX];
    const char *slash = strrchr(argv[1], '/');
    if (slash) {
        size_t dlen = (size_t)(slash - argv[1] + 1);
        snprintf(sample, sizeof(sample), "%.*s%s", (int)dlen, argv[1], base);
    } else {
        snprintf(sample, sizeof(sample), "%s", base);
    }

    char err[256] = "";
    char *text = read_extract_document_text(sample, err, sizeof(err));
    if (!text) {
        fprintf(stderr, "extract failed for %s: %s\n", sample, err);
        free(base);
        return 1;
    }
    /* base name for the fn field */
    const char *fn = strrchr(base, '/');
    fn = fn ? fn + 1 : base;
    printf("{\"fn\":%s,\"out\":%s}\n", js(fn), js(text));
    free(text);
    free(base);
    return 0;
}
