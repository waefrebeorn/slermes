/*
 * Oracle harness: recognition (voice_mode.py parity).
 *
 * Exercises the pure-logic functions that map 1:1 to Python:
 *   - is_whisper_hallucination(text)  -> {"result": bool}
 *   - transcribe_recording(path, model, chunk_seconds) via a faked STT
 *     result injected through the shared transcribe_audio() path is not
 *     possible headlessly, so we test the SHAPING directly by calling the
 *     internal helpers we mirror: is_whisper_hallucination + voice_block_rms.
 *
 * Reads fixture JSON from argv[1]:
 *   {"fn":"hallucination","text":"..."}
 *   {"fn":"rms","samples":[int,...]}
 * Prints JSON result to stdout.
 */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "transcribe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Forward decls for the functions we ported (link against voice_mode.o). */
int is_whisper_hallucination(const char *text);
double voice_block_rms(const int16_t *samples, unsigned long n);

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture.json>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1); fread(buf, 1, sz, f); buf[sz] = '\0'; fclose(f);

    json_t *doc = json_parse(buf, NULL);
    free(buf);
    if (!doc) { fprintf(stderr, "bad fixture json\n"); return 2; }

    const char *fn = json_get_str(doc, "fn", "");
    if (strcmp(fn, "hallucination") == 0) {
        const char *text = json_get_str(doc, "text", "");
        int r = is_whisper_hallucination(text);
        printf("{\"fn\":\"hallucination\",\"result\":%s}\n", r ? "true" : "false");
    } else if (strcmp(fn, "rms") == 0) {
        json_t *samples = json_obj_get(doc, "samples");
        int n = samples ? (int)json_array_size(samples) : 0;
        int16_t *s = malloc(sizeof(int16_t) * (n > 0 ? n : 1));
        for (int i = 0; i < n; i++) {
            json_t *e = json_array_get(samples, (size_t)i);
            s[i] = (int16_t)json_number_value(e);
        }
        double r = voice_block_rms(s, (unsigned long)n);
        char rb[64];
        snprintf(rb, sizeof(rb), "%.6f", r);
        /* Strip trailing zeros, then a trailing dot, match Python json float repr. */
        size_t rl = strlen(rb);
        while (rl > 0 && rb[rl-1] == '0' && rb[rl-2] != '.') rl--;
        if (rl > 0 && rb[rl-1] == '.') rl--;
        if (strchr(rb, '.') == NULL) { rb[rl] = '.'; rb[rl+1] = '0'; rb[rl+2] = '\0'; }
        else rb[rl] = '\0';
        printf("{\"fn\":\"rms\",\"result\":%s}\n", rb);
        free(s);
    } else {
        fprintf(stderr, "unknown fn %s\n", fn);
        json_free(doc);
        return 2;
    }
    json_free(doc);
    return 0;
}
