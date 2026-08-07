/*
 * Oracle harness for tools/flux3_video_tool.py.
 * Tests: _still_generating, _resolve_destination, _free_path.
 * (The 8 pure helpers are ported by port_tools_flux3_video.c and tested
 * via the f3_* symbol names; this file complements, not duplicates.)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "flux3_video_tool.h"

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s cases.json\n", argv[0]); return 2; }
    char *txt = NULL;
    long len = 0;
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("open cases"); return 2; }
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    txt = (char *)malloc(len + 1);
    len = fread(txt, 1, len, f); txt[len] = '\0'; fclose(f);

    char *err = NULL;
    json_t *root = json_parse(txt, &err);
    free(txt);
    if (err) { fprintf(stderr, "parse: %s\n", err); free(err); return 2; }
    if (!root || root->type != JSON_ARRAY) { fprintf(stderr, "not array\n"); return 2; }

    for (size_t i = 0; i < root->c.count; i++) {
        json_t *c = json_get(root, i);
        json_t *opj = json_obj_get(c, "op");
        const char *name = opj && opj->type == JSON_STRING ? opj->str_val : "";

        if (strcmp(name, "_still_generating") == 0) {
            const char *j = json_get_str(c, "job_id", "abc");
            char *r = flux3_still_generating(j);
            printf("%s\n", r ? r : "null"); free(r);
        } else if (strcmp(name, "_shared_submit_properties") == 0) {
            json_t *r = flux3_shared_submit_properties();
            char *s = json_serialize(r);
            printf("%s\n", s); free(s); json_free(r);
        } else if (strcmp(name, "_endpoints") == 0) {
            json_t *r = flux3_endpoints();
            if (!r) { printf("null\n"); continue; }
            char *s = json_serialize(r);
            printf("%s\n", s); free(s); json_free(r);
        } else if (strcmp(name, "_resolve_destination") == 0) {
            const char *save_to = json_obj_get(c, "save_to");
            save_to = save_to ? json_get_str(c, "save_to", "") : "";
            const char *filename = json_obj_get(c, "filename");
            char *r = flux3_resolve_destination(
                json_has(c, "save_to") ? json_get_str(c, "save_to", "") : NULL,
                json_has(c, "filename") ? json_get_str(c, "filename", "") : NULL);
            printf("%s\n", r ? r : "null"); free(r);
        } else if (strcmp(name, "_free_path") == 0) {
            const char *dir = json_get_str(c, "directory", "/tmp");
            const char *name2 = json_get_str(c, "name", "test.mp4");
            char *r = flux3_free_path(dir, name2);
            printf("%s\n", r ? r : "null"); free(r);
        } else {
            printf("unknown op\n");
        }
    }
    free(root);
    return 0;
}
