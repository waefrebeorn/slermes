#ifndef NOC_W13_AUXILIARIO_C4A08D_H
#define NOC_W13_AUXILIARIO_C4A08D_H

#ifdef __cplusplus
extern "C" {
#endif

/* C11 port of agent/auxiliary_openai_url.py (gate w13_auxiliaryo_c4a08d).
 * Returned strings are heap allocated and owned by the caller (free()). */

/* _is_dual_surface_anthropic_host(url): 1/0. */
int w13_auxiliaryo_c4a08d_is_dual_surface_anthropic_host(const char *url);

/* _to_openai_base_url(base_url): normalized OpenAI-compatible base URL;
 * heap string, caller frees. */
char *w13_auxiliaryo_c4a08d_to_openai_base_url(const char *base_url);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W13_AUXILIARIO_C4A08D_H */
