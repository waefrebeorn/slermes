/*
 * t_port_web_server_status.c — behavioral oracle harness for the dashboard
 * operational backbone (port_web_server_status.c).
 *
 * Reads a fixture JSON from argv[1]:
 *   {"op":"error_ring"}  -> record N errors, query counts
 *   {"op":"active_sessions","db":"/path"} -> set HERMES_HOME, count
 * Emits a single JSON object for the Python oracle to diff.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "web_server_status.h"

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "open fail\n"); return 2; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    fread(buf, 1, (size_t)sz, f); buf[sz] = '\0'; fclose(f);
    json_t *fx = json_parse(buf, NULL);
    free(buf);
    if (!fx || !json_node_is_object(fx)) { fprintf(stderr, "bad\n"); return 2; }

    const char *op = json_get_str(fx, "op", "");
    printf("{");
    if (strcmp(op, "error_ring") == 0) {
        int n = json_get_num(fx, "record", 0);
        for (int i = 0; i < n; i++)
            ws_record_error("dashboard", "boom");
        printf("\"total\":%d", ws_recent_error_count_all());
        printf(",\"window10\":%d", ws_recent_error_count(10));
        printf(",\"window0\":%d", ws_recent_error_count(0));
    } else if (strcmp(op, "active_sessions") == 0) {
        const char *db = json_get_str(fx, "db", "");
        if (db[0]) setenv("SLERMES_HOME", db, 1);
        printf("\"count\":%d", ws_count_active_sessions());
    } else {
        printf("\"error\":\"unknown\"");
    }
    printf("}\n");
    if (fx) json_free(fx);
    return 0;
}
