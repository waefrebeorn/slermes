/*
 * t_port_shared_session.c — faithful verification harness for
 * is_shared_multi_user_session() in src/gateway/gw_session.c (port of
 * gateway/session.py:is_shared_multi_user_session).
 *
 * The fixture (argv[1]) is one case per line, tab-separated:
 *   chat_type \t thread_id \t group_per_user \t thread_per_user
 * Each non-empty line builds a gw_session_source_t, calls the function, and
 * emits one JSON line:
 *   {"chat_type":"..","thread_id":"..","group":B,"thread":B,"shared":B}
 * The Python oracle (tests/sta_oracle_shared_session.py) recomputes from the
 * LIVE gateway/session.py is_shared_multi_user_session with a SessionSource
 * built from the same fields; the runner diffs them.
 *
 * NOTE: to avoid pulling hermes_gateway.h (which drags the libdb chain into
 * this harness), we reconstruct the EXACT gw_session_source_t layout (copied
 * verbatim from include/hermes_gateway.h) and forward-declare the function.
 * Offsets must match the real struct for the function's field reads to be
 * correct.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    char platform[32];
    char chat_id[128];
    char chat_name[256];
    char chat_type[32];
    char user_id[128];
    char user_name[256];
    char thread_id[64];
    char chat_topic[256];
    char user_id_alt[128];
    char chat_id_alt[128];
    char guild_id[128];
    char parent_chat_id[128];
    char message_id[128];
    bool is_bot;
    bool has_data;
} gw_session_source_t;

bool is_shared_multi_user_session(const gw_session_source_t *src,
                                  bool group_sessions_per_user,
                                  bool thread_sessions_per_user);

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.tsv>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r'))
            line[--n] = '\0';
        if (n == 0) continue;

        /* split the line on tabs into up to 4 fields, mirroring the oracle's
         * Python line.split("\t") so empty middle fields parse identically. */
        char fields[4][1024];
        int nf = 0;
        char *p = line;
        char *field_start = p;
        while (*p && nf < 4) {
            if (*p == '\t') {
                size_t fl = (size_t)(p - field_start);
                if (fl >= sizeof(fields[nf])) fl = sizeof(fields[nf]) - 1;
                memcpy(fields[nf], field_start, fl);
                fields[nf][fl] = '\0';
                nf++;
                field_start = p + 1;
            }
            p++;
        }
        if (nf < 4 && *field_start) {  /* last field (no trailing tab) */
            size_t fl = strlen(field_start);
            if (fl >= sizeof(fields[nf])) fl = sizeof(fields[nf]) - 1;
            memcpy(fields[nf], field_start, fl);
            fields[nf][fl] = '\0';
            nf++;
        }
        while (nf < 4) { fields[nf][0] = '\0'; nf++; }

        char *chat_type = fields[0];
        char *thread_id = fields[1][0] ? fields[1] : "";
        bool group = (fields[2][0] ? (strcmp(fields[2], "true") == 0) : true);
        bool thr   = (fields[3][0] ? (strcmp(fields[3], "true") == 0) : false);

        gw_session_source_t src;
        memset(&src, 0, sizeof(src));
        strncpy(src.chat_type, chat_type, sizeof(src.chat_type) - 1);
        strncpy(src.thread_id, fields[1], sizeof(src.thread_id) - 1);

        bool shared = is_shared_multi_user_session(&src, group, thr);
        printf("{\"chat_type\":\"%s\",\"thread_id\":\"%s\",\"group\":%s,\"thread\":%s,\"shared\":%s}\n",
               chat_type, thread_id,
               group ? "true" : "false", thr ? "true" : "false",
               shared ? "true" : "false");
    }
    fclose(f);
    return 0;
}
