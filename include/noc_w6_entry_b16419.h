#ifndef NOC_W6_ENTRY_B16419_H
#define NOC_W6_ENTRY_B16419_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_entry_b16419_parse_args @ acp_adapter/entry.py:_parse_args
 * Mirrors the argparse namespace as a JSON object with keys version, check,
 * setup, setup_browser, assume_yes (all booleans). Caller frees the result. */
struct json_t *w6_entry_b16419_parse_args(const struct json_t *argv);

/* PoP: w6_entry_b16419_is_benign_probe @ acp_adapter/entry.py:_BENIGN_PROBE_METHODS
 * True for the liveness-probe methods the benign filter silences. */
int w6_entry_b16419_is_benign_probe(const char *method);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_ENTRY_B16419_H */
