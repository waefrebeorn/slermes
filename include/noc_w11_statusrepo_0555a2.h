#ifndef NOC_W11_STATUSREPO_0555A2_H
#define NOC_W11_STATUSREPO_0555A2_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w11_statusrepo_0555a2_status_lines @ hermes_cli/status_report.py:status_lines
 * fields: JSON object of pre-formatted status values; keys: JSON array of field
 * names in the order to render. Returns a JSON array of "Label: value" strings.
 * Empty title is skipped; model renders "model (provider)" with the module's
 * placeholders; agent_running renders "Yes"/"No". */
struct json_t *w11_statusrepo_0555a2_status_lines(const struct json_t *fields,
                                                  const struct json_t *keys);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_STATUSREPO_0555A2_H */
