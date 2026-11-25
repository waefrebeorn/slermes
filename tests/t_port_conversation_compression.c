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

static void prog_case(const char *v, int marked)
{
    int r = cc_is_compaction_progress_status(v, marked);
    printf("{\"case\":%d,\"fn\":\"is_progress\",\"in\":{\"text\":", ++cs);
    jstr(v);
    printf(",\"marked\":%d},\"out\":%s}\n", marked, r ? "true" : "false");
    fflush(stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);

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
    prog_case("", 0);
    prog_case("   ", 0);
    prog_case("Compacting context", 0);
    prog_case("  Compacting context 45%  ", 0);
    prog_case("✓ Context compaction complete — continuing turn...", 0);
    prog_case("  ✓ Context compaction complete — continuing turn...  ", 0);
    prog_case("compaction complete", 0);
    prog_case("COMPACTION COMPLETE", 0);
    prog_case("compression is currently blocked", 0);
    prog_case("Compression is currently blocked by another job", 0);
    prog_case("compact", 0);
    prog_case("COMPACTING", 0);
    prog_case("compress", 0);
    prog_case("Context reduced to 4096 tokens", 0);
    prog_case("context reduced to", 0);
    prog_case("idle", 0);
    prog_case("preflight", 0);
    prog_case("retry 1/3", 0);
    prog_case("done", 0);
    /* marked=1: CompactionProgressText — the TYPE mark decides, not wording */
    prog_case("", 1);
    prog_case("   ", 1);
    prog_case("Compacting context", 1);
    prog_case("  Compacting context 45%  ", 1);
    prog_case("\xE2\x9C\x93 Context compaction complete", 1);
    prog_case("compaction complete", 1);
    prog_case("compression is currently blocked", 1);
    prog_case("idle", 1);
    prog_case("anything at all", 1);
    prog_case("Context compacted, reducing tokens", 0);
    prog_case("✓ compacted", 0);
    prog_case("uncompressed", 0);
    prog_case("decompress the archive", 0);

    return 0;
}
