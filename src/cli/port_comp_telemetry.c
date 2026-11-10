/*
 * port_comp_telemetry.c — pure helpers from agent/context_compressor_telemetry.py.
 */

#include "port_comp_telemetry.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

const char *ctele_method(int summary_fallback_used, int has_aux_failure_error)
{
    if (summary_fallback_used)
        return "deterministic_fallback";
    if (has_aux_failure_error)
        return "aux_fallback_main";
    return "llm_summary";
}

char *ctele_clamp_failure(const char *failure)
{
    const char *f = failure ? failure : "";
    size_t len = strlen(f);
    if (len <= 220) {
        char *o = malloc(len + 1);
        if (o) { memcpy(o, f, len); o[len] = '\0'; }
        return o;
    }
    /* len > 220: take first 217 chars, rstrip(), append "..." */
    {
        char tmp[218]; /* 217 + nul */
        size_t e = 217, n;
        char *o;
        memcpy(tmp, f, 217);
        tmp[217] = '\0';
        while (e > 0 && isspace((unsigned char)tmp[e - 1])) e--;
        n = e;
        o = malloc(n + 4);
        if (!o) return NULL;
        memcpy(o, tmp, n);
        memcpy(o + n, "...", 4);
        o[n + 3] = '\0';
        return o;
    }
}

long long ctele_fit_margin(long long effective_aux_context,
                           long long aux_prompt_tokens,
                           long long aux_output_reservation, int resv_present)
{
    long long resv = resv_present ? aux_output_reservation : 0;
    return effective_aux_context - aux_prompt_tokens - resv;
}
