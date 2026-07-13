/* Smoke test: link read_extract_document_text and compare vs Python ref. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *read_extract_document_text(const char *path, char *errbuf, size_t errsz);

static int compare_file(const char *got, const char *reffile) {
    FILE *f = fopen(reffile, "rb");
    if (!f) { fprintf(stderr, "cannot open ref %s\n", reffile); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *ref = (char *)malloc(n + 1);
    fread(ref, 1, n, f); ref[n] = '\0'; fclose(f);
    int rc = strcmp(got, ref) == 0 ? 0 : 1;
    if (rc) {
        fprintf(stderr, "=== MISMATCH ===\n--- GOT (%zu) ---\n%s\n--- REF (%ld) ---\n%s\n",
                strlen(got), got, n, ref);
    }
    free(ref);
    return rc;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <file> <reffile>\n", argv[0]); return 3; }
    char err[512];
    char *txt = read_extract_document_text(argv[1], err, sizeof(err));
    if (!txt) { fprintf(stderr, "C returned NULL: %s\n", err); return 1; }
    int rc = compare_file(txt, argv[2]);
    free(txt);
    if (rc == 0) printf("OK: %s matches python reference\n", argv[1]);
    return rc;
}
