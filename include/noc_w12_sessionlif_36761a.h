#ifndef NOC_W12_SESSIONLIF_36761A_H
#define NOC_W12_SESSIONLIF_36761A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_sessionlif_36761a_is_delegate_child_row @ gateway/session_lifecycle.py:is_delegate_child_row
 * Faithful port of is_delegate_child_row(row): parse_model_config(row.get("model_config")),
 * then bool(cfg.get("_delegate_from")) and "_reset_from" not in cfg
 * and not cfg.get("_chat_adopted"). */
int w12_sessionlif_36761a_is_delegate_child_row(const struct json_t *row);

/* PoP: w12_sessionlif_36761a_auto_continue_freshness_window_ex @ gateway/session_lifecycle.py:auto_continue_freshness_window
 * env_value is an injected HERMES_AUTO_CONTINUE_FRESHNESS value (NULL/"" => 3600.0);
 * an unparsable value also falls back to 3600.0. */
double w12_sessionlif_36761a_auto_continue_freshness_window_ex(const char *env_value);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_SESSIONLIF_36761A_H */
