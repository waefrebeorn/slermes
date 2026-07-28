/*
 * t_port_web_server_fs.c — behavioral oracle harness for the /api/fs cluster
 * (port_web_server_fs.c).
 *
 * Fixture ops:
 *   {"op":"list","path":"..."}                 -> {"entries":[{name,path,isDirectory}...],"error":...}
 *   {"op":"read_text","path":"..."}            -> fs_read_text response (or {"status":N})
 *   {"op":"write_text","path":"...","content":"..."} -> {"status":0,"byteSize":N} or {"status":N}
 *   {"op":"read_data_url","path":"..."}        -> {"dataUrl":"..."} or {"status":N}
 *   {"op":"preview_language","path":"..."}     -> {"language":"..."}
 *   {"op":"readdir_hidden","name":"..."}       -> {"hidden":bool}
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "web_server_fs.h"

static void print_json_escaped_n(const char *s, size_t len) {
    const unsigned char *p = (const unsigned char *)s;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = p[i];
        switch (c) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (c < 0x20) printf("\\u%04x", c);
            else fputc(c, stdout);
        }
    }
}

static void print_json_escaped(const char *s) {
    print_json_escaped_n(s, strlen(s));
}

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    json_t *fx = json_parse(buf, NULL);
    if (!fx) return 2;
    const char *op = json_get_str(fx, "op", "");

    if (strcmp(op, "list") == 0) {
        size_t cnt = 0;
        const char *err = NULL;
        bool errm = false;
        ws_fs_entry_t *arr = ws_fs_list(json_get_str(fx, "path", ""), &cnt,
                                        &err, &errm);
        printf("{\"entries\":[");
        for (size_t i = 0; i < cnt; i++) {
            if (i) fputc(',', stdout);
            printf("{\"name\":\"");
            print_json_escaped(arr[i].name);
            printf("\",\"isDirectory\":%s}", arr[i].is_directory ? "true" : "false");
        }
        printf("]");
        if (err) { printf(",\"error\":\""); print_json_escaped(err); printf("\""); }
        printf("}\n");
        free(arr);
        if (errm) free((void *)err);
    } else if (strcmp(op, "read_text") == 0) {
        ws_fs_read_text_t r;
        ws_fs_read_text(json_get_str(fx, "path", ""), &r);
        if (r.status != 0) {
            printf("{\"status\":%d}\n", r.status);
        } else {
            printf("{\"binary\":%s,\"byteSize\":%lld,\"language\":\"%s\",\"mimeType\":\"%s\",\"text\":\"",
                   r.binary ? "true" : "false", r.byte_size, r.language, r.mime);
            print_json_escaped_n(r.text, r.text_len);
            printf("\",\"truncated\":%s}\n", r.truncated ? "true" : "false");
        }
        free(r.text);
    } else if (strcmp(op, "write_text") == 0) {
        const char *content = json_get_str(fx, "content", "");
        int st = ws_fs_write_text(json_get_str(fx, "path", ""), content,
                                  strlen(content));
        if (st == 0)
            printf("{\"status\":0,\"byteSize\":%zu}\n", strlen(content));
        else
            printf("{\"status\":%d}\n", st);
    } else if (strcmp(op, "read_data_url") == 0) {
        int st = 0;
        char *url = ws_fs_read_data_url(json_get_str(fx, "path", ""), &st);
        if (url) {
            printf("{\"dataUrl\":\"");
            print_json_escaped(url);
            printf("\"}\n");
            free(url);
        } else {
            printf("{\"status\":%d}\n", st);
        }
    } else if (strcmp(op, "preview_language") == 0) {
        printf("{\"language\":\"%s\"}\n",
               ws_fs_preview_language(json_get_str(fx, "path", "")));
    } else if (strcmp(op, "readdir_hidden") == 0) {
        printf("{\"hidden\":%s}\n",
               ws_fs_readdir_hidden(json_get_str(fx, "name", "")) ? "true" : "false");
    } else {
        return 2;
    }
    json_free(fx);
    return 0;
}
