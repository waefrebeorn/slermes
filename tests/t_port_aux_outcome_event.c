/* t_port_aux_outcome_event.c — oracle harness for the pure helpers in
 * agent/auxiliary_call_outcome.py and gateway/platforms/event.py. */
#include "port_aux_outcome_event.h"

#include <stdio.h>
#include <stdlib.h>

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

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* clean_attachment_filename. */
    static const char *const fn[] = {
        "report.pdf", "dir/subdir/file.txt", "C:\\dir\\evil.exe", "..",
        "a\nb.txt", ".hidden", "\xE2\x80\x8Esneaky.txt", "x" /* pad later */,
        "averyveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryverylongname.withalongextensiondescription",
    };
    for (int i = 0; i < 9; i++) {
        char *o = eve_clean_attachment_filename(fn[i]);
        printf("{\"family\":\"caf\",\"case\":%d,\"name\":", i);
        jstr(fn[i]);
        printf(",\"out\":");
        jstr(o);
        printf("}\n");
        free(o);
    }

    /* _command_text. */
    static const char *const tx[] = {
        "@image:/tmp/1.png\nhello world", "@file:a b/c.txt\n\n/run",
        "@url:http://x\ny", "@image:/a@image:/b\ncmd", "plain", "  @image:/x\n",
        "@video:/x\nhi", "@image:[empty]\n/oops", "hi\n@image:/late",
    };
    for (int i = 0; i < 9; i++) {
        char *o = eve_command_text(tx[i]);
        printf("{\"family\":\"ct\",\"case\":%d,\"text\":", i);
        jstr(tx[i]);
        printf(",\"out\":");
        jstr(o);
        printf("}\n");
        free(o);
    }

    /* embedded_provider_error. */
    static const char *const er[] = {
        "{\"error\":{\"code\":\"X\",\"message\":\"m\"}}",
        "{\"error\":{}}",
        "{\"error\":\"boom\"}",
        "{}",
        "{\"error\":{\"type\":\"t\"},\"choices\":[]}",
        "{\"error\":null}",
    };
    for (int i = 0; i < 6; i++) {
        char *o = aco_embedded_provider_error(er[i]);
        printf("{\"family\":\"epe\",\"case\":%d,\"response\":%s,\"out\":%s}\n",
               i, er[i], o ? o : "null");
        free(o);
    }

    /* is_cancellation: (is_exception, is_interrupted). */
    static const int ic[][2] = {{1,0},{0,0},{1,1},{0,1}};
    for (int i = 0; i < 4; i++) {
        int out = aco_is_cancellation(ic[i][0], ic[i][1]);
        printf("{\"family\":\"ic\",\"case\":%d,\"exc\":%d,\"intr\":%d,\"out\":%d}\n",
               i, ic[i][0], ic[i][1], out);
    }
    return 0;
}
