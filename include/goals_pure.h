#ifndef HERMES_GOALS_PURE_H
#define HERMES_GOALS_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque contract/gate structs — use the _new/_free accessors. */
typedef struct goals_contract goals_contract_t;
typedef struct goals_gate goals_gate_t;

goals_contract_t *goals_contract_new(void);
void goals_contract_free(goals_contract_t *c);
int  goals_contract_is_empty(const goals_contract_t *c);
char *goals_contract_to_json(const goals_contract_t *c);
int  goals_contract_from_json(goals_contract_t *c, const char *json_str);
char *goals_contract_render_block(const goals_contract_t *c);

char *goals_parse_contract(const char *text);

goals_gate_t *goals_gate_new(void);
void goals_gate_free(goals_gate_t *g);
char *goals_gate_to_json(const goals_gate_t *g);
int  goals_gate_from_json(goals_gate_t *g, const char *json_str);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_GOALS_PURE_H */
