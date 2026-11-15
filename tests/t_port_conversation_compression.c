/* t_port_conversation_compression.c -- oracle harness for the pure slice of
 * agent/conversation_compression.py. Emits one JSON line per case. */
#include "port_conversation_compression.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cs = 0;

static void jstr(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar(ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

static void strip_case(const char *v)
{
    json_t *j = json_parse(v, NULL);
    json_t *o = cc_strip_marker_for_comparison(j);
    char *s = o ? json_serialize(o) : NULL;
    printf("{\"case\":%d,\"fn\":\"strip_marker\",\"in\":{\"msgs\":%s},\"out\":%s}\n",
           ++cs, v, s ? s : "null");
    fflush(stdout);
    free(s);
    json_free(o);
    json_free(j);
}

static void prog_case(const char *v)
{
    int r = cc_is_compaction_progress_status(v);
    printf("{\"case\":%d,\"fn\":\"is_progress\",\"in\":{\"text\":", ++cs);
    jstr(v);
    printf("},\"out\":%s}\n", r ? "true" : "false");
    fflush(stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* _strip_marker_for_comparison */
    strip_case("[]");
    strip_case("null");
    strip_case("\"str\"");
    strip_case("5");
    strip_case("[{\"role\":\"user\",\"content\":\"a\"}]");
    strip_case("[{\"_db_persisted\":true,\"role\":\"user\"}]");
    strip_case("[{\"_db_persisted\":true}]");
    strip_case("[1,\"x\",null,{\"a\":1},{\"b\":{\"_db_persisted\":1}}]");
    strip_case("[[]]");
    strip_case("{}");

    /* is_compaction_progress_status */
    prog_case("");
    prog_case("   ");
    prog_case("Compacting context");
    prog_case("  Compacting context 45%  ");
    prog_case("✓ Context compaction complete — continuing turn...");
    prog_case("  ✓ Context compaction complete — continuing turn...  ");
    prog_case("compaction complete");
    prog_case("COMPACTION COMPLETE");
    prog_case("compression is currently blocked");
    prog_case("Compression is currently blocked by another job");
    prog_case("compact");
    prog_case("COMPACTING");
    prog_case("compress");
    prog_case("Context reduced to 4096 tokens");
    prog_case("context reduced to");
    prog_case("idle");
    prog_case("preflight");
    prog_case("retry 1/3");
    prog_case("done");
    prog_case("Context compacted, reducing tokens");
    prog_case("✓ compacted");
    prog_case("uncompressed");
    prog_case("decompress the archive");

    return 0;
}
