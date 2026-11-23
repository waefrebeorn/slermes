#ifndef NOC_W12_SESSIONSAD_C876BA_H
#define NOC_W12_SESSIONSAD_C876BA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_sessionsad_c876ba_refusal @ hermes_cli/sessions_adopt.py:_refusal
 * One owner refusal as printed: "<reason> (<why>)".  `field`/`detail` model
 * exc.data.get('field')/get('detail'); NULL or "" is falsy.  Writes into out. */
void w12_sessionsad_c876ba_refusal(const char *reason, const char *field,
                                   const char *detail, char *out, size_t cap);

/* PoP: w12_sessionsad_c876ba_unreadable_store @ hermes_cli/sessions_adopt.py:_unreadable_store
 * One line for an unreadable store.  `kind` is the injected classification of
 * the exception: "lock", "schema" or "other"; `exc_text` is str(exc). */
void w12_sessionsad_c876ba_unreadable_store(const char *kind, const char *store,
                                            const char *exc_text,
                                            char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_SESSIONSAD_C876BA_H */
