/*
 * Oracle harness for tools/transcription_tools.py pure helpers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "transcription_tools_pure.h"

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

        if (strcmp(name, "ts_is_local_stt_provider") == 0) {
            const char *v = json_get_str(c, "value", "");
            bool r = ts_is_local_stt_provider(v, NULL);
            printf(r ? "true\n" : "false\n");
        } else if (strcmp(name, "ts_is_local_or_private_url") == 0) {
            const char *v = json_get_str(c, "value", "");
            bool r = ts_is_local_or_private_url(v);
            printf(r ? "true\n" : "false\n");
        } else if (strcmp(name, "ts_command_stt_env_passthrough") == 0 ||
                   strcmp(name, "tts_command_provider_env_passthrough") == 0) {
            json_t *vj = json_obj_get(c, "value");
            char *json_str = NULL;
            if (vj && vj->type == JSON_OBJECT) {
                json_str = json_serialize(vj);
            }
            int cnt;
            char **r;
            if (strcmp(name, "tts_command_provider_env_passthrough") == 0)
                r = tts_command_provider_env_passthrough(json_str ? json_str : "{}", &cnt);
            else
                r = ts_command_stt_env_passthrough(json_str ? json_str : "{}", &cnt);
            if (json_str) free(json_str);
            if (!r || cnt == 0) { printf("[]\n"); }
            else {
                printf("[");
                for (int j = 0; j < cnt; j++) {
                    printf("%s%s", j > 0 ? ", " : "", r[j]);
                }
                printf("]\n");
                for (int j = 0; j < cnt; j++) free(r[j]);
                free(r);
            }
        } else if (strcmp(name, "ts_confidence_thresholds") == 0) {
            json_t *vj = json_obj_get(c, "value");
            char *json_str = NULL;
            if (vj && vj->type == JSON_OBJECT) {
                json_str = json_serialize(vj);
            }
            double nsp, lp;
            ts_confidence_thresholds(json_str ? json_str : "{}", &nsp, &lp);
            if (json_str) free(json_str);
            printf("%.17g,%.17g\n", nsp, lp);
        } else if (strcmp(name, "ts_is_hallucinated_segment") == 0) {
            json_t *vj = json_obj_get(c, "value");
            char *json_str = NULL;
            if (vj) json_str = json_serialize(vj);
            double nsp = json_get_num(c, "no_speech_threshold", 0.6);
            double lp = json_get_num(c, "logprob_threshold", -1.0);
            bool r = ts_is_hallucinated_segment(json_str ? json_str : "{}", nsp, lp);
            if (json_str) free(json_str);
            printf(r ? "true\n" : "false\n");
        }
    }
    free(root);
    return 0;
}
