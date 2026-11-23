#ifndef NOC_W8_SESSIONREFI_552870_H
#define NOC_W8_SESSIONREFI_552870_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_sessionrefi_552870_is_local @ gateway/session_refine.py:_local_session
 * The authority-gated early decision: a source is local only when it is not
 * None and its platform equals Platform.LOCAL ("local"). */
int w8_sessionrefi_552870_is_local(const char *platform);

/* PoP: w8_sessionrefi_552870_focus_tail @ gateway/session_refine.py:refine_local_session
 * Mirrors `tail = t('gateway.refine.focus_suffix', focus=focus) if focus else ''`
 * using the English catalog template (the oracle injects the same template). */
struct json_t *w8_sessionrefi_552870_focus_tail(const char *focus);

/* PoP: w8_sessionrefi_552870_started @ gateway/session_refine.py:refine_local_session
 * Mirrors `t('gateway.refine.started', focus_suffix=tail)` with the English
 * catalog template (the oracle injects the same template). */
struct json_t *w8_sessionrefi_552870_started(const char *focus_suffix);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_SESSIONREFI_552870_H */
