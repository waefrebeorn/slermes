#ifndef NOC_W15_CLISESSION_7FFDFA_H
#define NOC_W15_CLISESSION_7FFDFA_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_clisession_7ffdfa_countdown @ hermes_cli/cli_session_dock.py:countdown */
struct json_t *w15_clisession_7ffdfa_countdown(double seconds);
/* PoP: w15_clisession_7ffdfa__prompt_text @ hermes_cli/cli_session_dock.py:_prompt_text */
struct json_t *w15_clisession_7ffdfa_prompt_text(const struct json_t *item);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_CLISESSION_7FFDFA_H */
