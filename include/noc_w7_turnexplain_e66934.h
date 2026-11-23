#ifndef NOC_W7_TURNEXPLAIN_E66934_H
#define NOC_W7_TURNEXPLAIN_E66934_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: turnexplain_persistence_explanation_key @ agent/turn_explainers.py:_persistence_explanation_key */
struct json_t *turnexplain_persistence_explanation_key(const char *cause);
/* PoP: turnexplain_format_turn_completion_explanation @ agent/turn_explainers.py:TurnExplainersMixin._format_turn_completion_explanation */
struct json_t *turnexplain_format_turn_completion_explanation(const char *reason,
                                                              const char *persistence_cause,
                                                              const char *model);
/* PoP: turnexplain_neutralize_footer_paths @ agent/turn_explainers.py:TurnExplainersMixin._neutralize_footer_paths */
struct json_t *turnexplain_neutralize_footer_paths(const char *text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_TURNEXPLAIN_E66934_H */
