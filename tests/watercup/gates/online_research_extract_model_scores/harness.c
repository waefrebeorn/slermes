/* gate: online_research_extract_model_scores — traced copy of
 * src/tools/research_helpers.c::research_extract_model_scores
 * (LIVE twin: tools/online_research.py::OnlineResearcher._extract_model_scores).
 *
 * Input per argv token: JSON [[title, snippet], ...].
 *
 * Rung ladder (must match twin.py EXACTLY):
 *   per-result L tick;
 *   per-model L tick; per-pattern L tick + B1 pattern-in-text
 *     (first hit runs the body, then breaks);
 *     percent scan over context window: per-index L tick,
 *       B2 digit starts a token / B3 '%' closes it / B4 val in [0,100]
 *       -> V best-so-far ("%.17g");
 *     B5 no valid percentage found -> default 0.5 boost -> V "0.5".
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"
#include <json.h>

static const struct { const char *id; const char *pats[4]; } MODELS[] = {
    {"moonshotai/kimi-k2.6", {"kimi-k2.6", "kimi k2.6", "kimi k2", NULL}},
    {"z-ai/glm-5", {"glm-5", "glm 5", "z.ai glm", NULL}},
    {"minimaxai/minimax-m2.5", {"minimax-m2.5", "minimax m2.5", "minimax 2.5", NULL}},
    {"nvidia/nemotron-3-ultra-550b-a55b", {"nemotron-3-ultra", "nemotron 3 ultra", "550b", NULL}},
    {"nvidia/nemotron-4-340b-instruct", {"nemotron-4-340b", "nemotron 4 340b", NULL, NULL}},
    {"nvidia/llama-3.1-nemotron-ultra-253b-v1", {"nemotron-ultra-253b", "nemotron ultra 253b", NULL, NULL}},
    {"nvidia/nemotron-3-super-120b-a12b", {"nemotron-3-super", "nemotron 3 super", "120b", NULL}},
    {"qwen/qwen3.5-397b-a17b", {"qwen3.5-397b", "qwen 3.5 397b", NULL, NULL}},
    {"deepseek-ai/deepseek-v3.2", {"deepseek-v3.2", "deepseek v3.2", NULL, NULL}},
    {"openai/gpt-oss-120b", {"gpt-oss-120b", "gpt oss 120b", NULL, NULL}},
    {"meta/llama-3.1-405b-instruct", {"llama-3.1-405b", "llama 405b", NULL, NULL}},
};
#define NMODELS ((int)(sizeof(MODELS) / sizeof(MODELS[0])))

static void trace_extract(wcup_t *wc, json_t *results)
{
    double best[NMODELS] = {0};
    int seen[NMODELS] = {0};

    size_t nres = (results && results->type == JSON_ARRAY) ? results->c.count : 0;
    for (size_t r = 0; r < nres; r++) {
        wcup_loop(wc);
        json_t *pair = results->c.items[r];
        const char *t = "", *s = "";
        if (pair && pair->type == JSON_ARRAY && pair->c.count > 0 &&
            pair->c.items[0] && pair->c.items[0]->type == JSON_STRING)
            t = pair->c.items[0]->str_val;
        if (pair && pair->type == JSON_ARRAY && pair->c.count > 1 &&
            pair->c.items[1] && pair->c.items[1]->type == JSON_STRING)
            s = pair->c.items[1]->str_val;
        char text[8192];
        snprintf(text, sizeof text, "%s %s", t, s);
        for (char *p = text; *p; p++) *p = (char)tolower((unsigned char)*p);
        size_t textlen = strlen(text);

        for (int mi = 0; mi < NMODELS; mi++) {
            wcup_loop(wc);
            for (int pi = 0; MODELS[mi].pats[pi]; pi++) {
                wcup_loop(wc);
                const char *pat = MODELS[mi].pats[pi];
                const char *hit = strstr(text, pat);
                int in_text = hit != NULL;
                wcup_branch(wc, in_text);                          /* B1 */
                if (!in_text) continue;

                size_t pos = (size_t)(hit - text);
                size_t cs = pos > 100 ? pos - 100 : 0;
                size_t ce = pos + strlen(pat) + 100;
                if (ce > textlen) ce = textlen;

                int found_pct = 0;
                for (size_t i = cs; i < ce; i++) {
                    wcup_loop(wc);
                    int is_dig = isdigit((unsigned char)text[i]);
                    wcup_branch(wc, is_dig);                       /* B2 */
                    if (!is_dig) continue;
                    size_t j = i;
                    char num[32]; size_t k = 0;
                    while (j < ce && k < sizeof(num) - 1 &&
                           (isdigit((unsigned char)text[j]) || text[j] == '.'))
                        num[k++] = text[j++];
                    num[k] = '\0';
                    int pct = j < ce && text[j] == '%';
                    wcup_branch(wc, pct);                          /* B3 */
                    if (!pct) continue;
                    double val = atof(num);
                    int valid = val >= 0 && val <= 100;
                    wcup_branch(wc, valid);                        /* B4 */
                    if (!valid) continue;
                    double nv = val / 100.0;
                    if (!seen[mi] || nv > best[mi]) best[mi] = nv;
                    seen[mi] = 1;
                    found_pct = 1;
                    char tag[40];
                    snprintf(tag, sizeof tag, "%.17g", best[mi]);
                    wcup_valstr(wc, tag);
                    i = j;
                }
                wcup_branch(wc, !found_pct);                       /* B5 */
                if (!found_pct) {
                    best[mi] = 0.5;
                    seen[mi] = 1;
                    wcup_valstr(wc, "0.5");
                }
                break;
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s extract_model_scores '[[title,snippet],...]'...\n",
                argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        char *jerr = NULL;
        json_t *root = json_parse(argv[i], &jerr);
        WCUP_BEGIN("extract_model_scores");
        trace_extract(&_wc, root);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("extract_model_scores");
    }
    return 0;
}
