/*
 * Oracle harness for tools/flux3_video_tool.py pure helpers.
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

        if (strcmp(name, "_looks_like_local_path") == 0) {
            const char *v = json_get_str(c, "value", "");
            bool r = flux3_looks_like_local_path(v);
            printf(r ? "true\n" : "false\n");
        } else if (strcmp(name, "_display_path") == 0) {
            const char *p = json_get_str(c, "value", "");
            char *r = flux3_display_path(p);
            printf("%s\n", r ? r : "null"); free(r);
        } else if (strcmp(name, "_filename_from_url") == 0) {
            const char *u = json_get_str(c, "value", "");
            char *r = flux3_filename_from_url(u);
            printf("%s\n", r ? r : "null"); free(r);
        } else if (strcmp(name, "_is_transport_error") == 0) {
            const char *r = json_get_str(c, "raw", "{}");
            bool v = flux3_is_transport_error(r);
            printf(v ? "true\n" : "false\n");
        } else if (strcmp(name, "_poll_is_finished") == 0) {
            const char *r = json_get_str(c, "raw", "{}");
            bool v = flux3_poll_is_finished(r);
            printf(v ? "true\n" : "false\n");
        } else if (strcmp(name, "_retry_after_seconds") == 0) {
            const char *r = json_get_str(c, "raw", "{}");
            double v = flux3_retry_after_seconds(r);
            printf("%g\n", v);
        } else if (strcmp(name, "_still_generating") == 0) {
            const char *j = json_get_str(c, "job_id", "abc");
            char *r = flux3_still_generating(j);
            printf("%s\n", r ? r : "null"); free(r);
        } else if (strcmp(name, "_without_media") == 0) {
            json_t *vj = json_obj_get(c, "args");
            json_t *r = flux3_without_media(vj);
            char *s = json_serialize(r); printf("%s\n", s); free(s); json_free(r);
        } else if (strcmp(name, "_submit_args") == 0) {
            json_t *vj = json_obj_get(c, "args");
            const char *mode = json_get_str(c, "mode", "text_to_video");
            json_t *r = flux3_submit_args(mode, vj);
            char *s = json_serialize(r); printf("%s\n", s); free(s); json_free(r);
        } else {
            printf("unknown op\n");
        }
    }
    free(root);
    return 0;
}
