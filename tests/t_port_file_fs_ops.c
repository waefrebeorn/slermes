/*
 * t_port_file_fs_ops.c — oracle harness for v553 file_fs_ops extraction.
 * Exercises the FS cluster (read_file_raw, delete_path, patch_replace,
 * is_likely_binary, is_image, detect_file_line_ending, file_has_bom)
 * against LIVE Python and emits JSON lines for the oracle.
 *
 *   gcc -O2 -g -I include -I src/tools -I src/agent -I lib/libjson \
 *       tests/t_port_file_fs_ops.c src/tools/file_fs_ops.o \
 *       src/tools/file_text_ops.o src/agent/file_safety.o \
 *       lib/libjson/json.o -o /tmp/t_fs
 */
#include "file_fs_ops.h"
#include "hermes_file_safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void hermes_log(int level, const char *fmt, ...)
{
    (void)level; (void)fmt;
}

static void emit_escaped(const char *s)
{
    if (!s) { printf("null"); return; }
    putchar('"');
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c < 0x20) printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}

static void emit_str(const char *fn, const char *in, const char *out)
{
    printf("{\"fn\":\"%s\"", fn);
    if (in) { printf(",\"in\":"); emit_escaped(in); }
    printf(",\"out\":"); emit_escaped(out);
    printf("}\n");
}

static void emit_bool(const char *fn, const char *in, int v)
{
    printf("{\"fn\":\"%s\"", fn);
    if (in) printf(",\"in\":\"%s\"", in);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

int main(void)
{
    file_safety_set_test_paths("/tmp/hermes-fs-test-home", "/tmp/hermes-fs-test-root");
    if (system("mkdir -p /tmp/hermes-fs-test-home /tmp/hermes-fs-test-root") != 0) {}

    /* write fixtures */
    char tf[256], bf[256], imgf[256], bomf[256], crf[256], lff[256], oneline[256];
    snprintf(tf, sizeof(tf), "/tmp/hermes-fs-test-home/fs-%d.txt", (int)getpid());
    snprintf(bf, sizeof(bf), "/tmp/hermes-fs-test-home/bin-%d.bin", (int)getpid());
    snprintf(imgf, sizeof(imgf), "/tmp/hermes-fs-test-home/pic-%d.png", (int)getpid());
    snprintf(bomf, sizeof(bomf), "/tmp/hermes-fs-test-home/bom-%d.txt", (int)getpid());
    snprintf(crf, sizeof(crf), "/tmp/hermes-fs-test-home/crlf-%d.txt", (int)getpid());
    snprintf(lff, sizeof(lff), "/tmp/hermes-fs-test-home/lf-%d.txt", (int)getpid());
    snprintf(oneline, sizeof(oneline), "/tmp/hermes-fs-test-home/one-%d.txt", (int)getpid());

    FILE *f;
    f = fopen(tf, "w"); if (f) { fputs("hello\nworld\n", f); fclose(f); }
    f = fopen(bf, "w"); if (f) { fwrite("AB\1\2\3CD\4", 1, 8, f); fclose(f); } /* non-printable >30% */
    f = fopen(imgf, "w"); if (f) { fputs("pngdata", f); fclose(f); }
    f = fopen(bomf, "wb"); if (f) { unsigned char b[]={0xEF,0xBB,0xBF,'h','i'}; fwrite(b,1,5,f); fclose(f); }
    f = fopen(crf, "wb"); if (f) { fputs("a\r\nb\r\n", f); fclose(f); }
    f = fopen(lff, "w"); if (f) { fputs("a\nb\n", f); fclose(f); }
    f = fopen(oneline, "w"); if (f) { fputs("onlyoneline", f); fclose(f); }

    /* read_file_raw */
    char *r = file_fs_ops_read_file_raw(tf);
    emit_str("read_file_raw", tf, r);
    free(r);

    /* delete_path: dedicated fixture (do not delete any oracle-read fixture) */
    char delf[256];
    snprintf(delf, sizeof(delf), "/tmp/hermes-fs-test-home/del-%d.txt", (int)getpid());
    f = fopen(delf, "w"); if (f) { fputs("delete me", f); fclose(f); }

    /* is_image */
    emit_bool("is_image", tf, file_fs_ops_is_image(tf));
    emit_bool("is_image", imgf, file_fs_ops_is_image(imgf));
    emit_bool("is_image", "/x/foo.ico", file_fs_ops_is_image("/x/foo.ico"));

    /* is_likely_binary */
    emit_bool("is_likely_binary", tf, file_fs_ops_is_likely_binary(tf));
    emit_bool("is_likely_binary", bf, file_fs_ops_is_likely_binary(bf));
    emit_bool("is_likely_binary", "/x/foo.exe", file_fs_ops_is_likely_binary("/x/foo.exe"));
    emit_bool("is_likely_binary", imgf, file_fs_ops_is_likely_binary(imgf));

    /* patch_replace */
    char *pr = file_fs_ops_patch_replace("aaaXbbbXccc", "X", "Y");
    emit_str("patch_replace", "aaaXbbbXccc|X|Y", pr);
    free(pr);
    char *pr2 = file_fs_ops_patch_replace("nomatch", "Z", "Q");
    emit_str("patch_replace", "nomatch|Z|Q", pr2);
    free(pr2);

    /* detect_file_line_ending */
    emit_str("detect_file_line_ending", crf, file_fs_ops_detect_file_line_ending(crf));
    emit_str("detect_file_line_ending", lff, file_fs_ops_detect_file_line_ending(lff));
    emit_str("detect_file_line_ending", oneline, file_fs_ops_detect_file_line_ending(oneline));

    /* file_has_bom */
    emit_bool("file_has_bom", bomf, file_fs_ops_file_has_bom(bomf));
    emit_bool("file_has_bom", tf, file_fs_ops_file_has_bom(tf));

    /* delete_path deny guard */
    emit_bool("delete_path_denied", "/etc/passwd", file_fs_ops_delete_path("/etc/passwd"));
    emit_bool("delete_path_denied", "~/.ssh/id_rsa", file_fs_ops_delete_path("~/.ssh/id_rsa"));
    emit_bool("delete_path_ok", delf, file_fs_ops_delete_path(delf));

    return 0;
}
