/*
 * Oracle harness for tools/voice_mode.py pure helpers ported to
 * src/tools/voice_mode_pure.c. Reads JSON array fixture from argv[1];
 * each element {"op":<name>, ...args}. Prints each result as compact JSON
 * on its own line.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hermes_json.h"
#include "voice_mode_pure.h"

/* Convert a JSON array of strings to a malloc'd char** (caller frees via
 * free_voice_stop_phrases). NULL on failure. */
static char **json_arr_to_phrases(json_t *arr, int *out_count) {
    *out_count = 0;
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    int n = (int)arr->c.count;
    char **r = (char **)calloc(n ? n : 1, sizeof(char*));
    int valid = 0;
    for (int i = 0; i < n; i++) {
        json_t *p = json_get(arr, (size_t)i);
        if (!p || p->type != JSON_STRING) continue;
        r[valid++] = strdup(p->str_val);
    }
    *out_count = valid;
    return r;
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
        json_t *op = json_obj_get(c, "op");
        const char *name = op && op->type == JSON_STRING ? op->str_val : "";

        if (strcmp(name, "is_nan") == 0) {
            json_t *v = json_obj_get(c, "value");
            double val = v && v->type == JSON_NUMBER ? v->num_val : 0.0;
            printf("%s\n", is_nan(val) ? "true" : "false");
        } else if (strcmp(name, "get_beep_volume") == 0) {
            json_t *vc = json_obj_get(c, "voice_cfg");
            printf("%.6g\n", voice_get_beep_volume(vc));
        } else if (strcmp(name, "_sounddevice_output_allowed") == 0) {
            printf("%s\n", _sounddevice_output_allowed() ? "true" : "false");
        } else if (strcmp(name, "_is_wsl") == 0) {
            printf("%s\n", _is_wsl() ? "true" : "false");
        } else if (strcmp(name, "_is_wsl2_env") == 0) {
            printf("%s\n", _is_wsl2_env() ? "true" : "false");
        } else if (strcmp(name, "_wsl_powershell_tts_available") == 0) {
            printf("%s\n", _wsl_powershell_tts_available() ? "true" : "false");
        } else if (strcmp(name, "_voice_debug_enabled") == 0) {
            printf("%s\n", _voice_debug_enabled() ? "true" : "false");
        } else if (strcmp(name, "_vad_log") == 0) {
            json_t *m = json_obj_get(c, "msg");
            const char *msg = m && m->type == JSON_STRING ? m->str_val : "";
            _vad_log(msg);
            printf("null\n");
        } else if (strcmp(name, "_load_voice_stop_phrases") == 0) {
            json_t *vc = json_obj_get(c, "voice_cfg");
            int count = 0;
            char **phrases = _load_voice_stop_phrases(vc, &count);
            json_t *arr = json_array();
            for (int i = 0; i < count; i++) json_append(arr, json_string(phrases[i]));
            char *s = json_serialize(arr); printf("%s\n", s); free(s); free(arr);
            free_voice_stop_phrases(phrases, count);
        } else if (strcmp(name, "is_voice_stop_phrase") == 0) {
            json_t *tr = json_obj_get(c, "transcript");
            json_t *pf = json_obj_get(c, "phrases");
            const char *trans = tr && tr->type == JSON_STRING ? tr->str_val : "";
            int count = 0;
            char **phrases = NULL;
            if (pf && pf->type == JSON_STRING) {
                char *e2 = NULL;
                json_t *parr = json_parse(pf->str_val, &e2);
                if (parr && parr->type == JSON_ARRAY)
                    phrases = json_arr_to_phrases(parr, &count);
                if (parr) free(parr);
                free(e2);
            } else if (pf && pf->type == JSON_ARRAY) {
                phrases = json_arr_to_phrases(pf, &count);
            }
            printf("%s\n", is_voice_stop_phrase(trans, phrases, count) ? "true" : "false");
            free_voice_stop_phrases(phrases, count);
        } else if (strcmp(name, "voice_stop_hint") == 0) {
            json_t *pf = json_obj_get(c, "phrases");
            int count = 0;
            char **phrases = NULL;
            if (pf && pf->type == JSON_STRING) {
                char *e2 = NULL;
                json_t *parr = json_parse(pf->str_val, &e2);
                if (parr && parr->type == JSON_ARRAY)
                    phrases = json_arr_to_phrases(parr, &count);
                if (parr) free(parr);
                free(e2);
            } else if (pf && pf->type == JSON_ARRAY) {
                phrases = json_arr_to_phrases(pf, &count);
            }
            char *hint = voice_stop_hint(phrases, count);
            json_t *hj = json_string(hint);
            char *hs = json_serialize(hj); printf("%s\n", hs); free(hs); free(hj);
            free(hint);
            free_voice_stop_phrases(phrases, count);
        } else if (strcmp(name, "thinking_sound_enabled") == 0) {
            json_t *vc = json_obj_get(c, "voice_cfg");
            printf("%s\n", thinking_sound_enabled(vc) ? "true" : "false");
        } else if (strcmp(name, "mark_audio_output_active") == 0) {
            json_t *a = json_obj_get(c, "active");
            bool val = a && a->type == JSON_BOOL ? a->bool_val : false;
            mark_audio_output_active(val);
            printf("%s\n", is_audio_output_active() ? "true" : "false");
        } else if (strcmp(name, "audio_recorder_max_duration_reached") == 0) {
            json_t *capj = json_obj_get(c, "cap");
            json_t *elj = json_obj_get(c, "elapsed");
            double capv = capj && capj->type == JSON_NUMBER ? capj->num_val : 0.0;
            double elv = elj && elj->type == JSON_NUMBER ? elj->num_val : 0.0;
            printf("%s\n", audio_recorder_max_duration_reached(capv, elv) ? "true" : "false");
        }
    }
    free(root);
    return 0;
}
