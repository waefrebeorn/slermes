#ifndef NOC_W15_SESSIONSDI_6183F9_H
#define NOC_W15_SESSIONSDI_6183F9_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_sessionsdi_6183f9_confirm_prompt @ hermes_cli/sessions_discard.py:_confirm */
struct json_t *w15_sessionsdi_6183f9_confirm_prompt(const char *session_id, const struct json_t *rows);
/* PoP: w15_sessionsdi_6183f9_discard_plan @ hermes_cli/sessions_discard.py:_discard */
struct json_t *w15_sessionsdi_6183f9_discard_plan(const struct json_t *snapshot,
                                                  const struct json_t *admission,
                                                  int yes, int isatty, int confirm);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_SESSIONSDI_6183F9_H */
