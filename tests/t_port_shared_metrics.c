/*
 * Oracle harness for hermes_cli/observability/shared_metrics.py.
 * Reads JSON array fixture from argv[1]; each element {"op":<fn>, ...}.
 * Emits one JSON result per line.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "hermes_json.h"
#include "shared_metrics.h"

static int get_int(json_t *obj, const char *key, int def) {
    json_t *v = json_obj_get(obj, key);
    if (!v || v->type != JSON_NUMBER) return def;
    return (int)v->num_val;
}

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

        if (strcmp(name, "_utc_now") == 0) {
            struct sm_datetime now;
            sm_utc_now(&now);
            json_t *o = json_object();
            json_set(o, "year", json_number(now.year));
            json_set(o, "mon", json_number(now.mon));
            json_set(o, "day", json_number(now.day));
            json_set(o, "hour", json_number(now.hour));
            json_set(o, "min", json_number(now.min));
            json_set(o, "sec", json_number(now.sec));
            char *s = json_serialize(o); printf("%s\n", s); free(s); json_free(o);
        } else if (strcmp(name, "_isoformat") == 0) {
            struct sm_datetime dt;
            json_t *vj = json_obj_get(c, "dt");
            dt.year = get_int(vj, "year", 0);
            dt.mon = get_int(vj, "mon", 0);
            dt.day = get_int(vj, "day", 0);
            dt.hour = get_int(vj, "hour", 0);
            dt.min = get_int(vj, "min", 0);
            dt.sec = get_int(vj, "sec", 0);
            char *iso = sm_isoformat(&dt);
            if (iso) {
                json_t *s = json_string(iso);
                char *ser = json_serialize(s); printf("%s\n", ser); free(ser);
                free(iso);
            } else printf("null\n");
        } else if (strcmp(name, "_ensure_private_directory") == 0) {
            const char *path = json_get_str(c, "path", "");
            int rc = sm_ensure_private_directory(path);
            json_t *o = json_object();
            json_set(o, "rc", json_number(rc));
            struct stat st;
            if (stat(path, &st) == 0)
                json_set(o, "mode", json_number(st.st_mode & 0777));
            else
                json_set(o, "mode", json_null());
            char *s = json_serialize(o); printf("%s\n", s); free(s); json_free(o);
        } else if (strcmp(name, "_ensure_private_file") == 0) {
            const char *path = json_get_str(c, "path", "");
            int rc = sm_ensure_private_file(path);
            json_t *o = json_object();
            json_set(o, "rc", json_number(rc));
            struct stat st;
            if (stat(path, &st) == 0)
                json_set(o, "mode", json_number(st.st_mode & 0777));
            else
                json_set(o, "mode", json_null());
            char *s = json_serialize(o); printf("%s\n", s); free(s); json_free(o);
        } else {
            printf("unknown op\n");
        }
    }
    free(root);
    return 0;
}
