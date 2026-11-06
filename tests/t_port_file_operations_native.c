/* t_port_file_operations_native.c — oracle harness for sha256_of.
 * Creates test files under a temp dir, calls fop_sha256_of, emits JSON-lines
 * {"tag":"sha","path":...,"out":<hex|null>}. The oracle drives the live Python
 * function on the same paths and compares. */
#include "port_file_operations_native.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    /* Use a repo-relative forward-slash dir (no backslashes) so emitted paths
     * are plain JSON — independent of the caller's TEMP/HOME. */
    char dir[1024];
    if (argc > 1) {
        snprintf(dir, sizeof dir, "%s", argv[1]);
    } else {
        snprintf(dir, sizeof dir, ".testbin/fops/%ld", (long)getpid());
        mkdir(".testbin/fops");
        mkdir(dir);
    }

    /* Case A: small text file. */
    {
        char p[4096];
        snprintf(p, sizeof p, "%s/foo_sha.txt", dir);
        FILE *f = fopen(p, "wb");
        fputs("hello world\nsecond line\n", f);
        fclose(f);
        char *out = fop_sha256_of(p);
        printf("{\"tag\":\"sha\",\"case\":\"text\",\"path\":");
        jstr(p);
        printf(",\"out\":");
        if (out) { fputc('"', stdout); fputs(out, stdout); fputc('"', stdout); }
        else     fputs("null", stdout);
        printf("}\n");
        free(out);
    }
    /* Case B: empty file. */
    {
        char p[4096];
        snprintf(p, sizeof p, "%s/empty_sha", dir);
        FILE *f = fopen(p, "wb");
        fclose(f);
        char *out = fop_sha256_of(p);
        printf("{\"tag\":\"sha\",\"case\":\"empty\",\"path\":");
        jstr(p);
        printf(",\"out\":");
        if (out) { fputc('"', stdout); fputs(out, stdout); fputc('"', stdout); }
        else     fputs("null", stdout);
        printf("}\n");
        free(out);
    }
    /* Case C: > 1 MiB (exercises the chunk loop). */
    {
        char p[4096];
        snprintf(p, sizeof p, "%s/big_sha.bin", dir);
        FILE *f = fopen(p, "wb");
        static const unsigned char pat[] = "the quick brown fox jumps over the lazy dog\n";
        for (size_t i = 0; i < 150000 / sizeof(pat); i++)
            fwrite(pat, 1, sizeof(pat), f);
        fclose(f);
        char *out = fop_sha256_of(p);
        printf("{\"tag\":\"sha\",\"case\":\"big\",\"path\":");
        jstr(p);
        printf(",\"out\":");
        if (out) { fputc('"', stdout); fputs(out, stdout); fputc('"', stdout); }
        else     fputs("null", stdout);
        printf("}\n");
        free(out);
    }
    /* Case D: missing file -> NULL. */
    {
        char p[4096];
        snprintf(p, sizeof p, "%s/definitely_missing_sha_%ld", dir, (long)getpid());
        char *out = fop_sha256_of(p);
        printf("{\"tag\":\"sha\",\"case\":\"missing\",\"path\":");
        jstr(p);
        printf(",\"out\":%s}\n", out ? out : "null");
        free(out);
    }
    /* Case E: directory (not a regular file) -> NULL. */
    {
        char p[4096];
        snprintf(p, sizeof p, "%s/subdir_sha_%ld", dir, (long)getpid());
        mkdir(p);
        char *out = fop_sha256_of(p);
        printf("{\"tag\":\"sha\",\"case\":\"dir\",\"path\":");
        jstr(p);
        printf(",\"out\":%s}\n", out ? out : "null");
        free(out);
    }
    return 0;
}
