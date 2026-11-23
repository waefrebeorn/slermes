#ifndef NOC_W15_CLIPROCESS_4E06FE_H
#define NOC_W15_CLIPROCESS_4E06FE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_cliprocess_4e06fe__last_output_line @ hermes_cli/cli_process_dock.py:_last_output_line */
struct json_t *w15_cliprocess_4e06fe_last_output_line(const char *buffer);
/* PoP: w15_cliprocess_4e06fe_process_status @ hermes_cli/cli_process_dock.py:process_status */
struct json_t *w15_cliprocess_4e06fe_process_status(const struct json_t *entry);
/* PoP: w15_cliprocess_4e06fe_process_glyph @ hermes_cli/cli_process_dock.py:process_glyph */
struct json_t *w15_cliprocess_4e06fe_process_glyph(const struct json_t *row);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_CLIPROCESS_4E06FE_H */
